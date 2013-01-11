@echo off
setlocal enableextensions
call :crmdir "%~dp0lib\fre"
call :crmdir "%~dp0lib\chk"
call :crmdir "%~dp0lib\public\fre"
call :crmdir "%~dp0lib\public\chk"
call :crmdir "%~dp0src\umapps\ndassvc\lib\fre"
call :crmdir "%~dp0src\umapps\ndassvc\lib\chk"
call :crmdir "%~dp0src\umapps\ndasbind\lib\fre"
call :crmdir "%~dp0src\umapps\ndasbind\lib\chk"
exit /b

:crmdir
if exist "%~1" echo Removing %1 && rmdir /s /q "%~1"
exit /b

