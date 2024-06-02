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
#include <synchapi.h>
#include <process.h>
#include <conio.h>
#include <winsock2.h>

#include "UDPdisplay.h"
#include "UDPdatain.h"

#pragma comment(lib, "Ws2_32.lib")

#include "UDPcommon.h"

double* x = NULL;  // data buffer
HANDLE xMutex; // mutex to control access to data

DWORD dtDisplay = 200;
BOOL paused = FALSE; // only write from input thread
SOCKET sock = (SOCKET) SOCKET_ERROR;
struct sockaddr_in srcaddr;
int srcaddrLen = sizeof(srcaddr);
__time32_t recvClock;
struct tm recvTime;
char recvTimeStr[26] = "                         ";
clock_t lastRecv = 0;

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
  CloseHandle(xMutex);
  if (x) free(x);
  if (screenBuf) free(screenBuf);
  return FALSE; // let any other handlers run
}

void __cdecl handleUserInput(void* in)
{
  (void) in; // unused parameter
  while (TRUE) {
    int inp = _getch();
    if (inp == ' ') {
      paused = paused ? FALSE : TRUE; // toggle paused
    } else if (inp == 'x') {
      // exit program
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
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
  _endthread();
}



void print_double(char* buf, char* dispName, int width, int precision,
                  double lowLimit, double lowWarn,
                  double highLimit, double highWarn) {
  double val = *( (double*) buf);
  // calc color based on degree into limit
  double wf = 0.2; // fraction of color to jump to at warning level
  const int LR = 0;  // values at full low limit
  const int LG = 80;
  const int LB = 133;
  const int HR = 152;
  const int HG = 58;
  const int HB = 18;
  
  // don't color between limits
  if (val > lowWarn && val < highWarn) {
    //printf_s("%s= %*.*f  ", dispName, width, precision, val);
    iBuf += sprintf_s(screenBuf+iBuf, NBUF, "%s= %*.*f  ", 
      dispName, width, precision, val);
    return;
  }
  int R = 0;
  int G = 0;
  int B = 0;
  double cf = 0.0;
  if (val < lowWarn) {
    cf = wf + (1.0-wf)*(lowWarn - val)/(lowWarn - lowLimit);
    cf = __min(cf, 1.0);
    R = lround(LR*cf);
    G = lround(LG*cf);
    B = lround(LB*cf);
  } else {
    // must be > highWarn
    cf = wf + (1.0-wf)*(val - highWarn)/(highLimit - highWarn);
    cf = __min(cf, 1.0);
    R = lround(HR*cf);
    G = lround(HG*cf);
    B = lround(HB*cf);
  }
  if (val < lowLimit || val > highLimit) {
    //printf_s(ESC "[4m" "%s= " ESC "[48;2;%d;%d;%dm" "%*.*f" ESC "[0m  " ,
    //  dispName, R, G, B, width, precision, val);
    iBuf += sprintf_s(screenBuf+iBuf, NBUF, 
      ESC "[4m" "%s= " ESC "[48;2;%d;%d;%dm" "%*.*f" ESC "[0m  " ,
      dispName, R, G, B, width, precision, val);
  } else {
    //printf_s("%s= " ESC "[48;2;%d;%d;%dm" "%*.*f" ESC "[0m  " ,
    //  dispName, R, G, B, width, precision, val);
    iBuf += sprintf_s(screenBuf+iBuf, NBUF, 
      "%s= " ESC "[48;2;%d;%d;%dm" "%*.*f" ESC "[0m  " ,
      dispName, R, G, B, width, precision, val);
  }
}

// perform initial setup tasks
int initial_setup()
{
  // setup console
  if (!setup_console()) return GetLastErrorAndPrint();
  
  // setup data buffer and mutex
  x = malloc(NUMX*sizeof(double));
  screenBuf = malloc(NBUF);
  xMutex = CreateMutex(NULL, FALSE, NULL);
  if (xMutex == NULL) {
    if (x) free(x); x = NULL;
    if (screenBuf) free(screenBuf); screenBuf = NULL;
    reset_console();
    return GetLastErrorAndPrint();
  }

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

  // handle user input in a different thread
  if (!_beginthread(&handleUserInput, 0, NULL)) {
    reset_console();
    printf("Error creating user input thread.\n");
    cleanup(10);
    return EXIT_FAILURE;
  }

  // handle UDP input in a different thread
  if(!_beginthread(&handleUDPInput, 0, NULL)) {
    reset_console();
    printf("Error creating UDP input thread.\n");
    cleanup(11);
    return EXIT_FAILURE;
  }
  lastRecv = clock();
  return EXIT_SUCCESS;
}

// wait for next full screen update
// printing packet and status info so we know we're running
void wait_for_screen_update()
{
  // sleep until next update, but want to update
  // display frequence faster so we appear responsive
  static clock_t lastClock = 0;
  static clock_t nextClock = 0;
  static int iRun = 0;
  const char runIndicator[] = {'|', '/', '-', '\\'};

  // initialize clock
  if (lastClock == 0) lastClock = clock();

  nextClock = lastClock + dtDisplay;
  while (TRUE) {
    DWORD dtSleep = __max(0, (nextClock - clock()));
    dtSleep = __min(dtSleep, 50);
    Sleep(dtSleep);
    
    // can print some stuff without the data mutex
    // this helps us feel better that stuff is running
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
      continue;
    }
    iRun = (iRun + 1) % 4;
    printf(RUN_MSG " %c\n", runIndicator[iRun]);

    if (clock() >= nextClock) break;
  }
  lastClock = nextClock;
}

// prints data to screen
void print_data()
{
  iBuf = 0;
  static char dispName[10] = "         ";
  for (int j = 0; j < NUMX; j++) {  
    if (j % 5 == 0) {
      iBuf += sprintf_s(screenBuf+iBuf, NBUF, "\n");
    }
    sprintf_s(dispName, 10, " x[%3d]", j);
    print_double(((char*) x) + sizeof(double)*j, 
      dispName, 8, 4, -0.9, -0.2, 0.9, 0.2);
  }
  if (inCleanup) return; // in case someone started cleanup
  printf_s("%s", screenBuf);
}

int main(void)
{
  if (initial_setup() != EXIT_SUCCESS) return EXIT_FAILURE;

  while (TRUE) {
    if (inCleanup) break;

    wait_for_screen_update();

    // wait for data mutex
    DWORD waitResult = WaitForSingleObject(xMutex, dtDisplay);    
    if (waitResult == WAIT_TIMEOUT) continue; // no new data
    if (waitResult == WAIT_ABANDONED || waitResult == WAIT_FAILED) {
      reset_console();
      if (!ReleaseMutex(xMutex)) GetLastErrorAndPrint();
      printf_s("Error in UDP thread getting mutex, %d\n", waitResult);
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    } 
        
    print_data();

    if (!ReleaseMutex(xMutex)) {
      reset_console();
      printf_s("Error releasing mutex in display thread.\n");
      GetLastErrorAndPrint();
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    }
    
  }
  if (!inCleanup) cleanup(7);
  return 0;
}
    
