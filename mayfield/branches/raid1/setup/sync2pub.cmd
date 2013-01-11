@echo off
setlocal
set _SRC_DIR=..\publish\fre\i386
if "%1-" == "chk-" set _SRC_DIR=..\publish\chk\i386
..\bin\robocopy %_SRC_DIR% publish /e /xd .svn /xd symbols /xf *.pdb

endlocal
