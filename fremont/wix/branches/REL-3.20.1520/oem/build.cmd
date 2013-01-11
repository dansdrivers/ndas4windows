@echo off
setlocal enableextensions
if "%*" == "" (
   echo usage: %~n0 /p:config=^<config^>
   echo Available configurations:
   echo.
   for %%a in (*.config) do echo  %%~na
   exit /b 1
)

@call %~dp0..\bin\runmsbuild.cmd wixoem.proj %*
