@echo off
setlocal ENABLEEXTENSIONS
if "%1" == "" goto usage

pushd .
cd ..
set REGEX_PARENTDIR=%CD:\=\\%
popd
echo %0\..\sed -e s/%REGEX_PARENTDIR%/..\\/g %1
%0\..\sed -e s/%REGEX_PARENTDIR%/..\\/g %1 > tmp
if errorlevel 1 goto error
move /y tmp %1


goto end

:error
echo halted by error...
goto end

:usage
echo recmake.cmd makefile

:end
endlocal