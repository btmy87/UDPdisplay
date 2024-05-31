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

double* x = NULL;  
int nskip = 10; // only write from input thread
BOOL paused = FALSE; // only write from input thread

#define PAUSE_MSG "!!!!!!!!!!!!!!!!!!!! PAUSED  !!!!!!!!!!!!!!!!!!!!!"
#define RUN_MSG   "-------------------- RUNNING ---------------------"
#define PAUSE_COLOR "\x1b[41m"
#define RESET_COLOR "\x1b[0m"

// get data from UDP packet
// early template generates data in place.
void get_data(double* x1, SOCKET sock)
{
  const int nbytesExpected = NUMX*sizeof(double);
  int nbytes = recv(sock, (char*)x1, nbytesExpected, 0);
  if (nbytes == SOCKET_ERROR) {
    printf_s("Error during recv: %d\n", nbytes);
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
  } else if (nbytes != nbytesExpected) {
    printf_s("Received %d bytes, expected %d bytes\n", 
      nbytes, nbytesExpected);
  }
}

void reset_console()
{
  printf("\n");
  printf("\x1b[!p"); // reset stuff
  printf("\x1b[?1049l"); // back to normal screen buffer
  printf("\n"); // force buffer to flush
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
  WSACleanup();
  return FALSE; // let any other handlers run
}

const int KEY_SPACE = ' ';
const int KEY_x = 'x'; 
void __cdecl handleInput(void* in)
{
  (void) in; // unused parameter
  while (TRUE) {
    if (_kbhit()) {
      int inp = _getch();
      if (inp == KEY_SPACE) {
        paused = paused ? FALSE : TRUE; // toggle paused
      } else if (inp == KEY_x) {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
        break;
      } else if (inp == '+') {
        nskip = __min(200, (nskip+1));
      } else if (inp == '-') {
        nskip = __max(1, (nskip-1));
      }
    }
    Sleep(20);
  }
  _endthread();
}

DWORD setup_console(void)
{
  // first we need to enable vt processing
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) return FALSE;
    
  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) return FALSE;

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) return FALSE;
  
  printf_s("\x1b[?1049h"); // switch to alternate buffer
  printf_s("\x1b[?25l"); // hide the cursor

  // things are good
  return TRUE;
}

SOCKET setup_socket()
{
  // setup winsock
  WSADATA wsaData;
  int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaStartupResult != 0) {
    printf_s("WSA Startup Failed: %d\n", wsaStartupResult);
    return (SOCKET) SOCKET_ERROR;
  }

  // create socket
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == SOCKET_ERROR) {
    printf_s("Socket creation failed: %d\n", WSAGetLastError());
    WSACleanup();
    return (SOCKET) SOCKET_ERROR;
  }

  // bind socket
  SOCKADDR_IN addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SENDPORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    printf_s("Error binding socket.\n");
    WSACleanup();
    return (SOCKET) SOCKET_ERROR;
  }
  printf_s("Listening for UDP packets on port %hu.\n", SENDPORT);

  return sock;
}

int main(void)
{

  if (!setup_console()) return GetLastErrorAndPrint();

  SOCKET sock = setup_socket();
  if (sock == SOCKET_ERROR) {
    reset_console();
    return EXIT_FAILURE;
  } 

  x = malloc(NUMX*sizeof(double));
  if (!SetConsoleCtrlHandler(&cleanup, TRUE)) return GetLastErrorAndPrint();

  // handle input in a different thread
  _beginthread(&handleInput, 2048, NULL);

  printf_s("Press <space> to pause.  Press `x` to quit.  Press `+` or `-` to display speed.\n");
  while (TRUE) {
    // need to read all data off socket, even if we can't show it
    for (int i = 0; i < nskip; i++) get_data(x, sock);
    if (inCleanup) break;
 
    printf_s("\x1b[1G\x1b[3d"); // bring cursor back to top
    printf_s("Displaying every %d packet\n", nskip);
    if (!paused) {
      printf(RUN_MSG "\n");
      for (int j = 0; j < NUMX; j++) {  
        if (j % 5 == 0) {
          printf_s("\r\n");
        }
        printf_s("x[%3d]=%8.4f  ", j, x[j]);
      }
    } else {
      printf(PAUSE_COLOR PAUSE_MSG RESET_COLOR "\n");
    }
  }
  if (!inCleanup) cleanup(7);
  return 0;
}
    
