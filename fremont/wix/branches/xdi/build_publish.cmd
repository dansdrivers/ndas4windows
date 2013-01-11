@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
if not defined MSIPUBLISHFOLDER (
	set MSIPUBLISHFOLDER=%~dp0publish
)
if /i "%1" equ "ndasscsi" (
	shift
	set BUILD_NDASSCSI=1
)

set _P=%*
set _P2=%_P:~8%

if defined BUILD_NDASSCSI (
	call %SCRIPT_DIR%bin\runmsbuild.cmd %SCRIPT_DIR%wixsrc\sku\ndasscsi\ndasscsi.wixproj %_P2%
) else (
	call %SCRIPT_DIR%bin\runmsbuild.cmd %SCRIPT_DIR%wixsrc\sku\ndasport\ndasport.wixproj %*
)
