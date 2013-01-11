@echo off
setlocal enableextensions
pushd .
cd /d %~dp0

if not defined WLHBASE (
	if exist %SYSTEMDRIVE%\WINDDK\6000 set WLHBASE=%SYSTEMDRIVE%\WINDDK\6000
)

if not defined WNETBASE (
	if exist %SYSTEMDRIVE%\WINDDK\3790.1830 set WNETBASE=%SYSTEMDRIVE%\WINDDK\3790.1830
)

if not defined BASEDIR if defined WLHBASE set BASEDIR=%WLHBASE%
if not defined BASEDIR if defined WNETBASE set BASEDIR=%WNETBASE%

if not defined BASEDIR (
	echo error: WLHBASE nor WNETBASE is defined.
	exit /b 1
)

if not exist "%BASEDIR%" (
	echo error: DDK Base Directory %BASEDIR% does not exists.
	exit /b 1
)

%BASEDIR%\bin\x86\nmake.exe -nologo -f "%~dp0wshlpx32.mk" %*
popd
endlocal
