@echo off
setlocal enableextensions
pushd .
cd /d %~dp0
%WNETBASE%\bin\x86\nmake.exe -nologo -f "%~dp0wshlpx32.mk" %*
popd
