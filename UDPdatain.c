// Called from UDP data read thread

#include <process.h>
#include <stdio.h>
#include <time.h>

#include "UDPdatain.h"
#include "UDPdisplay.h"
#include "UDPcommon.h"

#pragma comment(lib, "Ws2_32.lib")


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

// get data from UDP packet
void get_data()
{
  const int nbytesExpected = NUMX*sizeof(double);
  int nbytes = recvfrom(sock, (char*)x, nbytesExpected, 0, 
                        (struct sockaddr*) &srcaddr, &srcaddrLen);
  if (nbytes == SOCKET_ERROR) {
    reset_console();
    printf_s("Error during recvfrom:\n");
    WSAGetLastErrorAndPrint();
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
  } else if (nbytes != nbytesExpected) {
    printf_s("Received %d bytes, expected %d bytes\n", 
      nbytes, nbytesExpected);
  }
  _time32(&recvClock); // save the time we got the last message
}

void __cdecl handleUDPInput(void* in)
{
  (void) in; // unused parameter
  while (TRUE) {
    // wait until we can get data Mutex
    DWORD waitResult = WaitForSingleObject(xMutex, INFINITE);

    // get the data
    if (inCleanup) break;
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
  }
  _endthread();
}