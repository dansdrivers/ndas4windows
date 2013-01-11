@echo off
@cscript //nologo fdatecmp.js %*
echo %ERRORLEVEL%
if errorlevel 4 (
	echo error!
) else if errorlevel 3 (
	echo %1 is newer than %2
) else if errorlevel 2 (
	echo %1 and %2 have same timestamps.
) else if errorlevel 1 (
	echo %1 is older than %2
)