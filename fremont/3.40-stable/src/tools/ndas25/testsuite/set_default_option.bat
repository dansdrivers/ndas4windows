@echo off
call config.bat

%CLI% SetPacketDrop rx 0
%CLI% SetPacketDrop tx 0
%CLI% SetOption %TARGET% 0x0
%CLI% ResetAccount %TARGET%

pause