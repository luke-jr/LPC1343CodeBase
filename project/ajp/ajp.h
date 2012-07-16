#ifndef __AJP_H__
#define __AJP_H__

extern int8_t ajpRx(uint8_t c);

typedef void (*ajp_pkt_hook_t)(uint8_t*data, uint8_t len);
typedef void (*ajp_hook_t)();
struct ajp_cmd {
	ajp_hook_t init;
	ajp_pkt_hook_t pkt;
	ajp_hook_t finish;
	ajp_hook_t abort;
};
typedef struct ajp_cmd *ajp_cmd_p;
#define CMD(...)  &(struct ajp_cmd){__VA_ARGS__}

extern uint8_t ajpCmdstep;
extern uint8_t ajpCmdreqid, ajpCmdid, ajpCmddev;

extern uint8_t ajpCmdbuf[256];
extern uint8_t ajpCmdbufLen;
extern void ajpCmdbufInit();
extern void ajpCmdbufPkt(uint8_t*data, uint8_t len);

extern void ajpSendPkt(const uint8_t*data, uint8_t dataLen);
extern void ajpSendReplyHdr(uint8_t status);
extern void ajpSendEnd();
extern void ajpSendAbort();
extern void ajpSendReply(uint8_t status, const uint8_t*cmd, uint16_t cmdLen);

#endif
