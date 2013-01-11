@echo off

setlocal enableextensions

if /I "%1" EQU "--?" goto usage
if /I "%1" EQU "--h" goto usage

::
:: Target Environment
::

if /I "%1" EQU "-WNET2K" (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=W2K
    shift
    echo WNET2K
) else if /I "%1" EQU "-WNET" (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=WNET
    shift
    echo WNET
) else if /I "%1" EQU "-WNETXP" (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=WXP
    shift
    echo WNETXP
) else (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=W2K
    echo WNET2K : default
)

if not defined BASEDIR (
	goto end
)

if not exist "%BASEDIR%" (
	echo error: DDK Base Directory %BASEDIR% does not exists.
	goto end
)

::
:: Target Architecture
::

if /I "%1" EQU "IA64" (
    set BLDARCH=64
    shift
    echo IA64
) else if /I "%1" EQU "AMD64" (
    set BLDARCH=AMD64
    shift
    echo AMD64
) else if /I "%1" EQU "X86" (
    set BLDARCH=
    shift
    echo X86
) else (
    set BLDARCH=
    echo X86 : default
)

::
:: Build Configuration
::

if /I "%1" EQU "chk" (
    set BLDCONFIG=chk
    shift
    echo CHK
) else if /I "%1" EQU "fre" (
    set BLDCONFIG=fre
    shift
    echo FRE
) else if /I "%1" EQU "both" (
    set BLDCONFIG=both
    shift
    echo BOTH
) else (
    set BLDCONFIG=chk
    echo CHK : default
)

::
:: Target Directory
::
set BLDDIR=.
set BLDDIR2=%1
if defined BLDDIR2 (
    if /I "%BLDDIR2:~0,1%" NEQ "-" (
        if exist "%BLDDIR2%" (
            set BLDDIR=%BLDDIR2%
            shift
        )
    )
)

if not exist %BASEDIR%\bin\setenv.bat (
    echo error: %BASEDIR%\bin\setenv.bat does not exists.
)

if /I "%BLDCONFIG%" EQU "both" (
    set BLDCONFIG=chk
    call :build %1 %2 %3 %4 %5 %6 %7 %8 %9
    if errorlevel 1 goto err_on_build
    set BLDCONFIG=fre
    call :build %1 %2 %3 %4 %5 %6 %7 %8 %9
    if errorlevel 1 goto err_on_build
) else (
    call :build %1 %2 %3 %4 %5 %6 %7 %8 %9
    if errorlevel 1 goto err_on_build
)

goto end

:err_on_build
echo XBUILD: Error On Build! ERRORLEVEL: %ERRORLEVEL%
goto end

:end
endlocal
goto :EOF


:build
setlocal
pushd .
pushd .

echo XBUILD: BASEDIR=%BASEDIR%
echo XBUILD: BLDARCH=%BLDARCH%
echo XBUILD: BLDCONFIG=%BLDCONFIG%
echo XBUILD: BLDPLATFORM=%BLDPLATFORM%
echo XBUILD: BLDDIR=%BLDDIR%
echo XBUILD: BLDENVCMD=%BASEDIR%\bin\setenv.bat %BASEDIR% %BLDARCH% %BLDCONFIG% %BLDPLATFORM%
call %BASEDIR%\bin\setenv.bat %BASEDIR% %BLDARCH% %BLDCONFIG% %BLDPLATFORM%
popd

REM override BUILD_DEFAULT=-ei -nmake -i
REM set BUILD_DEFAULT=-nmake -i
set BUILD_DEFAULT=-i

set BLDOPT=%*
if "%BLDOPT%" EQU "" (
	set BLDOPT=-gPwi
)

cd /d %BLDDIR%
echo XBUILD: BLDCMD=build %BLDOPT%
build %BUILD_DEFAULT% %BLDOPT%

popd
endlocal
goto :EOF


:usage
echo xbuild [platform] [arch] [config] [directory] [build options]
echo.
echo platform: -wnet, -wnet2k, -w2k
echo arch: x86, ia64, amd64
echo config: fre, chk, both
echo.

goto :EOF
