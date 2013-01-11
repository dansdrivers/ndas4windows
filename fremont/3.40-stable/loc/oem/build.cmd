@echo off
setlocal enableextensions
set P1=%~dp0
set P2=%P1: =%

if "%P1%" NEQ "%P2%" (
   echo This build does not work with paths contains space.
   echo Please place files into the directory without spaces in its path.
   echo e.g. C:\ndas
   exit /b 1
)

if exist "%~dp0tools\runmsbuild.cmd" (
	call "%~dp0tools\runmsbuild.cmd" %*
) else if exist "%~dp0..\tools\runmsbuild.cmd" (
	call "%~dp0..\tools\runmsbuild.cmd" %*
) else if exist "%~dp0..\..\tools\runmsbuild.cmd" (
	call "%~dp0..\..\tools\runmsbuild.cmd" %*
)
