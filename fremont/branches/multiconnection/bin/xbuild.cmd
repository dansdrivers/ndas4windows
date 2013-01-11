@echo off

setlocal enableextensions

if /I "%XBUILD_DEBUG%" NEQ "" (
echo args: %*
echo args: [0:%0] [1:%1] [2:%2] [3:%3] [4:%4] [5:%5] [6:%6] [7:%7] [8:%8] [9:%9]
)

:preargs

if /I "%1" EQU "--noalt" (
	set NO_BUILD_ALT_DIR=1
	shift
	goto preargs
)

if /I "%1" EQU "--nomac" (
	set NO_DDK_MACROS=1
	shift
	goto preargs
)

if /I "%1" EQU "--prefast" (
	set USE_PREFAST=1	
	shift
	goto preargs
)

if /I "%1" EQU "--?" goto usage
if /I "%1" EQU "--h" goto usage

if /I "%1" EQU "--cmd" (
	set CUSTOM_CMD=1
	shift
)

::
:: Target Environment
::
set DEFAULT_PLATFORM_USED=
set BASEDIR=

if /I "%1" EQU "-WNET2K" (
	if not defined WNETBASE echo error: WNETBASE not defined && exit /b 1
	set BASEDIR=%WNETBASE%
	set BLDPLATFORM=W2K
	shift
) else if /I "%1" EQU "-WNET" (
	if not defined WNETBASE echo error: WNETBASE not defined && exit /b 1
	set BASEDIR=%WNETBASE%
	set BLDPLATFORM=WNET
	shift
) else if /I "%1" EQU "-WNETXP" (
	if not defined WNETBASE echo error: WNETBASE not defined && exit /b 1
	set BASEDIR=%WNETBASE%
	set BLDPLATFORM=WXP
	shift
) else if /I "%1" EQU "-WLH2K" (
	if not defined WLHBASE echo error: WLHBASE not defined && exit /b 1
	set BASEDIR=%WLHBASE%
	set BLDPLATFORM=W2K
	shift
) else if /I "%1" EQU "-WLHNET" (
	if not defined WLHBASE echo error: WLHBASE not defined && exit /b 1
	set BASEDIR=%WLHBASE%
	set BLDPLATFORM=WNET
	shift
) else if /I "%1" EQU "-WLHXP" (
	if not defined WLHBASE echo error: WLHBASE not defined && exit /b 1
	set BASEDIR=%WLHBASE%
	set BLDPLATFORM=WXP
	shift
) else if /I "%1" EQU "-WLH" (
	if not defined WLHBASE echo error: WLHBASE not defined && exit /b 1
	set BASEDIR=%WLHBASE%
	set BLDPLATFORM=WLH
	shift
) else if /I "%1" EQU "-WIN2K" (
	set BLDPLATFORM=W2K
	shift
) else if /I "%1" EQU "-WINXP" (
	set BLDPLATFORM=WXP
	shift
) else if /I "%1" EQU "-WS03" (
	set BLDPLATFORM=WNET
	shift
) else if /I "%1" EQU "-VISTA" (
	set BLDPLATFORM=WLH
	shift
) else (
	set DEFAULT_PLATFORM_USED=1
)

if not defined WLHBASE (
	if exist %SYSTEMDRIVE%\WINDDK\6000 set WLHBASE=%SYSTEMDRIVE%\WINDDK\6000
)

if not defined WNETBASE (
	if exist %SYSTEMDRIVE%\WINDDK\3790.1830 set WNETBASE=%SYSTEMDRIVE%\WINDDK\3790.1830
)

if not defined BASEDIR if defined WLHBASE set BASEDIR=%WLHBASE%
if not defined BASEDIR if defined WNETBASE set BASEDIR=%WNETBASE%
if not defined BLDPLATFORM set BLDPLATFORM=W2K

if not defined BASEDIR (
	echo error: WLHBASE nor WNETBASE is defined.
	exit /b 1
)

if not exist "%BASEDIR%" (
	echo error: DDK Base Directory %BASEDIR% does not exists.
	exit /b 1
)

::
:: Target Architecture
::

set BLDARCH=

if /I "%1" EQU "IA64" (
	set BLDARCH=64
	shift
) else if /I "%1" EQU "AMD64" (
	set BLDARCH=AMD64
	if defined DEFAULT_PLATFORM_USED set BLDPLATFORM=WNET
	shift
) else if /I "%1" EQU "X64" (
	set BLDARCH=AMD64
	if defined DEFAULT_PLATFORM_USED set BLDPLATFORM=WNET
	shift
) else if /I "%1" EQU "I386" (
	set BLDARCH=X86
	shift
) else if /I "%1" EQU "X86" (
	set BLDARCH=X86
	shift
) else if /I "%1" EQU "intel" (
	set BLDARCH=intel
	shift
)

if not defined BLDARCH set BLDARCH=X86

::
:: Build Configuration
::

set BLDCONFIG=

if /I "%1" EQU "chk" (
	set BLDCONFIG=chk
	shift
) else if /I "%1" EQU "fre" (
	set BLDCONFIG=fre
	shift
) else if /I "%1" EQU "both" (
	set BLDCONFIG=both
	shift
)

