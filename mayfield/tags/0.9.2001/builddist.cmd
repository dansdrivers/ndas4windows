@echo off
setlocal enableextensions
pushd .

call buildsrc %1
if errorlevel 1 goto err

if "%1" == "retail" (
	cd installer
) else (
	cd oem\%1\installer
)
if errorlevel 1 goto err

call sync2src
if errorlevel 1 goto err

call buildsetup %2
if errorlevel 1 goto err

goto end
:err
echo error %ERRORLEVEL%
goto end

:end
popd
