@echo off
setlocal ENABLEEXTENSIONS
pushd .
cd %0\..\
echo -- %CD%

set TFILE=NetDiskSetup\Binary\ndinst.dll
if exist %TFILE% (
	echo Deleting %TFILE%
	del /q %TFILE%
)

::
::--- RECURSIVELY RUN FOR CHILD DIRECTORIES
::
set TSUBDIR=.\release
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
