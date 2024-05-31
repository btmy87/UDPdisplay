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

#pragma comment(lib, "Ws2_32.lib")

// number of doubles in buffer
const size_t NUMX = 200;
const clock_t DT = CLOCKS_PER_SEC/100; // 100 Hz
const int MAX_SEND = 3600*100; // 1 hours worth of packets

const char SENDADDR[] = "127.0.0.1"; // send to myself for now
const u_short SENDPORT = 41952;

// more accurate sleep function at these times
// quite resource heavy, but it's just for testing
// the display code
void mysleep(clock_t wait)
{
  clock_t clockTarget = clock() + wait;
  while(clockTarget > clock()) { }
  return;
}

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
  double* x = calloc(NUMX, sizeof(double));
  int nbytes = (int) NUMX*sizeof(double);
  for (int i = 0; i < MAX_SEND; i++) {
    // generate data
    generate_data(x, i);
    
    // send data
    int result = sendto(client_socket, (char *)x, nbytes, 0, (struct sockaddr*)&dest, destlen);
    if (result == SOCKET_ERROR) {
      printf_s("Error calling sendto: %d\n", result);
      break;
    }

    // sleep
    mysleep(DT);

    // print progress
    if (i % 100 == 0) printf_s(".");
    if (i % 6000 == 0) printf_s("\n");
  }

  free(x);
  WSACleanup();
  return 0;
}