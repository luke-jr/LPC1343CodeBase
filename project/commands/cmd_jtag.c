/**************************************************************************/
/*!
    @file     cmd_jtag.c
    @author   Luke-Jr
*/
/**************************************************************************/
#include <stdio.h>

#include "core/cmd/cmd.h"
#include "project/commands.h"       // Generic helper functions
#include "drivers/jtag/jtag.h"

static int32_t myatoi(const char*s)
{
  int32_t n = 0;
  for ( ; s[0] ; ++s)
  {
    if (s[0] < '0' || s[0] > '9')
      return -1;
    n = (n*10) + (s[0]-'0');
  }
  return n;
}

static int8_t _hex2bin_i(uint8_t c)
{
  if (c >= '0' && c <= '9')
    return c-'0';
  if (c >= 'a' && c <= 'f')
    return 10+(c-'a');
  if (c >= 'A' && c <= 'F')
    return 10+(c-'A');
  return -1;
}

static int32_t hex2bin(uint8_t*s)
{
  int8_t c, c2, pad;
  uint32_t i, j;
  j = 0;
  pad = 0;
  for (i=0; s[j]; ++i)
  {
    if ((c = _hex2bin_i(s[j++])) == -1)
      return -1;
    if (s[j])
    {
      if ((c2 = _hex2bin_i(s[j++])) == -1)
        return -1;
    }
    else
    {
      c2 = 0;
      pad = 4;
    }
    s[i] = ((uint8_t)c << 4) | c2;
  }
  return (i*8)-pad;
}

static void bin2hex(uint8_t*s, uint32_t bitlen)
{
  static uint8_t b2h[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  uint32_t i, j;
  bitlen = (bitlen + 7) / 8;
  s[bitlen] = '\0';
  for (i=bitlen; i>0; )
  {
    --i;
    j = i*2;
    s[j+1] = b2h[s[i] & 0x0f];
    s[j] = b2h[(s[i] & 0xf0) >> 4];
  }
}

static int8_t parse_opts(char **argv, uint8_t*pjtag, enum jtagreg*pr)
{
  if (argv[0][1] != '\0' || argv[0][0] < '0' || argv[0][1] > '9')
  {
    printf("JTAG Port number out of range 0-9\n");
    return -1;
  }
  *pjtag = argv[0][0]-'0';
  if (!pr)
    return 0;
  if (argv[1][1] != '\0' && (argv[1][2] != '\0' || (argv[1][1] != 'R' && argv[1][1] != 'r')))
  {
badreg:
    printf("JTAG register must be IR or DR (not \"%s\")\n", argv[1]);
    return -1;
  }
  switch (argv[1][0]) {
  case 'I': case 'i':
    *pr = JTAG_REG_IR;
    break;
  case 'D': case 'd':
    *pr = JTAG_REG_DR;
    break;
  default:
    goto badreg;
  }
  return 0;
}

void cmd_jtagwrite(uint8_t argc, char **argv)
{
  uint8_t jtag;
  enum jtagreg r;
  int32_t datalen;

  if (parse_opts(argv, &jtag, &r))
    return;

  datalen = hex2bin((uint8_t*)argv[2]);
  if (datalen < 0)
  {
    printf("Error parsing data (hexadecimal)\n");
    return;
  }
  if (argc > 3)
  {
    if (argv[3][1] != '\0' || argv[3][0] < '0' || argv[3][1] > '7')
    {
      printf("Pad bits must be in range 0-7\n");
      return;
    }
    datalen -= argv[3][0]-'0';
  }
  jtagWrite(jtag, r, (uint8_t*)argv[2], datalen);
  printf("Wrote %u bits to JTAG", (unsigned int)datalen);
}

void cmd_jtagread(uint8_t argc, char **argv)
{
  uint8_t jtag;
  enum jtagreg r;
  int32_t datalen;

  if (parse_opts(argv, &jtag, &r))
    return;

  datalen = myatoi(argv[2]);
  if (datalen < 0)
  {
    printf("Error parsing bit count\n");
    return;
  }

  char buf[(datalen/4)+1];
  if (datalen % 8)
    buf[datalen/8] = '\0';

  jtagRead(jtag, r, (uint8_t*)buf, datalen);

  bin2hex((uint8_t*)buf, datalen);
  printf("Read %u bits from JTAG: %s\n", (unsigned int)datalen, buf);
}

void cmd_jtagreset(uint8_t argc, char **argv)
{
  uint8_t jtag;

  if (parse_opts(argv, &jtag, NULL))
    return;

  jtagReset(jtag);

  printf("Reset JTAG\n");
}

void cmd_jtagrun(uint8_t argc, char **argv)
{
  uint8_t jtag;

  if (parse_opts(argv, &jtag, NULL))
    return;

  jtagRun(jtag);

  printf("Ran JTAG\n");
}
