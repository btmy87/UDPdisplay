// display live data from UDP packets

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// TODO: should probably update
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <winnt.h>
#include <math.h>
#include <time.h>
#include <synchapi.h>
#include <process.h>
#include <conio.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

#include "UDPcommon.h"

double* x = NULL;  // data buffer
HANDLE xMutex; // mutex to control access to data

DWORD dtDisplay = 100;
BOOL paused = FALSE; // only write from input thread
SOCKET sock = (SOCKET) SOCKET_ERROR;

#define PAUSE_MSG "!!!!!!!!!!!!!!!!!!!! PAUSED  !!!!!!!!!!!!!!!!!!!!!"
#define RUN_MSG   "-------------------- RUNNING ---------------------"
#define ESC "\x1b"
#define PAUSE_COLOR "\x1b[41m"
#define RESET_COLOR "\x1b[0m"

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
    printf_s("\n");
    printf_s(ESC "[?25h"); // show cursor
    printf_s(ESC "[?1049l"); // back to normal screen buffer
    printf_s(ESC "8"); // reset cursor position
    printf_s("\n"); // force buffer to flush
    consoleReset = TRUE;
  }
}

// get data from UDP packet
void get_data()
{
  const int nbytesExpected = NUMX*sizeof(double);
  int nbytes = recv(sock, (char*)x, nbytesExpected, 0);
  if (nbytes == SOCKET_ERROR) {
    reset_console();
    printf_s("Error during recv:\n");
    WSAGetLastErrorAndPrint();
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
  } else if (nbytes != nbytesExpected) {
    printf_s("Received %d bytes, expected %d bytes\n", 
      nbytes, nbytesExpected);
  }
}


// register a handler so we can cleanup after a ctrl C
// since this runs in a different thread, we use the
// inCleanup variable to stop execution in the main thread
// as soon as we enter cleanup.  Otherwise, we can get strange
// results, with stuff printed to the normal screen buffer
BOOL inCleanup = FALSE;
BOOL __stdcall cleanup(DWORD dwCtrlType)
{
  //dwCtrlType; // don't need dwCtrlType
  inCleanup = TRUE;
  reset_console();
  if (dwCtrlType < 7) printf("Received ctrl signal: %d\n", dwCtrlType);
  free(x);
  CloseHandle(xMutex);
  closesocket(sock);
  WSACleanup();
  return FALSE; // let any other handlers run
}

void __cdecl handleUserInput(void* in)
{
  (void) in; // unused parameter
  while (TRUE) {
    if (_kbhit()) {
      int inp = _getch();
      if (inp == ' ') {
        paused = paused ? FALSE : TRUE; // toggle paused
      } else if (inp == 'x') {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
        break;
      } else if (inp == '+') {
        if (dtDisplay <= 480) {
          dtDisplay += 20;
        } else {
          dtDisplay = __min(5000, (dtDisplay+100));
        }
      } else if (inp == '-') {
        if (dtDisplay <= 520) {
          dtDisplay = __max(0, (dtDisplay-20));
        } else {
          dtDisplay -= 100;
        }
      }
    }
    Sleep(20);
  }
  _endthread();
}

void __cdecl handleUDPInput(void* in)
{
  (void) in; // unused parameter
  while (TRUE) {
    // wait until we can get data Mutex
    DWORD waitResult = WaitForSingleObject(xMutex, INFINITE);

    // get the data
    if (waitResult == WAIT_OBJECT_0) {
      get_data();
    } else if (waitResult == WAIT_ABANDONED) {
      reset_console();
      if (!ReleaseMutex(xMutex)) GetLastErrorAndPrint();
      printf_s("Error in UDP thread getting mutex, WAIT_ABANDONED\n");
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    } else if (waitResult == WAIT_FAILED) {
      reset_console();
      if (!ReleaseMutex(xMutex)) GetLastErrorAndPrint();
      printf_s("Error in UDP thread getting mutex, WAIT_FAILED\n");
      GetLastErrorAndPrint();
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    }

    // release the Mutex
    if (!ReleaseMutex(xMutex)) {
      reset_console();
      printf_s("Error releasing mutex\n");
      GetLastErrorAndPrint();
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    }

    Sleep(5);
  }
  _endthread();
}

