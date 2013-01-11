@echo off
call config.bat

SET IOSIZE=256
SET POS=10000

SET PATTERN1=4232
SET PATTERN2=3222
SET PATTERN3=1783
SET PATTERN4=37828

rem goto skip

echo #### Encryption Test
echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002

echo ### Test no Encryption option
echo ## Setting option
%CLI% SetOption %TARGET% 0x0


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS%


:skip

echo ### Test DATA Encryption option
echo ## Setting option
%CLI% SetOption %TARGET% 0x1

echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN2% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN2% %IOSIZE% %POS%


echo ### Test header Encryption option
echo ## Setting option
%CLI% SetOption %TARGET% 0x2


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN3% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN3% %IOSIZE% %POS%


echo ### Test header + Data Encryption option
echo ## Setting option
%CLI% SetOption %TARGET% 0x3


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN4% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN4% %IOSIZE% %POS%

pause
