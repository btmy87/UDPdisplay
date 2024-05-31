#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// number of doubles in buffer
#define NUMX 200

// packets will be sent to this address and port
#define SENDADDR "127.0.0.1"
#define SENDPORT 41952

// helper function
DWORD GetLastErrorAndPrint(void);