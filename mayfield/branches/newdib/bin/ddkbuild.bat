@echo OSR DDKBUILD.BAT V5.3 - OSR, Open Systems Resources, Inc.
@echo off
rem /////////////////////////////////////////////////////////////////////////////
rem //
rem //    This sofware is supplied for instructional purposes only.
rem //
rem //    OSR Open Systems Resources, Inc. (OSR) expressly disclaims any warranty
rem //    for this software.  THIS SOFTWARE IS PROVIDED  "AS IS" WITHOUT WARRANTY
rem //    OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION,
rem //    THE IMPLIED WARRANTIES OF MECHANTABILITY OR FITNESS FOR A PARTICULAR
rem //    PURPOSE.  THE ENTIRE RISK ARISING FROM THE USE OF THIS SOFTWARE REMAINS
rem //    WITH YOU.  OSR's entire liability and your exclusive remedy shall not
rem //    exceed the price paid for this material.  In no event shall OSR or its
rem //    suppliers be liable for any damages whatsoever (including, without
rem //    limitation, damages for loss of business profit, business interruption,
rem //    loss of business information, or any other pecuniary loss) arising out
rem //    of the use or inability to use this software, even if OSR has been
rem //    advised of the possibility of such damages.  Because some states/
rem //    jurisdictions do not allow the exclusion or limitation of liability for
rem //    consequential or incidental damages, the above limitation may not apply
rem //    to you.
rem //
rem //    OSR Open Systems Resources, Inc.
rem //    105 Route 101A Suite 19
rem //    Amherst, NH 03031  (603) 595-6500 FAX: (603) 595-6503
rem //    email bugs to: bugs@osr.com
rem //
rem //
rem //    MODULE:
rem //
rem //        ddkbuild.bat 
rem //
rem //    ABSTRACT:
rem //
rem //      This file allows drivers to be build with visual studio and visual studio.net
rem //
rem //    AUTHOR(S):
rem //
rem //        OSR Open Systems Resources, Inc.
rem // 
rem //    REVISION:   V5.3
rem //
rem //      Fix a couple of bugs where the parameter was suppose to be "parameter"
rem //
rem //
rem //    REQUIREMENTS:  Environment variables that must be set.
rem //
rem //		BASEDIR - Automatically set up by NT4 DDK. (e.g. D:\NT4DDK )
rem //      W2KBASE - must be set up by user to point to W2K DDK  (e.g D:\Nt50DDK )
rem //      WXPBASE - must be set up by user to point to WXP DDK  (e.g D:\WINDDK\2600)
rem //      WNETBASE - must be set up by user to point to WNET DDK (e.g D:\WINDDK\3615) 
rem //
rem //
rem //    COMMAND FORMAT:
rem //
rem //		ddkbuild -PLATFORM BUILDTYPE DIRECTORY [FLAGS]
rem //
rem //              PLATFORM is either 
rem //                   WXP, WXP64, WXP2K - builds using WXP DDK
rem //                   W2K, W2K64,  - builds using W2k DDK
rem //                   WNET, WNET64, WNET2K, WNETXP, WNETXP64 - builds using WNET DDK
rem //                   NT4  - build using NT4 DDK (NT4 is the default)
rem //              BUILDTYPE - free, checked, chk or fre
rem //				DIRECTORY is the path to the directory to be build.  It can be "."
rem //      
rem //
rem //	  BROWSE FILES:
rem //	
rem //       This procedure supports the building of BROWSE files to be used by 
rem //       Visual Studio 6 and by Visual Studio.Net  However, the BSCfiles created
rem //       by bscmake for the 2 studios are not compatible. When this command procedure
rem //       runs, it selects the first bscmake.exe found in the path.   So, make
rem //       sure that the correct bscmake.exe is in the path....
rem //
rem //    COMPILERS:
rem //
rem //        If you are building NT4 or Windows 2000 drivers you should really
rem //        be using the VC 6 compiler.   If you are building a WXP driver,
rem //        you should be using the compiler that comes with the DDK.  This 
rem //        procedure should use the correct compiler.
rem //       
rem //    GENERAL COMMENTS:
rem //        This procedure is not written to be elegant!  It is written to work 
rem //        and to be easy to debug.   While we could have reused a bunch of
rem //        code, we decided not to.   
rem //
rem ///////////////////////////////////////////////////////////////////////////////

set scriptDebug=off
setlocal ENABLEEXTENSIONS

@echo %scriptDebug%

rem //
rem // Check for NT 4 Build
rem //
if /I %1 NEQ -NT4   goto NoNT4Base

@echo NT4 BUILD using NT4 DDK

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=free
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=checked
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% "%MSDEVDIR%"
popd

