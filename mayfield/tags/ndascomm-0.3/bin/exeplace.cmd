@echo off
setlocal ENABLEEXTENSIONS

pushd .
cd %0\..\
set CMDDIR=%CD%
popd

set CONF=%1
shift
set SRCFILE=%1 %2 %3 %4 %5 %6 %7 %8 %9
if "%CONF%" == "" goto usage
if "%SRCFILE%" == "" goto usage
set EXEDIR=%CMDDIR%\..\exe

:: OEM Support
if not "%OEM_BUILD%" == "" (
	set EXEDIR=%CMDDIR%\..\..\oem\%OEM_BRAND%\branded\exe
)

echo --- EXEPLACE
echo Arguments: %*
echo EXEDIR  : %EXEDIR%
echo CONF    : %CONF%
echo SRCFILE : %SRCFILE%

if not exist %EXEDIR%\%CONF% mkdir %EXEDIR%\%CONF%

:beginstep
if "%1" == "" goto endstep
if exist %1 echo Copying %1 to %EXEDIR%\%CONF%\
if exist %1 copy %1 %EXEDIR%\%CONF%\
shift
goto beginstep
:endstep

goto end

:err
echo usage: exeplace.cmd configuration file

:end
endlocal
