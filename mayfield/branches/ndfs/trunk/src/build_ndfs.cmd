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
:: ndasfat.sys
::===================================================================

if "%_BUILD_X64%" == "1" (
echo === drivers\ndasfat ===
%BUILDER% -WNET AMD64 %CONF% drivers\ndasfat %PARAM% %BUILDLOGPARAM%_ndasfat
) else (
echo === drivers\ndasfat ===
%BUILDER% -WNETXP %CONF% drivers\ndasfat %PARAM% %BUILDLOGPARAM%_ndasfat
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: ndasntfs.sys
::===================================================================

if "%_BUILD_X64%" == "1" (
:echo === drivers\ndasntfs ===
:%BUILDER% -WNET AMD64 %CONF% drivers\ndasntfs %PARAM% %BUILDLOGPARAM%_ndasntfs
) else (
echo === drivers\ndasntfs ===
%BUILDER% -WNETXP %CONF% drivers\ndasntfs %PARAM% %BUILDLOGPARAM%_ndasntfs
)

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
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
echo Builds NDFS-related projects.
echo.
echo build_ndfs [/x64] ^<chk^|fre^> [options]
echo.
echo   /x64           x64 build  
echo   ^<chk^|fre^>      checked or free build
echo   build_options  options for build.exe (e.g. -cegZ)
goto :EOF
