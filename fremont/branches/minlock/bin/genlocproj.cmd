@echo off
setlocal enableextensions

set projectroot=%~dp0..\
for %%a in (%projectroot%) do set projectroot=%%~dpa
set locdir=%projectroot%publish\loc

if not exist %locdir% mkdir "%locdir%"

%~dp0resmap.exe /out:%locdir%\ndasmgmt.resmap %projectroot%src\umapps\ndasmgmt\ndasmgmt.rc
if errorlevel 0 echo ndasmgmt.resmap created.

%~dp0resmap.exe /out:%locdir%\ndasmgmt_app.resmap %projectroot%src\umapps\ndasmgmt\ndasmgmt_app.rc
if errorlevel 0 echo ndasmgmt_app.resmap created.

%~dp0resmap.exe /out:%locdir%\ndasbind.resmap %projectroot%src\umapps\ndasbind\ndasbind.rc
if errorlevel 0 echo ndasbind_app.resmap created.

%~dp0resmap.exe /out:%locdir%\ndasbind_app.resmap %projectroot%src\umapps\ndasbind\ndasbind_app.rc
if errorlevel 0 echo ndasbind.resmap created.

copy %projectroot%src\umapps\ndasmsg\ndasmsg.xml %locdir%\ndasmsg.xml > nul
if errorlevel 0 echo ndasmsg.xml created.


if not exist %locdir%\resdata mkdir %locdir%\resdata

echo.
echo copying resdata from %projectroot%src\umapps\ndasmgmt
%~dp0resmapcopy.exe %locdir%\ndasmgmt.resmap %projectroot%src\umapps\ndasmgmt %locdir%\resdata

echo.
echo copying resdata from %projectroot%src\umapps\ndasbind
%~dp0resmapcopy.exe %locdir%\ndasbind.resmap %projectroot%src\umapps\ndasbind %locdir%\resdata

if not exist %locdir%\base mkdir %locdir%\bin
%~dp0robocopy %projectroot%publish\fre\i386 %locdir%\bin\i386 /xf *.pdb /xf *.cdf /xf *.signed /njs /njh
%~dp0robocopy %projectroot%publish\fre\amd64 %locdir%\bin\amd64 /xf *.pdb /xf *.cdf /xf *.signed /njs /njh
