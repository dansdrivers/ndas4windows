@echo off
call config.bat

SET IOSIZE=512
SET POS=10000

SET PATTERN1=4232
SET PATTERN2=67432
SET PATTERN3=1783
SET PATTERN4=37828

SET USERPW1=userpw1

echo #### AES with diffrent password

echo ### Set usermode password.
%CLI% SetUserPw %TARGET% 1 %USERPW1%

echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002 %USERPW1%

echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002 %USERPW1%
echo ## Reading pattern
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS% 0 0x10002 %USERPW1%


echo ### Set default password
%CLI% SetUserPw %TARGET% 1 "" %USERPW1%

pause
