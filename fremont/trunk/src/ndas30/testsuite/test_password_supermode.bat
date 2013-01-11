@echo off
call config.bat

SET IOSIZE=256
SET POS1=10000
SET POS2=12000
SET POS3=13000
SET POS4=14000
SET POS5=15000
SET POS6=16000
SET POS7=17000

SET PATTERN1=123
SET PATTERN2=456
SET PATTERN3=789
SET PATTERN4=322
SET PATTERN6=54
SET PATTERN7=33


SET SUPERPW=superuserpw
SET USERPW1=abcde
SET USERPW2=fghij
SET USERPW3=klmno
SET USERPW4=pqrst
SET USERPW5=uvwxy
SET USERPW6=zabcd
SET USERPW7=efghi

echo #### Account Test
echo ### Turn on encryption/CRC to protect PW
%CLI% SetOption %TARGET% 0x0f

echo ### Reset account to default
%CLI% ResetAccount %TARGET%

echo ### Show Account
%CLI% ShowAccount %TARGET%


echo ### Warming up(can fail).
%CLI% LoginR %TARGET% 0x10001

echo ### Write pattern with old password
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS1% 0 0x10002

echo ### Set usermode password
%CLI% SetPw %TARGET% 1 %USERPW1%

echo ### Try old password.(Should fail)
%CLI% LoginR %TARGET% 0x10001

echo ### Read pattern with new password
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS1% 0 0x10002 %USERPW1%


echo ### Test with other accounts
echo ## Set usermode password and permission
%CLI% SetPermission %TARGET% 0x20007
%CLI% SetPermission %TARGET% 0x30007
%CLI% SetPermission %TARGET% 0x40007
%CLI% SetPermission %TARGET% 0x50007
%CLI% SetPermission %TARGET% 0x60007
%CLI% SetPermission %TARGET% 0x70007

echo ## Write pattern with old password
echo # Account 2
%CLI% WritePattern %TARGET% %PATTERN2% %IOSIZE% %POS2% 0 0x20002
echo # Account 3
%CLI% WritePattern %TARGET% %PATTERN3% %IOSIZE% %POS3% 0 0x30002
echo # Account 4
%CLI% WritePattern %TARGET% %PATTERN4% %IOSIZE% %POS4% 0 0x40002
echo # Account 5
%CLI% WritePattern %TARGET% %PATTERN5% %IOSIZE% %POS5% 0 0x50002
echo # Account 6
%CLI% WritePattern %TARGET% %PATTERN6% %IOSIZE% %POS6% 0 0x60002
echo # Account 7
%CLI% WritePattern %TARGET% %PATTERN7% %IOSIZE% %POS7% 0 0x70002

echo ## Set password
%CLI% SetPw %TARGET% 2 %USERPW2%
%CLI% SetPw %TARGET% 3 %USERPW3%
%CLI% SetPw %TARGET% 4 %USERPW4%
%CLI% SetPw %TARGET% 5 %USERPW5%
%CLI% SetPw %TARGET% 6 %USERPW6%
%CLI% SetPw %TARGET% 7 %USERPW7%

echo ## Read pattern with new password
echo # Account 2
%CLI% ReadPattern %TARGET% %PATTERN2% %IOSIZE% %POS2% 0 0x20002 %USERPW2%
echo # Account 3
%CLI% ReadPattern %TARGET% %PATTERN3% %IOSIZE% %POS3% 0 0x30002 %USERPW3%
echo # Account 4
%CLI% ReadPattern %TARGET% %PATTERN4% %IOSIZE% %POS4% 0 0x40002 %USERPW4%
echo # Account 5
%CLI% ReadPattern %TARGET% %PATTERN5% %IOSIZE% %POS5% 0 0x50002 %USERPW5%
echo # Account 6
%CLI% ReadPattern %TARGET% %PATTERN6% %IOSIZE% %POS6% 0 0x60002 %USERPW6%
echo # Account 7
%CLI% ReadPattern %TARGET% %PATTERN7% %IOSIZE% %POS7% 0 0x70002 %USERPW7%

echo ### Reset account to default
%CLI% ResetAccount %TARGET%

echo ### Reset Option
%CLI% SetOption %TARGET% 0x00

pause
