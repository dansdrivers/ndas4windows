@echo off
setlocal enableextensions
set ARGS=%*
set PASSING_ARGS=
set BUILD_X64=
if /i "%ARGS:~0,4%" equ "/x64" set BUILD_X64=1
if /i "%ARGS:~0,4%" equ "-x64" set BUILD_X64=1
if /i "%BUILD_X64%" equ "1" (
  set PASSING_ARGS=%ARGS:~5%
) else (
  set PASSING_ARGS=%ARGS%
)

set B_BUILD64PARAM=
if "%BUILD_X64%" equ "1" set B_BUILD64PARAM=/X64
set B_BUILDOPT=-cegwMiIBZ

call build_all.cmd %B_BUILD64PARAM% chk %B_BUILDOPT% %PASSING_ARGS%
if errorlevel 1 goto :EOF

call build_all.cmd %B_BUILD64PARAM% fre %B_BUILDOPT% %PASSING_ARGS%
if errorlevel 1 goto :EOF
