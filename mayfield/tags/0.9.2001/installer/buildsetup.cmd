@echo off
setlocal

REM
REM Set current directory as cmd file's directory
REM

pushd .
cd %0\..\
set CMDDIR=%CD%
echo %CMDDIR%

call %CMDDIR%\buildvar.cmd

if "%1" == "intl" goto intl
if "%1" == "us" goto us
if "%1" == "japan" goto japan
if "%1" == "korea" goto korea

goto usage

:intl
set _IS_RELEASE=International
set DISTROOT=..\dist
set DISTSUB=INTL
goto start

:us
set _IS_RELEASE=US
set DISTROOT=..\dist
set DISTSUB=US
goto start

:japan
set _IS_RELEASE=Japan
set DISTROOT=..\dist
set DISTSUB=JAPAN
goto start

:korea
set _IS_RELEASE=Korea
set DISTROOT=..\dist
set DISTSUB=KOREA
goto s1

:s1

if "%2" == "cdrom" set DISTSUB=%DISTSUB%_CDROM
if "%2" == "cdrom" set _IS_RELEASE=%_IS_RELEASE%_CDROM

:start

REM ---------------------------------------------------------------------
REM variables
REM
REM if you have InstallShield installed at the directory other than
REM specified below, correct the _IS_BUILDER variable.
REM ---------------------------------------------------------------------

if "NOSET%_IS_BUILDER9_SA%" == "NOSET" (
    set _IS_BUILDER9_SA="C:\Program Files\InstallShield\StandaloneBuild9SP1\IsSABld.exe"
    set _MSI_MERGE_MODULE_PATH="C:\Program Files\Common Files\Merge Modules"
)
if "NOSET%_IS_BUILDER9%" == "NOSET" (
    set _IS_BUILDER9="c:\Program Files\InstallShield\DevStudio 9\System\IsCmdBld.exe"
)
if exist %_IS_BUILDER9_SA% (
    set _IS_BUILDER=%_IS_BUILDER9_SA%
    set USE_IS9SABLD=1
) else (
    set _IS_BUILDER=%_IS_BUILDER9%
    set USE_IS9SABLD=
)

if not exist %_IS_BUILDER% (
    echo Neither InstallShield 9 IsCmdBld.exe nor Stand-alone builder does not exist.
    echo Check the following environmental variables and set those variables.
    echo _IS_BUILDER9_SA=%_IS_BUILDER9_SA%
    echo _IS_BUILDER9=%_IS_BUILDER%
    goto err
)

if defined USE_IS9SABLD (
    if not exist %_MSI_MERGE_MODULE_PATH% (
        echo MSI Merge Module Path does not exist.
        echo _MSI_MERGE_MODULE_PATH=%_MSI_MERGE_MODULE_PATH%
        goto err
    )
)

set _IS_PROJECT=".\NetDiskSetup.ism"
set _IS_PRJDATADIR="NetDiskSetup"
set _IS_CONF="Default"

if "%2" == "zip" goto zip
if "%2" == "cd" goto cd

REM ---------------------------------------------------------------------
REM build the release
REM ---------------------------------------------------------------------

echo --------------------------------------------------------------------
rem if not exist %_IS_PROJECT% call .\isv2ism.cmd
rem if errorlevel 1 goto err
if defined USE_IS9SABLD (
    echo Running %_IS_BUILDER% -p  %_IS_PROJECT% -r %_IS_RELEASE% -o %_MSI_MERGE_MODULE_PATH%
    %_IS_BUILDER% -p  %_IS_PROJECT% -r %_IS_RELEASE% -o %_MSI_MERGE_MODULE_PATH%
) else (
    echo Running %_IS_BUILDER% -p  %_IS_PROJECT% -r %_IS_RELEASE%
    %_IS_BUILDER% -p  %_IS_PROJECT% -r %_IS_RELEASE%
)
echo --------------------------------------------------------------------

if errorlevel 1 goto err

REM ---------------------------------------------------------------------
REM copy files from DISK1 to ..\dist\%LANG%
REM ---------------------------------------------------------------------

echo --------------------------------------------------------------------
echo Copying files from DISK1 to ..\dist\%LANG%
echo --------------------------------------------------------------------

