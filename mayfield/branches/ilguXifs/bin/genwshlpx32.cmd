@echo off
setlocal enableextensions
set PUBDIR=%~dp0..\publish
copy /y %PUBDIR%\chk\i386\wshlpx.dll %PUBDIR%\chk\amd64\wshlpx32.dll
copy /y %PUBDIR%\chk\i386\wshlpx.pdb %PUBDIR%\chk\amd64\wshlpx32.pdb
copy /y %PUBDIR%\fre\i386\wshlpx.dll %PUBDIR%\fre\amd64\wshlpx32.dll
copy /y %PUBDIR%\fre\i386\wshlpx.pdb %PUBDIR%\fre\amd64\wshlpx32.pdb

