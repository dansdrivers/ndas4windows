@echo off
setlocal enableextensions
echo PARAMS=%*
set RELEASE=%1
set CONF=%2

if defined RELEASE shift
if not defined RELEASE set RELEASE=XIMETA

if defined CONF shift
if not defined CONF set set CONF=ENU

if not defined ISBUILDER9 (
	set ISBUILDER9="C:\Program Files\InstallShield\StandaloneBuild9SP1\IsSABld.exe"
)
if not defined MSI_MERGE_MODULE_PATH (
	set MSI_MERGE_MODULE_PATH="C:\Program Files\Common Files\Merge Modules"
)

set ISBUILD_CMD=%ISBUILDER9% -p ndassetup.ism  -a %RELEASE% -r %CONF% -o %MSI_MERGE_MODULE_PATH% %1 %2 %3 %4 %5 %6 %7 %8 %9
echo %ISBUILD_CMD%
%ISBUILD_CMD%

endlocal
