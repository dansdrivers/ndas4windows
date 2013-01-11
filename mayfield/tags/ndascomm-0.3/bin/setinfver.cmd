@echo off
setlocal ENABLEEXTENSIONS

set SYSFILE=%1
set INFTPLFILE=%2
set INFFILE=%3

if not defined SYSFILE goto usage
if not defined INFTPLFILE goto usage
if not defined INFFILE goto usage

copy /y %INFTPLFILE% %INFFILE% > nul
for /f "usebackq" %%a in (`%0\..\getftime.exe date %SYSFILE%`) do set SYSFDATE=%%a
for /f "usebackq" %%a in (`%0\..\getfver.exe %SYSFILE%`) do set SYSFVER=%%a
set SYSDRIVERVER=%SYSFDATE%,%SYSFVER%
%0\..\editini set %INFFILE% Version DriverVer %SYSDRIVERVER%
echo -- %INFFILE%
echo [Version]
echo DRIVERVER=%SYSDRIVERVER%

goto end
:error
echo Error!

:usage
echo usage: setinfver sysfile inftpl inf
goto end

:end
endlocal
goto :eof
