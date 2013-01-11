@echo off
setlocal enableextensions
call %~dp0bin\runmsbuild.cmd /t:ReBuildAllPlatforms ndaswix.proj %*
