@echo off
setlocal ENABLEEXTENSIONS

set SYSFILE=%1
set INFTEMPLATE=%2
set INFFILE=%3
set CATFILE=%4

if not defined SYSFILE goto usage
if not defined INFTEMPLATE goto usage
if not defined INFFILE goto usage
:: catfile is optional

if "%_BUILDARCH%"=="" set _BUILDARCH=X86
if not exist "%INFTEMPLATE%" echo BUILDMSG: WARNING! File "%INFTEMPLATE%" does not exist!&& exit /b 1
::
:: Preprocessing $ARCH$ -> X86, AMD64 or other architectures
::
%~dp0sed.exe -e s/\$ARCH\$/%_BUILDARCH%/ %INFTEMPLATE% > %INFFILE%

::
:: Set inf version and date
::
for /f "usebackq" %%a in (`%~dp0getftime.exe date %SYSFILE%`) do set SYSFDATE=%%a
for /f "usebackq" %%a in (`%~dp0getfver.exe %SYSFILE%`) do set SYSFVER=%%a
set SYSDRIVERVER=%SYSFDATE%,%SYSFVER%
%~dp0editini set %INFFILE% Version DriverVer %SYSDRIVERVER%
echo -- %INFFILE%
echo [Version]
echo DRIVERVER=%SYSDRIVERVER%

::
:: Generate an empty cat file
::
if defined CATFILE echo empty catalog > "%CATFILE%"

exit /b 0

:error
echo Error!

:usage
echo usage: setinfver sysfile inftpl inf
exit /b 1
goto end

