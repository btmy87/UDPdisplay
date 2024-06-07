:: simple build script for now

@echo off
setlocal

:: check for debug build
set DEBUG=0
set RELDEBUG=0
if /I "%~1" == "debug" set DEBUG=1
if /I "%~1" == "reldebug" set RELDEBUG=1

set CLOPTS=/nologo /W4 /WX /c
set LINKOPTS=/NOLOGO /INCREMENTAL:NO

:: 3 options
:: - Debug
:: - Release build with debugging info (helps with profiling)
:: - Release (no debugging info)
if "%DEBUG%" == "1" (
  set CLOPTS=%CLOPTS% /Zi /MTd /Od
  set LINKOPTS=%LINKOPTS% /DEBUG
) else if "%RELDEBUG%" == "1" (
  set CLOPTS=%CLOPTS% /O2 /MT /Zi
  set LINKOPTS=%LINKOPTS% /DEBUG /OPT:REF /OPT:ICF
) else (
  set CLOPTS=%CLOPTS% /O2 /MT
  set LINKOPTS=%LINKOPTS% /RELEASE
)

set OBJS=UDPdisplay.obj UDPcommon.obj UDPdatain.obj
set OBJS=%OBJS% packetdef.obj UDPprint.obj

@echo on
cl %CLOPTS% *.c

link %LINKOPTS% %OBJS%
link %LINKOPTS% UDPsource.obj UDPcommon.obj