@echo off
rem %1: UserId %2: Time to Wait %3: PW
call config.bat

%CLI% LoginWait %TARGET% %1 %2 %3 %4

pause
exit
