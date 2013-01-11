@echo off
rem
rem NDAS SDK publishing
rem

rem Copy src\inc\public\ndas\*.* --> publish\sdk\inc\ndas
rem Copy lib\public\chk\i386\*.* --> publish\sdk\lib\chk\i386\
rem Copy lib\public\fre\i386\*.* --> publish\sdk\lib\i386\

rem lib\public\chk\i386\ndascomm_dll.lib
setlocal enableextensions
rmdir /s /q publish\sdk
echo.
echo Copying header files
call :do_copy .\src\inc\public\ndas publish\sdk\inc\ndas
echo.
echo Copying i386 lib files
call :do_copy .\lib\public\fre\i386 publish\sdk\lib\i386
echo.
echo Copying i386 lib files (checked)
call :do_copy .\lib\public\chk\i386 publish\sdk\chk\lib\i386

rem echo.
rem echo Copying amd64 lib files
rem call :do_copy .\lib\public\fre\amd64 publish\sdk\lib\amd64
rem echo.
rem echo Copying amd64 lib files (checked)
rem call :do_copy .\lib\public\chk\amd64 publish\sdk\chk\lib\amd64

call :do_copy_samples

echo Copying runtime files
set BP_FLAGS=-f -a -x -j -:LOGPDB  -:DBG
rem -n .\publish\sdk\symbols.pri 
for %%a in (.\publish\fre\i386\*.*) do (
    if /I "%%~xa" NEQ "pdb" (
    .\bin\binplace %BP_FLAGS% -r .\publish\sdk\runtime -p sdkplace.txt -s .\public\sdk\symbols %%a
    )
)
for %%a in (.\publish\chk\i386\*.*) do (
    if /I "%%~xa" NEQ "pdb" (
    .\bin\binplace %BP_FLAGS% -r .\publish\sdk\chk\runtime -p sdkplace.txt -s .\public\sdk\chk\symbols %%a
    )
)

REM ~ for %%a in (.\publish\fre\amd64\*.*) do (
    REM ~ if /I "%%~xa" NEQ "pdb" (
    REM ~ set _BUILDARCH=amd64
    REM ~ .\bin\binplace %BP_FLAGS% -v -r .\publish\sdk\runtime -p sdkplace.txt -s .\public\sdk\chk\symbols %%a
    REM ~ )
REM ~ )
REM ~ for %%a in (.\publish\chk\amd64\*.*) do (
    REM ~ if /I "%%~xa" NEQ "pdb" (
    REM ~ set _BUILDARCH=amd64
    REM ~ .\bin\binplace %BP_FLAGS% -r .\publish\sdk\chk\runtime -p sdkplace.txt -s .\public\sdk\chk\symbols %%a
    REM ~ )
REM ~ )


endlocal
goto :EOF

:do_copy
setlocal
set XC_FILES=/xf dirs
set XC_DIRS=/xd .svn
.\bin\robocopy %1 %2 /np /njh /njs /e %XC_FILES% %XC_DIRS% /ns /v
endlocal
goto :EOF

:do_copy_samples
setlocal
for /f %%d in (samples.dir) do (
    echo Copying %%~nd --^> .\publish\sdk\samples\%%~nd
    if not exist .\publish\sdk\samples\%%~nd mkdir .\publish\sdk\samples\%%~nd
    for /f %%f in (%%d\sample.lst) do (
        copy /y %%d\%%~nf%%~xf .\publish\sdk\samples\%%~nd
    )
)
endlocal
goto :EOF

