@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
call %SCRIPT_DIR%tools\runmsbuild.cmd %SCRIPT_DIR%setup.proj %*

