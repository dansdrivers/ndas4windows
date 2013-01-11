@echo off
setlocal enableextensions
for /f "usebackq" %%a in (`"%~dp0clrpath.exe"`) do set CLRPATH=%%a
"%CLRPATH%\msbuild.exe" %*
