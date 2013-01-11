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

echo ### Test Host to NDAS CRC error handling 
%CLI% DigestTest %TARGET%

echo ### Static CRC option test - Header CRC
echo ## Turn on header CRC
%CLI% SetOption %TARGET% 0x8

echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN1% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN1% %IOSIZE% %POS%


echo ### Static CRC option test - Data CRC
echo ## Turnon data CRC
%CLI% SetOption %TARGET% 0x4


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN2% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN2% %IOSIZE% %POS%


echo ### Static CRC option test - Header + Data CRC
echo ## Turnon data CRC
%CLI% SetOption %TARGET% 0xc


echo ## Writing pattern
%CLI% WritePattern %TARGET% %PATTERN3% %IOSIZE% %POS%

echo ## Read pattern
%CLI% ReadPattern %TARGET% %PATTERN3% %IOSIZE% %POS%

pause

