// display live data from UDP packets

#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <winnt.h>
#include <math.h>
#include <time.h>
#include <synchapi.h>
#include <process.h>
#include <conio.h>

#define NUMX 200
double* x = NULL;
BOOL paused = FALSE;

#define PAUSE_MSG "!!!!!!!!!!!!!!!!!!!! PAUSED  !!!!!!!!!!!!!!!!!!!!!"
#define RUN_MSG   "-------------------- RUNNING ---------------------"
#define PAUSE_COLOR "\x1b[41m"
#define RESET_COLOR "\x1b[0m"

// helper function for windows errors
DWORD GetLastErrorAndPrint() {
  DWORD out = GetLastError();
  //LPCVOID lpSource = NULL;
  LPSTR lpBuffer = NULL; 

  DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER 
                  | FORMAT_MESSAGE_FROM_SYSTEM;
  FormatMessage(dwFlags, NULL, out, LANG_SYSTEM_DEFAULT,
    (LPSTR)&lpBuffer, 0, NULL);
  printf("ErrCode: %d\n", out);
  printf("ErrMsg : %s\n", lpBuffer);
  return out;
}

// get data from UDP packet
// early template generates data in place.
void get_data(double* x1, int i)
{
  double t = 0.01*i;
  x1[0] = t;
  for (int j = 1; j < NUMX; j++) {
    x1[j] = sin(t + j*3.14159/NUMX);
  }
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
  printf("\n");
  printf("\x1b[!p"); // reset stuff
  printf("\x1b[?1049l"); // back to normal screen buffer
  printf("\n"); // force buffer to flush
  if (dwCtrlType < 7) printf("Received ctrl signal: %d\n", dwCtrlType);
  free(x);
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
      }
    }
    Sleep(20);
  }
  _endthread();
}

int main(void)
{

  // first we need to enable vt processing
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) return GetLastErrorAndPrint();
    
  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) return GetLastErrorAndPrint();

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) return GetLastErrorAndPrint();

  x = malloc(NUMX*sizeof(double));
  printf("\x1b[?1049h"); // switch to alternate buffer
  printf("\x1b[?25l"); // hide the cursor
  if (!SetConsoleCtrlHandler(&cleanup, TRUE)) return GetLastErrorAndPrint();

  // handle input in a different thread
  _beginthread(&handleInput, 2048, NULL);


  int nskip = 10;
  for (int i = 0; i < 10000; i+=nskip) {
    get_data(x, i);
    if (inCleanup) break;
 
    printf("\x1b[1G\x1b[0d"); 
    if (!paused) {
      printf(RUN_MSG "\n");
      for (int j = 0; j < NUMX; j++) {  
        if (j % 5 == 0) {
          printf("\r\n");
        }
        printf("x[%3d]=%8.4f  ", j, x[j]);
      }
    } else {
      printf(PAUSE_COLOR PAUSE_MSG RESET_COLOR "\n");
    }
    Sleep(10*nskip);

  }
  if (!inCleanup) cleanup(7);
  return 0;
}
    
