# Microsoft Developer Studio Generated NMAKE File, Based on NDInst.dsp
!IF "$(CFG)" == ""
CFG=NDInst - Win32 Debug
!MESSAGE No configuration specified. Defaulting to NDInst - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "NDInst - Win32 Release" && "$(CFG)" != "NDInst - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NDInst.mak" CFG="NDInst - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NDInst - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "NDInst - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "NDInst - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\NDInst.dll" "$(OUTDIR)\NDInst.bsc"

!ELSE 

ALL : "NDNetCompLib - Win32 Release" "NDLogLib - Win32 Release" "NDFilterLib - Win32 Release" "NDDeviceLib - Win32 Release" "$(OUTDIR)\NDInst.dll" "$(OUTDIR)\NDInst.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"NDDeviceLib - Win32 ReleaseCLEAN" "NDFilterLib - Win32 ReleaseCLEAN" "NDLogLib - Win32 ReleaseCLEAN" "NDNetCompLib - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ActivateWarnDlg.obj"
	-@erase "$(INTDIR)\ActivateWarnDlg.sbr"
	-@erase "$(INTDIR)\FndApp.obj"
	-@erase "$(INTDIR)\FndApp.sbr"
	-@erase "$(INTDIR)\InstallTipDlg.obj"
	-@erase "$(INTDIR)\InstallTipDlg.sbr"
	-@erase "$(INTDIR)\MultilangRes.obj"
	-@erase "$(INTDIR)\MultilangRes.sbr"
	-@erase "$(INTDIR)\NDInst.obj"
	-@erase "$(INTDIR)\NDInst.sbr"
	-@erase "$(INTDIR)\RebootFlag.obj"
	-@erase "$(INTDIR)\RebootFlag.sbr"
	-@erase "$(INTDIR)\SvcQuery.obj"
	-@erase "$(INTDIR)\SvcQuery.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\NDInst.bsc"
	-@erase "$(OUTDIR)\NDInst.dll"
	-@erase "$(OUTDIR)\NDInst.exp"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "..\inc" /I "..\..\..\lanscsisystemv2\src\inc" /D "NDEBUG" /D "_UNICODE" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_USRDLL" /D "NDINST_EXPORTS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(OEM_DEFINES) /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\NDInst.res" /d "NDEBUG" $(OEM_DEFINES) 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\NDInst.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\ActivateWarnDlg.sbr" \
	"$(INTDIR)\FndApp.sbr" \
	"$(INTDIR)\InstallTipDlg.sbr" \
	"$(INTDIR)\MultilangRes.sbr" \
	"$(INTDIR)\NDInst.sbr" \
	"$(INTDIR)\RebootFlag.sbr" \
	"$(INTDIR)\SvcQuery.sbr"

"$(OUTDIR)\NDInst.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib nddevice.lib ndfilter.lib ndlog.lib ndnetcomp.lib WSnmp32.lib NetDiskUILib.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\NDInst.pdb" /machine:I386 /def:".\NDInst.def" /out:"$(OUTDIR)\NDInst.dll" /implib:"$(OUTDIR)\NDInst.lib" /libpath:"..\..\lib\fre_w2k_x86\i386" /libpath:"$(WNETBASE)\lib\w2k\i386" /libpath:"..\..\..\lanscsisystemv2\lib\Release" 
DEF_FILE= \
	".\NDInst.def"
LINK32_OBJS= \
	"$(INTDIR)\ActivateWarnDlg.obj" \
	"$(INTDIR)\FndApp.obj" \
	"$(INTDIR)\InstallTipDlg.obj" \
	"$(INTDIR)\MultilangRes.obj" \
	"$(INTDIR)\NDInst.obj" \
	"$(INTDIR)\RebootFlag.obj" \
	"$(INTDIR)\SvcQuery.obj" \
	"$(INTDIR)\NDInst.res" \
	"..\..\lib\fre_w2k_x86\i386\NDDevice.lib" \
	"..\..\lib\fre_w2k_x86\i386\NDFilter.lib" \
	"..\..\lib\fre_w2k_x86\i386\NDLog.lib" \
	"..\..\lib\fre_w2k_x86\i386\NDNetComp.lib"

"$(OUTDIR)\NDInst.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\NDInst.dll" "$(OUTDIR)\NDInst.bsc"

