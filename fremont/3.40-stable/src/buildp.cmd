@echo off
setlocal enableextensions

REM
REM buildp.cmd invokes xbuild.cmd with appropriate arguments
REM to build base component, lfsfilt, ndasfs, etc.
REM
REM Environment variables:
REM
REM   BUILDP_LOGFILE - file name of the combined log file
REM   BUILDP_LOGDIR  - directory of the log file
REM   BUILDP_DONT_DELETE_LOG - do not delete the log file before the build
REM

set _BUILDP_BASE_DIR=%~dp0
set _BUILDP_BIN_DIR=%~dp0..\bin
for %%a in ("%_BUILDP_BIN_DIR%") do set _BUILDP_BIN_DIR=%%~fa
set BUILDER=call "%_BUILDP_BIN_DIR%\xbuild"

call :debug_message _BUILDP_BASE_DIR=%_BUILDP_BASE_DIR%
call :debug_message _BUILDP_BIN_DIR=%_BUILDP_BIN_DIR%
call :debug_message _BUILDER=%BUILDER%

set BUILDP_BASE=
set BUILDP_LFS=
set BUILDP_NDFS=
set BUILDP_ARCH=i386

:argcheck
set ARG=%1
call :arg_in /h /help help || ( shift
	 goto usage )
call :arg_in /signcode_ximeta -signcode_ximeta || ( shift
	 set SIGNCODE_XIMETA=1
	 goto argcheck )
call :arg_in /x64 -x64 x64 /amd64 -amd64 amd64 || ( shift
	 set BUILDP_ARCH=amd64
	 goto argcheck )
call :arg_in /x86 -x86 x86 /i386 -i386 i386 || ( shift
	 set BUILDP_ARCH=i386
	 goto argcheck )
call :arg_in /k -k || ( shift
	 set _noerrorstop=1
	 goto argcheck )
call :arg_in /lfs -lfs || ( shift
	 set BUILDP_LFS=1
	 goto argcheck )
call :arg_in /ndfs -ndfs || ( shift
	 set BUILDP_NDFS=1
	 goto argcheck )
call :arg_in /base -base || ( shift
	 set BUILDP_BASE=1
	 goto argcheck )
call :arg_in /dir -dir || (
	 set BUILDP_DIR=%~2
	 shift && shift && goto argcheck )
call :clear_last_error

if not defined BUILDP_DIR (
	if not defined BUILDP_BASE (
		if not defined BUILDP_NDFS (
			if not defined BUILDP_LFS (
   	set BUILDP_BASE=1
   	set BUILDP_NDFS=1
  	set BUILDP_LFS=1
))))

if "%1" neq "" (
	set CONF=%1
	shift
) else (
	set CONF=%NDAS_CONF%
)

if "%CONF%" equ "" (
	goto usage
)

set valid_conf=
if /i "%CONF%" equ "fre" set valid_conf=1
if /i "%CONF%" equ "free" set valid_conf=1 && set CONF=fre
if /i "%CONF%" equ "chk" set valid_conf=1
if /i "%CONF%" equ "checked" set valid_conf=1 && set CONF=chk
if not defined valid_conf goto usage

call :setlog

if "%BUILDP_DONT_DELETE_LOG%" == "1" (
	REM do nothing
) else (
	call :delete_build_logs %BUILDP_LOGFILEPATH%
)

set LAST_BUILD_ERRORLEVEL=0

set PARAM=%1 %2 %3 %4 %5 %6 %7 %8 %9
set PARAM2=%PARAM: =%
if "%PARAM2%" == "" set PARAM=-ei

if /i "%BUILDP_ARCH%" equ "i386" (

	if defined BUILDP_BASE (
	
		(call :run_build -WIN2K %CONF% %_BUILDP_BASE_DIR%)		|| goto build_error
		(call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%\drivers)	|| goto build_error
	)
	
	if defined BUILDP_DIR (
	
		(call :run_build -WINXP %CONF% %BUILDP_DIR%) || goto build_error
	)
)

if /i "%BUILDP_ARCH%" equ "amd64" (

	if defined BUILDP_BASE (
	
		(call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%)		|| goto build_error
		(call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%\drivers)	|| goto build_error
	)
	
	if defined BUILDP_DIR (
	
		(call :run_build -WS03 AMD64 %CONF% %BUILDP_DIR%) || goto build_error
	)
)

