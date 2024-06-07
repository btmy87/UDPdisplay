// define structure used in print data

enum udp_item_type {UDP_DOUBLE, UDP_INT, UDP_UINT, UDP_SHORT, UDP_TIME, UDP_NONE};
typedef struct udp_packet_item {
  char name[16];
  double lowLimit;
  double lowWarn;
  double highWarn;
  double highLimit;
  int width; // width for print field
  int precision; // precision for print field
  enum udp_item_type type;
  int start_byte;
} udp_packet_item;

// define rows and columns to print
#define NROWS 13
#define NCOLS 4

// this structure array will hold our packet
extern udp_packet_item packet[NROWS*NCOLS];