!ELSE 

ALL : "NDNetCompLib - Win32 Debug" "NDLogLib - Win32 Debug" "NDFilterLib - Win32 Debug" "NDDeviceLib - Win32 Debug" "$(OUTDIR)\NDInst.dll" "$(OUTDIR)\NDInst.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"NDDeviceLib - Win32 DebugCLEAN" "NDFilterLib - Win32 DebugCLEAN" "NDLogLib - Win32 DebugCLEAN" "NDNetCompLib - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ActivateWarnDlg.obj"
	-@erase "$(INTDIR)\ActivateWarnDlg.sbr"
	-@erase "$(INTDIR)\FndApp.obj"
	-@erase "$(INTDIR)\FndApp.sbr"
	-@erase "$(INTDIR)\InstallTipDlg.obj"
	-@erase "$(INTDIR)\InstallTipDlg.sbr"
	-@erase "$(INTDIR)\MultilangRes.obj"
	-@erase "$(INTDIR)\MultilangRes.sbr"
	-@erase "$(INTDIR)\NDInst.obj"
	-@erase "$(INTDIR)\NDInst.sbr"
	-@erase "$(INTDIR)\RebootFlag.obj"
	-@erase "$(INTDIR)\RebootFlag.sbr"
	-@erase "$(INTDIR)\SvcQuery.obj"
	-@erase "$(INTDIR)\SvcQuery.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\NDInst.bsc"
	-@erase "$(OUTDIR)\NDInst.dll"
	-@erase "$(OUTDIR)\NDInst.exp"
	-@erase "$(OUTDIR)\NDInst.ilk"
	-@erase "$(OUTDIR)\NDInst.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\inc" /I "..\..\..\lanscsisystemv2\src\inc" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "_USRDLL" /D "NDINST_EXPORTS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ $(OEM_DEFINES) /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\NDInst.res" /d "_DEBUG" $(OEM_DEFINES) 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\NDInst.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\ActivateWarnDlg.sbr" \
	"$(INTDIR)\FndApp.sbr" \
	"$(INTDIR)\InstallTipDlg.sbr" \
	"$(INTDIR)\MultilangRes.sbr" \
	"$(INTDIR)\NDInst.sbr" \
	"$(INTDIR)\RebootFlag.sbr" \
	"$(INTDIR)\SvcQuery.sbr"

"$(OUTDIR)\NDInst.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib nddevice.lib ndfilter.lib ndlog.lib ndnetcomp.lib WSnmp32.lib NetDiskUILib.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\NDInst.pdb" /debug /machine:I386 /def:".\NDInstDebug.def" /out:"$(OUTDIR)\NDInst.dll" /implib:"$(OUTDIR)\NDInst.lib" /pdbtype:sept /libpath:"..\..\lib\chk_w2k_x86\i386" /libpath:"$(WNETBASE)\lib\w2k\i386" /libpath:"..\..\..\lanscsisystemv2\lib\Debug" 
DEF_FILE= \
	".\NDInstDebug.def"
LINK32_OBJS= \
	"$(INTDIR)\ActivateWarnDlg.obj" \
	"$(INTDIR)\FndApp.obj" \
	"$(INTDIR)\InstallTipDlg.obj" \
	"$(INTDIR)\MultilangRes.obj" \
	"$(INTDIR)\NDInst.obj" \
	"$(INTDIR)\RebootFlag.obj" \
	"$(INTDIR)\SvcQuery.obj" \
	"$(INTDIR)\NDInst.res" \
	"..\..\lib\chk_w2k_x86\i386\NDDevice.lib" \
	"..\..\lib\chk_w2k_x86\i386\NDFilter.lib" \
	"..\..\lib\chk_w2k_x86\i386\NDLog.lib" \
	"..\..\lib\chk_w2k_x86\i386\NDNetComp.lib"

"$(OUTDIR)\NDInst.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

TargetPath=.\Debug\NDInst.dll
TargetName=NDInst
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "NDNetCompLib - Win32 Debug" "NDLogLib - Win32 Debug" "NDFilterLib - Win32 Debug" "NDDeviceLib - Win32 Debug" "$(OUTDIR)\NDInst.dll" "$(OUTDIR)\NDInst.bsc"
   ..\..\bin\exeplace Debug .\Debug\NDInst.dll Debug\NDInst.pdb
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("NDInst.dep")
!INCLUDE "NDInst.dep"
!ELSE 
!MESSAGE Warning: cannot find "NDInst.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "NDInst - Win32 Release" || "$(CFG)" == "NDInst - Win32 Debug"
SOURCE=.\ActivateWarnDlg.cpp

