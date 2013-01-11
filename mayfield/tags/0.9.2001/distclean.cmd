@echo off
setlocal ENABLEEXTENSIONS
pushd .
cd %0\..\
echo -- %CD%
set TDIR=.\DIST
if exist %TDIR% (
 echo Deleting directory %TDIR%
 rmdir /s /q %TDIR%
)

::
::--- RECURSIVELY RUN FOR CHILD DIRECTORIES
::
set TSUBDIR=.\installer
if exist %TSUBDIR% (
 if exist %TSUBDIR%\distclean.cmd (
  pushd . 
  cd %TSUBDIR%
  call .\distclean.cmd
  popd
 )
)

set TSUBDIR=.\installhelper
if exist %TSUBDIR% (
 if exist %TSUBDIR%\distclean.cmd (
  pushd . 
  cd %TSUBDIR%
  call .\distclean.cmd
  popd
 )
)

set TSUBDIR=.\lanscsisystemv2
if exist %TSUBDIR% (
 if exist %TSUBDIR%\distclean.cmd (
  pushd . 
  cd %TSUBDIR%
  call .\distclean.cmd
  popd
 )
)

popd
endlocal
