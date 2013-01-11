@echo off
setlocal enableextensions
call :crmdir "%~dp0publish\fre"
call :crmdir "%~dp0publish\chk"
exit /b

:crmdir
if exist "%~1" echo Removing %~1 && rmdir /s /q "%~1"
exit /b

