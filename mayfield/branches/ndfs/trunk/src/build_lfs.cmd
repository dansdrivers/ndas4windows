@echo off
setlocal enableextensions
set BUILDER=call %~dp0..\bin\xbuild

:argcheck
if /I "%1-" equ "/x64-" (
   set _BUILD_X64=1
   shift
   goto argcheck
)

if /I "%1-" equ "/k-" (
   set _noerrorstop=1
   shift
   goto argcheck
)


if "%1-" neq "-" (
	set CONF=%1
	shift
) else (
	set CONF=%NDAS_CONF%
)

if "%CONF%-" equ "-" (
	goto usage
)

call :setlog

set PARAM=%1 %2 %3 %4 %5 %6 %7 %8 %9

if "%PARAM%-" == "-" goto usage

::===================================================================
:: w2kfatlib
::===================================================================

if "%_BUILD_X64%" == "1" (
rem skipped
) else (
echo === drivers\w2kfatlib === WNET2K %CONF% %PARAM%
%BUILDER% -WNET2K %CONF% drivers\w2kfatlib %PARAM% %BUILDLOGPARAM%2
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: w2kntfslib
::===================================================================

if "%_BUILD_X64%" == "1" (
rem skipped
) else (
echo === drivers\w2kntfslib ===
%BUILDER% -WNET2K %CONF% drivers\w2kntfslib %PARAM% %BUILDLOGPARAM%3
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wxpfatlib
::===================================================================

if "%_BUILD_X64%" == "1" (
rem skipped
) else (
echo === drivers\wxpfatlib === 
%BUILDER% -WNETXP %CONF% drivers\wxpfatlib %PARAM% %BUILDLOGPARAM%4
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wxpntfslib
::===================================================================

if "%_BUILD_X64%" == "1" (
rem skipped
) else (
echo === drivers\wxpntfslib ===
%BUILDER% -WNETXP %CONF% drivers\wxpntfslib %PARAM% %BUILDLOGPARAM%5
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wnetfatlib
::===================================================================

if "%_BUILD_X64%" == "1" (
echo === drivers\wnetfatlib ===
%BUILDER% -WNET AMD64 %CONF% drivers\wnetfatlib %PARAM% %BUILDLOGPARAM%6
) else (
echo === drivers\wnetfatlib ===
%BUILDER% -WNET %CONF% drivers\wnetfatlib %PARAM% %BUILDLOGPARAM%6
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: lfsfiltlib
::===================================================================

if "%_BUILD_X64%" == "1" (
echo === drivers\lfsfiltlib ===
%BUILDER% -WNET AMD64 %CONF% drivers\lfsfiltlib %PARAM% %BUILDLOGPARAM%7
) else (
echo === drivers\lfsfiltlib ===
%BUILDER% -WNETXP %CONF% drivers\lfsfiltlib %PARAM% %BUILDLOGPARAM%7
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: lfsfilt.sys
::===================================================================

if "%_BUILD_X64%" == "1" (
echo === drivers\lfsfilter ===
%BUILDER% -WNET AMD64 %CONF% drivers\lfsfilter %PARAM% %BUILDLOGPARAM%8
) else (
echo === drivers\lfsfilter ===
%BUILDER% -WNETXP %CONF% drivers\lfsfilter %PARAM% %BUILDLOGPARAM%8
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

endlocal
goto :EOF

:builderr
echo ERROR: Error on build %ERRORLEVEL%
exit /b %ERRORLEVEL%
goto :EOF

:setlog
set BUILDLOGFILE=_build
set BUILDLOGPATH=%~dp0_logs
if "%_BUILD_X64%" equ "1" set BUILDLOGFILE=%BUILDLOGFILE%x64
if /i "%CONF%" equ "chk" set BUILDLOGFILE=%BUILDLOGFILE%d
if not exist %BUILDLOGPATH% mkdir %BUILDLOGPATH%
set BUILDLOGFILE_P=-j %BUILDLOGFILE%
set BUILDLOGPATH_P=-jpath %BUILDLOGPATH%
set BUILDLOGPARAM=%BUILDLOGPATH_P% %BUILDLOGFILE_P%
goto :EOF

:usage
echo Builds LFS-related projects.
echo.
echo build_lfs [/x64] ^<chk^|fre^> [options]
echo.
echo   /x64           x64 build  
echo   ^<chk^|fre^>      checked or free build
echo   build_options  options for build.exe (e.g. -cegZ)
goto :EOF
