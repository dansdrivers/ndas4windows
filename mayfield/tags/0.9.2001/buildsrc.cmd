@echo off
setlocal ENABLEEXTENSIONS
pushd .
%0\..\bin\logtime "BEGIN BUILDSRC %*" > nul

:: LANSCSISYSTEMv2
%0\..\bin\logtime "  BEGIN lanscsisystemv2" > nul
pushd .

cd lanscsisystemv2\src
call .\oembuild %*
if errorlevel 1 goto error

popd
%0\..\bin\logtime "  END   lanscsisystemv2" > nul

:: INSTALLHELPER
%0\..\bin\logtime "  BEGIN installhelper" > nul
pushd .

cd installhelper\src
call .\oembuild %*
if errorlevel 1 goto error

popd
%0\..\bin\logtime "  END   installhelper" > nul

:: OEM branded src
if "%1" == "retail" goto skipoem
pushd .
if not exist oem\%1 echo WARNING oem\%1 not exist!
if exist oem\%1 (
  cd oem\%1
  if exist buildsrc.cmd (
    call buildsrc %*
    if errorlevel 1 goto error
  ) else (
    echo WARNING oem\%1\buildsrc not exist!
  ) 
  cd ..\..
)
popd

:skipoem
goto end

:error
popd
echo STOP ON ERROR!!!
goto end

:end
%0\..\bin\logtime "END   BUILDSRC" > nul
popd
endlocal
