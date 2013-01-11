@echo off
setlocal enableextensions

%~dp0resxml extract %~dp0..\base\bin\i386\ndasmgmt.enu.dll %TEMP%\~ndasmgmt.resxml
%~dp0resxml extract %~dp0..\base\bin\i386\ndasbind.enu.dll %TEMP%\~ndasbind.resxml
copy %~dp0..\base\meta\ndasmsg.xml %TEMP%\~ndasmsg.xml

%~dp0msxsl.exe -xw %TEMP%\~ndasmgmt.resxml %~dp0resxml2loc.xsl -o loc_enu_ndasmgmt.xml
%~dp0msxsl.exe -xw %TEMP%\~ndasbind.resxml %~dp0resxml2loc.xsl -o loc_enu_ndasbind.xml
%~dp0msxsl.exe -xw %TEMP%\~ndasmsg.xml %~dp0msgxml2loc.xsl -o loc_enu_ndasmsg.xml

set TARGETLANGS=%*
if not defined TARGETLANGS exit /b 0
if "%TARGETLANGS%" == "all" set TARGETLANGS=deu fra esn ptg ita jpn kor chs cht

call :make_languages ndasmgmt %TARGETLANGS%
call :make_languages ndasbind %TARGETLANGS%
call :make_languages ndasmsg  %TARGETLANGS%

exit /b 0

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

exit /b 0
