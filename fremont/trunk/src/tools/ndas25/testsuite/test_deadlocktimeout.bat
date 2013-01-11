@echo off
call config.bat

echo #### Testing buffer-lock deadlock timer.
echo ### Setting timeout value to 5
%CLI% RawVendor %TARGET% 0x4 0x28 "" 0 0 4 

echo ## Wait for 3 seconds after taking BL.(Should not be disconnected) 
%CLI% BLDeadlockTest %TARGET% 3

call wait 3

echo ## Wait for 10 seconds after taking BL.(Should be disconnected.)
%CLI% BLDeadlockTest %TARGET% 10


echo ### Setting timeout value to 10
%CLI% RawVendor %TARGET% 0x4 0x28 "" 0 0 9


echo ## Wait for 5 seconds after taking BL. (Should not be disconnected) 
%CLI% BLDeadlockTest %TARGET% 5

call wait 3

echo ## Wait for 15 seconds after taking BL.(Should be disconnected.)
%CLI% BLDeadlockTest %TARGET% 15


echo #### Testing buffer-lock timeout with active other IOs
echo ### Warming up
%CLI% LoginR %TARGET% 0x10004

echo ### Starting 3 IO connections
start lockedwrite 256 1 10000 0x21
start lockedwrite 256 1 11000 0x21
start lockedwrite 256 1 12000 0x21
call wait 2
echo ## Wait for 15 seconds after taking BL.This connection should be disconnected and lockedwrite should be completed normally.
%CLI% BLDeadlockTest %TARGET% 15

echo ## Wait for IO completing successfully.
pause

start lockedwrite 256 1 10000 0x21
start lockedwrite 256 1 11000 0x21
start lockedwrite 256 1 12000 0x21
call wait 2
echo ## Wait for 5 seconds after taking BL. This connection should be closed normally and lockedwrite should be completed normally.
%CLI% BLDeadlockTest %TARGET% 5

echo ## Wait for IO completing successfully.
pause
