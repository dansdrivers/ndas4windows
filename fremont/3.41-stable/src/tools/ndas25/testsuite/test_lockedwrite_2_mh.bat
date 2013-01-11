@echo off
call config.bat
call config_per_host.bat

SET ITE=16
SET IOSIZE=256

echo #### Multi Host Lockedwrite test. (This test requires multiple hosts)
echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002

echo ### Vendor command unlock mode. No yield, lock per write mode.
%CLI% LockedWrite %TARGET% %IOSIZE% %ITE% %POS% 0x1

echo ### Check every write is completed successfully.
pause

echo ### Vendor command unlock mode. Yield mode
%CLI% LockedWrite %TARGET% %IOSIZE% %ITE% %POS% 0x21

echo ### Check every write is completed successfully.
pause

echo ### PDU unlock mode. Lock per write mode.
%CLI% LockedWrite %TARGET% %IOSIZE% %ITE% %POS% 0x2

echo ### Check every write is completed successfully.
pause

echo ### PDU unlock mode. Lock per tranaction mode.
%CLI% LockedWrite %TARGET% %IOSIZE% %ITE% %POS% 0x12

echo ### Check every write is completed successfully.
pause

echo ### Mixed lock mode
start lockedwrite %IOSIZE% %ITE% %POS% %LOCKMODE%

echo ### Check every write is completed successfully.
pause
