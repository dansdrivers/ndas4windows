@echo off
%0\..\chkosver -q eq 5 0
if errorlevel 2 goto error
if errorlevel 1 goto winxporlater

:win2k
rundll32.exe setupapi,InstallHinfSection DefaultInstall 132 .\lfsflt2k.inf
goto end

:winxporlater
rundll32.exe setupapi,InstallHinfSection DefaultInstall 132 .\lfsfilt.inf
goto end

:error
echo Cannot determine the OS version!
goto end

:end

