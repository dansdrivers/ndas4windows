@echo off
setlocal enableextensions
call %~dp0bin\runmsbuild.cmd /t:ReBuild %~dp0ndaswix.proj /p:platform=x64 %*
