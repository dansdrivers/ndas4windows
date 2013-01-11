@echo off
setlocal enableextensions

set SELFRELPATH=%0\..\
for %%p in (%SELFRELPATH%) do set SELFPATH=%%~pp

if not defined SIGN_SIGNER (
	set SIGN_SIGNER=%SELFPATH%signcode.exe
)

set _SIGNING_FILE=%1
shift
if "%_SIGNING_FILE%-" == "-" goto usage
if not exist %_SIGNING_FILE% goto err_fn

if not defined SIGN_TIMESTAMP (
	set SIGN_TIMESTAMP=-t http://timestamp.verisign.com/scripts/timstamp.dll
)
if not defined SIGN_SUBJECT (
	set SIGN_SUBJECT=-cn "XIMETA, Inc."
)

set _SIGN_OPTIONS=%1 %2 %3 %4 %5 %6 %7 %8 %9
if "%_SIGN_OPTIONS: =%-" == "-" (
	set _SIGN_OPTIONS=%SIGN_SUBJECT% %SIGN_TIMESTAMP%
)

echo %SIGN_SIGNER% %_SIGN_OPTIONS% %_SIGNING_FILE%
%SIGN_SIGNER% %_SIGN_OPTIONS% %_SIGNING_FILE%

:err_fn
echo signcode: %SIGNING_FILE% 
goto end

:usage
echo signcode_ximeta ^<target_file_name^> [additional options]
goto end

:end
endlocal
