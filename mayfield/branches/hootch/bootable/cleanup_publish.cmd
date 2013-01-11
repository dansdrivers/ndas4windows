@echo off

call :rmdir %~dp0publish\fre
call :rmdir %~dp0publish\chk
call :rmdir %~dp0lib\fre
call :rmdir %~dp0lib\chk
call :rmdir %~dp0lib\public\fre
call :rmdir %~dp0lib\public\chk
call :rmdir %~dp0src\umapps\ndassvc\lib\fre
call :rmdir %~dp0src\umapps\ndassvc\lib\chk
call :rmdir %~dp0src\umapps\ndasbind\lib\fre
call :rmdir %~dp0src\umapps\ndasbind\lib\chk

goto :EOF

:rmdir
if exist "%1" echo Removing %1 && rmdir /s /q "%1"
goto :EOF

