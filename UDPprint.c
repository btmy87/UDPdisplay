
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "packetdef.h"
#include "UDPdisplay.h"

const size_t blinkOn = 1;
const size_t blinkOff = 1;

void print_double(char* buf, udp_packet_item* pdef) {
  double val = *( (double*) (buf + pdef->start_byte));

  const char* colorHigh = ESC "[48;2;152;58;18m";
  const char* colorLow = ESC "[48;2;0;80;133m";
  const char* colorNone = ESC "[0m";
  const char* indHighWarn = "HW";
  const char* indHighLimit = "HL";
  const char* indLowWarn = "LW";
  const char* indLowLimit = "LL";
  const char* indNone = "  ";

  // constant color for warnings, blinking for limits
  char* color = (char*) colorNone;
  char* indicator = (char*) indNone;
  BOOL isBlink = (frameCount % (blinkOn+blinkOff)) >= blinkOn;
  if ( val > pdef->lowLimit && val <= pdef->lowWarn) {
    color = (char*) colorLow;
    indicator = (char*) indLowWarn;
  } else if ( val <= pdef->lowLimit) {
    if (isBlink) color = (char*) colorLow;
    indicator = (char*) indLowLimit;
  } else if ( val < pdef->highLimit && val >= pdef->highWarn) {
    color = (char*) colorHigh;
    indicator = (char*) indHighWarn;
  } else if ( val >= pdef->highLimit) {
    if (isBlink) color = (char*) colorHigh;
    indicator = (char*) indHighLimit;
  }

  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf, 
    "%15s =%s%s%s%11.*f  ", pdef->name, color, indicator, colorNone,
    pdef->precision, val);
}

void print_float(char* buf, udp_packet_item* pdef) 
{
  float val = *( (float*) (buf + pdef->start_byte));
  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf,
    "%15s =  %*.*f  ", pdef->name, 11 - pdef->precision,
    pdef->precision, val);

}
void print_i32(char* buf, udp_packet_item* pdef) 
{
  int val = *( (int*) (buf + pdef->start_byte));
  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf,
    "%15s = %12d  ", pdef->name, val);
}
void print_u32(char* buf, udp_packet_item* pdef)
{
  int val = *( (unsigned int*) (buf + pdef->start_byte));
  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf,
    "%15s =   0x%08x  ", pdef->name, val);
}
void print_u16(char* buf, udp_packet_item* pdef)
{
  int val = *( (unsigned short*) (buf + pdef->start_byte));
  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf,
    "%15s =   0x%04x      ", pdef->name, val);
}
void print_u8(char* buf, udp_packet_item* pdef) 
{
  int val = *( (unsigned char*) (buf + pdef->start_byte));
  iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf,
    "%15s =   0x%02x        ", pdef->name, val);
}