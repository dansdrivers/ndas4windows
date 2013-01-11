@echo off
setlocal enableextensions
if not exist .\PRODUCTVER.txt echo Run from the project root directory! && exit /b 1
for /f %%a in (PRODUCTVER.txt) do set PRODVER=%%a
pushd .

if not exist dist mkdir dist

cd publish
set TARGET_FILE_NAME=ndas-%PRODVER%-bin.zip
set TARGET_FILE_PATH=..\dist\%TARGET_FILE_NAME%
if exist %TARGET_FILE_PATH% del /q %TARGET_FILE_PATH%
..\bin\zip -r %TARGET_FILE_PATH% fre
..\bin\zip -r %TARGET_FILE_PATH% chk
cd ..

for %%a in (%TARGET_FILE_PATH%) do echo created: %%~fa

popd
endlocal
