#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

// number of doubles in buffer
#define NUMX 200

// packets will be sent to this address and port
#define SENDADDR "127.0.0.1"
#define SENDPORT 41952

// Print last windows error and message
DWORD GetLastErrorAndPrint(void);

// Print last WSA error number and message
DWORD WSAGetLastErrorAndPrint(void);