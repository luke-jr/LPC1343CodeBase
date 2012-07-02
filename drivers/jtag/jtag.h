/**************************************************************************/
/*! 
    @file     jtag.h
    @author   Luke-Jr
    @date     1 July 2012
    @version  0.1
*/
/**************************************************************************/

#ifndef _JTAG_H_
#define _JTAG_H_

#include <stdint.h>

extern void jtagInit();
extern uint8_t jtagDetectPorts();
extern uint8_t jtagClock(uint8_t jtagPort, uint8_t tdo, uint8_t tms);
extern void jtagWrite(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength);
extern void jtagReset(uint8_t jtagPort);
extern int8_t jtagDetect(uint8_t jtagPort);
extern void jtagDR(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength);
extern void jtagIR(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength);
extern void jtagRun(uint8_t jtagPort);

#endif
