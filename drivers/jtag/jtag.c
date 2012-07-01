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

// Expects to start at the Capture step, to handle 0-length gracefully
void jtagWrite(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength)
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
    j = data[i];
    jtagClock(jtagPort, j & 0x80, 0);
    jtagClock(jtagPort, j & 0x40, 0);
    jtagClock(jtagPort, j & 0x20, 0);
    jtagClock(jtagPort, j & 0x10, 0);
    jtagClock(jtagPort, j & 0x08, 0);
    jtagClock(jtagPort, j & 0x04, 0);
    jtagClock(jtagPort, j & 0x02, 0);
    jtagClock(jtagPort, j & 0x01, 0);
  }
  j = data[i];
  for (i=0; i<d.rem; ++i)
  {
    jtagClock(jtagPort, j & 0x80, 0);
    j <<= 1;
  }
  jtagClock(jtagPort, j & 0x80, 1);
}

void jtagReset(uint8_t jtagPort)
{
  uint8_t i;
  for (i=0; i<5; ++i)
    jtagClock(jtagPort, 0, 1);
  jtagClock(jtagPort, 0, 0);
}

uint8_t jtagDetect(uint8_t jtagPort)
{
  // TODO: detect more than 1 device
  uint8_t i;
  jtagIR(jtagPort, (const uint8_t*)"\xff", 8);
  jtagClock(jtagPort, 0, 1);  // Select DR
  jtagClock(jtagPort, 0, 0);  // Capture DR
  jtagClock(jtagPort, 0, 0);  // Shift DR
  for (i=0; i<4; ++i)
    jtagClock(jtagPort, 0, 0);
  for (i=0; i<4; ++i)
    if (jtagClock(jtagPort, 1, 0))
      break;
  jtagReset(jtagPort);
  return i == 1;
}

void jtagDR(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength)
{
  jtagClock(jtagPort, 0, 1);  // Select DR
  jtagClock(jtagPort, 0, 0);  // Capture DR
  jtagWrite(jtagPort, data, bitlength);  // Exit1 DR
  jtagClock(jtagPort, 0, 1);  // Update DR
}

void jtagIR(uint8_t jtagPort, const uint8_t data[], uint32_t bitlength)
{
  jtagClock(jtagPort, 0, 1);
  jtagDR(jtagPort, data, bitlength);
}

void jtagRun(uint8_t jtagPort)
{
  jtagClock(jtagPort, 0, 0);
}
