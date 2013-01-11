@echo off
setlocal enableextensions
set BUILDER=call %~dp0..\bin\xbuild

:argcheck
if /I "%1-" equ "/?-" (
	goto usage
)

if /I "%1-" equ "/h-" (
	goto usage
)

if /I "%1-" equ "/help-" (
	goto usage
)

if /I "%1-" equ "/x64-" (
   set _BUILD_X64=1
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
	goto usage;
)

call :setlog

set PARAM=%1 %2 %3 %4 %5 %6 %7 %8 %9

if "%_BUILD_X64%" == "1" (
set _BUILD_BASE_PARAM=-WNET AMD64
) else (
set _BUILD_BASE_PARAM=-WNET2K
)

echo === base build === WNET2K %CONF% %PARAM%
%BUILDER% %_BUILD_BASE_PARAM% %CONF% . %PARAM% %BUILDLOGPARAM%
if errorlevel 1 goto builderr
if "%_BUILD_X64%" == "1" (
call %~dp0build_lfs.cmd /X64 %CONF% %PARAM%
call %~dp0build_ndfs.cmd /X64 %CONF% %PARAM%
) else (
call %~dp0build_lfs.cmd %CONF% %PARAM%
call %~dp0build_ndfs.cmd %CONF% %PARAM%
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
echo Builds all driver projects.
echo.
echo build_all [/x64] ^<chk^|fre^> [build_options]
echo.
echo   /x64           x64 build  
echo   ^<chk^|fre^>      checked or free build
echo   build_options  options for build.exe (e.g. -cegZ)
goto :EOF
