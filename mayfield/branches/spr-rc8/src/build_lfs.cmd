@echo off
setlocal enableextensions
set BUILDER=call ..\bin\xbuild

:argcheck
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
	goto usage;
)

set PARAM=%1 %2 %3 %4 %5 %6 %7 %8 %9

if "%PARAM%-" == "-" goto usage

::===================================================================
:: w2kfatlib
::===================================================================

echo BUILDMSG: %BUILDER% -WNET2K %CONF% drivers\w2kfatlib %PARAM%
%BUILDER% -WNET2K %CONF% drivers\w2kfatlib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: w2kntfslib
::===================================================================

echo BUILDMSG: %BUILDER% -WNET2K %CONF% drivers\w2kntfslib %PARAM%
%BUILDER% -WNET2K %CONF% drivers\w2kntfslib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wxpfatlib
::===================================================================

echo BUILDMSG: %BUILDER% -WNETXP %CONF% drivers\wxpfatlib %PARAM%
%BUILDER% -WNETXP %CONF% drivers\wxpfatlib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wxpntfslib
::===================================================================

echo BUILDMSG: %BUILDER% -WNETXP %CONF% drivers\wxpntfslib %PARAM%
%BUILDER% -WNETXP %CONF% drivers\wxpntfslib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: wnetfatlib
::===================================================================

echo BUILDMSG: %BUILDER% -WNET %CONF% drivers\wnetfatlib %PARAM%
%BUILDER% -WNET %CONF% drivers\wnetfatlib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: lfsfiltlib
::===================================================================

echo BUILDMSG: %BUILDER% -WNETXP %CONF% drivers\lfsfiltlib %PARAM%
%BUILDER% -WNETXP %CONF% drivers\lfsfiltlib %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

::===================================================================
:: lfsfilt.sys
::===================================================================

echo BUILDMSG: %BUILDER% -WNETXP %CONF% drivers\lfsfilter %PARAM%
%BUILDER% -WNETXP %CONF% drivers\lfsfilter %PARAM%

if not defined _noerrorstop (
	if errorlevel 1 goto builderr
)

endlocal
goto :EOF

:builderr
echo ERROR: Error on build %ERRORLEVEL%
goto :EOF

:usage
echo Builds LFS-related projects.
echo.
echo build_lfs [/k] ^<chk^|fre^> [options]
echo.
echo   /k             stop on first error (nmake stop only)
echo   ^<chk^|fre^>      checked or free build
echo   build_options  options for build.exe (e.g. -cegZ)
goto :EOF
