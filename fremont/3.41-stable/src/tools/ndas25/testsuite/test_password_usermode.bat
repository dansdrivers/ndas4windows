@echo off
call config.bat

SET IOSIZE=256
SET POS=10000

SET PATTERN1=4773
SET PATTERN2=211
SET PATTERN3=7322
SET PATTERN4=12222
SET PATTERN6=432
SET PATTERN7=12


SET SUPERPW=superuserpw
SET USERPW1=userpw1
SET USERPW2=userpw2
SET USERPW3=userpw3
SET USERPW4=userpw4
SET USERPW5=userpw5
SET USERPW6=userpw6
SET USERPW7=userpw7

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
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002

echo ### Set usermode password
%CLI% SetUserPw %TARGET% 1 %USERPW1%

echo ### Try old password.(Should fail)
%CLI% LoginR %TARGET% 0x10001

echo ### Read pattern with new password
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002 %USERPW1%

echo ### Test with other accounts
echo ## Set usermode password and permission
rem %CLI% SetPermission %TARGET% 0x20007
rem %CLI% SetPermission %TARGET% 0x30007
rem %CLI% SetPermission %TARGET% 0x40007
rem %CLI% SetPermission %TARGET% 0x50007
rem %CLI% SetPermission %TARGET% 0x60007
%CLI% SetPermission %TARGET% 0x70007


echo ### Write pattern with old password
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x70002

rem %CLI% SetUserPw %TARGET% 2 %USERPW2%
rem %CLI% SetUserPw %TARGET% 3 %USERPW3%
rem %CLI% SetUserPw %TARGET% 4 %USERPW4%
rem %CLI% SetUserPw %TARGET% 5 %USERPW5%
rem %CLI% SetUserPw %TARGET% 6 %USERPW6%
%CLI% SetUserPw %TARGET% 7 %USERPW7%

echo ## Show Account
%CLI% ShowAccount %TARGET%

echo ## Try with old account(Should fail)
%CLI% LoginR %TARGET% 0x70001

echo ## Try with other account's password(Should fail)
%CLI% LoginR %TARGET% 0x70001 %USERPW1%

echo ## Read/write pattern with new password
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x70002 %USERPW7%


echo ### Change password when another user is logged in.
echo ## Set account 1 to default PW
%CLI% SetUserPw %TARGET% 1 "" %USERPW1%

start lockedwrite 256 1 150000 0x21
call wait 5

echo ## Changing password while another user is logged
%CLI% SetUserPw %TARGET% 1 %USERPW1%

echo ## Write/read with new password
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002 %USERPW1%
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002 %USERPW1%


echo ## Check whether IO is completed successfully
pause

echo ### Reset account to default
%CLI% ResetAccount %TARGET%

echo ### Reset Option
%CLI% SetOption %TARGET% 0x00

pause
