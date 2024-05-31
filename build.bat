:: simple build script for now

@echo off
setlocal

:: check for debug build
set DEBUG=0
if /I "%~1" == "debug" set DEBUG=1

set CLOPTS=/nologo /W4 /WX /c
set LINKOPTS=/nologo

if "%DEBUG%" == "0" (
  set CLOPTS=%CLOPTS% /O2 /MT
) else (
  set CLOPTS=%CLOPTS% /Zi /MTd /Od
  set LINKOPTS=%LINKOPTS% /DEBUG
)

@echo on
cl %CLOPTS% *.c

link %LINKOPTS% UDPdisplay.obj UDPcommon.obj
link %LINKOPTS% UDPsource.obj UDPcommon.obj