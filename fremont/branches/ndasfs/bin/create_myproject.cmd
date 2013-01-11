@echo off
setlocal enableextensions

call :path_normalize NDAS_ROOT "%~dp0.."

echo NDAS_ROOT=%NDAS_ROOT%
echo NDAS_INC_PATH=%NDAS_ROOT%\src\inc
echo NDAS_PUBLIC_INC_PATH=%NDAS_ROOT%\src\inc\public
echo NDAS_DRIVER_INC_PATH=%NDAS_ROOT%\src\drivers\inc

call :create_myproject %NDAS_ROOT%\src
for /d %%a in (%NDAS_ROOT%\src\*) do call :create_myproject %%a

exit /b 0

:path_normalize
set %1=%~f2
exit /b

:create_myproject
setlocal
set TARGET_DIR=%~1
set PROJECT_MK_FILE=%TARGET_DIR%\project.mk
set MYPROJECT_FILE=%TARGET_DIR%\myproject.mk
if exist "%PROJECT_MK_FILE%" (
   echo creating %MYPROJECT_FILE%
   echo !IF 0 > %MYPROJECT_FILE%
   echo NDAS_INC_PATH=%NDAS_ROOT%\src\inc >> %MYPROJECT_FILE%
   echo NDAS_PUBLIC_INC_PATH=%NDAS_ROOT%\src\inc\public >> %MYPROJECT_FILE%
   echo NDAS_DRIVER_INC_PATH=%NDAS_ROOT%\src\drivers\inc >> %MYPROJECT_FILE%
   echo !ENDIF >> %MYPROJECT_FILE%
)
exit /b

