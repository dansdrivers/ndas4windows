@echo off
::
:: Creates driver catalog files with CDF
:: Copyright (C) 2003-2004 XIMETA, Inc.
::
:: 11 Oct, 2004 Chesong Lee
::
setlocal enableextensions
pushd .

::
:: Resolve the directory path of make_catalog.cmd
::
set SELFPATH_REF=%~dp0

::
:: Resolve NDAS_BIN_PATH, which is relative to SELFPATH\..\bin
:: or set NDAS_BIN_PATH prior to run this command
::
if not defined NDAS_BIN_PATH (
   set NDAS_BIN_PATH=%SELFPATH_REF%
)

::
:: path to makecat.exe 
::
if not defined MAKECAT_CMD (
   set MAKECAT_CMD=%NDAS_BIN_PATH%makecat.exe -v
)
rem echo mkcat: MKCAT_CMD=%MAKECAT_CMD%
if defined XM_SIGNCODE_CMD (
   rem echo mkcat: SIGNCODE_CMD=%XM_SIGNCODE_CMD%
)

::
:: By default, we process all CDF's in the current directory
:: To override this, specify CDF file
::
set TARGETDIR=.
if "%1" == "" (
  set TARGETDIR=.
) else (
  set TARGETDIR=%1
)

if not exist %TARGETDIR% (
   echo Error: Directory '%TARGETDIR%' does not exist.
   goto :EOF
)

pushd .
cd /d %TARGETDIR%
if errorlevel 1 (
   echo Error: Directory '%TARGETDIR%' is inaccessible.
   goto :EOF
)

for %%c in (*.cdf) do (
	call :make_catalog %%~fc %%~nc.cat
)
popd

popd
endlocal
goto :EOF

:make_catalog
::
:: Making a cat file
::
echo params: %*
echo mkcat: Creating a catalog file from %~nx1
%MAKECAT_CMD% %1
::
:: Signing a cat file
::
:: Cat file will be signed only if XM_SIGNCODE_CMD
:: is specified. You must provide this command,
:: before running this command
::
if defined XM_SIGNCODE_CMD (
   echo mkcat: Signing a catalog file: %~f2
   %XM_SIGNCODE_CMD% %~f2
)
goto :EOF
