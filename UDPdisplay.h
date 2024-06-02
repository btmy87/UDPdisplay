#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include <winsock2.h>

// General program architecture
// 3 threads
// 1. Main / Display - Displays output
// 2. Data input - Reads UDP packets
// 3. User input - Reads user input
// 
// Data input and user input are generally stuck in blocking IO calls

// data buffer
// set by data thread, read by display thread
// access controlled via mutex
#define NUMX 200
extern double* x;  // data buffer
extern HANDLE xMutex; // mutex to control access to data

// pause flag
// used to communicate if the user has paused the data display
// only ever written by user input thread
extern BOOL paused;

// socket for UDP communications
// initialized on main thread
// then only used on data thread
extern SOCKET sock;

// data source information
// written on UDP thread, read on display thread
extern struct sockaddr_in srcaddr;
extern int srcaddrLen;
extern __time32_t recvClock;
extern clock_t lastRecv;  // clock() from last packet

// cleanup flag
// indicates to other threads that we're in the process of shutting down
// attempt to more cleanly exit.
// want to avoid printing data to main screen buffer, and ensure
// all errors are printed to the main scree nbuffer
extern BOOL inCleanup;

// reset_console function
// switches back to main screen buffer
// needs to be called before any error message is printed
void reset_console();