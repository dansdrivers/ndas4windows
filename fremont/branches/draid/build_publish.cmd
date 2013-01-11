@echo off
setlocal enableextensions

if /i "%~1" == "official" set XIMETA_OFFICIAL_BUILD=1
if /i "%~1" == "/official" set XIMETA_OFFICIAL_BUILD=1
if /i "%~1" == "-official" set XIMETA_OFFICIAL_BUILD=1

set COPYCMD=/Y
call "%~dp0cleanup_publish.cmd"
call "%~dp0cleanup_libs.cmd"

REM THIS LINES are as a workaround for removing racing 
REM making the directories from publish.js
call :precreate_lib i386
call :precreate_lib amd64

call "%~dp0src\cleanup.cmd" "%~dp0src"

set _BP_OPTS=-cegwMiIBZ
call "%~dp0src\buildp.cmd" -i386 chk %_BP_OPTS%
if errorlevel 1 exit /b

call "%~dp0src\buildp.cmd" -i386 fre %_BP_OPTS%
if errorlevel 1 exit /b

call "%~dp0src\buildp.cmd" -amd64 chk %_BP_OPTS%
if errorlevel 1 exit /b

call "%~dp0src\buildp.cmd" -amd64 fre %_BP_OPTS%
if errorlevel 1 exit /b

call "%~dp0build_postprocess.cmd"

exit /b

:precreate_lib
setlocal
set ARCH=%1
call :precreate_dir "%~dp0\lib\fre\%ARCH%"
call :precreate_dir "%~dp0\lib\fre\kernel\%ARCH%"
call :precreate_dir "%~dp0\lib\chk\%ARCH%"
call :precreate_dir "%~dp0\lib\chk\kernel\%ARCH%"
endlocal
exit /b

:precreate_dir
if not exist "%~1" mkdir "%~1"
exit /b
