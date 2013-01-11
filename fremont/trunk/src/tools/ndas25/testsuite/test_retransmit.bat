@echo off
call config.bat

SET IOSIZE=512
SET POS=13000
SET ITE=2

goto skip

echo #### Test max retransmit option
echo ### Testing 30ms retransmition 
echo ## Setting max retransmition to 30
%CLI% rawvendor %TARGET% 0x4 0x01 "" 0 0 29
echo ## Check changes
%CLI% Getconfig %TARGET%
echo ## Set incoming packet drop
%CLI% SetPacketDrop rx 1

echo ## Start packet capturing before pressing any key.
pause
%CLI% Read %TARGET% 128 1 100000 128
echo ## Check captured packets for retransmits
pause

echo ### Testing 200ms retransmition 
echo ## Setting max retransmition to 200
%CLI% rawvendor %TARGET% 0x4 0x01 "" 0 0 199
echo ## Check changes
%CLI% Getconfig %TARGET%
echo ## Set incoming packet drop
%CLI% SetPacketDrop rx 1

echo ## Start packet capturing before pressing any key.
pause
%CLI% Read %TARGET% 128 1 100000 128
echo ## Check captured packets for retransmits
pause


echo ### Testing 500ms retransmition 
echo ## Setting max retransmition to 500
%CLI% rawvendor %TARGET% 0x4 0x01 "" 0 0 499
echo ## Check changes
%CLI% Getconfig %TARGET%
echo ## Set incoming packet drop
%CLI% SetPacketDrop rx 1

echo ## Start packet capturing before pressing any key.
pause
%CLI% Read %TARGET% 128 1 100000 128
echo ## Check captured packets for retransmits

pause
echo ### Retransmit time test ended. Restoring default value.
echo ## Set incoming packet drop to 0
%CLI% SetPacketDrop rx 0
echo ## Setting max retransmition to default(200)
%CLI% rawvendor %TARGET% 0x4 0x01 "" 0 0 199

pause

:skip

echo #### Test retransmit data. 
echo ### Run test_lockedwrite_3.bat from another host and press any key
pause
echo ### Set incoming packet drop. 
%CLI% SetPacketDrop rx 2
%CLI% SetPacketDrop tx 2
echo ### Start IO
call lockedwrite %IOSIZE% %ITE% %POS% 0x21

echo ### Set incoming packet drop to 0
%CLI% SetPacketDrop rx 0
%CLI% SetPacketDrop tx 0

echo ### Check IO completed successfully

pause
