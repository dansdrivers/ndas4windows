@echo off
setlocal enableextensions
pushd .
pushd %0\..\
popd

for /f "usebackq" %%a in (`type .\productver.txt`) do set VERSIONSTR=%%a
echo %VERSIONSTR%

for /f "tokens=1 delims=." %%a in ("%VERSIONSTR%") do set VMAJOR=%%a
for /f "tokens=2 delims=." %%a in ("%VERSIONSTR%") do set VMINOR=%%a
for /f "tokens=3 delims=." %%a in ("%VERSIONSTR%") do set VBUILD=%%a
for /f "tokens=4 delims=." %%a in ("%VERSIONSTR%") do set VPRIV=%%a

echo MAJOR: %VMAJOR%
echo MINOR: %VMINOR%
echo BUILD: %VBUILD%
echo PRIV : %VPRIV%

:: EXEC

cscript //nologo .\bin\setprodver.js prodver .\lanscsisystemv2\src\inc\prodver.h %VERSIONSTR%
cscript //nologo .\bin\setisxmlver.js .\installer\NetDiskSetup.ism %VERSIONSTR%
rem cscript //nologo .\bin\setisxmlver.js .\oem\iomega\installer\NetDiskSetup.ism %VERSIONSTR%
cscript //nologo .\bin\setisxmlver.js .\oem\logitec\installer\NetDiskSetup.ism %VERSIONSTR%
cscript //nologo .\bin\setisxmlver.js .\oem\moritani\installer\NetDiskSetup.ism %VERSIONSTR%
cscript //nologo .\bin\setisxmlver.js .\oem\gennetworks\installer\NetDiskSetup.ism %VERSIONSTR%

:end
popd
endlocal
