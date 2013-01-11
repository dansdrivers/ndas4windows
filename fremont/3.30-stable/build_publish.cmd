@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0

:procarg

if /i "%~1" equ "official"  (set SIGNCODE_XIMETA=1 && set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)
if /i "%~1" equ "/official" (set SIGNCODE_XIMETA=1 && set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)
if /i "%~1" equ "-official" (set SIGNCODE_XIMETA=1 && set XIMETA_OFFICIAL_BUILD=1 && shift && goto procarg)

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

if defined BUILDPUB_NO_CLEAN_BUILD (
	set _BP_OPTS=-egwiIB -M 1
	REM set NTNOPCH=1
) else (
	set _BP_OPTS=-cegwMiIBZ
)

if not defined BUILDPUB_NO_CLEAN_BUILD (
	call %SCRIPT_DIR%bin\update_builddate.cmd
)
call %SCRIPT_DIR%bin\update_version.cmd

rem
rem Set BUILD_LOGxxx variables
rem 
set _BUILDPUB_LOGFILE=_build_publish
set BUILDP_LOGFILE=%_BUILDPUB_LOGFILE%_i
set BUILDP_LOGDIR=%SCRIPT_DIR%

set _BUILDPUB_I_LOGPATH=%BUILDP_LOGDIR%%BUILDP_LOGFILE%
set _BUILDPUB_LOGPATH=%BUILDP_LOGDIR%%_BUILDPUB_LOGFILE%

call :delete_log
call :purge_log
call :delete_i_log

call :say building checked i.3 8 6
(call "%SCRIPT_DIR%src\buildp.cmd" -i386 chk %_BP_OPTS%) || goto halt
call :merge_log

call :say building fre i.3 8 6
(call "%SCRIPT_DIR%src\buildp.cmd" -i386 fre %_BP_OPTS%) || goto halt
call :merge_log

call :say building checked a.m.d.6 4
(call "%SCRIPT_DIR%src\buildp.cmd" -amd64 chk %_BP_OPTS%) || goto halt
call :merge_log

call :say building free a.m.d.6 4
(call "%SCRIPT_DIR%src\buildp.cmd" -amd64 fre %_BP_OPTS%) || goto halt
call :merge_log

call :delete_i_log

call :say build completed.

exit /b 0

:halt

call :merge_log
call :delete_i_log

call :say An error occurred. Build halted.
exit /b 2

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

:merge_log
if exist %_BUILDPUB_I_LOGPATH%.log type %_BUILDPUB_I_LOGPATH%.log >> %_BUILDPUB_LOGPATH%.log
if exist %_BUILDPUB_I_LOGPATH%.wrn type %_BUILDPUB_I_LOGPATH%.wrn >> %_BUILDPUB_LOGPATH%.wrn
if exist %_BUILDPUB_I_LOGPATH%.err type %_BUILDPUB_I_LOGPATH%.err >> %_BUILDPUB_LOGPATH%.err
exit /b

:delete_log
if exist %_BUILDPUB_LOGPATH%.log del /q %_BUILDPUB_LOGPATH%.log
if exist %_BUILDPUB_LOGPATH%.wrn del /q %_BUILDPUB_LOGPATH%.wrn
if exist %_BUILDPUB_LOGPATH%.err del /q %_BUILDPUB_LOGPATH%.err
exit /b

:delete_i_log
if exist %_BUILDPUB_I_LOGPATH%.log del /q %_BUILDPUB_I_LOGPATH%.log
if exist %_BUILDPUB_I_LOGPATH%.wrn del /q %_BUILDPUB_I_LOGPATH%.wrn
if exist %_BUILDPUB_I_LOGPATH%.err del /q %_BUILDPUB_I_LOGPATH%.err
exit /b

:purge_log
echo. > %_BUILDPUB_LOGPATH%.log
