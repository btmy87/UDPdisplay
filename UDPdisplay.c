// display live data from UDP packets

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// TODO: should probably update
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <wchar.h>
#include <winnt.h>
#include <math.h>
#include <time.h>
#include <conio.h>
#include <winsock2.h>

#include "UDPdisplay.h"
#include "UDPdatain.h"
#include "packetdef.h"

#pragma comment(lib, "Ws2_32.lib")

#include "UDPcommon.h"

char* x = NULL;  // data buffer

clock_t dtDisplay = 200;
BOOL paused = FALSE; // only write from input thread
SOCKET sock = (SOCKET) SOCKET_ERROR;
struct sockaddr_in srcaddr;
int srcaddrLen = sizeof(srcaddr);
__time32_t recvClock;
struct tm recvTime;
char recvTimeStr[26] = "                         ";
clock_t lastRecv = 0;
size_t frameCount = 0;
const size_t blinkOn = 1;
const size_t blinkOff = 1;

#define PAUSE_MSG "!!!!!!!!!!!!!!!!!!!! PAUSED  !!!!!!!!!!!!!!!!!!!!!"
#define RUN_MSG   "-------------------- RUNNING ---------------------"
#define ESC "\x1b"
#define PAUSE_COLOR "\x1b[41m"
#define RESET_COLOR "\x1b[0m"

#define NBUF 16384
char* screenBuf = NULL;
int iBuf = 0; // counter for tracking our location in buffer
BOOL inCleanup = FALSE;

DWORD setup_console(void)
{
  // first we need to enable vt processing
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) return FALSE;
    
  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) return FALSE;

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) return FALSE;
  
  printf_s(ESC "7"); // save cursor position
  printf_s(ESC "[?1049h"); // switch to alternate buffer
  printf_s(ESC "[?25l"); // hide the cursor
  printf_s(ESC "[1;1H"); // set cursor position

  // things are good
  return TRUE;
}

// Ideally, we'll reset the console before printing any
// error message.  But we need to make sure we only do this once.
BOOL consoleReset = FALSE;
void reset_console()
{
  if (!consoleReset) {
    inCleanup = TRUE;
    printf_s("\n");
    printf_s(ESC "[?25h"); // show cursor
    printf_s(ESC "[?1049l"); // back to normal screen buffer
    printf_s(ESC "8"); // reset cursor position
    printf_s("\n"); // force buffer to flush
    consoleReset = TRUE;
  }
}


// register a handler so we can cleanup after a ctrl C
// since this runs in a different thread, we use the
// inCleanup variable to stop execution in the main thread
// as soon as we enter cleanup.  Otherwise, we can get strange
// results, with stuff printed to the normal screen buffer
BOOL __stdcall cleanup(DWORD dwCtrlType)
{
  //dwCtrlType; // don't need dwCtrlType
  inCleanup = TRUE;
  reset_console();
  if (dwCtrlType < 7) printf("Received ctrl signal: %d\n", dwCtrlType);

  closesocket(sock);
  WSACleanup();  
  if (x) free(x);
  if (screenBuf) free(screenBuf);
  return FALSE; // let any other handlers run
}

// get input from the user
DWORD get_user_input(void)
{
  DWORD out = EXIT_SUCCESS;
  if (_kbhit()) {
    int inp = _getch();
    if (inp == ' ') {
      paused = paused ? FALSE : TRUE; // toggle paused
    } else if (inp == 'x') {
      // exit program
      out = EXIT_FAILURE;
    } else if (inp == '+') {
      // increase display update dt
      if (dtDisplay <= 480) {
        dtDisplay += 20;
      } else {
        dtDisplay = __min(5000, (dtDisplay+100));
      }
    } else if (inp == '-') {
      // decrease display update dt
      if (dtDisplay <= 520) {
        dtDisplay = __max(20, (dtDisplay-20));
      } else {
        dtDisplay -= 100;
      }
    } else if (inp == 'c') {
      // clear screen, maybe after a resize?
      // not sure why, but need re-hide cursor
      printf_s(ESC "[2J" ESC "[?25l");
    }
  }
  return out;
}



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
    "%s =%s%s%s%*.*f  ", pdef->name, color, indicator, colorNone,
    pdef->width, pdef->precision, val);
}

// perform initial setup tasks
int initial_setup()
{
  // setup console
  if (!setup_console()) return GetLastErrorAndPrint();
  
  // setup data buffer and mutex
  x = malloc(NUMX*sizeof(double));
  screenBuf = malloc(NBUF);

  // setup socket
  if (setup_socket() == SOCKET_ERROR) {
    if (x) free(x); x = NULL;
    if (screenBuf) free(screenBuf); screenBuf = NULL;
    reset_console();
    return EXIT_FAILURE;
  } 

  // setup ctrl-c handler to cleanup
  if (!SetConsoleCtrlHandler(&cleanup, TRUE)) {
    cleanup(9);
    return GetLastErrorAndPrint();
  }

  lastRecv = clock();
  return EXIT_SUCCESS;
}


void print_header()
{
  static int iRun = 0;
  const char runIndicator[] = {'|', '/', '-', '\\'};

  printf_s(ESC "[2;1H"); // set cursor position  
  // print help message
  printf_s("<space> pause, `x` quit, `+/-` display interval, `c` reset screen\n");
  printf_s("Display updates every %4d ms\n", dtDisplay);
  _localtime32_s( &recvTime, &recvClock);
  asctime_s(recvTimeStr, 26, &recvTime);
  printf_s("Last message from: %15s:%5d at %25s" 
           "Time since last message: %8d ms\n",
    inet_ntoa(srcaddr.sin_addr), ntohs(srcaddr.sin_port),
    recvTimeStr, clock() - lastRecv);
  if (paused) {
    printf(PAUSE_COLOR PAUSE_MSG RESET_COLOR "\n");
  } else {
    iRun = (iRun + 1) % 4;
    printf(RUN_MSG " %c\n", runIndicator[iRun]);
  }
}

void print_data()
{
  frameCount++;
  iBuf = 0;
  for (int i = 0; i < NROWS*NCOLS; i++) {
    if (i % NCOLS == 0) {
      iBuf += sprintf_s(screenBuf+iBuf, NBUF-iBuf, "\n");
    }
    if (packet[i].type == UDP_DOUBLE) print_double(x, &packet[i]);
    
  }
  if (inCleanup) return; // in case someone started cleanup
  printf_s("%s", screenBuf);
}

int main(void)
{
  if (initial_setup() != EXIT_SUCCESS) return EXIT_FAILURE;

  clock_t lastFrame = clock();   // minor frame for polling input
  clock_t nextFrame = lastFrame;
  clock_t lastPrint = lastFrame; // longer frame for printing data
  clock_t nextPrint = lastFrame;
  while (TRUE) {
    DWORD nSleep = __max(0, (nextFrame - clock()));
    Sleep(nSleep);
    lastFrame = nextFrame;
    nextFrame = lastFrame + 50;
    if (get_user_input()) break;
    if (get_data()) break;
    print_header();
    if (clock() >= nextPrint) {
      if (!paused) print_data();
      lastPrint = nextPrint;
      nextPrint = lastPrint + dtDisplay;
    }   
  }
  if (!inCleanup) cleanup(7);
  return 0;
}
    
