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
) else if /I "%1" EQU "-WNET" (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=WNET
    shift
) else if /I "%1" EQU "-WNETXP" (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=WXP
    shift
) else (
	if not defined WNETBASE echo error: WNETBASE not defined && goto end
	set BASEDIR=%WNETBASE%
    set BLDPLATFORM=W2K
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
) else if /I "%1" EQU "AMD64" (
    set BLDARCH=AMD64
    shift
) else if /I "%1" EQU "X86" (
    set BLDARCH=X86
    shift
) else (
    set BLDARCH=X86
)

::
:: Build Configuration
::

if /I "%1" EQU "chk" (
    set BLDCONFIG=chk
    shift
) else if /I "%1" EQU "fre" (
    set BLDCONFIG=fre
    shift
) else if /I "%1" EQU "both" (
    set BLDCONFIG=both
    shift
) else (
    set BLDCONFIG=chk
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
endlocal
echo Error On Build -- Exit Code: %ERRORLEVEL%
rem echo build%BLDCONFIG%_%BLDPLATFORM%_%BLDARCH%.err
rem if exist build%BLDCONFIG%_%BLDPLATFORM%_%BLDARCH%.err type build%BLDCONFIG%_%BLDPLATFORM%_%BLDARCH%.err
rem if exist build%BLDCONFIG%_%BLDPLATFORM%_%BLDARCH%.wrn type build%BLDCONFIG%_%BLDPLATFORM%_%BLDARCH%.wrn
exit /b %ERRORLEVEL%
goto :EOF

:end
endlocal
goto :EOF


:build
setlocal
pushd .
pushd .

echo XBUILD: BASEDIR=%BASEDIR%,ARCH=%BLDARCH%,CONFIG=%BLDCONFIG%,PLATFORM=%BLDPLATFORM%,BLDDIR=%BLDDIR%
rem echo XBUILD: BLDENVCMD=%BASEDIR%\bin\setenv.bat %BASEDIR% %BLDARCH% %BLDCONFIG% %BLDPLATFORM%
if /I "%BLDARCH%" == "X86" set BLDARCH=
call %BASEDIR%\bin\setenv.bat %BASEDIR% %BLDARCH% %BLDCONFIG% %BLDPLATFORM%
popd

REM override BUILD_DEFAULT=-ei -nmake -i
REM set BUILD_DEFAULT=-nmake -i
set BUILD_DEFAULT=-i

set BLDOPT=%*
if "%BLDOPT%" EQU "" (
	set BLDOPT=-egPwi
) 
cd /d %BLDDIR%
call :set_base_dir
echo XBUILD: PROJECT_BASEDIR=%BASEDIR%
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

:set_base_dir
rem Find for the project root
set BASEDIR_TAG_FILE=base.dir
if exist .\%BASEDIR_TAG_FILE% for %%a in (.) do (set BASEDIR=%%~fa& goto :EOF)
if exist ..\%BASEDIR_TAG_FILE% for %%a in (..) do (set BASEDIR=%%~fa& goto :EOF)
if exist ..\..\%BASEDIR_TAG_FILE% for %%a in (..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
if exist ..\..\..\..\..\..\..\..\..\..\..\..\%BASEDIR_TAG_FILE% for %%a in (..\..\..\..\..\..\..\..\..\..\..\..) do (set BASEDIR=%%~fa& goto:EOF)
goto :EOF
