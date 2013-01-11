@echo off
setlocal enableextensions

if not defined XM_VENDOR_PATH (
	if exist %SystemDrive%\WINDDK (
		set XM_VENDOR_PATH=%SystemDrive%\WINDDK\Supplement
	)
)
if not defined XM_VENDOR_PATH (
	echo XM_VENDOR_PATH is not defined ^(and %SystemDrive%\WINDDK does not exist^)
	exit /b 1
)

if not exist %XM_VENDOR_PATH% mkdir %XM_VENDOR_PATH%

if not exist %XM_VENDOR_PATH%\psdk (
	if exist "%ProgramFiles%\Microsoft Platform SDK" (
		echo creating link %XM_VENDOR_PATH%\psdk -^> %ProgramFiles%\Microsoft Platform SDK
		%~dp0linkd.exe %XM_VENDOR_PATH%\psdk "%ProgramFiles%\Microsoft Platform SDK"
		if errorlevel 1 (
			echo error: linkd failed, error=%ERRORLEVEL%
		) else (
			echo Platform SDK symbolic link, psdk=%XM_VENDOR_PATH%\psdk
		)
	) else (
		echo warning: Microsoft Platform SDK is not available.
		echo          - not required for DDK 6000 build environment
	)
) else (
	echo Platform SDK symbolic link, psdk=%XM_VENDOR_PATH%\psdk
)

if not exist %XM_VENDOR_PATH%\winsdk60 (
   if exist "%ProgramFiles%\Microsoft SDKs\Windows\v6.0" (
		echo creating link %XM_VENDOR_PATH%\winsdk60 -^> %ProgramFiles%\Microsoft SDKs\Windows\v6.0
		%~dp0linkd.exe %XM_VENDOR_PATH%\winsdk60 "%ProgramFiles%\Microsoft SDKs\Windows\v6.0"
		if errorlevel 1 (
			echo error: linkd failed, error=%ERRORLEVEL%
		) else (
			echo Windows SDK 6.0 symbolic link, winsdk60=%XM_VENDOR_PATH%\winsdk60
		)
	) else (
		echo warning: Microsoft Windows SDK 6.0 is not available.
		echo          - not required for DDK 3790.1830 build environment
	)
) else (
	echo Windows SDK 6.0 symbolic link, winsdk60=%XM_VENDOR_PATH%\winsdk60
)

call :path_check atl71 "%XM_VENDOR_PATH%\atl71" "ATL 7.1 library"
call :path_check wtl71 "%XM_VENDOR_PATH%\wtl71" "WTL 7.1 library"
call :path_check ntoskit "%XM_VENDOR_PATH%\ntoskit" "NT OS header files"
call :path_check boost-1_33 "%XM_VENDOR_PATH%\boost-1_33" "Boost library 1.33"

exit /b 0

:path_check
setlocal
set _NAME=%1
set _PATH=%~2
set _DESC=%~3
if not exist "%_PATH%" (
	echo error: %_DESC% is missing, not found at ^(%_PATH%^)
) else (
	echo %_DESC%, %_NAME%=%_PATH%
)
endlocal
exit /b