call :delete_build_logs %BUILDP_LOGFILEPATH%_tmp

exit /b

:build_error

call :delete_build_logs %BUILDP_LOGFILEPATH%_tmp

exit /b %BUILD_ERROR%

:run_build
set PP=%PARAM:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
echo.
echo ^>^>^> %* %PP%^<^<^<
echo.
(%BUILDER% %* %BUILDP_LOGPARAM% %PARAM%) || set BUILD_ERROR=2
if defined _noerrorstop set BUILD_ERROR=0

if exist %BUILDP_LOGFILEPATH%_tmp.log (
	type %BUILDP_LOGFILEPATH%_tmp.log >> %BUILDP_LOGFILEPATH%.log
	del /q %BUILDP_LOGFILEPATH%_tmp.log
)
if exist %BUILDP_LOGFILEPATH%_tmp.wrn (
	type %BUILDP_LOGFILEPATH%_tmp.wrn >> %BUILDP_LOGFILEPATH%.wrn
	del /q %BUILDP_LOGFILEPATH%_tmp.wrn
)
if exist %BUILDP_LOGFILEPATH%_tmp.err (
	type %BUILDP_LOGFILEPATH%_tmp.err >> %BUILDP_LOGFILEPATH%.err
	del /q %BUILDP_LOGFILEPATH%_tmp.err
)

exit /b %BUILD_ERROR%

:delete_build_logs
if exist %1.log del /q %1.log
if exist %1.wrn del /q %1.wrn
if exist %1.err del /q %1.err
exit /b

:setlog
call :debug_message BUILDP_LOGFILE=%BUILDP_LOGFILE%
call :debug_message BUILDP_LOGDIR=%BUILDP_LOGDIR%

if not defined BUILDP_LOGFILE set BUILDP_LOGFILE=_build
if not defined BUILDP_LOGDIR set BUILDP_LOGDIR=%~dp0

call :debug_message BUILDP_LOGFILE=%BUILDP_LOGFILE%
call :debug_message BUILDP_LOGDIR=%BUILDP_LOGDIR%

if not exist %BUILDP_LOGDIR% mkdir %BUILDP_LOGDIR%
set BUILDP_LOGFILE_FLAGS=-j %BUILDP_LOGFILE%_tmp
set BUILDP_LOGDIR_FLAGS=-jpath %BUILDP_LOGDIR%
set BUILDP_LOGFILEPATH=%BUILDP_LOGDIR%\%BUILDP_LOGFILE%
set BUILDP_LOGPARAM=%BUILDP_LOGDIR_FLAGS% %BUILDP_LOGFILE_FLAGS%

call :debug_message BUILDP_LOGFILE_FLAGS=%BUILDP_LOGFILE_FLAGS%
call :debug_message BUILDP_LOGDIR_FLAGS=%BUILDP_LOGDIR_FLAGS%
call :debug_message BUILDP_LOGFILEPATH=%BUILDP_LOGFILEPATH%
call :debug_message BUILDP_LOGPARAM=%BUILDP_LOGPARAM%

exit /b 0

:arg_in
for %%a in (%*) do (
	if /i "%ARG%" == "%%a" exit /b 1
)
exit /b 0

:clear_last_error
exit /b 0

:debug_message
if defined BUILDP_DEBUG echo.%*
exit /b 0

:usage
echo usage: buildp [switches] ^<chk^|fre^> [build options]
echo where: [/h] display this message
echo        [/k] ignore errors and continue the next step
echo.
echo.       [/signcode_ximeta] use ximeta SPC to sign codes
echo.
echo        [/lfs]  build lfs components
echo        [/base] build base components
echo        [/ndfs] build ndfs components
echo        (if no components are specified, all components are selected by default)
echo.
echo        [amd64 ^| /amd64] amd64 build
echo        [i386  ^| /i386 ] i386 build (default)
echo.
echo        [build options] options for build.exe (e.g. -egi)
echo.
echo examples:
echo.
echo  buildp chk        (checked i386 build for all components)
echo  buildp fre -cei   (checked i386 re-build for all components)
echo  buildp /lfs fre   (free i386 build for lfs)
echo  buildp amd64 fre  (free amd64 build for all components)
exit /b 1
