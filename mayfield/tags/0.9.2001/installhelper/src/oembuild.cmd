@echo off
setlocal ENABLEEXTENSIONS
set OEM_BUILD=
set OEM_BASEDIR=

if "%1" == "" goto usage
set OEM_BRAND=%1
if "%OEM_BRAND%" == "iomega" goto n1
if "%OEM_BRAND%" == "moritani" goto n1
if "%OEM_BRAND%" == "gennetworks" goto n1
if "%OEM_BRAND%" == "logitec" goto n1
if "%OEM_BRAND%" == "retail" goto n1
goto usage
:n1
shift

set BUILD=%1
if "%BUILD%" == "" set BUILD=fre
if "%BUILD%" == "release" set BUILD=fre
if "%BUILD%" == "debug" set BUILD=chk
if "%BUILD%" == "free" set BUILD=fre
if "%BUILD%" == "checked" set BUILD=chk
if "%BUILD%" == "fre" goto n2
if "%BUILD%" == "chk" goto n2
goto usage
:n2
shift

if "%OEM_BRAND%" == "retail" goto retail

:oem

set OEM_BUILD=%OEM_BRAND%
if "%BUILD%" == "fre" set OEM_BASEDIR=..\..\oem\%OEM_BRAND%\branded\sys\fre_w2k_x86\i386
if "%BUILD%" == "chk" set OEM_BASEDIR=..\..\oem\%OEM_BRAND%\branded\sys\chk_w2k_x86\i386
rem if not exist %OEM_BASEDIR% mkdir %OEM_BASEDIR%

set TREEROOT=..\..\oem\%OEM_BRAND%\branded
goto bld

:retail
set TREEROOT=..\
goto bld

:bld
set DDKBUILD_OPTION=%1
if defined DDKBUILD_OPTION (
	set DDKBUILD_OPTION=%DDKBUILD_OPTION:~1,-1%
)
if not defined DDKBUILD_OPTION set DDKBUILD_OPTION=-cg
echo DDK Build Option: %DDKBUILD_OPTION%

shift

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::
:: DDKBUILD
::
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::
:: PSDK_INC_PATH support
::
if not defined MSSDK set MSSDK=C:\Program Files\Microsoft SDK
if not defined PSDK_INC_PATH for /f "usebackq" %%a in (`..\bin\shrtpath.exe "%MSSDK%\include"`) do set PSDK_INC_PATH=%%a
if not defined PSDK_LIB_PATH for /f "usebackq" %%a in (`..\bin\shrtpath.exe "%MSSDK%\include"`) do set PSDK_LIB_PATH=%%a
echo PSDK_INC_PATH=%PSDK_INC_PATH%
echo PSDK_LIB_PATH=%PSDK_LIB_PATH%

echo --------------------------------------------------------------------------
echo ---- RUNNING DDKBULID
echo --------------------------------------------------------------------------
echo.
echo [[[ ..\bin\ddkbuild.bat -WNET2K %BUILD% . %DDKBUILD_OPTION% ]]]
echo.

call ..\bin\ddkbuild.bat -WNET2K %BUILD% . %DDKBUILD_OPTION%
if errorlevel 1 goto error

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::
:: SDKBUILD
::
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set OEM_DEFINES=/DOEM_BUILD=1

set SDKBUILD_OPTION=%1
if defined SDKBUILD_OPTION (
	set SDKBUILD_OPTION=%SDKBUILD_OPTION:~1,-1%
)
if not defined SDKBUILD_OPTION set SDKBUILD_OPTION=-a
echo SDK Build Option: %SDKBUILD_OPTION%
shift

if "%BUILD%" == "fre" set CFG=Release 
if "%BUILD%" == "chk" set CFG=Debug

if "%OEM_BRAND%" == "iomega"		set OEM_DEFINES=%OEM_DEFINES% /DOEM_IOMEGA=1
if "%OEM_BRAND%" == "moritani"		set OEM_DEFINES=%OEM_DEFINES% /DOEM_MORITANI=1
if "%OEM_BRAND%" == "gennetworks"	set OEM_DEFINES=%OEM_DEFINES% /DOEM_GENNETWORKS=1
if "%OEM_BRAND%" == "logitec"		set OEM_DEFINES=%OEM_DEFINES% /DOEM_LOGITEC=1
if "%OEM_BRAND%" == "retail" 		set OEM_DEFINES=

echo --------------------------------------------------------------------------
echo ---- RUNNING SDKBUILD
echo --------------------------------------------------------------------------
echo.
echo [[[ ..\bin\sdkbuild.cmd %CFG% %SDKBUILD_OPTION% ]]]
echo.

call ..\bin\sdkbuild.cmd %CFG% %SDKBUILD_OPTION%
if errorlevel 1 goto error

goto end






:error
echo.
echo STOP on ERROR!!!
echo.
goto end

:usage
echo usage: oembuild.cmd oem_brand [build]
echo.
echo   oem_brand: iomega moritani gennetworks logitec retail
echo   build    : fre chk
echo.
goto end

:end
endlocal
