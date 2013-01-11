@echo off
setlocal enableextensions
set BP_BINDIR=%~dp0bin
set BP_PUBDIR=%~dp0publish
call %BP_BINDIR%\genwshlpx32.cmd
call %BP_BINDIR%\mkcat.cmd %BP_PUBDIR%\chk\i386
call %BP_BINDIR%\mkcat.cmd %BP_PUBDIR%\fre\i386
call %BP_BINDIR%\mkcat.cmd %BP_PUBDIR%\chk\amd64
call %BP_BINDIR%\mkcat.cmd %BP_PUBDIR%\fre\amd64

