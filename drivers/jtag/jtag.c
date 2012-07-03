/**************************************************************************/
/*! 
    @file     jtag.c
    @author   Luke-Jr
    @date     1 July, 2012
    @version  0.1

    JTAG via GPIO
*/
/**************************************************************************/

#include <stdint.h>
#include <stdlib.h>

#include "core/gpio/gpio.h"

#include "jtag.h"

struct _jtagport {
  uint32_t tck[2];
  uint32_t tms[2];
  uint32_t tdo[2];
  uint32_t tdi[2];
};

static struct _jtagport _jtagports[] = CFG_JTAG_PORTS;

static uint8_t _jtagportCount = sizeof(_jtagports)/sizeof(_jtagports[0]);

void jtagInit()
{
  uint8_t i;
  for (i=0; i<_jtagportCount; ++i)
  {
    struct _jtagport *pi = &_jtagports[i];
    gpioSetValue(pi->tms[0], pi->tms[1], 0);
    gpioSetValue(pi->tdi[0], pi->tdi[1], 0);
    gpioSetValue(pi->tck[0], pi->tck[1], 0);
    gpioSetDir(pi->tck[0], pi->tck[1], gpioDirection_Output);
    gpioSetDir(pi->tms[0], pi->tms[1], gpioDirection_Output);
    gpioSetDir(pi->tdo[0], pi->tdo[1], gpioDirection_Input );
    gpioSetDir(pi->tdi[0], pi->tdi[1], gpioDirection_Output);
    jtagReset(i);
  }
}

uint8_t jtagDetectPorts()
{
  return _jtagportCount;
}

uint8_t jtagClock(uint8_t jtagPort, uint8_t tdo, uint8_t tms)
{
  struct _jtagport *pi = &_jtagports[jtagPort];
  gpioSetValue(pi->tms[0], pi->tms[1], tms);
  gpioSetValue(pi->tdi[0], pi->tdi[1], tdo);
  gpioSetValue(pi->tck[0], pi->tck[1], 0);
  gpioSetValue(pi->tck[0], pi->tck[1], 1);
  return gpioGetValue(pi->tdo[0], pi->tdo[1]);
}

static void _jtagRWBit(uint8_t jtagPort, uint8_t*byte, uint8_t mask, uint8_t tms, uint8_t read)
{
  uint8_t rv = jtagClock(jtagPort, read ? 0 : (byte[0] & mask), tms);
  if (!read)
    return;
  if (rv)
    byte[0] |= mask;
  else
    byte[0] &= ~mask;
}

// Expects to start at the Capture step, to handle 0-length gracefully
static void _jtagLLReadWrite(uint8_t jtagPort, uint8_t data[], uint32_t bitlength, uint8_t read)
{
  uint8_t i, j;
  div_t d;
  
  if (!bitlength)
  {
    jtagClock(jtagPort, 0, 1);
    return;
  }
  
  jtagClock(jtagPort, 0, 0);
  
  // d = div(bitlength - 1, 8);  // NOTE: This hangs for some reason
  --bitlength;
  d.quot = bitlength / 8;
  d.rem  = bitlength % 8;
  
  for (i=0; i<d.quot; ++i)
  {
    _jtagRWBit(jtagPort, &data[i], 0x80, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x40, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x20, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x10, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x08, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x04, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x02, 0, read);
    _jtagRWBit(jtagPort, &data[i], 0x01, 0, read);
  }
  for (j=0; j<d.rem; ++j)
    _jtagRWBit(jtagPort, &data[i], 0x80>>j, 0, read);
  _jtagRWBit(jtagPort, &data[i], 0x80>>j, 1, read);
}

void jtagReset(uint8_t jtagPort)
{
  uint8_t i;
  for (i=0; i<5; ++i)
    jtagClock(jtagPort, 0, 1);
  jtagClock(jtagPort, 0, 0);
}

// Returns -1 for failure, -2 for unknown, or zero and higher for number of devices
int8_t jtagDetect(uint8_t jtagPort)
{
  // TODO: detect more than 1 device
  uint8_t i;
  jtagWrite(jtagPort, JTAG_REG_IR, (uint8_t*)"\xff", 8);
  jtagClock(jtagPort, 0, 1);  // Select DR
  jtagClock(jtagPort, 0, 0);  // Capture DR
  jtagClock(jtagPort, 0, 0);  // Shift DR
  for (i=0; i<4; ++i)
    jtagClock(jtagPort, 0, 0);
  if (jtagClock(jtagPort, 0, 0))
    return -1;
  for (i=0; i<4; ++i)
    if (jtagClock(jtagPort, 1, 0))
      break;
  jtagReset(jtagPort);
  return i < 2 ? i : -2;
}

static void _jtagReadWrite(uint8_t jtagPort, enum jtagreg r, uint8_t data[], uint32_t bitlength, uint8_t read)
{
  jtagClock(jtagPort, 0, 1);  // Select DR
  if (r == JTAG_REG_IR)
    jtagClock(jtagPort, 0, 1);  // Select IR
  jtagClock(jtagPort, 0, 0);  // Capture
  _jtagLLReadWrite(jtagPort, data, bitlength, read);  // Exit1
  jtagClock(jtagPort, 0, 1);  // Update
}

void jtagRead(uint8_t jtagPort, enum jtagreg r, uint8_t data[], uint32_t bitlength)
{
  _jtagReadWrite(jtagPort, r, data, bitlength, 1);
}

void jtagWrite(uint8_t jtagPort, enum jtagreg r, const uint8_t data[], uint32_t bitlength)
{
  _jtagReadWrite(jtagPort, r, (uint8_t*)data, bitlength, 0);
}

void jtagRun(uint8_t jtagPort)
{
  jtagClock(jtagPort, 0, 0);
}
