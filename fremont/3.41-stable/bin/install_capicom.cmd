@echo off
setlocal enableextensions

if exist %systemroot%\system32\capicom.dll (
echo CAPICOM seeems to be registered already.
exit /b 0
)
copy /y %~dp0capicom.dll %systemroot%\system32\capicom.dll
start /w regsvr32 /s %systemroot%\system32\capicom.dll
echo CAPICOM registered.
