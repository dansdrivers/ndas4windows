@echo off
call config.bat

SET ITE=8
SET IOSIZE=512

echo #### Lockwrite test. 
echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002

echo ### Vendor command unlock mode. Yield mode
call lockedwrite %IOSIZE% %ITE% 10000 0x21
pause
