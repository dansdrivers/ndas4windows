@echo off
setlocal ENABLEEXTENSIONS
pushd .
cd %0\..\
echo -- %CD%

set TDIR=.\EXE
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

set TDIR=.\SYS
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

set TDIR=.\LIB
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

popd
endlocal
