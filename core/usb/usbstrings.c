/**************************************************************************/
/*!
    @file     usbstrings.c
    @author   Luke-Jr
    @date     14 July 2012
    @version  0.1
*/
/**************************************************************************/

#include "core/iap/iap.h"
#include "usb.h"

#ifndef WBVAL
#define WBVAL(x) ((x) & 0xFF),(((x) >> 8) & 0xFF)
#endif

/* USB String Descriptor (optional) */
uint8_t USB_StringDescriptor[74
 + 2*sizeof(CFG_USB_MANUFACTURER)
 + 2*sizeof(CFG_USB_PRODUCT)
 + 2*sizeof(CFG_USB_ALTSET0)
] = {
  /* Index 0x00: LANGID Codes */
  0x04,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  WBVAL(0x0409), /* US English */    /* wLANGID */
};

static inline void _EmbedStr(uint8_t**pos, const char*str, size_t strLen)
{
  uint8_t i;
  *++*pos = strLen*2 + 2;
  *++*pos = USB_STRING_DESCRIPTOR_TYPE;
  for (i = 0; i < strLen; ++i)
  {
    *++*pos = str[i];
    *++*pos = 0;
  }
}

void USB_Init_Descriptors()
{
  IAP_return_t iap_return;
  char buf[65];
  uint8_t*pos = &USB_StringDescriptor[3];
  _EmbedStr(&pos, CFG_USB_MANUFACTURER, sizeof(CFG_USB_MANUFACTURER)-1);
  _EmbedStr(&pos, CFG_USB_PRODUCT, sizeof(CFG_USB_PRODUCT)-1);
  iap_return = iapReadSerialNumber();
  if(iap_return.ReturnCode == 0)
  {
    sprintf(buf, "%08X%08X%08X%08X"
      ,iap_return.Result[0],iap_return.Result[1]
      ,iap_return.Result[2],iap_return.Result[3]
    );
    _EmbedStr(&pos, buf, 32);
  }
  else
    _EmbedStr(&pos, "Unknown", 7);
  _EmbedStr(&pos, CFG_USB_ALTSET0, sizeof(CFG_USB_ALTSET0)-1);
}
