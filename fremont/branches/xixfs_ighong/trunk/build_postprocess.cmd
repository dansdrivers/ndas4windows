@echo off
verify other 2>nul
setlocal enableextensions enabledelayedexpansion
if errorlevel 1 echo Unable to enable extensions && exit /b

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

REM Sign executable files
set SIGNTARGETS=
for %%a in (%DIRS%) do (
	if exist %%a\ call :append_exe_sign_targets %%a
)
call :sign_files

REM Create catalogs (files must be signed before creating catalogs)
if defined DIRS (
call "%BP_BINDIR%\create_catalog.cmd" %DIRS%
)

REM Signing catalog files
set SIGNTARGETS=
for %%a in (%DIRS%) do (
	for %%b in (%%a\*.cat) do (
		set SIGNTARGETS=!SIGNTARGETS! %%b
	)
)
call :sign_files
exit /b

:append_exe_sign_targets
for /r %1 %%a in (*.dll *.exe *.sys) do (
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
