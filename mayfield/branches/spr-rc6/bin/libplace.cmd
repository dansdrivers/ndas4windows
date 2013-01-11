@echo off
setlocal ENABLEEXTENSIONS

pushd .
cd %0\..\
set CMDDIR=%CD%
popd

set CONF=%1
set SRCFILE=%2
if "%CONF%" == "" goto usage
if "%SRCFILE%" == "" goto usage
set LIBDIR=%CMDDIR%\..\lib

echo --- LIBPLACE
echo Arguments: %*
echo LIBDIR  : %LIBDIR%
echo CONF    : %CONF%
echo SRCFILE : %SRCFILE%

if not exist %LIBDIR%\%CONF% mkdir %LIBDIR%\%CONF%

echo Copying %SRCFILE% to %LIBDIR%\%CONF%\
copy %SRCFILE% %LIBDIR%\%CONF%\

goto end

:err
echo usage: libplace.cmd configuration file

:end
endlocal
