@echo off
setlocal enableextensions
::
:: Configurations
::
:: Resolve full path to BINPATH
set SELFPATH=%0\..
for %%f in (%SELFPATH%) do set SELFPATH=%%~ff
set BINPATH=%SELFPATH%\bin
for %%f in (%BINPATH%) do set BINPATH=%%~ff
set ROBOCOPY=%BINPATH%\robocopy.exe
:: Creates obk\base directory
set OBKROOT=obk\base
if not exist %OBKROOT% mkdir %OBKROOT%

:: Copy src\umapps\ndasmgmt\resource -> ndasmgmtres
set CPPARAMS=/xf dirs /NJH /NJS
%ROBOCOPY% %SELFPATH%\src\umapps\ndasmgmt\resource %OBKROOT%\ndasmgmtres %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\src\umapps\ndasmgmt\resource\res %OBKROOT%\ndasmgmtres\res %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\src\umapps\ndasbind\resource %OBKROOT%\ndasbindres %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\src\umapps\ndasbind\resource\res %OBKROOT%\ndasbindres\res %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\src\umapps\ndasmsg %OBKROOT%\ndasmsg /xf _*.* %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\setup %OBKROOT%\setup %CPPARAMS% /xf _*.ism /purge
%ROBOCOPY% %SELFPATH%\setup\support %OBKROOT%\setup\support %CPPARAMS% /purge
%ROBOCOPY% %SELFPATH%\publish\fre\i386 %OBKROOT%\setup\publish /xf *.pdb %CPPARAMS% /purge

goto end

:usage
echo build_obk
:end
endlocal
