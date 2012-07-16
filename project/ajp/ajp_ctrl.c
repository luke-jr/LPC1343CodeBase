#include <stdint.h>

#include "drivers/jtag/jtag.h"

#include "ajp.h"

/** e0 -- ping **/

static void pingFin()
{
	ajpSendReply(0, ajpCmdbuf, ajpCmdbufLen);
}


/** e1 -- get device count **/

static void probeFin()
{
	uint8_t jtagportCount, jtagport, jtagdevTotal;
	int8_t jtagdevCount;
	jtagdevTotal = 0;
	jtagportCount = jtagDetectPorts();
	uint8_t rbuf[jtagportCount+1];
	for (jtagport = 0; jtagport < jtagportCount; ++jtagport)
	{
		jtagdevCount = jtagDetect(jtagport);
		if (jtagdevCount > 0)
			rbuf[++jtagdevTotal] = jtagport;
	}
	rbuf[0] = jtagdevTotal;
	ajpSendReply(0, rbuf, jtagdevTotal + 1);
}


/** e5 -- watch/poll **/

static void pollFin()
{
	// TODO
	ajpSendReply(0x80, NULL, 0);
}

const ajp_cmd_p ajp_cmds_ctrl[0x10] = {
	CMD(ajpCmdbufInit, ajpCmdbufPkt, pingFin, NULL),
	CMD(NULL, NULL, probeFin, NULL),
	NULL,
//	{NULL, NULL, hwinfoFin, NULL),
	NULL,
//	{NULL, NULL, swinfoFin, NULL),
	NULL,
//	{NULL, NULL, capsFin, NULL),
	CMD(NULL, NULL, pollFin, NULL),
	//CMD(pollInit, pollPkt, pollFin, pollAbort),
};
