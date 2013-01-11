@echo off
call config.bat

%CLI% LockedWrite %TARGET% %1 %2 %3 %4

pause

