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

set TFILE=.\inf\lanscsibus.inf
if exist %TFILE% (
 echo Deleting %TFILE%
 del %TFILE%
)

set TFILE=.\inf\lanscsiminiport.inf
if exist %TFILE% (
 echo Deleting %TFILE%
 del %TFILE%
)

set TFILE=.\inf\netlpx.inf
if exist %TFILE% (
 echo Deleting %TFILE%
 del %TFILE%
)

popd
endlocal