if not defined BLDCONFIG set BLDCONFIG=chk

::
:: Target Directory
::
set BLDDIR=.
set BLDDIR2=%1

if defined BLDDIR2 (
	if "%BLDDIR2%" NEQ "*" (
	  if /I "%BLDDIR2:~0,1%" NEQ "-" (
		 if exist "%BLDDIR2%" (
			set BLDDIR=%BLDDIR2%
			shift
		 )
	  )
	)
)

if not exist %BASEDIR%\bin\setenv.bat (
	echo error: %BASEDIR%\bin\setenv.bat does not exist.
	exit /b 1
)

if exist "%~dp0ctitle.exe" (
	for /f "usebackq delims=-" %%a in (`"%~dp0ctitle.exe"`) do set _SAVED_TITLE=%%a
)

call :build_by_arch %1 %2 %3 %4 %5 %6 %7 %8 %9

if defined _SAVED_TITLE title %_SAVED_TITLE%

if errorlevel 1 goto err_on_build
endlocal
exit /b

:err_on_build
endlocal
echo error: build failed (%ERRORLEVEL%)
exit /b %ERRORLEVEL%

:build_by_arch
if /I "%XBUILD_DEBUG%" NEQ "" echo build_by_arch: %BLDPLATFORM%
if /I "%BLDARCH%" equ "intel" (

	set BLDARCH=X86
	call :build_by_config %*
	if errorlevel 1 exit /b %ERRORLEVEL%

	set BLDARCH=AMD64
	set BLDPLATFORM_ORG=%BLDPLATFORM%
	if /i "%BLDPLATFORM%" equ "W2K" set BLDPLATFORM=WNET
	if /i "%BLDPLATFORM%" equ "WXP" set BLDPLATFORM=WNET
	call :build_by_config %*
	set BLDPLATFORM=%BLDPLATFORM_ORG%
	if errorlevel 1 exit /b %ERRORLEVEL%

) else (

	call :build_by_config %*
	if errorlevel 1 exit /b %ERRORLEVEL%

)
exit /b

:build_by_config
if /I "%XBUILD_DEBUG%" NEQ "" echo build_by_config: %BLDCONFIG%
if /i "%BLDCONFIG%" equ "both" (

	set BLDCONFIG=chk
	call :build %*
	if errorlevel 1 exit /b %ERRORLEVEL%

	set BLDCONFIG=fre
	call :build %*
	if errorlevel 1 exit /b %ERRORLEVEL%

	set BLDCONFIG=both

) else (

	call :build %*

)
exit /b

:build
if /I "%XBUILD_DEBUG%" NEQ "" echo build: %BLDPLATFORM% %BLDCONFIG%
setlocal
pushd .
pushd .

echo XBUILD: BASEDIR=%BASEDIR%,ARCH=%BLDARCH%,CONFIG=%BLDCONFIG%,PLATFORM=%BLDPLATFORM%,BLDDIR=%BLDDIR%
if /I "%BLDARCH%" == "X86" set BLDARCH=
call %BASEDIR%\bin\setenv.bat %BASEDIR% %BLDARCH% %BLDCONFIG% %BLDPLATFORM%
popd

set BUILD_DEFAULT=-gi
if defined NUMBER_OF_PROCESSOR (
	if "%NUMBER_OF_PROCESSORS%" NEQ "1" (
		set BUILD_DEFAULT=%BUILD_DEFAULT% -MI
	)
)

set BLDOPT=%*
if "%BLDOPT%" EQU "" (
	set BLDOPT=-egPwi
) 
if "%BLDOPT%" EQU "*" (
	set BLDOPT=-egPwi
)
cd /d %BLDDIR%
rem call :set_base_dir
echo XBUILD: PROJECT_BASEDIR=%BASEDIR%
echo XBUILD: BLDCMD=build %BLDOPT%
if /I "%XBUILD_DEBUG%" NEQ "" (
	set
)
if "%NO_BUILD_ALT_DIR%" == "1" (
	if /I "%BLDCONFIG%" EQU "fre" set BUILD_ALT_DIR=
	if /I "%BLDCONFIG%" EQU "chk" set BUILD_ALT_DIR=d
	rem if /I "%BLDCONFIG%" EQU "chk" set CHECKED_ALT_DIR=1
	rem set BUILD_ALT_DIR=
)

if defined USE_PREFAST (
	prefast /log=_defects.xml build %BLDOPT%
	prefast /log=_defects.xml list
) else if defined CUSTOM_CMD (
	rem DO NOT DELETE THIS REMARK (fill-out for a possibly empty line)
	%*
) else (
	build %BLDOPT%
)

popd
endlocal
goto :EOF


:usage
echo xbuild [platform] [arch] [config] [directory] [build options]
echo.
echo platform: ^(general^)       -win2k,  -winxp,  -ws03,   -vista
echo           ^(DDK 3790.1830^) -wnet2k, -wnetxp, -wnet
echo           ^(DDK 6000^)      -wlh2k,  -wlhxp,  -wlhnet, -wlh
echo arch: i386, ia64, amd64 or intel (i386+amd64)
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
