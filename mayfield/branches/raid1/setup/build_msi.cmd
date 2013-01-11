@echo off
setlocal enableextensions
set ISSETUP=1
if /I "%1" EQU "/nobuild" (
	set ISSETUP=0
	shift
)

set RELEASE=MSI
set CONF=%1
if defined CONF shift
if not defined CONF set CONF=ENU
if /I "%CONF%" EQU "ENU" set CONF=ENU
if /I "%CONF%" EQU "ALL" set CONF=ALL

if "%ISSETUP%" EQU "1" (
	echo call build_setup.cmd MSI %CONF% %1 %2 %3 %4 %5 %6 %7 %8 %9
	call build_setup.cmd MSI %CONF% %1 %2 %3 %4 %5 %6 %7 %8 %9
)

set PACKAGEDIR=.\packager\%CONF%
set ISBUILDDIR=.\%RELEASE%\%CONF%\DiskImages\Disk1
set NDASETUPDIR=.\publish
set NDASETUP=ndasetup.exe

if not defined INSTMSIDIR (
	set INSTMSIDIR=C:\Program Files\InstallShield\StandaloneBuild9SP1
)
if not defined INSTMSI (
	set INSTMSI=instmsiw.exe
)

if errorlevel 1 goto err_on_build
if /I "%RELEASE%" EQU "MSI" (
	if not exist "%PACKAGEDIR%" (
		echo build_msi: making directory "%PACKAGEDIR%"
		mkdir "%PACKAGEDIR%"
	)
	if /I "%CONF%" EQU "ENU" (
		call :copyproc "%ISBUILDDIR%\ndas.msi" "%PACKAGEDIR%\ndas.msi"
		call :signcodeproc "%PACKAGEDIR%\ndas.msi"
	) else (
		echo build_msi: making embbed msi ".\%RELEASE%\%CONF%\DiskImages\Disk1\ndas.msi" -^> ".\packager\%CONF%\ndas.msi"
		call ..\bin\makeemb.cmd ".\%RELEASE%\%CONF%\DiskImages\Disk1\ndas.msi" ".\packager\%CONF%\ndas.msi"
		call :signcodeproc "%PACKAGEDIR%\ndas.msi"
	)
	call :copyproc "%NDASETUPDIR%\%NDASETUP%" "%PACKAGEDIR%\%NDASETUP%"
	call :copyproc "%NDASETUPDIR%\ndasetup.ini" "%PACKAGEDIR%\ndasetup.ini"
	call :copyproc "%INSTMSIDIR%\%INSTMSI%" "%PACKAGEDIR%\%INSTMSI%"
)
goto end
:err_on_build
echo build_msi: stopped on build_setup.
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

:end

endlocal
