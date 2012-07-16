#include <stdint.h>

#include "drivers/jtag/jtag.h"

#include "ajp.h"

/* Format of ajpCmdbuf during _delayloop:
 * [0] - read?
 * [1] - delayed byte
 * [2] - options
 */

typedef void (*_delayed_t)(uint8_t*data, uint8_t len);
static void _delayloop(_delayed_t first, _delayed_t cont, uint8_t*data, uint8_t len)
{
	for ( ; ajpCmdstep < 0x14; ++data, --len)
	{
		++ajpCmdstep;
		switch (ajpCmdstep) {
		case 0x11:
		case 0x12:
			ajpCmdbuf[ajpCmdbufLen++] = data[0];
			break;
		case 0x13:
		{
			uint8_t c = ajpCmdbuf[0];
			ajpCmdbuf[2] = c;
			ajpCmdbuf[0] = c &= 0x10;
			if (c)
				ajpSendReplyHdr(0);
			first(&ajpCmdbuf[1], 1);
			ajpCmdbuf[1] = data[0];
		}
		}
		if (!len)
			return;
	}
	
	// All access is delayed by a byte, since the last needs "finish" set
	--data;  // NOTE: this is overwriting the packet length in buf
	data[0] = ajpCmdbuf[1];
	cont(data, len);
	ajpCmdbuf[1] = data[len];
}


/** c0 -- clock **/

static void clkInit()
{
	ajpCmdbufInit();
	ajpCmdbuf[3] = 0;
}

static void _clkPktCont(uint8_t*data, uint8_t len) {
	uint8_t i, c, x;
	uint8_t firsthalf = ajpCmdbuf[3];
	uint8_t secondhalf = ajpCmdbuf[4];
	uint8_t rvLen = (len + secondhalf) / 2;
	uint8_t rv[rvLen];
	uint8_t*rvp = rv;
	for (i = 0; i < len; ++i)
	{
		c = data[len];
		x = 0;
		if (jtagClock(ajpCmddev, /*tdo=*/c & 0x40, /*tms=*/c & 0x80))  x |= 8;
		if (jtagClock(ajpCmddev, /*tdo=*/c & 0x10, /*tms=*/c & 0x20))  x |= 4;
		if (jtagClock(ajpCmddev, /*tdo=*/c & 0x04, /*tms=*/c & 0x08))  x |= 2;
		if (jtagClock(ajpCmddev, /*tdo=*/c & 0x01, /*tms=*/c & 0x02))  x |= 1;
		if (secondhalf)
			*(rvp++) = firsthalf | x;
		else
			firsthalf = x << 4;
		secondhalf = !secondhalf;
	}
	if (!ajpCmdbuf[0])
		return;
	
	ajpSendPkt(rv, rvLen);
	ajpCmdbuf[3] = firsthalf;
	ajpCmdbuf[4] = secondhalf;
}

static void clkPkt(uint8_t*data, uint8_t len)
{
	_delayloop(_clkPktCont, _clkPktCont, data, len);
}

static void clkFin()
{
	uint8_t c, x = 0;
	uint8_t chopbits = ajpCmdbuf[2] & 3;
	uint8_t firsthalf = ajpCmdbuf[3];
	uint8_t secondhalf = ajpCmdbuf[4];
	c = ajpCmdbuf[1];
	if (                 jtagClock(ajpCmddev, /*tdo=*/c & 0x40, /*tms=*/c & 0x80))  x |= 8;
	if (chopbits <= 2 && jtagClock(ajpCmddev, /*tdo=*/c & 0x10, /*tms=*/c & 0x20))  x |= 4;
	if (chopbits <= 1 && jtagClock(ajpCmddev, /*tdo=*/c & 0x04, /*tms=*/c & 0x08))  x |= 2;
	if (chopbits == 0 && jtagClock(ajpCmddev, /*tdo=*/c & 0x01, /*tms=*/c & 0x02))  x |= 1;
	if (secondhalf)
		c = firsthalf | x;
	else
		c = x << 4;
	if (!ajpCmdbuf[0])
	{
		ajpSendReply(0, NULL, 0);
		return;
	}
	
	ajpSendPkt(&c, 1);
	ajpSendEnd();
}


/** c1 -- read/write register **/

static void _regPktCont(uint8_t*data, uint8_t len);

static void _regPktFirst(uint8_t*data, uint8_t len) {
	if (!(ajpCmdbuf[2] & 0xc0))
	{
		// Continuation
		_regPktCont(data, len);
		return;
	}
	enum jtagreg r = (ajpCmdbuf[2] & 0x40) ? JTAG_REG_IR : JTAG_REG_DR;
	jtagSRead(ajpCmddev, r, data, 8*len);
	if (ajpCmdbuf[0])
		ajpSendPkt(data, len);
}

static void _regPktCont(uint8_t*data, uint8_t len) {
	jtagSReadMore(ajpCmddev, data, 8*len, 0);
	if (ajpCmdbuf[0])
		ajpSendPkt(data, len);
}

static void regPkt(uint8_t*data, uint8_t len)
{
	_delayloop(_regPktFirst, _regPktCont, data, len);
}

static void regFin()
{
	uint8_t chopbits = ajpCmdbuf[2] & 7;
	jtagSReadMore(ajpCmddev, &ajpCmdbuf[1], 8 - chopbits, ajpCmdbuf[2] & 0x20);
	if (ajpCmdbuf[0])
	{
		ajpSendPkt(&ajpCmdbuf[1], 1);
		ajpSendEnd();
	}
	else
		ajpSendReply(0, NULL, 0);
}

static void regAbort()
{
	jtagReset(ajpCmddev);
	if (ajpCmdbuf[0])
		ajpSendAbort();
}


/** c2 -- reset **/

static void rstFin()
{
	jtagReset(ajpCmddev);
	ajpSendReply(0, NULL, 0);
}


/** c3 -- run **/

static void runFin()
{
	uint8_t i;
	for (i = ajpCmdbuf[0]; i > 0; --i)
		jtagRun(ajpCmddev);
	ajpSendReply(0, NULL, 0);
}

const ajp_cmd_p ajp_cmds_jtag[0x10] = {
	CMD(clkInit, clkPkt, clkFin, NULL),
	CMD(ajpCmdbufInit, regPkt, regFin, regAbort),
	CMD(ajpCmdbufInit, ajpCmdbufPkt, rstFin, NULL),
	CMD(ajpCmdbufInit, ajpCmdbufPkt, runFin, NULL),
};
