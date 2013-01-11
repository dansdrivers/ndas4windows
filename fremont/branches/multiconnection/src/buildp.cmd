@echo off
setlocal enableextensions

set _BUILDP_BASE_DIR=%~dp0
set _BUILDP_BIN_DIR=%~dp0..\bin
for %%a in ("%_BUILDP_BIN_DIR%") do set _BUILDP_BIN_DIR=%%~fa
set BUILDER=call "%_BUILDP_BIN_DIR%\xbuild"

set BUILDP_BASE=
set BUILDP_LFS=
set BUILDP_NDFS=
set BUILDP_ARCH=i386

:argcheck
set ARG=%1
call :arg_in /h /help help || ( shift
	 goto usage )
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
call :delete_build_logs %BUILDLOGFILEPATH%

set LAST_BUILD_ERRORLEVEL=0

set PARAM=%1 %2 %3 %4 %5 %6 %7 %8 %9
set PARAM2=%PARAM: =%
if "%PARAM2%" == "" set PARAM=-ei

if /i "%BUILDP_ARCH%" equ "i386" (

	if defined BUILDP_BASE (
call :run_build -WIN2K %CONF% %_BUILDP_BASE_DIR%
	)
	if defined BUILDP_LFS (
REM call :run_build -WIN2K %CONF% %_BUILDP_BASE_DIR%drivers\w2kfatlib
REM call :run_build -WIN2K %CONF% %_BUILDP_BASE_DIR%drivers\w2kntfslib
REM call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\wxpfatlib
REM call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\wxpntfslib
REM call :run_build -WS03   %CONF% %_BUILDP_BASE_DIR%drivers\wnetfatlib
REM call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\lfsfiltlib
call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\lfsfilter
	)
	if defined BUILDP_NDFS (
call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\ndasfat
call :run_build -WINXP %CONF% %_BUILDP_BASE_DIR%drivers\ndasntfs
	)
	if defined BUILDP_DIR (
call :run_build -WINXP %CONF% %BUILDP_DIR%
	)
)

if /i "%BUILDP_ARCH%" equ "amd64" (

	if defined BUILDP_BASE (
call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%
	)
	if defined BUILDP_LFS (
REM call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\wxpfatlib
REM call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\wnetfatlib
REM call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\lfsfiltlib
call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\lfsfilter
	)
	if defined BUILDP_NDFS (
call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\ndasfat
call :run_build -WS03 AMD64 %CONF% %_BUILDP_BASE_DIR%drivers\ndasntfs
	)
	if defined BUILDP_DIR (
call :run_build -WS03 AMD64 %CONF% %BUILDP_DIR%
	)
)

call :delete_build_logs %BUILDLOGFILEPATH%.tmp

exit /b %LAST_BUILD_ERRORLEVEL%

:run_build
if "%LAST_BUILD_ERRORLEVEL%" neq "0" exit /b %LAST_BUILD_ERRORLEVEL%
set PP=%PARAM:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
set PP=%PP:  = %
echo.
echo ^>^>^> %* %PP%^<^<^<
echo.
%BUILDER% %* %BUILDLOGPARAM%.tmp %PARAM%
if not defined _noerrorstop (
   if errorlevel 1 set LAST_BUILD_ERRORLEVEL=%ERRORLEVEL%
)
if exist %BUILDLOGFILEPATH%.tmp.log type %BUILDLOGFILEPATH%.tmp.log >> %BUILDLOGFILEPATH%.log
if exist %BUILDLOGFILEPATH%.tmp.wrn type %BUILDLOGFILEPATH%.tmp.wrn >> %BUILDLOGFILEPATH%.wrn
if exist %BUILDLOGFILEPATH%.tmp.err type %BUILDLOGFILEPATH%.tmp.err >> %BUILDLOGFILEPATH%.err
exit /b %LAST_BUILD_ERRORLEVEL%

:delete_build_logs
if exist %1.log del %1.log
if exist %1.wrn del %1.wrn
if exist %1.err del %1.err
exit /b

:setlog
set BUILDLOGFILE=build
set BUILDLOGPATH=%~dp0
if defined CONF set BUILDLOGFILE=%BUILDLOGFILE%_%CONF%
if defined BUILDP_ARCH set BUILDLOGFILE=%BUILDLOGFILE%_%BUILDP_ARCH%

set BUILDLOGFILE=%BUILDLOGFILE%
if not exist %BUILDLOGPATH% mkdir %BUILDLOGPATH%
set BUILDLOGFILE_P=-j %BUILDLOGFILE%
set BUILDLOGPATH_P=-jpath %BUILDLOGPATH%
set BUILDLOGFILEPATH=%BUILDLOGPATH%\%BUILDLOGFILE%
set BUILDLOGPARAM=%BUILDLOGPATH_P% %BUILDLOGFILE_P%
goto :EOF

:arg_in
for %%a in (%*) do (
	if /i "%ARG%" == "%%a" exit /b 1
)
exit /b 0

:clear_last_error
exit /b 0

:usage
echo usage: buildp [switches] ^<chk^|fre^> [build options]
echo where: [/h] display this message
echo        [/k] ignore errors and continue the next step
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
goto :EOF
