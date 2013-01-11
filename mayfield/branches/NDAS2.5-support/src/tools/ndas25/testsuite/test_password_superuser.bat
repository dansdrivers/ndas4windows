@echo off
call config.bat


SET IOSIZE=256
SET POS=10000

SET PATTERN1=2931

SET SUPERPW=superuserpw

echo #### !!!CAUTION !!! Superuser Password change test. 
echo ### Turn on encryption/CRC to protect PW
%CLI% SetOption %TARGET% 0x0f

echo ### Show Account
%CLI% ShowAccount %TARGET%

echo ### Warming up in superuser mode(can fail).
%CLI% LoginR %TARGET% 0x00001

echo ### Write pattern with old password
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x00002

echo ### Set usermode password
%CLI% SetPw %TARGET% 0 %SUPERPW%

echo ### Try old password.(Should fail)
%CLI% LoginR %TARGET% 0x00001

echo ### Read pattern with new password
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x00002 %SUPERPW%

echo ### Reset account to default
%CLI% SetPw %TARGET% 0 "" %SUPERPW%

echo ### Reset Option
%CLI% SetOption %TARGET% 0x00

pause
