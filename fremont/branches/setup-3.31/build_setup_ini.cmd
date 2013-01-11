@echo off
setlocal enabledelayedexpansion

set LOCALE_TABLE=en-US.1033 zh-CN.2052 zh-TW.1028 de-DE.1031 es-ES.1034 fr-FR.1036 it-IT.1040 ja-JP.1041 ko-KR.1042 pt-PT.2070 da-DK.1030 nl-NL.1043 nb-NO.1044 sv-SE.1053 ru-RU.1049

set PACKAGE_PATH=%1
set SETUP_INI=%PACKAGE_PATH%\setup.ini

for /r %%f in (%PACKAGE_PATH%\*.msi) do set BASE_NAME=%%~nf

echo [MSI] > %SETUP_INI%
echo MSI=%BASE_NAME%.msi >> %SETUP_INI%
echo. >> %SETUP_INI% 
echo [Product] >> %SETUP_INI%
echo ProductCode= >> %SETUP_INI%
echo ProductName= >> %SETUP_INI%
echo ProductVersion= >> %SETUP_INI%
echo Recache=1 >> %SETUP_INI%
echo. >> %SETUP_INI% 
echo [MUI] >> %SETUP_INI%
echo PROMPT=1 >> %SETUP_INI%

for /r %%f in (%PACKAGE_PATH%\%BASE_NAME%*.mst) do call :SET_MUI %%~nf

echo. >> %SETUP_INI%
echo [Options] >> %SETUP_INI%
echo. >> %SETUP_INI%
echo [Display] >> %SETUP_INI%
echo. >> %SETUP_INI%
echo [Logging] >> %SETUP_INI%
echo Template=NDAS Software Setup.txt >> %SETUP_INI%
echo Type=v* >> %SETUP_INI%
echo. >> %SETUP_INI%
echo [MinOSRequirement] >> %SETUP_INI%
echo VersionNT_1=500 >> %SETUP_INI%
echo WindowsBuild_1=2195 >> %SETUP_INI%
echo ServicePackLevel_1=0 >> %SETUP_INI%
echo. >> %SETUP_INI%
echo [MSIEngine] >> %SETUP_INI%
echo Version=200 >> %SETUP_INI%
echo Installer=instmsiw.exe /c:"msiinst.exe /delayrebootq" >> %SETUP_INI%

for /r %%f in (%PACKAGE_PATH%\%BASE_NAME%*.mst) do call :SET_MST %%~nf

REM --------------------------------------------------------------------------------
:SET_MUI
set LOCALE=%1
set LOCALE=%LOCALE:~-5%

for %%i in (%LOCALE_TABLE%) do (
set LOCALE_SET=%%i
if "!LOCALE_SET:~0,5!" equ "%LOCALE%" echo !LOCALE_SET:~6,100!=1 >> %SETUP_INI%
)

goto :eof

REM --------------------------------------------------------------------------------
:SET_MST
set LOCALE=%1
set LOCALE=%LOCALE:~-5%

for %%i in (%LOCALE_TABLE%) do (
set LOCALE_SET=%%i
if "!LOCALE_SET:~0,5!" equ "%LOCALE%" (
echo. >> %SETUP_INI%
echo [MST.!LOCALE_SET:~6,100!] >> %SETUP_INI%
echo MST1=%BASE_NAME%-%LOCALE%.mst >> %SETUP_INI%
)
)
