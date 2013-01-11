@echo off
setlocal enableextensions

say build started

if /i "%~1" == "official" set XIMETA_OFFICIAL_BUILD=1
if /i "%~1" == "/official" set XIMETA_OFFICIAL_BUILD=1
if /i "%~1" == "-official" set XIMETA_OFFICIAL_BUILD=1

start say cleaning build

set COPYCMD=/Y
call "%~dp0cleanup_publish.cmd"
call "%~dp0cleanup_libs.cmd"

REM THIS LINES are as a workaround for removing racing 
REM making the directories from publish.js
call :precreate_lib i386
call :precreate_lib amd64

call "%~dp0src\cleanup.cmd" "%~dp0src"

set _BP_OPTS=-cegwMiIBZ

start say building checked i.3 8 6

call "%~dp0src\buildp.cmd" -i386 chk %_BP_OPTS%
if errorlevel 1 (
	start say An error occurred. Build halted!
	exit /b
)

start say building free i.3 8 6

call "%~dp0src\buildp.cmd" -i386 fre %_BP_OPTS%
if errorlevel 1 (
	start say An error occurred. Build halted!
	exit /b
)

start say building checked a.m.d.6 4

call "%~dp0src\buildp.cmd" -amd64 chk %_BP_OPTS%
if errorlevel 1 (
	start say An error occurred. Build halted!
	exit /b
)

start say building free a.m.d.6 4

call "%~dp0src\buildp.cmd" -amd64 fre %_BP_OPTS%
if errorlevel 1 (
	start say An error occurred. Build halted!
	exit /b
)

start say post processing

call "%~dp0build_postprocess.cmd"

start say build completed. Congrats.

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
