@echo off
setlocal enableextensions
set BUILD_PARAMS=%*
if not defined BUILD_PARAMS (
   echo Build oem kit from the current localization data in ..\loc   
   echo usage: %~n0 /p:oem=^<oem-name^> ^(e.g. coworld^)
   echo.
   echo oem-name is the directory name in ..\loc.
   echo.
   dir /a:-hd /b %~dp0..\oem
   echo.
   exit /b 1
)
%~dp0..\tools\runmsbuild.cmd oemkit.proj %*
