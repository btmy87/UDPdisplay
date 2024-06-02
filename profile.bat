:: launch profiling
@echo off
setlocal

:: launch source
start UDPSource.exe

:: start profiling
set proj=UDPdisplay
set vsdiag=VSDiagnostics.exe
set agentconfigs=%VSINSTALLDIR%\Team Tools\DiagnosticsHub\Collector\AgentConfigs
set profid=1
set args=
set args=%args% start %profid%
set args=%args% /launch:%proj%.exe
set args=%args% /loadConfig:"%agentconfigs%\CPUUsageHigh.json"
set outfile=%proj%.diagsession

echo Starting profiling
@echo on
%vsdiag% %args%
@echo off

echo Continue when ready to stop debugging
pause

%vsdiag% stop %profid% /output:%outfile%

:: open output results
devenv %outfile%