DWORD setup_socket()
{
  // setup winsock
  WSADATA wsaData;
  int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaStartupResult != 0) {
    reset_console();
    printf_s("WSA Startup Failed: %d\n", wsaStartupResult);
    return (SOCKET) SOCKET_ERROR;
  }

  // create socket
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == SOCKET_ERROR) {
    reset_console();
    printf_s("Socket creation failed.\n");
    WSAGetLastErrorAndPrint();
    WSACleanup();
    return (SOCKET) SOCKET_ERROR;
  }

  // bind socket
  SOCKADDR_IN addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SENDPORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    reset_console();
    printf_s("Error binding socket.\n");
    WSAGetLastErrorAndPrint();
    closesocket(sock);
    WSACleanup();
    return (SOCKET) SOCKET_ERROR;
  }
  printf_s("Listening for UDP packets on port %hu.\n", SENDPORT);

  return 0;
}

void print_double(char* buf, char* dispName, int width, int precision,
                  double lowLimit, double lowWarn,
                  double highLimit, double highWarn) {
  double val = *( (double*) buf);
  int formatColor = 0;
  if (val <= lowLimit) { formatColor = 44;} // blue background
  else if (val <= lowWarn) {formatColor = 94;} // blue text
  else if (val >= highLimit) {formatColor = 41;} // red background
  else if (val >= highWarn) {formatColor = 33;} // yellow text
  printf_s("%s= " ESC "[%dm%*.*f" ESC "[0m  ", dispName, formatColor, width, precision, val);
  (void) formatColor;
  //printf_s("%s= %*.*f  ", dispName, width, precision, val);
}

int main(void)
{

  // setup console
  if (!setup_console()) return GetLastErrorAndPrint();
  
  // setup data buffer and mutex
  x = malloc(NUMX*sizeof(double));
  xMutex = CreateMutex(NULL, FALSE, NULL);
  if (xMutex == NULL) {
    reset_console();
    return GetLastErrorAndPrint();
  }

  // setup socket
  if (setup_socket() == SOCKET_ERROR) {
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

  printf_s("Press <space> to pause.  Press `x` to quit.  Press `+` or `-` to display speed.\n");
  clock_t lastClock = clock();
  clock_t nextClock = lastClock;
  int iRun = 0;
  char runIndicator[] = {'|', '/', '-', '\\'};
  char dispName[10] = "         ";
  while (TRUE) {
    if (inCleanup) break;

    // sleep until next update, but want to update
    // display frequence faster so we appear responsive
    nextClock = lastClock + dtDisplay;
    for (int i = 0; i < INT_MAX; i++) {
    //while (nextClock > clock()) {
      // force at least one loop
      DWORD dtSleep = __max(0, (nextClock - clock()));
      dtSleep = __min(dtSleep, 50);
      Sleep(dtSleep);
      
      // can print some stuff without the data mutex
      // this helps us feel better that stuff is running
      printf_s(ESC "[3;1H"); // set cursor position
      printf_s("Display updates every %4d ms\n", dtDisplay);
      if (paused) {
        printf(PAUSE_COLOR PAUSE_MSG RESET_COLOR "\n");
        continue;
      }
      iRun = (iRun + 1) % 4;
      printf(RUN_MSG " %c\n", runIndicator[iRun]);

      if (clock() >= nextClock) break;
    }
    lastClock = nextClock;

    // wait for data mutex
    DWORD waitResult = WaitForSingleObject(xMutex, dtDisplay);    
    if (waitResult == WAIT_TIMEOUT) {
      // nothing new to display
      continue;
    } else if (waitResult == WAIT_ABANDONED) {
      reset_console();
      if (!ReleaseMutex(xMutex)) GetLastErrorAndPrint();
      printf_s("Error in UDP thread getting mutex, WAIT_ABANDONED\n");
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    } else if (waitResult == WAIT_FAILED) {
      if (!ReleaseMutex(xMutex)) GetLastErrorAndPrint();
      reset_console();
      printf_s("Error in UDP thread getting mutex, WAIT_FAILED\n");
      GetLastErrorAndPrint();
      GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
      break;
    }
    
    // print data
    for (int j = 0; j < NUMX; j++) {  
      if (j % 5 == 0) {
        printf_s("\r\n");
      }
      //printf_s("x[%3d]=%8.4f  ", j, x[j]);
      sprintf_s(dispName, 10, "x[%3d]", j);
      print_double(((char*) x) + sizeof(double)*j, 
        dispName, 8, 4, -0.9, -0.7, 0.9, 0.7);
    }
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
    
