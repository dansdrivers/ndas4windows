::
:: CONFIGURATION: MUI, ENU, EUR, JPN, KOR
:: PACKAGE: NOMSI or FULL (with MSI)
::

@echo off
setlocal enableextensions

set CONF=%1
set PACKAGE=%2

if "%CONF%" EQU "" set CONF=ENU
if "%PACKAGE%" EQU "" set PACKAGE=FULL

call :find_ndas_root
if errorlevel 1 exit /b %ERRORLEVEL%
set NDASROOT=%_RET%
set BINPATH=%NDASROOT%\bin
set IEXPRESS_CMD=start /w %BINPATH%\iexpress.exe
set MAKESED_CMD=cscript.exe //nologo %BINPATH%\makesed.js
set VERSIONFILE=%NDASROOT%\PRODUCTVER.TXT

:: Get Product Version
if not defined NDAS_PRODUCT_VERSION (
	for /f %%a in (%VERSIONFILE%) do set NDAS_PRODUCT_VERSION=%%a
)

if not defined NDAS_PRODUCT_BASEFILENAME (
	set NDAS_PRODUCT_BASEFILENAME=ndas
)

if not defined NDAS_PRODUCT_DISPLAY_NAME (
	set NDAS_PRODUCT_DISPLAY_NAME=NDAS Software
)

set FRIENDLYNAMEBASE=%NDAS_PRODUCT_DISPLAY_NAME% %NDAS_PRODUCT_VERSION%
set CONFL=
if /I "%CONF%" EQU "MUI" ( set CONFL=mui&& set FNAMEPOSTFIX=^(Multilingual^) )
if /I "%CONF%" EQU "ENU" ( set CONFL=enu&& set FNAMEPOSTFIX=^(English^) )
if /I "%CONF%" EQU "EUR" ( set CONFL=eur&& set FNAMEPOSTFIX=^(European Languages^) )
if /I "%CONF%" EQU "KOR" ( set CONFL=kor&& set FNAMEPOSTFIX=^(Korean^) )
if /I "%CONF%" EQU "JPN" ( set CONFL=jpn&& set FNAMEPOSTFIX=^(Japanese^) )
if not defined CONFL goto err_undefined_conf

set FRIENDLYNAME=%FRIENDLYNAMEBASE% %FNAMEPOSTFIX%

if /I "%PACKAGE%" EQU "NOMSI" set PACKAGEPOSTFIX=
if /I "%PACKAGE%" EQU "FULL" set PACKAGEPOSTFIX=-full

if not defined NDAS_SETUP_PACKAGE_NAME_BASE (
	set NDAS_SETUP_PACKAGE_NAME_BASE=%NDAS_PRODUCT_BASEFILENAME%-%NDAS_PRODUCT_VERSION%-setup-%CONFL%%PACKAGEPOSTFIX%
)
set NDAS_SETUP_PACKAGE_EXE_NAME=%NDAS_SETUP_PACKAGE_NAME_BASE%.exe
set NDAS_SETUP_PACKAGE_SED_NAME=%NDAS_SETUP_PACKAGE_NAME_BASE%.sed

echo Product Version: %NDAS_PRODUCT_VERSION%
echo Product Friendly Name: %FRIENDLYNAME%
echo Setup Package Name: %NDAS_SETUP_PACKAGE_EXE_NAME%

for %%f in (MSI\%CONF%) do set DATAPATH=%%~ff

pushd .
if not exist tmp mkdir tmp
cd tmp

set LSTFILE=%NDAS_SETUP_PACKAGE_NAME_BASE%.lst
echo. > %LSTFILE%

for %%f in (%DATAPATH%\*.*) do (
	if /I "%PACKAGE%" EQU "NOMSI" (
		if /I "%%~nf" EQU "instmsiw" (
			echo skipping %%~ff.
		) else (
			echo adding %%~ff
			echo %%~ff >> %LSTFILE%
		)
	) else (
		echo adding %%~ff
		echo %%~ff >> %LSTFILE%
	)
)

%MAKESED_CMD% /t:"%NDAS_SETUP_PACKAGE_EXE_NAME%" /basesed:"..\package.sed" /f:"%FRIENDLYNAME%" /app:ndasetup.exe %LSTFILE%
popd

%IEXPRESS_CMD% /N tmp\%NDAS_SETUP_PACKAGE_SED_NAME%


call :signcodeproc %NDAS_SETUP_PACKAGE_EXE_NAME%
goto :EOF

:err_undefined_conf
echo error - undefined configuration: %CONF%
goto :EOF

REM =======================================================================
REM CODE SIGNER
REM =======================================================================

:signcodeproc
::
:: Code signing will be done only if XM_SIGNCODE_CMD
:: is specified. You must provide this command,
:: before running this command
::
if defined XM_SIGNCODE_CMD (
   echo build_msi: Signing a file: %~f1
   %XM_SIGNCODE_CMD% %~f1
)
goto :EOF

REM =======================================================================
REM FIND NDAS ROOT DIRECTORY
REM =======================================================================

:find_ndas_root
set _RET=
if exist .\ndasdir.tag set _RET=.
if exist ..\ndasdir.tag set _RET=..
if exist ..\..\ndasdir.tag set _RET=..\..
if exist ..\..\..\ndasdir.tag set _RET=..\..\..
if exist ..\..\..\..\ndasdir.tag set _RET=..\..\..\..
if exist ..\..\..\..\..\ndasdir.tag set _RET=..\..\..\..\..
if exist ..\..\..\..\..\..\ndasdir.tag set _RET=..\..\..\..\..\..
if exist ..\..\..\..\..\..\..\ndasdir.tag set _RET=..\..\..\..\..\..\..

if not defined _RET (
	echo ERROR: cannot find script fdatecmp.js in bin directories.
	exit /b 1
)

REM -----------------------------------------------------------------------
REM Make full path
REM -----------------------------------------------------------------------

for %%f in (%_RET%) do set _RET=%%~ff
exit /b 0
