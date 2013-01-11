@echo off
setlocal enableextensions
if defined XM_SIGNCODE_CMD (
	call %~dp0bin\runmsbuild.cmd /t:BuildPublishX86 ndaswix.proj /p:SignCode=yes %*
) else (
	call %~dp0bin\runmsbuild.cmd /t:BuildPublishX86 ndaswix.proj %*
)



