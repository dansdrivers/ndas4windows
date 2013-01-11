@echo off
REM --------------------------------------------------------------------------------
REM After you touched any setup resource files (ibd, eula, xml). Run this batch file.
REM This batch file does followings. (suppose %1 is "generic"
REM . Copy original resource files (image, icon, eula) to generic\branded\
REM . Copy (overwrite) branded resource files to generic\branded\ if any
REM . If appropriate xml file exist, create new wxl into each generic\msi_*\wxl\
REM . Otherwise, copy original wxl files into each generic\msi_*\wxl\

set WIXSRC_PATH=..\wixsrc
set GENWXL_EXE=..\tools\genwxl.exe

set OEM_PATH=%1
if not exist %1 (
echo Directory "%1" not found
goto :eof
)

if not exist %1\branded mkdir %1\branded

call :RESOURCE
call :WXL

goto :eof

REM --------------------------------------------------------------------------------
:RESOURCE

if not exist %OEM_PATH%\branded mkdir %OEM_PATH%\branded

call :BUILD_RESOURCE_DIRECTORY ibd
call :BUILD_RESOURCE_DIRECTORY eula
call :BUILD_RESOURCE_DIRECTORY ico

goto :eof

REM --------------------------------------------------------------------------------
:BUILD_RESOURCE_DIRECTORY

if not exist %OEM_PATH%\branded\%1 mkdir %OEM_PATH%\branded\%1
del /F /Q %OEM_PATH%\branded\%1\*
copy /y %WIXSRC_PATH%\%1\* %OEM_PATH%\branded\%1\
if exist %OEM_PATH%\%1 copy /y %OEM_PATH%\%1\* %OEM_PATH%\branded\%1\

goto :eof

REM --------------------------------------------------------------------------------
:WXL

del /S /F /Q %OEM_PATH%\*.wxl

for /d %%f in (%OEM_PATH%\msi_*) do (
if not exist %%f\wxl mkdir %%f\wxl
if not exist %%f\wxl\ui mkdir %%f\wxl\ui
)

call :GEN_WXLS ndas.wxl loc_ndaswixstdex
call :GEN_WXLS ndasxdi.wxl _
call :GEN_WXLS oem.wxl loc_ndaswixoem
call :GEN_WXLS xdi.wxl _

call :GEN_WXLS ui\ActionText.wxl loc_ndaswixstd
call :GEN_WXLS ui\Dialogs.wxl loc_ndaswixstd
call :GEN_WXLS ui\Error.wxl loc_ndaswixstd
call :GEN_WXLS ui\UIText.wxl loc_ndaswixstd

call :GEN_WXLS ui\ndasscui.wxl loc_ndaswixmsm

copy /y %OEM_PATH%\msi_en-US\wxl\*.wxl %OEM_PATH%\msi_neutral\wxl\
copy /y %OEM_PATH%\msi_en-US\wxl\ui\*.wxl %OEM_PATH%\msi_neutral\wxl\ui\

goto :eof

REM --------------------------------------------------------------------------------
REM GEN_WXLS finds generic\msi_* and calls GEN_WXL for each
:GEN_WXLS

echo.
echo ---- PROCESSING WXL : %1 ----

for /d %%f in (%OEM_PATH%\msi_*) do (
call :GEN_WXL %%~nf %1 %2
)

goto :eof

REM --------------------------------------------------------------------------------
REM GEN_WXL generates or copies original wxl into oem build directory
:GEN_WXL
set LANGUAGE=%1
set LANGUAGE=%LANGUAGE:~4,100%

if /i "%LANGUAGE%" equ "neutral" goto :eof
set FILE_WXL=%WIXSRC_PATH%\%2

if not exist %FILE_WXL% (
echo.
echo *** "%FILE_WXL%" NOT EXIST ***
echo.

goto :eof
)

set FILE_WXL_OUT=%OEM_PATH%\%1\wxl\%2

REM set FILE_XML_SRC to local xml first, if not exist, set to that of WIXSRC_PATH
set FILE_XML_SRC=%OEM_PATH%\%1\loc\%3.xml

if not exist %FILE_XML_SRC% (
set FILE_XML_SRC=%WIXSRC_PATH%\loc\%3_%LANGUAGE%.xml
)

if not exist %FILE_XML_SRC% (
echo *** "%FILE_XML_SRC%" NOT EXIST. COPYING DIRECTLY ***
copy /y %FILE_WXL% %FILE_WXL_OUT%
) else (
echo %GENWXL_EXE% %FILE_WXL% %FILE_XML_SRC% %FILE_WXL_OUT%
%GENWXL_EXE% %FILE_WXL% %FILE_XML_SRC% %FILE_WXL_OUT%
)

goto :eof