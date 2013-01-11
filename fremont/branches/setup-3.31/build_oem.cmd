@echo off 
REM --------------------------------------------------------------------------------
REM 

set CONFIGURATION=fre
set SKU=ndasscsi
REM set ADDITIONAL_PREDEFINITION="/p:includendasntfs=true"
set SCRIPT_DIR=%~dp0

REM requires oem name like "generic"
if "%1" equ "" (
echo oem name required
goto :eof
)

set OEMPATH=oembuild\%1

if not exist %OEMPATH% (
echo "%1" not exist
goto :eof
)

if "%2" equ "" (
echo "ProductVersion must be specified. (e.g. 3.31.1701)"
goto :eof
)

set PRODUCT_VERSION=%2

REM LOCALES will be like ";res.en-US;res.ko-KR". Required at BUILD_MSI
set LOCALES=
for /d %%f in (%OEMPATH%\msi_*) do (
call :BUILD_LOCALES %%~nf
)
set LOCALES="%LOCALES%"

REM build *.msi for each platforms, locales
for /d %%f in (%OEMPATH%\msi_*) do (
call :BUILD_MSI %%~nf
)

REM build *.mst for each platforms, locales
if not exist %OEMPATH%\package mkdir %OEMPATH%\package

call :BUILD_MST_PLATFORM i386
call :BUILD_MST_PLATFORM amd64

copy /y supplements\bootstrap\x86\instmsiw.exe %OEMPATH%\package\i386\
call :BUILD_SETUP i386
call :BUILD_SETUP amd64

if not exist %OEMPATH%\package\common mkdir %OEMPATH%\package\common
copy /y %OEMPATH%\package\i386\*.msi %OEMPATH%\package\common\ 
copy /y %OEMPATH%\package\i386\*.mst %OEMPATH%\package\common\ 
copy /y %OEMPATH%\package\i386\setup.ini %OEMPATH%\package\common\setup.i386.ini
copy /y %OEMPATH%\package\i386\setup.exe %OEMPATH%\package\common\
copy /y %OEMPATH%\package\i386\instmsiw.exe %OEMPATH%\package\common\

copy /y %OEMPATH%\package\amd64\*.msi %OEMPATH%\package\common\ 
copy /y %OEMPATH%\package\amd64\*.mst %OEMPATH%\package\common\ 
copy /y %OEMPATH%\package\amd64\setup.ini %OEMPATH%\package\common\setup.amd64.ini

call :SELF_EXTRACT i386
call :SELF_EXTRACT amd64
call :SELF_EXTRACT common

goto :eof

REM --------------------------------------------------------------------------------
:SELF_EXTRACT

pushd .
set PLATFORM=%1
cd %OEMPATH%\package\%PLATFORM%
%SCRIPT_DIR%\tools\7za.exe a ..\%PLATFORM%.7z *
cd ..
copy /b %SCRIPT_DIR%\tools\self-extract\7zS.sfx + %SCRIPT_DIR%\tools\self-extract\config.txt + %PLATFORM%.7z setup-%PLATFORM%.exe
popd

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_SETUP
set PLATFORM=%1
copy /y %OEMPATH%\branded\%CONFIGURATION%\%PLATFORM%\setup\setup.exe %OEMPATH%\package\%PLATFORM%\
call build_setup_ini.cmd %OEMPATH%\package\%PLATFORM%

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_LOCALES
set LOCALE=%1
set LOCALE=%LOCALE:~4,100%

if /i "%LOCALE%" equ "neutral" goto :eof

set LOCALES=%LOCALES%;res.%LOCALE%

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_MSI
set LOCALEPATH=%1
set LOCALE=%1
set LOCALE=%LOCALE:~4,100%

set THE_COMMAND=build_setup.cmd
set THE_COMMAND=%THE_COMMAND% /p:sku=%SKU%
set THE_COMMAND=%THE_COMMAND% /p:ProductVersion=%PRODUCT_VERSION%
REM set THE_COMMAND=%THE_COMMAND% /p:ProductRevision=12345
set THE_COMMAND=%THE_COMMAND% /p:SetupFileSource=%OEMPATH%\branded\
set THE_COMMAND=%THE_COMMAND% /p:Configuration=%CONFIGURATION%
if /i "%LOCALE%" equ "neutral" (
set THE_COMMAND=%THE_COMMAND% /p:SetupLanguage=0
) else (
set THE_COMMAND=%THE_COMMAND% /p:SetupLanguage=%LOCALE%
)
set THE_COMMAND=%THE_COMMAND% /p:SetupWxlDir=%OEMPATH%\%LOCALEPATH%\wxl
set THE_COMMAND=%THE_COMMAND% /p:Platform=all
set THE_COMMAND=%THE_COMMAND% /p:LocalBuildBaseDir=..\..\..\%OEMPATH%\%LOCALEPATH%\
set THE_COMMAND=%THE_COMMAND% /p:LocalizedResources=%LOCALES%
set THE_COMMAND=%THE_COMMAND% %ADDITIONAL_PREDEFINITION%

call %THE_COMMAND%

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_MST_LOCALE
set LOCALE=%1
set LOCALE=%LOCALE:~4,100%

set THE_COMMAND=tools\MsiTran.exe -g
set THE_COMMAND=%THE_COMMAND% %OEMPATH%\msi_neutral\obj\bin\%MSI_PLATFORM%\%MSI_BASENAME%.msi
set THE_COMMAND=%THE_COMMAND% %OEMPATH%\msi_%LOCALE%\obj\bin\%MSI_PLATFORM%\%MSI_BASENAME%-%LOCALE%.msi
REM set THE_COMMAND=%THE_COMMAND% package\%MSI_BASENAME%-%LOCALE%.mst
set THE_COMMAND=%THE_COMMAND% %OEMPATH%\package\%MSI_PLATFORM%\%MSI_BASENAME%-%LOCALE%.mst

call %THE_COMMAND%
 
goto :eof

REM --------------------------------------------------------------------------------
:BUILD_MST_BASENAME
set MSI_BASENAME=%1

for /d %%f in (%OEMPATH%\msi_*) do (
if "%%~nf" neq "msi_neutral" (
call :BUILD_MST_LOCALE %%~nf
)
)

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_MST_PLATFORM
set MSI_PLATFORM=%1
for /r %%f in (%OEMPATH%\msi_neutral\obj\bin\%1\*.msi) do (
if not exist %OEMPATH%\package\%MSI_PLATFORM%\ mkdir %OEMPATH%\package\%MSI_PLATFORM%\
copy /y %%f %OEMPATH%\package\%MSI_PLATFORM%\
call :BUILD_MST_BASENAME %%~nf
)

goto :eof
