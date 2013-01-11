@echo off
::
:: Creates driver catalog files with CDF
:: Copyright (C) 2003-2006 XIMETA, Inc.
::
:: August 2006 Chesong Lee
::  Revised to accept multiple CDF files or directories
::
:: October, 2004 Chesong Lee
::  Initial Implementation
::
setlocal enableextensions
set MAKECAT=%~dp0makecat.exe

:: This script changes the current path during the processing
pushd "%CD%"
call :main %*
popd
exit /b

:main
:loop
if "%~1" == "" exit /b
call :process_arg "%~1"
if errorlevel 1 exit /b
shift
goto loop

:process_arg
REM Directory
if exist "%1\" goto process_dir
REM CDF file
if exist "%1" goto process_cdf
REM Otherwise error
echo ERROR: Invalid file or directory (%1)
exit /b 1

:process_dir
echo Processing %~1
pushd .
cd %~f1
for %%a in (*.cdf) do (
	call :process_cdf %%a
	if errorlevel 1 exit /b
)
popd
exit /b

:process_cdf
pushd .
cd %~dp1
echo. ^<%~nx1^>
rem echo makecat %~f1
%MAKECAT% %~nx1
popd
exit /b
