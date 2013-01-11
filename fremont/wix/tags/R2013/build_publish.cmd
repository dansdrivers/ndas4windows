@echo off
setlocal enableextensions
call %~dp0bin\runmsbuild.cmd /t:ReBuildAllPlatforms %~dp0ndaswix.proj %*
