@echo off
for /f "usebackq" %%a in (`%0\..\..\..\bin\shrtpath.exe "C:\Program Files\Microsoft SDK\include"`) do set PSDK_INC_PATH=%%a
for /f "usebackq" %%a in (`%0\..\..\..\bin\shrtpath.exe "C:\Program Files\Microsoft SDK\lib"`) do set PSDK_LIB_PATH=%%a
echo PSDK_INC_PATH=%PSDK_INC_PATH%
echo PSDK_LIB_PATH=%PSDK_LIB_PATH%

