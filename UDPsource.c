// generate stream of UDP packets

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// TODO: should probably update
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <synchapi.h>

#include "UDPcommon.h"

#pragma comment(lib, "Ws2_32.lib")

// control progress bar printing
#define NPRINT 50
#define NLINE 80

double* x; // buffer of doubles
const clock_t DT = CLOCKS_PER_SEC/100; // 100 Hz
const int MAX_SEND = 3600*100; // 1 hours worth of packets

// get data from UDP packet
// early template generates data in place.
void generate_data(double* x1, int i)
{
  double t = 0.01*i;
  x1[0] = t;
  for (int j = 1; j < NUMX; j++) {
    x1[j] = sin(t + j*3.14159/NUMX);
  }
}

// erase console line, assuming we only write to 80 chars
// leave back at start of line
void clear_line() {
  printf_s("\r");
  for (int j = 0; j < NLINE; j++) printf_s(" ");
  printf_s("\r");
}

BOOL inCleanup = FALSE;
BOOL __stdcall cleanup(DWORD dwCtrlType)
{
  (void) dwCtrlType; // unused
  inCleanup = TRUE;
  free(x);
  WSACleanup();
  clear_line();
  printf_s("\n");
  return FALSE; // let any other handlers run
}

int main(void)
{
  // initialize winsock
  WSADATA wsaData;
  int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaStartupResult != 0) {
    printf_s("WSA Startup Failed: %d\n", wsaStartupResult);
    return EXIT_FAILURE;
  }

  // open UDP port
  SOCKET client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (client_socket == SOCKET_ERROR) {
    printf_s("Socket creation failed: %d\n", WSAGetLastError());
    return EXIT_FAILURE;
  }
  

  // create address to send data
  struct sockaddr_in dest;
  memset(&dest, 0, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(SENDPORT);
  dest.sin_addr.S_un.S_addr = inet_addr(SENDADDR);
  int destlen = sizeof(struct sockaddr_in);

  // make a buffer to hold data
  x = calloc(NUMX, sizeof(double));
  int nbytes = (int) NUMX*sizeof(double);

  // attempt to cleanup on ctrl-c
  if (!SetConsoleCtrlHandler(&cleanup, TRUE)) return GetLastErrorAndPrint();

  // Main loop, wait and send packets
  printf_s("Sending test UDP packets to %s: %hu\n", SENDADDR, SENDPORT);
  printf_s("Press Ctrl-C to exit\n");
  clock_t clockLast = clock();
  clock_t clockNext = clockLast;
  for (int i = 0; i < MAX_SEND; i++) {

    // really inefficient sleep, but best timing
    // timing stays at 10 msec regardless of how long everything else takes
    clockNext = clockLast + DT;
    Sleep(__max(clockNext-clock(), 0));
    //while (clockNext > clock()) {}
    clockLast = clockNext;

    // make sure this thread pauses while Ctrl-C cleanup is executed 
    // in another thread
    if (inCleanup) continue;

    // generate data
    generate_data(x, i);
    
    // send data
    int result = sendto(client_socket, (char *)x, nbytes, 0, (struct sockaddr*)&dest, destlen);
    if (result == SOCKET_ERROR) {
      printf_s("Error calling sendto: %d\n", result);
      break;
    }
    // print progress
    if (i % NPRINT == 0) printf_s(".");
    if (i % (NPRINT*NLINE) == 0) clear_line();
    
  }

  free(x);
  WSACleanup();
  return 0;
}