/**************************************************************************/
/*! 
    @file     main.c
    @author   K. Townsend (microBuilder.eu)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2011, microBuilder SARL
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "projectconfig.h"
#include "sysinit.h"

#include "core/gpio/gpio.h"
#include "core/systick/systick.h"

#ifdef CFG_INTERFACE
  #include "core/cmd/cmd.h"
#endif

#ifdef CFG_PRINTF_UART
#include "core/uart/uart.h"
#endif

#ifdef CFG_PRINTF_USBCDC
  #include "core/usbcdc/cdcuser.h"
  #include "core/usbcdc/cdc_buf.h"
  #include "core/usbcdc/usb.h"
  #include "core/usbcdc/usbhw.h"
  static char usbcdcBuf [32];
#endif

#include "drivers/jtag/jtag.h"

void int2bits(uint32_t n, uint8_t*b, uint8_t bits)
{
	uint8_t i;
	for (i=(bits+7)/8; i>0; )
		b[--i] = 0;
	for (i=0; i<bits; ++i)
	{
		if (n & 1)
			b[i/8] |= 0x80 >> (i % 8);
		n >>= 1;
	}
}

uint32_t bits2int(uint8_t*b, uint8_t bits)
{
	uint32_t n, i;
	n = 0;
	for (i=0; i<bits; ++i)
		if (b[i/8] & (0x80 >> (i % 8)))
			n |= 1<<i;
	return n;
}

void bitendianflip(void*n, size_t bits)
{
	int i;
	uint8_t*b = n;
	// FIXME: this should probably work with non-byte boundaries
	bits /= 8;
	for (i=0; i<bits; ++i)
		b[i] = ((b[i] &    1) ? 0x80 : 0)
		     | ((b[i] &    2) ? 0x40 : 0)
		     | ((b[i] &    4) ? 0x20 : 0)
		     | ((b[i] &    8) ? 0x10 : 0)
		     | ((b[i] & 0x10) ?    8 : 0)
		     | ((b[i] & 0x20) ?    4 : 0)
		     | ((b[i] & 0x40) ?    2 : 0)
		     | ((b[i] & 0x80) ?    1 : 0);
}

void checksum(uint8_t*b, uint8_t bits)
{
	uint8_t i;
	uint8_t checksum = 1;
	for(i=0; i<bits; ++i)
	    checksum ^= (b[i/8] & (0x80 >> (i % 8))) ? 1 : 0;
	if (checksum)
		b[i/8] |= 0x80 >> (i % 8);
}

void fpgaGetRegisterAsBytes(uint8_t jtag, uint8_t addr, uint8_t*buf)
{
	jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\x40", 6);
	int2bits(addr, &buf[0], 4);
	checksum(buf, 5);
	jtagWrite(jtag, JTAG_REG_DR, buf, 6);
	jtagRead (jtag, JTAG_REG_DR, buf, 32);
	jtagReset(jtag);
}

uint32_t fpgaGetRegister(uint8_t jtag, uint8_t addr)
{
	uint8_t buf[4];
	fpgaGetRegisterAsBytes(jtag, addr, buf);
	return bits2int(buf, 32);
}

void fpgaSetRegisterFromBytes(uint8_t jtag, uint8_t addr, uint8_t*nv)
{
	uint8_t buf[38];
	jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\x40", 6);
	buf[0] = nv[0];
	buf[1] = nv[1];
	buf[2] = nv[2];
	buf[3] = nv[3];
	int2bits(addr, &buf[4], 4);
	buf[4] |= 8;
	checksum(buf, 37);
	jtagWrite(jtag, JTAG_REG_DR, buf, 38);
	jtagRun(jtag);
}

void fpgaSetRegister(uint8_t jtag, uint8_t addr, uint32_t nv)
{
	uint8_t buf[38];
	jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\x40", 6);
	int2bits(nv, &buf[0], 32);
	int2bits(addr, &buf[4], 4);
	buf[4] |= 8;
	checksum(buf, 37);
	jtagWrite(jtag, JTAG_REG_DR, buf, 38);
	jtagRun(jtag);
}

void muxRx(uint8_t);

void muxPoll()
{
  #if defined CFG_PRINTF_UART
  while (uartRxBufferDataPending())
  {
    uint8_t c = uartRxBufferRead();
    muxRx(c);
  }
  #endif

  #if defined CFG_PRINTF_USBCDC
    int  numBytesToRead, numBytesRead, numAvailByte;
  
    CDC_OutBufAvailChar (&numAvailByte);
    if (numAvailByte > 0)
    {
      numBytesToRead = numAvailByte > 32 ? 32 : numAvailByte;
      numBytesRead = CDC_RdOutBuf (&usbcdcBuf[0], &numBytesToRead);
      int i;
      for (i = 0; i < numBytesRead; i++) 
        muxRx(usbcdcBuf[i]);
    }
  #endif
}

unsigned int lastTick;
int muxWrite(const uint8_t * str, size_t len)
{
  // There must be at least 1ms between USB frames (of up to 64 bytes)
  // This buffers all data and writes it out from the buffer one frame
  // and one millisecond at a time
  #ifdef CFG_PRINTF_USBCDC
//FIXME    if (USB_Configuration)
    {
      size_t p;
      for (p=0; p<len; ++p)
        cdcBufferWrite(*str++);
      // Check if we can flush the buffer now or if we need to wait
      unsigned int currentTick = systickGetTicks();
      if (currentTick != lastTick)
      {
        uint8_t frame[64];
        uint32_t bytesRead = 0;
        while (cdcBufferDataPending())
        {
          // Read up to 64 bytes as long as possible
          bytesRead = cdcBufferReadLen(frame, 64);
          USB_WriteEP (CDC_DEP_IN, frame, bytesRead);
          systickDelay(1);
        }
        lastTick = currentTick;
      }
    }
  #else
    // Handle output character by character in __putchar
    while(*str) __putchar(*str++);
  #endif

  return 0;
}

static enum {
  MUX_NONE,
  MUX_CMD,
  MUX_COMPAT,
  MUX_MHBP,
} muxMode;
static uint8_t step;
static uint32_t elen;

static uint8_t msg[256];
static uint8_t msglen;

#define PRODID "ModMiner Quad v0.4"

uint8_t fpgamax;
uint8_t fpgaidx[5] = {0,0,0,0,0xff};
uint8_t bcs[5];

void muxRx(uint8_t c)
{
	if (muxMode == MUX_NONE)
	{
tryNewMux:
		msglen = 0;
		step = 0;
		if (c == 0xfe)
			muxMode = MUX_MHBP;
		else
		if (c < 0xb)
			muxMode = MUX_COMPAT;
		else
		if (c < 0x80)
			muxMode = MUX_CMD;
	}
	switch (muxMode) {
	case MUX_NONE:
		break;
	case MUX_CMD:
		if (c < 8 || c > 0x7f)
			goto tryNewMux;
		if (cmdRx(c))
			goto muxDone;
		break;
	case MUX_COMPAT:
	{
		// Old ModMiner protocol
		uint8_t jtag;
		msg[msglen++] = c;
		jtag = fpgaidx[msg[1]];
		switch (msg[0]) {
		case 0:  // Ping Pong
			muxWrite((const uint8_t*)"\0", 1);
			goto muxDone;
		case 1:  // Version
			muxWrite((const uint8_t*)PRODID, sizeof(PRODID));
			goto muxDone;
		case 2:  // Get FPGA Count
		{
			uint8_t jtagportCount, jtagport, jtagdevTotal;
			int8_t jtagdevCount;
			jtagdevTotal = 0;
			jtagportCount = jtagDetectPorts();
			for (jtagport = 0; jtagport < jtagportCount; ++jtagport)
			{
				jtagdevCount = jtagDetect(jtagport);
				if (jtagdevCount > 0)
					fpgaidx[jtagdevTotal++] = jtagport;
			}
			fpgamax = jtagdevTotal;
			muxWrite(&jtagdevTotal, 1);
			goto muxDone;
		}
		case 3:  // Read ID Code
		{
			uint8_t idcode[4];
			if (msglen < 2)
				break;
			jtagReset(jtag);
			jtagRead (jtag, JTAG_REG_DR, idcode, 32);
			jtagReset(jtag);
			bitendianflip(idcode, 32);
			muxWrite(idcode, 4);
			goto muxDone;
		}
		case 4:  // Read USER Code
		{
			uint8_t usercode[4];
			if (msglen < 2)
				break;
			jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\x10", 6);
			jtagRead (jtag, JTAG_REG_DR, usercode, 32);
			jtagReset(jtag);
			bitendianflip(usercode, 32);
			muxWrite(usercode, 4);
			goto muxDone;
		}
		case 5:  // Program Bitstream
			switch (step) {
			case 0:
			{
				uint8_t i;
				if (msglen < 6)
					break;
				step = 1;
				msglen = 2;
				elen = msg[2] | ((uint32_t)msg[3] << 8) | ((uint32_t)msg[4] << 16) | ((uint32_t)msg[5] << 24);
				jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\xd0", 6);  // JPROGRAM
				do {
					i = 0xff;  // BYPASS while reading status
					jtagRead(jtag, JTAG_REG_IR, &i, 6);
				} while (i & 8);
				jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\xa0", 6);  // CFG_IN
				muxWrite((const uint8_t*)"\1", 1);
				// NOTE: for whatever reason, the FPGAs don't like immediately filling DR after CFG_IN; therefore, don't try to optimize this
				break;
			}
			case 1:
			{
				if (msglen < 34)
					break;
				muxWrite((const uint8_t*)"\1", 1);
				jtagSWrite(jtag, JTAG_REG_DR, &msg[2], 256);
				step = 2;
				msglen = 2;
				elen -= 32;
				break;
			}
			case 2:
			{
				uint8_t i;
				uint8_t needlen = (elen < 32) ? elen : 32;
				if (msglen < needlen+2)
					break;
				muxWrite((const uint8_t*)"\1", 1);
				elen -= needlen;
				jtagSWriteMore(jtag, &msg[2], 8*needlen, !elen);
				if (elen)
				{
					msglen = 2;
					break;
				}
				// Last data block
				jtagWrite(jtag, JTAG_REG_IR, (const uint8_t*)"\x30", 6);  // JSTART
				for (i=0; i<16; ++i)
					jtagRun(jtag);
				i = 0xff;  // BYPASS
				jtagRead(jtag, JTAG_REG_IR, &i, 6);
				i = (i & 4) ? 1 : 0;
				muxWrite(&i, 1);
				goto muxDone;
			}
			}
			break;
		case 6:  // Set Clock Speed
		{
			uint8_t rv;
			if (msglen < 6)
				break;
			bcs[jtag] = msg[2]>200;
			if (bcs[jtag]) msg[2] = 50;
			fpgaSetRegister(jtag, 0xD, msg[2]);
			rv = !bcs[jtag];
			muxWrite(&rv, 1);
			goto muxDone;
		}
		case 7:  // Read Clock Speed
		{
			uint8_t buf[4];
			if (msglen < 2)
				break;
			fpgaGetRegisterAsBytes(jtag, 0xD, buf);
			bitendianflip(buf, 32);
			muxWrite(buf, 4);
			goto muxDone;
		}
		case 8:  // Send Job
		{
			uint8_t i, j;
			if (msglen < 46)
				break;
			for (i=1, j=2; i<12; ++i, j+=4)
				fpgaSetRegister(jtag, i, msg[j] | (msg[j+1]<<8) | (msg[j+2]<<16) | (msg[j+3]<<24));
			muxWrite((const uint8_t*)"\1", 1);
			goto muxDone;
		}
		case 9:  // Read Nonce
		{
			uint8_t buf[4];
			if (msglen < 2)
				break;
			fpgaGetRegisterAsBytes(jtag, 0xE, buf);
			if (bcs[jtag])
				buf[0] = 0;
			bitendianflip(buf, 32);
			muxWrite(buf, 4);
			goto muxDone;
		}
		case 0xa:  // Read Temperature
		{
			uint8_t i, temp, tt;
			if (msglen < 2)
				break;
			if (msg[1])
			{
				muxWrite((const uint8_t*)"\0", 1);
				goto muxDone;
			}
			temp = 0;
			for (i=0; i<fpgamax; ++i)
			{
				tt = 0;  // FIXME TODO
				if (tt > temp)
					temp = tt;
			}
			muxWrite(&temp, 1);
			goto muxDone;
		}
		}
		break;
	}
	case MUX_MHBP:
		// TODO: MHBP
		break;
	}
	return;

muxDone:
	muxMode = MUX_NONE;
}

/**************************************************************************/
/*! 
    Main program entry point.  After reset, normal code execution will
    begin here.
*/
/**************************************************************************/
int main(void)
{
  // Configure cpu and mandatory peripherals
  systemInit();

  uint32_t currentSecond, lastSecond;
  currentSecond = lastSecond = 0;
  
  while (1)
  {
    // Toggle LED once per second
    currentSecond = systickGetSecondsActive();
    if (currentSecond != lastSecond)
    {
      lastSecond = currentSecond;
      gpioSetValue(CFG_LED_PORT, CFG_LED_PIN, lastSecond % 2);
    }

    muxPoll();
  }

  return 0;
}
