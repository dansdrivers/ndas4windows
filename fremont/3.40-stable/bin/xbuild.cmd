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
	set BLDARCH=allarch
	shift
) else if /I "%1" EQU "all" (
	set BLDARCH=allarch
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

set BUILD_ERROR=
(call :build_by_arch %1 %2 %3 %4 %5 %6 %7 %8 %9) || set BUILD_ERROR=1
if defined _SAVED_TITLE title %_SAVED_TITLE%
exit /b %BUILD_ERROR%

:build_by_arch
if /I "%XBUILD_DEBUG%" NEQ "" echo build_by_arch: %BLDPLATFORM%
if /I "%BLDARCH%" equ "allarch" (

	set BLDARCH=X86
	(call :build_by_config %*) || exit /b

	set BLDARCH=AMD64
	setlocal
	if /i "%BLDPLATFORM%" equ "W2K" set BLDPLATFORM=WNET
	if /i "%BLDPLATFORM%" equ "WXP" set BLDPLATFORM=WNET
	(call :build_by_config %*) || exit /b
	endlocal

) else (

	(call :build_by_config %*) || exit /b
)
exit /b

:build_by_config
if /I "%XBUILD_DEBUG%" NEQ "" echo build_by_config: %BLDCONFIG%
if /i "%BLDCONFIG%" equ "both" (

	set BLDCONFIG=chk
	(call :build %*) || exit /b

	set BLDCONFIG=fre
	(call :build %*) || exit /b

	set BLDCONFIG=both

) else (

	(call :build %*) || exit /b

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

if not defined XBUILD_NO_DEFAULT (
	set BUILD_DEFAULT=-gi
) else (
	echo XBUILD_NO_DEFAULT defined!
	set BUILD_DEFAULT=-g
)

if not defined XBUILD_NO_DEFAULT (

	if not defined XBUILD_NO_MP (
   		if defined NUMBER_OF_PROCESSOR (
			if "%NUMBER_OF_PROCESSORS%" NEQ "1" (
				set BUILD_DEFAULT=-MI %BUILD_DEFAULT%
			)
		)
	)

	if defined XBUILD_NO_MP (
		set BUILD_DEFAULT=-M 1 %BUILD_DEFAULT%
	)
	
)

echo XBUILD: BUILD_DEFAULT=%BUILD_DEFAULT%

set BLDOPT=%*
if "%BLDOPT%" EQU "" (
	set BLDOPT=-egPw -B
) 
if "%BLDOPT%" EQU "*" (
	set BLDOPT=* -egPw -B
)
cd /d %BLDDIR%
if /I "%XBUILD_DEBUG%" NEQ "" (
	set
)
if "%NO_BUILD_ALT_DIR%" EQU "1" (
	if /I "%BLDCONFIG%" EQU "fre" set BUILD_ALT_DIR=
	if /I "%BLDCONFIG%" EQU "chk" set BUILD_ALT_DIR=d
	rem if /I "%BLDCONFIG%" EQU "chk" set CHECKED_ALT_DIR=1
	rem set BUILD_ALT_DIR=
)
if "%NO_BUILD_ALT_LOG%" EQU "1" (
	rem nothing      
) else (
	call :find_option_j %BLDOPT%
	if errorlevel 1 goto end_alt_log
	set BLDOPT=%BLDOPT% -j _build
)

:end_alt_log

rem echo XBUILD: PROJECT_BASEDIR=%BASEDIR%
echo XBUILD: build %BLDOPT%

if defined USE_PREFAST (
	prefast /log=_defects.xml build %BLDOPT%
	prefast /log=_defects.xml list
) else if defined CUSTOM_CMD (
	rem DO NOT DELETE THIS REMARK (fill-out for a possibly empty line)
	%*
) else (
	if defined XBUILD_USE_DBG (
	    (%XBUILD_USE_DBG% build.exe %BLDOPT%) ||  goto build_error
	) else (
		(build.exe %BLDOPT%) || goto build_error
	)
)

popd
exit /b 

:build_error
popd
REM Strangely we cannot get ERRORLEVEL from build.exe.
REM So we have to return the hard-coded error code here (2)
echo BUILD FAILED %ERRORLEVEL%
exit /b 2

:usage
echo xbuild [platform] [arch] [config] [directory] [build options]
echo.
echo platform: ^(general^)       -win2k,  -winxp,  -ws03,   -vista
echo           ^(DDK 3790.1830^) -wnet2k, -wnetxp, -wnet
echo           ^(DDK 6000^)      -wlh2k,  -wlhxp,  -wlhnet, -wlh
echo arch: i386, ia64, amd64 or all (i386+amd64)
echo config: fre, chk, both
echo.
exit /b 1

:find_option_j
for %%a in (%*) do (
	if "%%a" equ "-j" exit /b 1
	if "%%a" equ "-/j" exit /b 1
)
exit /b 0
