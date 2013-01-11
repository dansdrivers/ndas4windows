@echo off
setlocal enableextensions
call %~dp0bin\runmsbuild.cmd /t:ReBuild ndaswix.proj /p:platform=x64 %*
