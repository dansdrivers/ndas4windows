@echo off
setlocal enableextensions
echo PARAMS=%*
set ISMFILE_BASE=ndassetup.ism
set ISMFILE=_ndassetup.ism

set NOCOPY_PACKAGE=
if /I "%1" EQU "/nocopy" (
	set NOCOPY_PACKAGE=1
	shift
)
	
if /I "%1" EQU "/f" (
    set ISMFILE=%2
    shift
    shift
)
if exist %ISMFILE% (
    call :check_ism_dependency
    if errorlevel 1 goto :EOF
) else (
    echo %ISMFILE% does not exist. 
    echo Creating %ISMFILE% from %ISMFILE_BASE%.
    call createism.cmd
    if errorlevel 1 goto :EOF
) 

set _ISCONF=%1
if defined _ISCONF (shift) else (set _ISCONF=MSI)

set _ISRELEASE=%1
if defined _ISRELEASE (shift) else (set _ISRELEASE=ENU)

if not defined ISBUILDER9 (
	set _ISBUILDER9="C:\Program Files\InstallShield\StandaloneBuild9SP1\IsSABld.exe"
)
if not defined MSI_MERGE_MODULE_PATH (
	set MSI_MERGE_MODULE_PATH="C:\Program Files\Common Files\Merge Modules"
)

echo IS Configuration: %_ISCONF%
echo IS Release      : %_ISRELEASE%
set ISBUILD_CMD=%_ISBUILDER9% -p %ISMFILE% -a %_ISCONF% -r %_ISRELEASE% -o %MSI_MERGE_MODULE_PATH% %1 %2 %3 %4 %5 %6 %7 %8 %9
echo %ISBUILD_CMD%
%ISBUILD_CMD%
if errorlevel 1 goto :EOF

if defined NOCOPY_PACKAGE (
	exit /B 0
)

set _IS_IMAGE_PATH=%_ISCONF%\%_ISRELEASE%\DiskImages\DISK1
if exist ..\ndasdir.tag (
	REM Package Directory for Source Tree
	set _PACKAGE_PATH=..\package\%_ISCONF%\%_ISRELEASE%
) else (
	REM Package Directory for OBK
	set _PACKAGE_PATH=..\..\package\%_ISCONF%\%_ISRELEASE%
)

echo Copying %_IS_IMAGE_PATH% -^> %_PACKAGE_PATH%

call :find_bin_cmd_path robocopy.exe
if errorlevel 1 goto :EOF
%_RET% %_IS_IMAGE_PATH% %_PACKAGE_PATH% /purge /njh /njs
rem robocopy returns 0,1,2 for informational
if errorlevel 4 goto :EOF

if "%_ISCONF%" EQU "IS" (
    echo Fixing INI...
    call :fix_ini %_PACKAGE_PATH%\setup.ini
)

endlocal
exit /B 0

:fix_ini
call :find_bin_cmd_path editini.exe
if errorlevel 1 goto :EOF
%_RET% set %1 Startup Product Software
goto :EOF

:check_ism_dependency
call :find_bin_cmd_path fdatecmp.js
cscript //nologo %_RET% %ISMFILE_BASE% %ISMFILE%
if errorlevel 102 (
    rem error
    echo ERROR: cannot compare timestamps of %ISMFILE_BASE% and %ISMFILE%
    exit /B 1
) else if errorlevel 101 (
    rem baseismfile is newer than ismfile
    echo %ISMFILE% is older than %ISMFILE_BASE%. 
    echo Recreating %ISMFILE% from %ISMFILE_BASE%...
    call createism.cmd
    if errorlevel 1 exit /B 1
) else if errorlevel 100 (
    rem baseismfile and ismfile have same timestamps
    echo %ISMFILE% is older than %ISMFILE_BASE%. 
    echo Recreating %ISMFILE% from %ISMFILE_BASE%...
    call createism.cmd
    if errorlevel 1 exit /B 1
) else if errorlevel 99 (
    rem baseismfile is older than ismfile
) else (
    rem error
    echo ERROR: cannot compare timestamps of %ISMFILE_BASE% and %ISMFILE%
    if errorlevel 1 exit /B 1
)
exit /B 0

:find_bin_cmd_path
set _RET=
if exist ..\bin\%1 set _RET=..\bin\%1
if exist ..\..\bin\%1 set _RET=..\..\bin\%1
if exist ..\..\..\bin\%1 set _RET=..\..\..\bin\%1
if exist ..\..\..\..\bin\%1 set _RET=..\..\..\..\bin\%1
if not defined _RET (
	echo ERROR: cannot find script fdatecmp.js in bin directories.
	exit /b 1
)
exit /b 0
