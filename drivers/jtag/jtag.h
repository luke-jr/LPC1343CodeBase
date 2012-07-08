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

#include "sysdefs.h"

extern void jtagInit();
extern uint8_t jtagDetectPorts();
extern uint8_t jtagClock(uint8_t jtagPort, uint8_t tdo, uint8_t tms);
extern void _jtagLLReadWrite(uint8_t jtagPort, uint8_t data[], uint32_t bitlength, uint8_t read, uint8_t stage);
extern void jtagReset(uint8_t jtagPort);
extern int8_t jtagDetect(uint8_t jtagPort);
enum jtagreg {
  JTAG_REG_DR,
  JTAG_REG_IR,
};
extern void _jtagReadWrite(uint8_t jtagPort, enum jtagreg, uint8_t data[], uint32_t bitlen, uint8_t read, uint8_t stage);
#define jtagRead(jtagPort, r, data, bitlen)  _jtagReadWrite(jtagPort, r, data, bitlen, 1, 0xff)
INLINE void jtagWrite(uint8_t jtagPort, enum jtagreg r, const uint8_t data[], uint32_t bitlen) INLINE_POST;
#define jtagSRead(jtagPort, r, data, bitlen)  _jtagReadWrite(jtagPort, r, data, bitlen, 1, 1)
INLINE void jtagSWrite(uint8_t jtagPort, enum jtagreg r, const uint8_t data[], uint32_t bitlen) INLINE_POST;
#define jtagSReadMore(jtagPort, data, bitlen, finish)  _jtagLLReadWrite(jtagPort, data, bitlen, 1, (finish) ? 2 : 0)
INLINE void jtagSWriteMore(uint8_t jtagPort, const uint8_t data[], uint32_t bitlen, uint8_t finish) INLINE_POST;
extern void jtagRun(uint8_t jtagPort);


// Inline wrappers are used to accept const data - while it strips the compiler attribute, it still won't modify it
INLINE void jtagWrite(uint8_t jtagPort, enum jtagreg r, const uint8_t data[], uint32_t bitlen)
{
  _jtagReadWrite(jtagPort, r, (uint8_t*)data, bitlen, 0, 0xff);
}
INLINE void jtagSWrite(uint8_t jtagPort, enum jtagreg r, const uint8_t data[], uint32_t bitlen)
{
  _jtagReadWrite(jtagPort, r, (uint8_t*)data, bitlen, 0, 1);
}
INLINE void jtagSWriteMore(uint8_t jtagPort, const uint8_t data[], uint32_t bitlen, uint8_t finish)
{
  _jtagLLReadWrite(jtagPort, (uint8_t*)data, bitlen, 0, finish ? 2 : 0);
}

#endif
