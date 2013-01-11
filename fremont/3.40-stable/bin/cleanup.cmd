@echo off
setlocal ENABLEEXTENSIONS
pushd .

set CURDIR=%CD%
echo -- Cleaning %CURDIR%

:next

set TARGETDIR=.\Release
if not exist %TARGETDIR% goto next
echo Deleting %TARGETDIR%
rmdir /s /q %TARGETDIR%

:next

set TARGETDIR=.\Debug
if not exist %TARGETDIR% goto next
echo Deleting %TARGETDIR%
rmdir /s /q %TARGETDIR%

:next

set TARGETDIR=.\objfre_w2k_x86
if not exist %TARGETDIR% goto next
echo Deleting %TARGETDIR%
rmdir /s /q %TARGETDIR%

:next

set TARGETDIR=.\objchk_w2k_x86
if not exist %TARGETDIR% goto next
echo Deleting %TARGETDIR%
rmdir /s /q %TARGETDIR%

:next
if "%1" == "/r" ( set opt=/r & shift )

for %opt% %%a in (*.bak build*.log build*.wrn build*.err) do (echo Deleting %%a & erase /q %%a)

if "%1" == "" goto next
if "%1" == "dist" for %opt% %%a in (*.bsc *.opt *.ncb *.aps) do (echo Deleting %%a & erase /q %%a & shift)
if "%1" == "_" for %opt% %%a in (_*.*) do (echo Deleting %%a & erase /q %%a & shift)
:next

:end
popd
endlocal
