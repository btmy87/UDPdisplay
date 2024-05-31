:: simple build script for now

@echo off
setlocal

:: check for debug build
set DEBUG=0
if /I "%~1" == "debug" set DEBUG=1


set CLOPTS=/nologo /W4 /WX

if "%DEBUG%" == "0" (
  set CLOPTS=%CLOPTS% /O2 /MT
) else (
  set CLOPTS=%CLOPTS% /Zi /MTd /Od
)

@echo on
cl %CLOPTS% UDPdisplay.c
cl %CLOPTS% UDPsource.c