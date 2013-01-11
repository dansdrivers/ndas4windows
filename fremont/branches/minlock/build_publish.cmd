@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0

:procarg

if /i "%~1" equ "official"  (set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)
if /i "%~1" equ "/official" (set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)
if /i "%~1" equ "-official" (set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)

if /i "%~1" equ "/nocleanup" (set BUILDPUB_NO_CLEAN_BUILD=1 && shift && goto procarg)
if /i "%~1" equ "-nocleanup" (set BUILDPUB_NO_CLEAN_BUILD=1 && shift && goto procarg)

if /i "%~1" equ "/voice" (set BUILDPUB_USE_VOICE=1 && shift && goto procarg)
if /i "%~1" equ "-voice" (set BUILDPUB_USE_VOICE=1 && shift && goto procarg)

set COPYCMD=/Y

if not defined BUILDPUB_NO_CLEAN_BUILD (
call :say cleaning build
)

if not defined BUILDPUB_NO_CLEAN_BUILD (
call "%SCRIPT_DIR%cleanup_publish.cmd"
call "%SCRIPT_DIR%cleanup_libs.cmd"
)

REM THIS LINES are as a workaround for removing racing 
REM making the directories from publish.js
call :precreate_lib i386
call :precreate_lib amd64

if not defined BUILDPUB_NO_CLEAN_BUILD (
call "%SCRIPT_DIR%src\cleanup.cmd" "%SCRIPT_DIR%src"
)

if not defined BUILDPUB_NO_CLEAN_BUILD (
set _BP_OPTS=-cegwMiIBZ
)

call :say building checked i.3 8 6

call "%SCRIPT_DIR%src\buildp.cmd" -i386 chk %_BP_OPTS%
if errorlevel 1 (
   call :say An error occurred. Build halted.
   exit /b
)

call :say building fre i.3 8 6

call "%SCRIPT_DIR%src\buildp.cmd" -i386 fre %_BP_OPTS%
if errorlevel 1 (
   call :say An error occurred. Build halted.
   exit /b
)

call :say building checked a.m.d.6 4

call "%SCRIPT_DIR%src\buildp.cmd" -amd64 chk %_BP_OPTS%
if errorlevel 1 (
   call :say An error occurred. Build halted.
   exit /b
)

call :say building free a.m.d.6 4

call "%SCRIPT_DIR%src\buildp.cmd" -amd64 fre %_BP_OPTS%
if errorlevel 1 (
   call :say An error occurred. Build halted.
   exit /b
)

call :say post processing

call "%SCRIPT_DIR%build_postprocess.cmd"

call :say build completed.

exit /b

:precreate_lib
setlocal
set ARCH=%1
call :precreate_dir "%SCRIPT_DIR%\lib\fre\%ARCH%"
call :precreate_dir "%SCRIPT_DIR%\lib\fre\kernel\%ARCH%"
call :precreate_dir "%SCRIPT_DIR%\lib\chk\%ARCH%"
call :precreate_dir "%SCRIPT_DIR%\lib\chk\kernel\%ARCH%"
endlocal
exit /b

:precreate_dir
if not exist "%~1" mkdir "%~1"
exit /b

:say
if defined BUILDPUB_USE_VOICE (
start say %*
)
exit /b
