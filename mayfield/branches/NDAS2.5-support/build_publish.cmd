@echo off
call %~dp0cleanup_publish.cmd

rem
rem THIS LINES are for an workaround for removing racing making the directories from publish.js
rem

call :precreate_lib
call :precreate_lib amd64

pushd .
cd src
call cleanup.cmd
call buildup.cmd
if errorlevel 1 goto err_exit
call buildup.cmd /x64
if errorlevel 1 goto err_exit
cd ..
popd

call %~dp0build_postprocess.cmd

goto :EOF

:precreate_lib
setlocal
if "%1" == "" (
   set ARCH=i386
) else (
  set ARCH=%1
)
call :precreate_dir .\lib\fre\%ARCH%
call :precreate_dir .\lib\fre\kernel\%ARCH%
call :precreate_dir .\lib\chk\%ARCH%
call :precreate_dir .\lib\chk\kernel\%ARCH%
endlocal
goto :EOF

:precreate_dir
if not exist %1 mkdir %1
goto :EOF

:err
cd ..
popd
