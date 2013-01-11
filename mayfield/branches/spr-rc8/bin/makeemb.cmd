@echo off
setlocal

set MSISRC=%1
set MSINEW=%2
if "%MSISRC%-" == "-" set MSISRC=ndas.msi
if not exist %MSISRC% goto err_file_not_found

if "%MSINEW%-" == "-" set MSINEW=ndas-int.msi
echo makeemb: Copying %MSISRC% to %MSINEW%
copy /y %MSISRC% %MSINEW%

for %%p in (%MSISRC%) do set MSITRS_DIR=%%~pp
for %%p in (%MSINEW%) do set MSINEW_PATH=%%~fp
for %%p in (%0\..\emb.vbs) do set EMB_PATH=%%~fp

pushd .
cd %MSITRS_DIR%
for %%a in (*.mst) do (
	echo makeemb: Embedding %%a to %MSINEW_PATH%
	cscript.exe //nologo %EMB_PATH% %MSINEW_PATH% %%a %%a
)
popd

goto end
:err_file_not_found
echo %MSISRC% does not exist.
goto end
:end
endlocal
