
#include <stdio.h>

#include "UDPcommon.h"

// helper function for windows errors
DWORD GetLastErrorAndPrint(void) {
  DWORD out = GetLastError();
  LPSTR lpBuffer = NULL; 

  DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER 
                  | FORMAT_MESSAGE_FROM_SYSTEM;
  FormatMessage(dwFlags, NULL, out, LANG_SYSTEM_DEFAULT,
    (LPSTR)&lpBuffer, 0, NULL);
  printf_s("ErrCode: %d\n", out);
  printf_s("ErrMsg : %s\n", lpBuffer);
  return out;
}