@echo off
call config.bat

SET ITE=1
SET IOSIZE=128
SET IOPOS=150000


echo #### Various Request size test.
echo ## Warming up(IO timeout can happen..)
%CLI% LoginR %TARGET% 0x10001

FOR /L %%N IN (1,1,128) DO %CLI% BlockVariedIo 00:0b:d0:00:ff:d1 %IOSIZE% %IOPOS% %ITE% %%N

pause

