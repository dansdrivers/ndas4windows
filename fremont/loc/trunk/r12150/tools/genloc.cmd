@echo off
setlocal enableextensions

resxml extract ..\fre\i386\ndasmgmt.enu.dll ndasmgmt.resxml
resxml extract ..\fre\i386\ndasbind.enu.dll ndasbind.resxml
msxsl.exe -xw ndasmgmt.resxml resxml2loc.xsl -o loc_enu_ndasmgmt.xml
msxsl.exe -xw ndasbind.resxml resxml2loc.xsl -o loc_enu_ndasbind.xml
msxsl.exe -xw ndasmsg.xml msgxml2loc.xsl -o loc_enu_ndasmsg.xml

call :make_languages ndasmgmt deu fra esn ptg ita jpn kor chs cht
call :make_languages ndasbind deu fra esn ptg ita jpn kor chs cht
call :make_languages ndasmsg  deu fra esn ptg ita jpn kor chs cht

goto :EOF

:make_languages
setlocal
set name=%1
shift
:loop
if "%1" neq "" (
   copy loc_enu_%name%.xml loc_%1_%name%.xml
   shift
   goto loop
)   

goto :EOF
