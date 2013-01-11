@echo off
call config.bat

SET IOSIZE=64
SET POS=10000

SET PATTERN1=4232
SET PATTERN2=67432
SET PATTERN3=1783
SET PATTERN4=37828

echo #### Digest test
echo ### Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002


echo ### Dynamic option test 
%CLI% DynamicOptionTest %TARGET%


echo ### Static option test
echo ## Turn on all
%CLI% SetOption %TARGET% 0x0f


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS%


echo ### Static option test
echo ## Turn off all
%CLI% SetOption %TARGET% 0x00


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS%

pause