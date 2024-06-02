#pragma once
// Things expected to be called from UDP data thread

#include <stdio.h>

#include "UDPdisplay.h"

// setups up socket and WinSock
DWORD setup_socket();

// new thread will be spawned with this function
// to loop and continuously update new data
void __cdecl handleUDPInput(void* in);