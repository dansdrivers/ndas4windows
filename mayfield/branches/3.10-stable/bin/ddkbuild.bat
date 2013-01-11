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

set
ó               ı!p                                                 à                              h 1	 ˆÿÿh 1	 ˆÿÿx 1	 ˆÿÿx 1	 ˆÿÿ                         1	 ˆÿÿ  1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿğ1	 ˆÿÿ        9            ½KoP    MÑ    XnP    ¢s#    XnP    ¢s#                        `              X1	 ˆÿÿX1	 ˆÿÿ                                ˆ1	 ˆÿÿˆ1	 ˆÿÿ˜1	 ˆÿÿ˜1	 ˆÿÿ¨1	 ˆÿÿ¨1	 ˆÿÿ¸1	 ˆÿÿ¸1	 ˆÿÿ       N               €f‚ÿÿÿÿ        ° 1	 ˆÿÿ                                     1	 ˆÿÿ 1	 ˆÿÿ       81	 ˆÿÿ81	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        €1	 ˆÿÿ€1	 ˆÿÿ                        ¨1	 ˆÿÿ¨1	 ˆÿÿ        ÁV
Ü                                            ¹#P    „a‰     1	 ˆÿÿ 1	 ˆÿÿ    ÿÿÿÿ                               81	 ˆÿÿ81	 ˆÿÿ                        y  y          
ó               ş!p                                                 à                              Ø1	 ˆÿÿØ1	 ˆÿÿè1	 ˆÿÿè1	 ˆÿÿ                       1	 ˆÿÿ1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ`1	 ˆÿÿ        :            ½KoP    MÑ    XnP    ¢s#    XnP    ¢s#                        `              È1	 ˆÿÿÈ1	 ˆÿÿ                                ø1	 ˆÿÿø1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ(1	 ˆÿÿ(1	 ˆÿÿ                      €f‚ÿÿÿÿ         1	 ˆÿÿ                                    1	 ˆÿÿ1	 ˆÿÿ       ¨1	 ˆÿÿ¨1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        ğ1	 ˆÿÿğ1	 ˆÿÿ                        1	 ˆÿÿ1	 ˆÿÿ        ÂV
Ü                                            ¹#P    „a‰    p1	 ˆÿÿp1	 ˆÿÿ    ÿÿÿÿ                               ¨1	 ˆÿÿ¨1	 ˆÿÿ                        y  y    1	 ˆÿÿ
ó               ÿ!p                                                 à                              H1	 ˆÿÿH1	 ˆÿÿX1	 ˆÿÿX1	 ˆÿÿ                       €1	 ˆÿÿ€1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿĞ1	 ˆÿÿ        ;            ½KoP    MÑ    XnP    ¢s#    XnP    ¢s#                        `              81	 ˆÿÿ81	 ˆÿÿ                                h1	 ˆÿÿh1	 ˆÿÿx1	 ˆÿÿx1	 ˆÿÿˆ1	 ˆÿÿˆ1	 ˆÿÿ˜1	 ˆÿÿ˜1	 ˆÿÿ       ‚              €f‚ÿÿÿÿ        1	 ˆÿÿ                                     	1	 ˆÿÿ 	1	 ˆÿÿ       	1	 ˆÿÿ	1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        `	1	 ˆÿÿ`	1	 ˆÿÿ                        ˆ	1	 ˆÿÿˆ	1	 ˆÿÿ        ÃV
Ü                                            ¹#P    „a‰    à	1	 ˆÿÿà	1	 ˆÿÿ    ÿÿÿÿ                               
1	 ˆÿÿ
1	 ˆÿÿ                        y  y  p1	 ˆÿÿ
ó                $p       % p       ] p                         à                             ¸
1	 ˆÿÿ¸
1	 ˆÿÿÈ
1	 ˆÿÿÈ
1	 ˆÿÿ 0                      ğ
1	 ˆÿÿğ
1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ@1	 ˆÿÿ        <            ½KoP    MÑ    XnP    ¢s#    XnP    ¢s#                  0      `              ¨1	 ˆÿÿ¨1	 ˆÿÿ                                Ø1	 ˆÿÿØ1	 ˆÿÿè1	 ˆÿÿè1	 ˆÿÿø1	 ˆÿÿø1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ       Á              €f‚ÿÿÿÿ         1	 ˆÿÿ                                    p1	 ˆÿÿp1	 ˆÿÿ       ˆ1	 ˆÿÿˆ1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        Ğ1	 ˆÿÿĞ1	 ˆÿÿ                        ø1	 ˆÿÿø1	 ˆÿÿ        ÄV
Ü                                            ¹#P    „a‰    P1	 ˆÿÿP1	 ˆÿÿ    ÿÿÿÿ                               ˆ1	 ˆÿÿˆ1	 ˆÿÿ                        y  y  à1	 ˆÿÿ
ó               $p                                                 à                              (1	 ˆÿÿ(1	 ˆÿÿ81	 ˆÿÿ81	 ˆÿÿ                       `1	 ˆÿÿ`1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ°1	 ˆÿÿ        =            ½KoP    MÑ    XnP    ¢jæ    XnP    ¢jæ                        `              1	 ˆÿÿ1	 ˆÿÿ                                H1	 ˆÿÿH1	 ˆÿÿX1	 ˆÿÿX1	 ˆÿÿh1	 ˆÿÿh1	 ˆÿÿx1	 ˆÿÿx1	 ˆÿÿ       -               €f‚ÿÿÿÿ        p1	 ˆÿÿ                                    à1	 ˆÿÿà1	 ˆÿÿ       ø1	 ˆÿÿø1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        @1	 ˆÿÿ@1	 ˆÿÿ                        h1	 ˆÿÿh1	 ˆÿÿ        ÅV
Ü                                            ¹#P    „a‰    À1	 ˆÿÿÀ1	 ˆÿÿ    ÿÿÿÿ                               ø1	 ˆÿÿø1	 ˆÿÿ                        y  y  P
1	 ˆÿÿ
ó               ø!p                                                 à                              ˜1	 ˆÿÿ˜1	 ˆÿÿ¨1	 ˆÿÿ¨1	 ˆÿÿ                       Ğ1	 ˆÿÿĞ1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ 1	 ˆÿÿ        3            ½KoP    MÑ    XnP    ¢s#    XnP    ¢s#                        `              ˆ1	 ˆÿÿˆ1	 ˆÿÿ                                ¸1	 ˆÿÿ¸1	 ˆÿÿÈ1	 ˆÿÿÈ1	 ˆÿÿØ1	 ˆÿÿØ1	 ˆÿÿè1	 ˆÿÿè1	 ˆÿÿ       o               €f‚ÿÿÿÿ        à1	 ˆÿÿ                                    P1	 ˆÿÿP1	 ˆÿÿ       h1	 ˆÿÿh1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        °1	 ˆÿÿ°1	 ˆÿÿ                        Ø1	 ˆÿÿØ1	 ˆÿÿ        »V
Ü                                            ¹#P    „a‰    01	 ˆÿÿ01	 ˆÿÿ    ÿÿÿÿ                               h1	 ˆÿÿh1	 ˆÿÿ                        y  y  À1	 ˆÿÿ
ó           '$p   p       a p        p       ğ p             à                             1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ P                      @1	 ˆÿÿ@1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ1	 ˆÿÿ        4            ½KoP    MÑ    XnP    ¢…    XnP    ¢…          0        P      `              ø1	 ˆÿÿø1	 ˆÿÿ                                (1	 ˆÿÿ(1	 ˆÿÿ81	 ˆÿÿ81	 ˆÿÿH1	 ˆÿÿH1	 ˆÿÿX1	 ˆÿÿX1	 ˆÿÿ       Ó              €f‚ÿÿÿÿ        P1	 ˆÿÿ                                    À1	 ˆÿÿÀ1	 ˆÿÿ       Ø1	 ˆÿÿØ1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ         1	 ˆÿÿ 1	 ˆÿÿ                        H1	 ˆÿÿH1	 ˆÿÿ        ¼V
Ü                                            ¹#P    „a‰     1	 ˆÿÿ 1	 ˆÿÿ    ÿÿÿÿ                               Ø1	 ˆÿÿØ1	 ˆÿÿ                        y  y  01	 ˆÿÿ
ó               ö!p                                                 à                              x1	 ˆÿÿx1	 ˆÿÿˆ1	 ˆÿÿˆ1	 ˆÿÿ                       °1	 ˆÿÿ°1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿ 1	 ˆÿÿ        1            ½KoP    HCv&    Ù ˜O            î#P    ç4!                        `              h1	 ˆÿÿh1	 ˆÿÿ                                ˜1	 ˆÿÿ˜1	 ˆÿÿ¨1	 ˆÿÿ¨1	 ˆÿÿ¸1	 ˆÿÿ¸1	 ˆÿÿÈ1	 ˆÿÿÈ1	 ˆÿÿ                      €f‚ÿÿÿÿ        À1	 ˆÿÿ                                    01	 ˆÿÿ01	 ˆÿÿ       H1	 ˆÿÿH1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ        1	 ˆÿÿ1	 ˆÿÿ                        ¸1	 ˆÿÿ¸1	 ˆÿÿ        ¹V
Ü                                            ¹#P    „a‰    1	 ˆÿÿ1	 ˆÿÿ    ÿÿÿÿ                               H1	 ˆÿÿH1	 ˆÿÿ                        y  y   1	 ˆÿÿ
ó               ã#H                                                                               è1	 ˆÿÿè1	 ˆÿÿø1	 ˆÿÿø1	 ˆÿÿ                        1	 ˆÿÿ 1	 ˆÿÿíA             ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÀp‚ÿÿÿÿ àœ6 ˆÿÿp1	 ˆÿÿ        q÷            ½KoP    HCv&    Şz¢O            î#P    æØõ                        `              Ø1	 ˆÿÿØ1	 ˆÿÿ                                1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ1	 ˆÿÿ(1	 ˆÿÿ(1	 ˆÿÿ81	 ˆÿÿ81	 ˆÿÿ                      €f‚ÿÿÿÿ        01	 ˆÿÿ                                     1	 ˆÿÿ 1	 ˆÿÿ       ¸1	 ˆÿÿ¸1	 ˆÿÿ                         «ÿÿÿÿÚ       4ƒ; ˆÿÿ         1	 ˆÿÿ 1	 ˆÿÿ                        (1	 ˆÿÿ(1	 ˆÿÿ        Z
Ü                            ã#H            ®#P    ×¸/6    €1	 ˆÿÿ€1	 ˆÿÿ    ÿÿÿÿ                               ¸1	 ˆÿÿ¸1	 ˆÿÿ                      y  y  1	 ˆÿÿ
ó               $p                                                 à                              X1	 ˆÿÿX1	 ˆÿÿh1	 ˆÿÿh1	 ˆÿÿ               