if not exist %CMDDIR%\%DISTROOT% mkdir %CMDDIR%\%DISTROOT%
if exist %CMDDIR%\%DISTROOT%\%DISTSUB% rmdir /s /q %CMDDIR%\%DISTROOT%\%DISTSUB%
if errorlevel 1 pause
if "%_IS_RELEASE%" neq "Korea" (
  xcopy /y /e /i %CMDDIR%\%_IS_PRJDATADIR%\%_IS_CONF%\%_IS_RELEASE%\"DiskImages\DISK1" %CMDDIR%\%DISTROOT%\%DISTSUB% /exclude:%CMDDIR%\.copyignore
)
if errorlevel 1 pause

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: create CD-ROM distribution layout
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:cd
echo --------------------------------------------------------------------
echo Creating CD-ROM layout
echo --------------------------------------------------------------------

xcopy /y /e /i %CMDDIR%\RELEASE\DRIVERS    %DISTROOT%\%DISTSUB%\DRIVERS /exclude:%CMDDIR%\.copyignore
if "%_IS_RELEASE%" neq "Korea" (
	if exist %CMDDIR%\RELEASE\CD\DEFAULT (
		xcopy /y /e /i %CMDDIR%\RELEASE\CD\DEFAULT %DISTROOT%\%DISTSUB% /exclude:%CMDDIR%\.copyignore
	)
)

if "%_IS_RELEASE%" == "US" goto cdus
if "%_IS_RELEASE%" == "Japan" goto cdjpn
if "%_IS_RELEASE%" == "Korea" goto cdkor
if "%_IS_RELEASE%" == "Korea_CDROM" goto cdkor
if "%_IS_RELEASE%" == "International" goto cdintl

echo -- invalid configuration specified
pause
goto end

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: ADDITIONAL PROCESSING FOR US RELEASE
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:cdus

goto cdend

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: ADDITIONAL PROCESSING FOR INTERNATIONAL RELEASE
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:cdintl

if exist %CMDDIR%\RELEASE\CD\INTL (
    xcopy /y /e /i %CMDDIR%\RELEASE\CD\INTL %DISTROOT%\%DISTSUB% /exclude:%CMDDIR%\.copyignore
)

goto cdend

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: JAPAN RELEASE
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:cdjpn

if exist %CMDDIR%\RELEASE\CD\JAPAN (
    xcopy /y /e /i %CMDDIR%\RELEASE\CD\JAPAN %DISTROOT%\%DISTSUB% /exclude:%CMDDIR%\.copyignore
)

rem erase /a %DISTROOT%\%DISTSUB%\autorun.exe
rem erase /a %DISTROOT%\%DISTSUB%\autorun.ini

goto cdend

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: KOREA RELEASE
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:cdkor

if exist .\RELEASE\CD\KOREA (
	xcopy /y /e /i .\RELEASE\CD\KOREA %DISTROOT%\%DISTSUB% /exclude:%CMDDIR%\.copyignore
)

goto cdend


:cdend
if errorlevel 1 pause

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: zipping the distribution file
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:zip
pushd .

    for /f "usebackq" %%a in (`type %PRODVERFILE%`) do set VERSIONSTR=%%a
    for /f "tokens=1 delims=." %%a in ("%VERSIONSTR%") do set VMAJOR=%%a
    for /f "tokens=2 delims=." %%a in ("%VERSIONSTR%") do set VMINOR=%%a
    for /f "tokens=3 delims=." %%a in ("%VERSIONSTR%") do set VBUILD=%%a
    for /f "tokens=4 delims=." %%a in ("%VERSIONSTR%") do set VPRIV=%%a

    set DISTFILENAME="%COMPANY%_%PRODUCT%_%VMAJOR%.%VMINOR%.%VBUILD%.%VPRIV%_%DISTSUB%.zip"

    echo -- CREATING %DISTFILENAME%

    cd %DISTROOT%\%DISTSUB%
    %CMDDIR%\bin\zip.exe -r %DISTFILENAME% *.*
    move  %DISTFILENAME% ..\

popd

goto end

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Error handlers
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:err
echo ---
echo error on executing %_IS_BUILDER% -p %_IS_PROJECT% -r %_IS_RELEASE% 
pause
goto end

:nonetwork
echo Running this script from UNC path is not supported.
echo Try to run after mapping a network drive.
pause
goto end

:usage
echo build.cmd [us,intl,japan]
goto end

:end
popd
endlocal
