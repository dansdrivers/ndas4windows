@echo off
setlocal enableextensions
set ISSETUP=1
if /I "%1" EQU "/nobuild" (
	set ISSETUP=0
	shift
)

set ISCONF=MSI
set ISRELEASE=%1
if defined ISRELEASE shift
if not defined ISRELEASE set ISRELEASE=ENU

rem
rem Configuration is case-sensitive!!!
rem 

if /I "%ISRELEASE%" EQU "ENU" set ISRELEASE=ENU
if /I "%ISRELEASE%" EQU "ALL" set ISRELEASE=ALL
if /I "%ISRELEASE%" EQU "EUR" set ISRELEASE=EUR
if /I "%ISRELEASE%" EQU "CHS" set ISRELEASE=CHS
if /I "%ISRELEASE%" EQU "JPN" set ISRELEASE=JPN
if /I "%ISRELEASE%" EQU "KOR" set ISRELEASE=KOR

if "%ISSETUP%" EQU "1" (
	echo call build_setup.cmd /nocopy MSI %ISRELEASE% %1 %2 %3 %4 %5 %6 %7 %8 %9
	call build_setup.cmd /nocopy MSI %ISRELEASE% %1 %2 %3 %4 %5 %6 %7 %8 %9
    if errorlevel 1 goto err_on_build
)

if exist ..\ndasdir.tag (
	REM Package Directory for Source Tree
	set PACKAGEBASEDIR=..
) else (
	REM Package Directory for OBK
	set PACKAGEBASEDIR=..\..
)

set PACKAGEDIR=%PACKAGEBASEDIR%\package\%ISCONF%\%ISRELEASE%
set ISBUILDDIR=.\%ISCONF%\%ISRELEASE%\DiskImages\Disk1
set NDASETUPDIR=.\publish
set NDASETUP=ndasetup.exe

if not defined INSTMSIDIR (
	set INSTMSIDIR=C:\Program Files\InstallShield\StandaloneBuild9SP1
)
if not defined INSTMSI (
	set INSTMSI=instmsiw.exe
)

if /I "%ISCONF%" EQU "MSI" (
	if not exist "%PACKAGEDIR%" (
		echo build_msi: making directory "%PACKAGEDIR%"
		mkdir "%PACKAGEDIR%"
	)
	if /I "%ISRELEASE%" EQU "ENU" (
		call :copyproc "%ISBUILDDIR%\ndas.msi" "%PACKAGEDIR%\ndas.msi"
		call :signcodeproc "%PACKAGEDIR%\ndas.msi"
	) else (
		echo build_msi: making embbed msi ".\%ISCONF%\%ISRELEASE%\DiskImages\Disk1\ndas.msi" -^> "%PACKAGEDIR%\ndas.msi"
		call ..\bin\makeemb.cmd ".\%ISCONF%\%ISRELEASE%\DiskImages\Disk1\ndas.msi" "%PACKAGEDIR%\ndas.msi"
		call :signcodeproc "%PACKAGEDIR%\ndas.msi"
	)
	call :copyproc "%NDASETUPDIR%\%NDASETUP%" "%PACKAGEDIR%\%NDASETUP%"
	call :copyproc "%NDASETUPDIR%\ndasetup.ini" "%PACKAGEDIR%\ndasetup.ini"
	call :copyproc "%INSTMSIDIR%\%INSTMSI%" "%PACKAGEDIR%\%INSTMSI%"
)
goto end

:err_on_build
echo build_msi: stopped on build_setup, error %ERRORLEVEL%
goto end

:copyproc
echo build_msi: copying %1 -^> %2
copy /y %1 %2
goto :EOF

:signcodeproc
::
:: Code signing will be done only if XM_SIGNCODE_CMD
:: is specified. You must provide this command,
:: before running this command
::
if defined XM_SIGNCODE_CMD (
   echo build_msi: Signing a file: %~f1
   %XM_SIGNCODE_CMD% %~f1
)
goto :EOF

:end

endlocal