@echo %scriptDebug%

goto RegularBuild

:NoNT4Base

rem //
rem // Check for WNET Windows 2000 Build using WNET DDK
rem //
if /I %1 NEQ -WNET2K goto NoWNET2KBase

@echo W2K BUILD using WNET DDK

set BASEDIR=%WNETBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=f
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=c
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% W2K %mode% 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWNET2KBase

rem //
rem // Check for WXP Build using WNET DDK
rem //
if /I %1 NEQ -WNETXP goto NoWNETXPBase

@echo WXP BUILD using WNET DDK

set BASEDIR=%WNETBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode


if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% WXP 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWNETXPBase

rem //
rem // Check for WXP 64 bit Build using WNET DDK
rem //
if /I %1 NEQ -WNETXP64 goto NoWNETXP64Base

@echo WXP 64 BIT BUILD using WNET DDK

set BASEDIR=%WNETBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% 64 WXP 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWNETXP64Base

rem //
rem // Check for WNET 64 bit Build using WNET DDK
rem //
if /I %1 NEQ -WNET64 goto NoWNET64Base

@echo WNET 64 BIT BUILD using WNET DDK

set BASEDIR=%WNETBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% 64 WNET 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWNET64Base

rem //
rem // Check for WNET 32 BIT BUILD using WNET DDK
rem //
if /I %1 NEQ -WNET goto NoWNETBase

@echo WNET 32 BIT BUILD using WNET DDK

set BASEDIR=%WNETBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode%
popd

@echo %scriptDebug%

goto RegularBuild

:NoWNETBase

rem //
rem // Check for WXP 64 BIT BUILD using WXP DDK
rem //
if /I %1 NEQ -WXP64 goto NoWxp64Base

@echo WXP 64 BIT BUILD using WXP DDK

set BASEDIR=%WXPBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% 64
popd

@echo %scriptDebug%

goto RegularBuild

:NoWxp64Base

rem //
rem // Check for WXP 32 BIT BUILD using WXP DDK
rem //
if /I %1 NEQ -WXP goto NoWxpBase

@echo WXP 32 BIT BUILD using WXP DDK

set BASEDIR=%WXPBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWxpBase

rem //
rem // Check for W2K 32 BIT BUILD using WXP DDK
rem //
if /I %1 NEQ -WXP2K goto NoWxp2KBase

@echo W2K 32 BIT BUILD using WXP DDK

set BASEDIR=%WXPBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\w2k\set2k.bat %BASEDIR% %mode% 
popd

@echo %scriptDebug%

goto RegularBuild

:NoWxp2KBase

rem //
rem // Check for W2K 64 BIT BUILD using W2K DDK
rem //
if /I %1 NEQ -W2K64 goto NoW2k64Base

@echo W2K 64 BIT BUILD using W2K DDK

set BASEDIR=%W2KBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=fre
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=chk
if "%mode% "=="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv64.bat %BASEDIR% %mode% 
popd

@echo %scriptDebug%

goto RegularBuild

:NoW2k64Base

rem //
rem // Check for W2K 32 BIT BUILD using W2K DDK
rem //
if /I %1 NEQ -W2K goto NoW2kBase

@echo W2K 32 BIT BUILD using W2K DDK

set BASEDIR=%W2KBASE%

shift

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=free
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=checked
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% 
popd

@echo %scriptDebug%

goto RegularBuild

:NoW2kBase

rem //
rem // Defaulting to NT4 BUILD using NT4 DDK
rem //
@echo Defaulting NT4 BUILD using NT4 DDK

if "%BASEDIR%"=="" goto ErrNoBASEDIR

set path=%BASEDIR%\bin;%path%

set mode=
for %%f in (free FREE fre FRE) do if %%f == %1 set mode=free
for %%f in (checked CHECKED chk CHK) do if %%f == %1 set mode=checked
if "%mode%" =="" goto ErrBadMode

if "%2" == "" goto ErrNoDir

if not exist %2 goto ErrNoDir

pushd .
call %BASEDIR%\bin\setenv.bat %BASEDIR% %mode% "%MSDEVDIR%"
popd

@echo %scriptDebug%

goto RegularBuild

rem //
rem // All builds go here for the rest of the procedure.  Now,
rem // we are getting ready to call build.  The big problem
rem // here is to figure our the name of the buildxxx files being
rem // generated for the different platforms.
rem //
 
:RegularBuild

set mpFlag=-M

if "%BUILD_ALT_DIR%"=="" goto NT4

rem win2k sets this!
set W2kEXT=%BUILD_ALT_DIR%

set mpFlag=-MI

:NT4

if "%NUMBER_OF_PROCESSORS%"=="" set mpFlag=
if "%NUMBER_OF_PROCESSORS%"=="1" set mpFlag=

