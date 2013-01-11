@echo off
call config.bat

SET IOSIZE=256
SET POS=10000

rem goto skip

echo #### Test NOP
%CLI% nop %TARGET%

:skip

echo #### Test max connection time
echo ### Set connection time out to 10 sec.
%CLI% rawvendor %TARGET% 0x00002 0x02 "" 0 0 9

echo ### Disconnect cable for 8 seconds after pressing any key.(Should success) - Currently not working because lpx driver close connection after 5 sec.
pause
%CLI% loginwait %TARGET% 0x10002 15

echo ### Disconnect cable for 12 seconds after pressing any key.(Should be disconnected)
pause
%CLI% loginwait %TARGET% 0x10002 20

pause