"$(INTDIR)\ActivateWarnDlg.obj"	"$(INTDIR)\ActivateWarnDlg.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\FndApp.c

"$(INTDIR)\FndApp.obj"	"$(INTDIR)\FndApp.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\InstallTipDlg.cpp

"$(INTDIR)\InstallTipDlg.obj"	"$(INTDIR)\InstallTipDlg.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MultilangRes.cpp

"$(INTDIR)\MultilangRes.obj"	"$(INTDIR)\MultilangRes.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\NDInst.cpp

"$(INTDIR)\NDInst.obj"	"$(INTDIR)\NDInst.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\RebootFlag.cpp

"$(INTDIR)\RebootFlag.obj"	"$(INTDIR)\RebootFlag.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\SvcQuery.cpp

"$(INTDIR)\SvcQuery.obj"	"$(INTDIR)\SvcQuery.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\NDInst.rc

!IF  "$(CFG)" == "NDInst - Win32 Release"

InputPath=.\NDInst.rc

"$(INTDIR)\NDInst.res" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	rc /l 0x409 /fo"Release/NDInst.res" /d "NDEBUG" $(OEM_DEFINES)  $(InputPath)
<< 
	

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

InputPath=.\NDInst.rc

"$(INTDIR)\NDInst.res" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	rc /l 0x409 /fo"Debug/NDInst.res" /d "_DEBUG" $(OEM_DEFINES) $(InputPath)
<< 
	

!ENDIF 

!IF  "$(CFG)" == "NDInst - Win32 Release"

"NDDeviceLib - Win32 Release" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDDeviceLib"
   ..\..\bin\ddkbuild -WNET2K free .
   cd "..\NDInst"

"NDDeviceLib - Win32 ReleaseCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDDeviceLib"
   cd "..\NDInst"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

"NDDeviceLib - Win32 Debug" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDDeviceLib"
   ..\..\bin\ddkbuild -WNET2K checked .
   cd "..\NDInst"

"NDDeviceLib - Win32 DebugCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDDeviceLib"
   cd "..\NDInst"

!ENDIF 

!IF  "$(CFG)" == "NDInst - Win32 Release"

"NDFilterLib - Win32 Release" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDFilterLib"
   ..\..\bin\ddkbuild -WNET2K free .
   cd "..\NDInst"

"NDFilterLib - Win32 ReleaseCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDFilterLib"
   cd "..\NDInst"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

"NDFilterLib - Win32 Debug" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDFilterLib"
   ..\..\bin\ddkbuild -WNET2K checked .
   cd "..\NDInst"

"NDFilterLib - Win32 DebugCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDFilterLib"
   cd "..\NDInst"

!ENDIF 

!IF  "$(CFG)" == "NDInst - Win32 Release"

"NDLogLib - Win32 Release" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDLogLib"
   ..\..\bin\ddkbuild -WNET2K free .
   cd "..\NDInst"

"NDLogLib - Win32 ReleaseCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDLogLib"
   cd "..\NDInst"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

"NDLogLib - Win32 Debug" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDLogLib"
   ..\..\bin\ddkbuild -WNET2K chk .
   cd "..\NDInst"

"NDLogLib - Win32 DebugCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDLogLib"
   cd "..\NDInst"

!ENDIF 

!IF  "$(CFG)" == "NDInst - Win32 Release"

"NDNetCompLib - Win32 Release" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDNetCompLib"
   ..\..\bin\ddkbuild -WNET2K free .
   cd "..\NDInst"

"NDNetCompLib - Win32 ReleaseCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDNetCompLib"
   cd "..\NDInst"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

"NDNetCompLib - Win32 Debug" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDNetCompLib"
   ..\..\bin\ddkbuild -WNET2K chk .
   cd "..\NDInst"

"NDNetCompLib - Win32 DebugCLEAN" : 
   cd "\workspc\NDSWVer3\trunk\installhelper\src\NDNetCompLib"
   cd "..\NDInst"

!ENDIF 


!ENDIF 

