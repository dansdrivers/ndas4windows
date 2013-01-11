@echo off
setlocal ENABLEEXTENSIONS
set INCLUDE=
set LIB=

if not defined VS60DIR set VS60DIR=C:\Program Files\Microsoft Visual Studio
if not defined MSSDK set MSSDK=C:\Program Files\Microsoft SDK
call "%VS60DIR%\VC98\Bin\VCVARS32.BAT"
call "%MSSDK%\setenv" /RETAIL /2000
title SDKBUILD
nmake %*
title %NEWTITLE%
endlocal

