@echo off
call config.bat

SET IOSIZE=256
SET POS=10000

echo #### User Permission test
echo ### Reset account to default
%CLI% ResetAccount %TARGET%

echo ### Warming up(can fail).
%CLI% LoginR %TARGET% 0x10001

echo ### User 2: EW
%CLI% SetPermission %TARGET% 0x20007

echo ### User 3: SW
%CLI% SetPermission %TARGET% 0x30003

echo ### User 4: RO
%CLI% SetPermission %TARGET% 0x40001

echo ### User 5: No permission
%CLI% SetPermission %TARGET% 0x50000


echo ### Login mode and permission test
echo ## Login user 2 with RO(Can login but disconected when writing)
%CLI% LoginRw %TARGET% 0x20001

pause

echo ## Login user 2 with RW(Should success)
%CLI% LoginRw %TARGET% 0x20002

pause

echo ## Login user 2 with EW(Should success)
%CLI% LoginRw %TARGET% 0x20004

pause

echo ## Login user 3 with RO(Can login but disconected when writing)
%CLI% LoginRw %TARGET% 0x30001

pause

echo ## Login user 3 with RW(Should success)
%CLI% LoginRw %TARGET% 0x30002

echo ## Login user 3 with EW(Should fail)
%CLI% LoginRw %TARGET% 0x30004


echo ## Login user 4 with RO(Can login but disconected when writing)
%CLI% LoginRw %TARGET% 0x40001

echo ## Login user 4 with RW(Should fail)
%CLI% LoginRw %TARGET% 0x40002

echo ## Login user 4 with EW(Should fail)
%CLI% LoginRw %TARGET% 0x40004


echo ## Login user 5 with no perm(Should fail)
%CLI% LoginR %TARGET% 0x50000

echo ## Login user 5 with RO(Should fail)
%CLI% LoginR %TARGET% 0x50001

echo ## Login user 5 with RW(Should fail)
%CLI% LoginR %TARGET% 0x50002

echo ## Login user 5 with EW(Should fail)
%CLI% LoginR %TARGET% 0x50004


:multiuser
echo ### Testing login mode when other hosts are connected.
echo ## Login EW when other host is connected in EW.(Should fail)
start loginwait 0x10004 10
call wait 2
%CLI% LoginR %TARGET% 0x20004

pause

echo ## Login EW when other host is connected in SW.(Should fail)
start loginwait 0x10002 10
call wait 2
%CLI% LoginR %TARGET% 0x20004

pause

echo ## Login EW when other host is connected in RO.(Should success)
start loginwait 0x10001 10
call wait 2
%CLI% LoginR %TARGET% 0x20004

pause

echo ## Login SW when other host is connected in EW.(Should fail)
start loginwait 0x10004 10
call wait 2
%CLI% LoginR %TARGET% 0x20002

pause

echo ## Login SW when other host is connected in SW.(Should success)
start loginwait 0x10002 10
call wait 2
%CLI% LoginR %TARGET% 0x20002

pause

echo ## Login SW when other host is connected in RO.(Should success)
start loginwait 0x10001 10
call wait 2
%CLI% LoginR %TARGET% 0x20002

pause

echo ## Login RO when other host is connected in EW.(Should success)
start loginwait 0x10004 10
call wait 2
%CLI% LoginR %TARGET% 0x20001

pause

echo ## Login RO when other host is connected in SW.(Should success)
start loginwait 0x10002 10
call wait 2
%CLI% LoginR %TARGET% 0x20001

pause

echo ## Login SW when other host is connected in RO.(Should success)
start loginwait 0x10001 10
call wait 2
%CLI% LoginR %TARGET% 0x20001

pause

:multicon

echo ### Connect with multiple permission, same user
echo ## EX + EX (Should fail)
start loginwait 0x10004 5
call wait 2
%CLI% LoginR %TARGET% 0x10004
pause 
echo ## EX + SW (Should fail)
start loginwait 0x10004 5
call wait 2
%CLI% LoginR %TARGET% 0x10002
pause 

echo ## SW + EX (Should fail)
start loginwait 0x10002 5
call wait 2
%CLI% LoginR %TARGET% 0x10004
pause 

echo ## SW + SW (Should success)
start loginwait 0x10002 5
call wait 2
%CLI% LoginR %TARGET% 0x10002
pause 

echo ## EX + RO (Should success)
start loginwait 0x10004 5
call wait 2
%CLI% LoginR %TARGET% 0x10001
pause 

echo ## RO + RO (Should success)
start loginwait 0x10001 5
call wait 2
%CLI% LoginR %TARGET% 0x10001
pause 


echo ### Reset account to default
%CLI% ResetAccount %TARGET%

pause
