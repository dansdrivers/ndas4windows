@echo off
setlocal ENABLEEXTENSIONS
pushd .
pushd %0\..\
popd

set BASE_SRCDIR=..\lanscsisystemv2
set BASE_DESTDIR=.\release

set SRC_PRGDIR=%BASE_SRCDIR%\exe\Release
set SRC_DRVDIR=%BASE_SRCDIR%\sys\fre_w2k_x86\i386
set SRC_DRVDIR_LFSFILTXP=%BASE_SRCDIR%\sys\fre_wxp_x86\i386
set SRC_INFDIR=%BASE_SRCDIR%\inf

set TARGET_PRGDIR=%BASE_DESTDIR%\PROGRAMFILES
set TARGET_DRVDIR=%BASE_DESTDIR%\DRIVERS
set TARGET_INFDIR=%BASE_DESTDIR%\DRIVERS


::
:: Fulfill PROGRAMFILES
::

set LST_PROGRAM=Admin.exe AggrMirUI.exe LDServ.exe
set LST_PROGRAM=%LST_PROGRAM% 1033\uires.dll
set LST_PROGRAM=%LST_PROGRAM% 1031\uires.dll 1034\uires.dll 1036\uires.dll 1040\uires.dll 1046\uires.dll
set LST_PROGRAM=%LST_PROGRAM% 1041\uires.dll 1042\uires.dll
set LST_PROGRAM_ADIR=1033 1031 1034 1036 1040 1041 1042 1046

set _SDIR=%SRC_PRGDIR%
set _TDIR=%TARGET_PRGDIR%
set _TADIR=%LST_PROGRAM_ADIR%
set _LST=%LST_PROGRAM%
call :copy

::
:: Fulfill DRIVERS (SYS)
::

set LST_DRIVERS=lanscsibus.sys lanscsiminiport.sys lpx.sys wshlpx.dll 

set _SDIR=%SRC_DRVDIR%
set _TDIR=%TARGET_DRVDIR%
set _TADIR=
set _LST=%LST_DRIVERS%
call :copy

set _LST=lfsfilt.sys
set _SDIR=%SRC_DRVDIR_LFSFILTXP%
call :copy

::
:: Fulfill DRIVERS (INF)
::
set LST_INFS=lanscsibus.inf lanscsiminiport.inf netlpx.inf
set LST_INFS=%LST_INFS% lanscsibus.cat lanscsiminiport.cat netlpx.cat

set _SDIR=%SRC_INFDIR%
set _TDIR=%TARGET_INFDIR%
set _TADIR=
set _LST=%LST_INFS%
call :copy

::
:: Fulfill NDINST.DLL
::
call :copyfile ..\installhelper\exe\fre_w2k_x86\i386\ndinst.dll  .\NetDiskSetup\Binary\


:end
popd
endlocal
goto :EOF

REM --------------------------------------------------------------------------
REM Support Routines
REM --------------------------------------------------------------------------
:copy

:: creates target directory
if not exist %_TDIR% (
  mkdir %_TDIR%
)

:: additional directory creation
if defined _TADIR (
	for %%a in (%_TADIR%) do (
		if not exist %_TDIR%\%%a mkdir %_TDIR%\%%a
	)
)

:: copying files
for %%a in (%_LST%) do (
	call :copyfile %_SDIR%\%%a %_TDIR%\%%a
)
goto :EOF

:copyfile
if exist %1 (
	echo Copying %1 to %2
	copy /y %1 %2 > nul
	if errorlevel 1 echo WARNING: Error copying %1 to %2
) else (
	echo WARNING: Missing %1
)
goto :EOF
