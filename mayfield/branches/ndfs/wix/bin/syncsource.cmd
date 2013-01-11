@echo off
setlocal enableextensions
set BASEDIR=%~dp0..
set PUBDIR=%1
if "%PUBDIR%" equ "" set PUBDIR=%BASEDIR%\..\publish
for %%a in (%BASEDIR%) do set BASEDIR=%%~fa
for %%a in (%PUBDIR%) do set PUBDIR=%%~fa

if not exist %PUBDIR% goto nopub

set ROBOCOPY_FLAGS=/njh /njs /xf /xx *.pdb /xf *.cdf
%~dp0robocopy %PUBDIR%\fre\i386 %BASEDIR%\SourceDir %ROBOCOPY_FLAGS%
%~dp0robocopy %PUBDIR%\fre\amd64 %BASEDIR%\SourceDir\amd64 %ROBOCOPY_FLAGS%

goto :EOF

:nopub
echo %PUBDIR% does not exist.
goto :EOF
