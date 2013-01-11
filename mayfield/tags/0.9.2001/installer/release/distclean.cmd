@echo off
setlocal ENABLEEXTENSIONS
pushd .
cd %0\..\
echo -- %CD%

set TDIR=.\DRIVERS
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

set TDIR=.\PROGRAMFILES
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

set TFILE=.\CD\DEFAULT\AutoRun.exe
if exist %TFILE% (
 echo Deleting file %TFILE%
 erase /q %TFILE%
)

popd
endlocal
