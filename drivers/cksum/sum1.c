/**************************************************************************/
/*! 
    @file     sum1.c
    @author   Luke-Jr
    @date     15 July, 2012
    @version  0.1

    Basic BSD checksum.

    Based on FreeBSD's cksum program.
*/
/**************************************************************************/

#include <stdint.h>

uint16_t csum1(uint16_t sum, uint8_t c)
{
  return ((uint32_t)((sum >> 1) | ((sum & 1) ? 0x8000 : 0)) + (uint32_t)c) & 0xffff;
}
