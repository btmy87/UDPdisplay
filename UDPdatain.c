// Called from UDP data read thread

#include <process.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "UDPdatain.h"
#include "UDPdisplay.h"
#include "UDPcommon.h"

#pragma comment(lib, "Ws2_32.lib")


WSAPOLLFD pollfd[1];
DWORD setup_socket(void)
{
  // setup winsock
  WSADATA wsaData;
  int wsaStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaStartupResult != 0) {
    reset_console();
    printf_s("WSA Startup Failed: %d\n", wsaStartupResult);
    return EXIT_FAILURE;
  }

  // create socket
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == SOCKET_ERROR) {
    reset_console();
    printf_s("Socket creation failed.\n");
    WSAGetLastErrorAndPrint();
    WSACleanup();
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }
  printf_s("Listening for UDP packets on port %hu.\n", SENDPORT);
  pollfd[0].fd = sock;
  pollfd[0].events = POLLRDNORM;
  pollfd[0].revents = 0x0000;

  return EXIT_SUCCESS;
}

// get data from UDP packet
DWORD get_data(void)
{
  DWORD out = EXIT_SUCCESS;
  const int nbytesExpected = NUMX*sizeof(double);

  while (TRUE) {
    int hasData = WSAPoll(pollfd, 1, 0);
    if (hasData == SOCKET_ERROR) {
      reset_console();
      printf_s("Error during recvfrom:\n");
      WSAGetLastErrorAndPrint();
      out = EXIT_FAILURE;
      break;
    } else if (hasData > 0) {
      int nbytes = recvfrom(sock, x, nbytesExpected, 0, 
                            (struct sockaddr*) &srcaddr, &srcaddrLen);
      if (nbytes == SOCKET_ERROR) {
        reset_console();
        printf_s("Error during recvfrom:\n");
        WSAGetLastErrorAndPrint();
        out = EXIT_FAILURE;
      } else if (nbytes != nbytesExpected) {
        printf_s("Received %d bytes, expected %d bytes\n", 
          nbytes, nbytesExpected);
          out = EXIT_FAILURE;
      }
      _time32(&recvClock); // save the time we got the last message
      lastRecv = clock();
    } else {
      // no data
      break;
    }
  }
  return out;
}
