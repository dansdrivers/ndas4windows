@echo off
call config.bat

rem goto 2

:1
echo #### Testing no PNP message option.
net stop ndassvc

echo ### Turning off PNP
%CLI% SetOption %TARGET% 0x20
echo ### Waiting PNP message. Packet should NOT be arrived.
%CLI% PnpListen %TARGET% %HOST%

call wait 2

echo ### Turning on PNP
%CLI% SetOption %TARGET% 0x00
echo ### Waiting PNP message. Packet should be arrived.
%CLI% PnpListen %TARGET% %HOST%

call wait 2

echo #### Testing PNP message interval
echo ### Setting to 1 seconds
%CLI% rawvendor %TARGET% 0x4 0x0c "" 0 0 0
echo ### Waiting PNP message. Check message interval is 1.
%CLI% PnpListen %TARGET% %HOST%

call wait 2

echo ### Setting to 2 seconds
%CLI% rawvendor %TARGET% 0x4 0x0c "" 0 0 1
echo ### Waiting PNP message. Check message interval is 2.
%CLI% PnpListen %TARGET% %HOST%

call wait 2

echo ### Setting to 5 seconds
%CLI% rawvendor %TARGET% 0x4 0x0c "" 0 0 4
echo ### Waiting PNP message. Check message interval is 5.
%CLI% PnpListen %TARGET% %HOST%

call wait 2
echo #### Testing GET_HEART_TIME. Param2 should be 4
%CLI% rawvendor %TARGET% 0x4 0x0d "" 0 0 0

call wait 5

echo #### Setting to default heart time
%CLI% rawvendor %TARGET% 0x4 0x0c "" 0 0 1

net start ndassvc

:2
echo #### Testing PNP reqeust message.
echo ### Test without PNP broadcast
%CLI% SetOption %TARGET% 0x20
call wait 5
%CLI% PnpRequest %TARGET% %HOST%

echo ### Test with PNP broadcast on
%CLI% SetOption %TARGET% 0x00
call wait 5

%CLI% PnpRequest %TARGET% %HOST%

pause