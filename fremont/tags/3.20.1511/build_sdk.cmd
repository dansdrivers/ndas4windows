@echo off
rem
rem NDAS SDK publishing
rem

rem Copy src\inc\public\ndas\*.* --> publish\sdk\inc\ndas
rem Copy lib\public\chk\i386\*.* --> publish\sdk\lib\chk\i386\
rem Copy lib\public\fre\i386\*.* --> publish\sdk\lib\i386\

setlocal enableextensions

if not defined BINPLACE_CMD (
   set BINPLACE_CMD=%WNETBASE%\bin\x86\binplace.exe
)
if not exist "%BINPLACE_CMD%" (
   echo Unable to find binplace.exe in %WNETBASE%\bin\x86
   echo Either Set WNETBASE to DDK path
   echo or	   Set BINPLACE_CMD to the path of binplace.exe
   exit /b 1
)

pushd .
cd %~dp0

if exist publish\sdk\ rmdir /s /q publish\sdk
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
setlocal
set BP_BASE_FLAGS=-a -x
set BINPLACE_PLACEFILE=%~dp0sdkplace.txt

set _NT386TREE=%~dp0publish\sdk\runtime
set BP_FLAGS=%BP_BASE_FLAGS%
rem set BP_FLAGS=%BP_FLAGS% -n %~dp0publish\sdk\runtime\symbols.pri
set BP_FLAGS=%BP_FLAGS% -s %~dp0publish\sdk\runtime\symbols

set SRC_DIRS=.\publish\fre\i386\*.*
for %%a in (%SRC_DIRS%) do (
	if /I "%%~xa" NEQ ".pdb" %BINPLACE_CMD% %BP_FLAGS% %%a
)

set _NT386TREE=%~dp0publish\sdk\chk\runtime
set BP_FLAGS=%BP_BASE_FLAGS%
rem set BP_FLAGS=%BP_FLAGS% -n %~dp0publish\sdk\chk\runtime\symbols.pri
set BP_FLAGS=%BP_FLAGS% -s %~dp0publish\sdk\chk\runtime\symbols
set SRC_DIRS=.\publish\chk\i386\*.*
for %%a in (%SRC_DIRS%) do (
	if /I "%%~xa" NEQ ".pdb" %BINPLACE_CMD% %BP_FLAGS% %%a
)

REM for %%a in (.\publish\fre\amd64\*.*) do (
REM		if /I "%%~xa" NEQ ".pdb" (
REM			set _BUILDARCH=amd64
REM			.\bin\binplace %BP_FLAGS% -v -r .\publish\sdk\runtime -p sdkplace.txt -s .\public\sdk\chk\symbols %%a
REM		)
REM )
REM for %%a in (.\publish\chk\amd64\*.*) do (
REM		if /I "%%~xa" NEQ ".pdb" (
REM		set _BUILDARCH=amd64
REM		.\bin\binplace %BP_FLAGS% -r .\publish\sdk\chk\runtime -p sdkplace.txt -s .\public\sdk\chk\symbols %%a
REM		)
REM )
endlocal

popd
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

