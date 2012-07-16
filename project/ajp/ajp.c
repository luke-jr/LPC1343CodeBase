#include <stdint.h>
#include <stdio.h>

#include "sysdefs.h"
#include "drivers/cksum/cksum.h"

#include "ajp.h"

static uint8_t pktstep;
static uint8_t buf[244];
static uint8_t bufLen;
static uint16_t bufSum;

uint8_t ajpCmdstep;
uint8_t ajpCmdreqid, ajpCmdid, ajpCmddev;

static ajp_pkt_hook_t pktHook;
static ajp_hook_t abortHook;
static ajp_hook_t finishHook;

static void hookResetAndDo(ajp_hook_t h)
{
	pktHook = NULL;
	abortHook = NULL;
	finishHook = NULL;
	ajpCmdstep = 0;
	if (h)  h();
}

static void _noopPkt(uint8_t*data, uint8_t len)
{
}

extern const ajp_cmd_p ajp_cmds_jtag[];
extern const ajp_cmd_p ajp_cmds_ctrl[];

void ajpPkt()
{
	uint8_t bufPos, c;
	for (bufPos = 1; bufPos < bufLen; ++bufPos)
	{
		if (ajpCmdstep >= 0x10)
		{
			// NOTE: jtag:regPkt depends on being able to change/use arg[0][-1]
			pktHook(&buf[bufPos], bufLen - bufPos);
			return;
		}
		++ajpCmdstep;
		c = buf[bufPos];
		switch (ajpCmdstep) {
		case 1:  // Request ID
			ajpCmdreqid = c;
			break;
		case 2:  // Command ID
		{
			ajpCmdid = c;
			ajp_cmd_p cmd_p;
			switch (c & 0xf0) {
			case 0xc0:
				cmd_p = ajp_cmds_jtag[c & 0xf];
				break;
			case 0xe0:
				cmd_p = ajp_cmds_ctrl[c & 0xf];
				break;
			default:
				cmd_p = NULL;
			}
			struct ajp_cmd cmd = {0};
			if (cmd_p)
				cmd = *cmd_p;
			if (cmd.init)
				cmd.init();
			pktHook = cmd.pkt;
			if (!pktHook)
				pktHook = _noopPkt;
			abortHook = cmd.abort;
			finishHook = cmd.finish;
			break;
		}
		case 3:  // Device number
			ajpCmddev = buf[bufPos];
			break;
		case 4:  // Status (always 00, ignored)
			ajpCmdstep = 0x10;
		}
	}
}

// Returns:
//   -1 -- Character invalid/rejected
//   0 -- Character accepted
//   1 -- Character completed packet
int8_t ajpRx(uint8_t c)
{
	switch (pktstep) {
	case 0: case 1: case 2: case 3:
	{
		// Magic sequence
		const uint8_t magic[] = {0xfd, 0x41, 0x4a, 0x50};
		if (c == magic[pktstep])
			++pktstep;
		else
		if (c == 0xfd)
			pktstep = 1;
		else
		{
			pktstep = 0;
			return -1;
		}
		return 0;
	}
	case 4:
		// Length/type
		bufSum = csum1(0xe0a4, c);
		buf[0] = c;
		bufLen = 1;
		if (c && c < 0xf0)
			++pktstep;
		else
			// No data to get
			pktstep += 2;
		return 0;
	case 5:
		// Loading data
		bufSum = csum1(bufSum, c);
		buf[bufLen] = c;
		if (bufLen == buf[0])
			++pktstep;
		++bufLen;
		return 0;
	case 6:
		buf[bufLen] = c;
		++pktstep;
		return 0;
	case 7:
		if (bufSum != ((((uint16_t)buf[bufLen]) << 8) | c))
		{
			// Bad checksum
			pf_write("\xfd\x41\x4a\x50\xfd\x71\x4f", 7);
			goto pktDone;
		}
		switch (buf[0]) {
		case 0xef:
			// We send this *before* processing the packet, so the host always
			// keeps the next packet queued in the USB layer
			pf_write("\xfd\x41\x4a\x50\xfe\x71\x50", 7);  // ACK
			break;
		case 0:  // Abort
			hookResetAndDo(abortHook);
		case 0xfd:  // Bad packet
		case 0xfe:  // ACK
			break;
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc:
			// Reserved
			pf_write("\xfd\x41\x4a\x50\xfd\x71\x4f", 7);
		case 0xff:
			// End of command
			hookResetAndDo(finishHook);
			break;
		default:
			ajpPkt();
			break;
		}
		goto pktDone;
	}
	// This should be impossible
	return -1;

pktDone:
	pktstep = 0;
	return 1;
}


void ajpSendPkt(const uint8_t*data, uint8_t dataLen)
{
	uint8_t pkthdr[5] = "\xfd\x41\x4a\x50", i;
	uint16_t sum;
	pkthdr[4] = dataLen;
	pf_write(pkthdr, sizeof(pkthdr));
	pf_write(data, dataLen);
	sum = csum1(0xe0a4, dataLen);
	for (i = 0; i < dataLen; ++i)
		sum = csum1(sum, data[i]);
	pkthdr[0] = (sum >> 8) & 0xff;
	pkthdr[1] = (sum >> 0) & 0xff;
	pf_write(pkthdr, 2);
}

void ajpSendReplyHdr(uint8_t status)
{
	uint8_t rhdr[4];
	rhdr[0] = ajpCmdreqid;
	rhdr[1] = ajpCmdid & ~0x10;
	rhdr[2] = ajpCmddev;
	rhdr[3] = status;
	ajpSendPkt(rhdr, sizeof(rhdr));
}

void ajpSendEnd()
{
	pf_write("\xfd\x41\x4a\x50\xff\x71\x51", 7);
}

void ajpSendAbort()
{
	pf_write("\xfd\x41\x4a\x50\x00\x70\x52", 7);
}

void ajpSendReply(uint8_t status, const uint8_t*cmd, uint16_t cmdLen)
{
	ajpSendReplyHdr(status);
	for ( ; cmdLen > 239; cmd += 239, cmdLen -= 239)
		ajpSendPkt(cmd, 239);
	if (cmdLen)
		ajpSendPkt(cmd, cmdLen);
	ajpSendEnd();
}


uint8_t ajpCmdbuf[256];
uint8_t ajpCmdbufLen;

void ajpCmdbufInit()
{
	ajpCmdbufLen = 0;
}

void ajpCmdbufPkt(uint8_t*data, uint8_t len)
{
	uint8_t i;
	for (i = 0; i < len && ajpCmdbufLen < 256; ++i)
		ajpCmdbuf[ajpCmdbufLen++] = data[i];
}
