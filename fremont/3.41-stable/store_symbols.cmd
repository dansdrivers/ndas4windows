@echo off
setlocal enableextensions
set SOURCEDIR=%~dp0
set BINSOURCEDIR=%SOURCEDIR%publish

if not defined SYMSTORE_CMD call :set_symstore

if not exist %SYMSTORE_CMD% (
	echo symstore.exe is not available.
)

set _LLSYMSTORE=%1
if defined _LLSYMSTORE (
	set LOCAL_SYMSTORE=%_LLSYMSTORE%
)

if not defined LOCAL_SYMSTORE (
	set LOCAL_SYMSTORE=C:\symbols\local
)

if not exist %LOCAL_SYMSTORE% mkdir %LOCAL_SYMSTORE%

set LRT=%~dp0_lastrevision.tmp
svn info "%~dp0." | findstr /C:"Last Changed Rev:" > "%LRT%"

REM Last Changed Rev: 8177
for /f "usebackq tokens=4" %%a in ("%LRT%") do set LAST_REVISION=%%a
for /f "usebackq" %%a in ("%SOURCEDIR%PRODUCTVER.txt") do set PRODUCT_VERSION=%%a

set SS_PRODUCT=NDAS Software
set SS_VERSION=%PRODUCT_VERSION% r%LAST_REVISION%

echo SymStore    : %LOCAL_SYMSTORE%
echo SourceDir   : %SOURCEDIR%
echo BinSourceDir: %BINSOURCEDIR%
echo Version     : %SS_PRODUCT% %SS_VERSION%

%SYMSTORE_CMD% add /r /f "%BINSOURCEDIR%" /s %LOCAL_SYMSTORE% /t "%SS_PRODUCT%" /v "%SS_VERSION%"

exit /b

:set_symstore
set SYMSTORE_CMD="%PROGRAMFILES%\Debugging Tools for Windows\symstore.exe"
if not exist %SYMSTORE_CMD% (
	set SYMSTORE_CMD="%PROGRAMFILES%\Debugging Tools for Windows 64-bit\symstore.exe"
)
exit /b