@echo build in directory %2 with arguments %3 (basedir %BASEDIR%)

cd /D %2
set bflags=-Ze
set bscFlags=""

if "%3" == "" goto done

if "%3" == "/a" goto rebuildall

set bscFlags=/n

set bflags=%3 -e

goto done

:rebuildall

set bscFlags=/n
set bflags=-cfeZ

:done

if EXIST build%W2kEXT%.err	erase build%W2kEXT%.err
if EXIST build%W2kEXT%.wrn  erase build%W2kEXT%.wrn
if EXIST build%W2kEXT%.log	erase build%W2kEXT%.log


@echo run build %bflags% %mpFlag% for %mode% version in %2
pushd .
build  %bflags% %mpFlag%
popd

@echo %scriptDebug%

rem assume that the onscreen errors are complete!

@echo =============== build warnings ======================
if exist build%W2kEXT%.log findstr "warning.*[CLU][0-9]*" build%W2kEXT%.log

@echo. 
@echo. 
@echo build complete

@echo building browse information files

if EXIST buildbrowse.cmd goto doBrowsescript

set sbrlist=sbrList.txt

if not EXIST sbrList%CPU%.txt goto sbrDefault

set sbrlist=sbrList%CPU%.txt

:sbrDefault

if not EXIST %sbrlist% goto end

if %bscFlags% == "" goto noBscFlags

bscmake %bscFlags% @%sbrlist%

goto end

:noBscFlags

bscmake @%sbrlist%

goto end

:doBrowsescript

call buildBrowse %mode%

goto end

:ErrBadMode
@echo error: first param must be "checked", "free", "chk" or "fre"
goto usage

:ErrNoPlatform
@echo error: 

:ErrNoBASEDIR
@echo error: BASEDIR, W2KBASE, WXPBASE, or WNETBASE environment variable not set.
goto usage

:ErrnoDir
@echo Error: second parameter must be a valid directory

:usage
@echo usage: ddkbuild [-W2K] "checked | free | chk | fre" "directory-to-build" [flags] 
@echo        -W2K       indicates development system uses W2KBASE environment variable
@echo                   to locate the win2000 ddk, otherwise BASEDIR is used (optional)
@echo        -W2K64     indicates development sytsem uses W2KBASE environment variable
@echo                   to locate the win2000 64 ddk, otherwise BASEDIR is used (optional)
@echo        -WXP       to indicate WXP Build uses WXPBASE enviornment variable.
@echo        -WXP64     to indicate WXP 64 bit build, uses WXPBASE
@echo        -WXP2K     to indicate Windows 2000 build using WXP ddk
@echo        -WNET      to indicate Windows .Net builds using WNET ddk
@echo        -WNET64    to indicate Windows .Net 64 bit builds using WNET DDK
@echo        -WNETXP    to indicate Windows XP builds suing WNET DDK
@echo        -WNETXP64  to indicate Windows XP 64 bit builds suing WNET DDK
@echo        -WNET2K    to indicate Windows 2000 builds using WNET DDK
@echo        -NT4       to indicate NT4 build. This is the default, if not specified.
@echo         checked   indicates a checked build
@echo         free      indicates a free build
@echo         chk		indicates a checked build
@echo         fre		indicates a free build
@echo         directory path to build directory, try . (cwd)
@echo         flags     any random flags you think should be passed to build (try /a for clean)
@echo.
@echo         ex: ddkbuild checked .    or ddkbuild -NT4 checked . (for NT4 BUILD)
@echo         ex: ddkbuild -WXP64 chk .
@echo         ex: ddkbuild -WXP chk c:\projects\myproject
@echo         ex: ddkbuild -WNET64 chk . -amd64     (AMD 64 bit build)
@echo         ex: ddkbuild -WNET64 fre . -arm       (will leg be next?)
@echo.
@echo         In order for this procedure to work correctly for each platform, it requires
@echo         an environment variable to be set up for certain platforms.   The environment
@echo         variables are as follows:
@echo. 
@echo         BASEDIR - set up by the installation of the NT4 DDK, needed for -NT4 builds
@echo         W2KBASE - You must set this up to do -W2K and -W2K64 builds
@echo         WXPBASE - You must set this up to do -WXP, -WXP64, -WXP2K builds
@echo         WNETBASE - You must set this up to do -WNET, -WNET64, -WNETXP, -WNETXP64, and
@echo                    -WNET2K builds
@echo.
@echo.
@echo   OSR DDKBUILD.BAT V5.3 - OSR, Open Systems Resources, Inc.
@echo     report any problems found to info@osr.com
@echo.   

rem goto end

:end

@echo ddkbuild complete
