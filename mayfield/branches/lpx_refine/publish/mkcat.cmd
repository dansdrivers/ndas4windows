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
set SELFPATH_REF=%0\..\
for %%p in (%SELFPATH_REF%) do set SELFPATH=%%~dpp

::
:: Resolve NDAS_BIN_PATH, which is relative to SELFPATH\..\bin
:: or set NDAS_BIN_PATH prior to run this command
::
if not defined NDAS_BIN_PATH (
   for %%p in (%SELFPATH%\..\bin\) do set NDAS_BIN_PATH=%%~dpp
)

::
:: path to makecat.exe 
::
if not defined MAKECAT_CMD (
   set MAKECAT_CMD=%NDAS_BIN_PATH%makecat.exe -v
)
echo mkcat: MKCAT_CMD=%MAKECAT_CMD%
if defined XM_SIGNCODE_CMD (
   echo mkcat: SIGNCODE_CMD=%XM_SIGNCODE_CMD%
)

::
:: By default, we process all CDF's in SELFPATH.
:: To override this, specify CDF file
::
set TARGETCDF=%1
if "%TARGETCDF%" == "" set TARGETCDF=%SELFPATH%*.cdf

for %%c in (%TARGETCDF%) do (
	if exist %SELFPATH%fre\i386 (
		cd /d %SELFPATH%fre\i386
		call :make_catalog %%~fc %%~nc.cat
	)
	if exist %SELFPATH%fre\amd64 (
		cd /d %SELFPATH%fre\amd64
		call :make_catalog %%~fc %%~nc.cat
	)
	if exist %SELFPATH%chk\i386 (
		cd /d %SELFPATH%chk\i386
		call :make_catalog %%~fc %%~nc.cat
	)
	if exist %SELFPATH%chk\amd64 (
		cd /d %SELFPATH%chk\amd64
		call :make_catalog %%~fc %%~nc.cat
	)
)

popd

endlocal
goto :EOF

:make_catalog
::
:: Making a cat file
::
echo mkcat: Creating a catalog file from: %1
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
