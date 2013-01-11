@echo off
call config.bat

SET ITE=4
SET IOSIZE=256
SET SMALLIOSIZE=16

echo #### Lockwrite test. 
echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10002

echo ### Vendor command unlock mode. No yield, lock per write mode.
echo ## Starting 5 write instance. 
start lockedwrite %IOSIZE% %ITE% 10000 0x1
call sleep 1
start lockedwrite %IOSIZE% %ITE% 11000 0x1
call sleep 1
start lockedwrite %IOSIZE% %ITE% 12000 0x1
call sleep 1
start lockedwrite %IOSIZE% %ITE% 13000 0x1
call sleep 1
start lockedwrite %IOSIZE% %ITE% 14000 0x1

echo ### Check every write is completed successfully.
pause

echo ### Vendor command unlock mode. Yield mode
echo ## Starting 5 write instance. 
start lockedwrite %IOSIZE% %ITE% 10000 0x21
call sleep 1
start lockedwrite %IOSIZE% %ITE% 11000 0x21
call sleep 1
start lockedwrite %IOSIZE% %ITE% 12000 0x21
call sleep 1
start lockedwrite %IOSIZE% %ITE% 13000 0x21
call sleep 1
start lockedwrite %IOSIZE% %ITE% 14000 0x21

echo ### Check every write is completed successfully.
pause

echo ### PDU unlock mode. Lock per write mode.
echo ## Starting 5 write instance. 
start lockedwrite %IOSIZE% %ITE% 10000 0x2
call sleep 1
start lockedwrite %IOSIZE% %ITE% 11000 0x2
call sleep 1
start lockedwrite %IOSIZE% %ITE% 12000 0x2
call sleep 1
start lockedwrite %IOSIZE% %ITE% 13000 0x2
call sleep 1
start lockedwrite %IOSIZE% %ITE% 14000 0x2
echo ### Check every write is completed successfully.
pause

echo ### PDU unlock mode. Lock per tranaction mode.
echo ## Starting 5 write instance. 
start lockedwrite %IOSIZE% %ITE% 10000 0x12
call sleep 1
start lockedwrite %IOSIZE% %ITE% 11000 0x12
call sleep 1
start lockedwrite %IOSIZE% %ITE% 12000 0x12
call sleep 1
start lockedwrite %IOSIZE% %ITE% 13000 0x12
call sleep 1
start lockedwrite %IOSIZE% %ITE% 14000 0x12
echo ### Check every write is completed successfully.
pause

echo ### Mixed lock mode
echo ## Starting 5 write instance. 
start lockedwrite %IOSIZE% %ITE% 10000 0x01
call sleep 1
start lockedwrite %IOSIZE% %ITE% 11000 0x11
call sleep 1
start lockedwrite %IOSIZE% %ITE% 12000 0x21
call sleep 1
start lockedwrite %IOSIZE% %ITE% 13000 0x02
call sleep 1
start lockedwrite %IOSIZE% %ITE% 14000 0x12
echo ### Check every write is completed successfully.
pause
