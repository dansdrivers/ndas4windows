@echo off
call config.bat

echo #### Discovery test. 
echo ## Warming up(IO timeout can happen..)
%CLI% Discovery %TARGET%

pause

