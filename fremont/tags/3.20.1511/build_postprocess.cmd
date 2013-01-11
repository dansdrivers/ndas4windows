@echo off
verify other 2>nul
setlocal enableextensions enabledelayedexpansion
if errorlevel 1 echo Unable to enable extensions && exit /b

cd /d %~dp0

set BP_BINDIR=%~dp0bin
set BP_PUBDIR=%~dp0publish

:argloop
if /i "%1" == "/nosign" set NO_SIGNCODE_CMD=1 && shift && goto argloop
if /i "%1" == "-nosign" set NO_SIGNCODE_CMD=1 && shift && goto argloop
if /i "%1" == "/official" set XIMETA_OFFICIAL_BUILD=1 && shift && goto argloop
if /i "%1" == "-official" set XIMETA_OFFICIAL_BUILD=1 && shift && goto argloop

call "%BP_BINDIR%\genwshlpx32.cmd"

set DIRS=
if exist "%BP_PUBDIR%\fre\i386"  set DIRS=%DIRS% "%BP_PUBDIR%\fre\i386"
if exist "%BP_PUBDIR%\fre\amd64" set DIRS=%DIRS% "%BP_PUBDIR%\fre\amd64"
if exist "%BP_PUBDIR%\chk\i386"  set DIRS=%DIRS% "%BP_PUBDIR%\chk\i386"
if exist "%BP_PUBDIR%\chk\amd64" set DIRS=%DIRS% "%BP_PUBDIR%\chk\amd64"

REM Sign executable files (BATCH MODE)
for %%a in (%DIRS%) do (
	set SIGNTARGETS=
	if exist %%a\ call :append_sign_targets %%a "*.dll *.exe *.sys"
	call :sign_files
)

REM Sign executable files (NON-BATCH MODE)
REM for %%a in (%DIRS%) do (
REM 	if exist "%%~a\" call :sign_files_in_dir %%a "*.exe *.dll *.sys"
REM )

REM Create catalogs (files must be signed before creating catalogs)
if defined DIRS (
   call "%BP_BINDIR%\create_catalog.cmd" %DIRS%
)

REM Signing catalog files (BATCH MODE)
for %%a in (%DIRS%) do (
	set SIGNTARGETS=
	if exist %%a\ call :append_sign_targets %%a "*.cat"
	call :sign_files
)

REM Sign cat files (NON-BATCH MODE)
REM for %%a in (%DIRS%) do (
REM 	if exist %%a\ call :sign_files_in_dir %%a *.cat
REM )

exit /b

:sign_files_in_dir
set SIGNTARGETS=
for /r %1 %%a in (%~2) do (
	set SIGNTARGETS=%%a
	call :sign_files
)
exit /b

:append_sign_targets
for /r %1 %%a in (%~2) do (
	set SIGNTARGETS=!SIGNTARGETS! %%a
)
exit /b

:sign_files
if not defined SIGNTARGETS exit /b
if not defined NO_SIGNCODE_CMD (
if defined XIMETA_OFFICIAL_BUILD (
	echo Signing catalogs with XIMETA SPC Certificates...
	call "%BP_BINDIR%\signcode_ximeta.cmd" %SIGNTARGETS%
) else (
	echo Signing catalogs with Self-signed Test Certificates...
	call "%BP_BINDIR%\signcode_selfsign_ximeta.cmd" %SIGNTARGETS%
)
)
exit /b
