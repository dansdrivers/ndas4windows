@echo om
setlocal enableextensions
pushd .
cd %0\..\
set CMDDIR=%CD%
popd

set SRCBASEDIR=%1
if not defined SRCBASEDIR goto usage

:: -- lanscsibus.inf
::
set SYSFILE=%SRCBASEDIR%\sys\fre_w2k_x86\i386\lanscsibus.sys
set INFTPLFILE=%SRCBASEDIR%\inf\lanscsibus_tpl.inf
set INFFILE=%SRCBASEDIR%\inf\lanscsibus.inf

call %CMDDIR%\setinfver.cmd %SYSFILE% %INFTPLFILE% %INFFILE%

:: -- lanscsiminiport.inf
::
set SYSFILE=%SRCBASEDIR%\sys\fre_w2k_x86\i386\lanscsiminiport.sys
set INFTPLFILE=%SRCBASEDIR%\inf\lanscsiminiport_tpl.inf
set INFFILE=%SRCBASEDIR%\inf\lanscsiminiport.inf

call %CMDDIR%\setinfver.cmd %SYSFILE% %INFTPLFILE% %INFFILE%

:: -- netlpx.inf
::
set SYSFILE=%SRCBASEDIR%\sys\fre_w2k_x86\i386\lpx.sys
set INFTPLFILE=%SRCBASEDIR%\inf\netlpx_tpl.inf
set INFFILE=%SRCBASEDIR%\inf\netlpx.inf

call %CMDDIR%\setinfver.cmd %SYSFILE% %INFTPLFILE% %INFFILE%

goto end

:usage
echo setinfverall lanscsisystemv2path
goto end

:end
popd
endlocal
