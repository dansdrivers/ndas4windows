# Microsoft Developer Studio Generated NMAKE File, Based on LanScsiOpLib.dsp
!IF "$(CFG)" == ""
CFG=LanScsiOpLib - Win32 Debug
!MESSAGE No configuration specified. Defaulting to LanScsiOpLib - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "LanscsiOpLib - Win32 Release" && "$(CFG)" != "LanscsiOpLib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LanScsiOpLib.mak" CFG="LanScsiOpLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LanscsiOpLib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "LanscsiOpLib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "LanscsiOpLib - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\LanScsiOpLib.lib" "$(OUTDIR)\LanScsiOpLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\LanScsiOpLib.obj"
	-@erase "$(INTDIR)\LanScsiOpLib.sbr"
	-@erase "$(INTDIR)\NDASOpLib.obj"
	-@erase "$(INTDIR)\NDASOpLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\LanScsiOpLib.bsc"
	-@erase "$(OUTDIR)\LanScsiOpLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

#CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "__NDASCHIP20_ALPHA_SUPPORT__" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LanScsiOpLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\LanScsiOpLib.sbr" \
	"$(INTDIR)\NDASOpLib.sbr"

"$(OUTDIR)\LanScsiOpLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\LanScsiOpLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\LanScsiOpLib.obj" \
	"$(INTDIR)\NDASOpLib.obj"

"$(OUTDIR)\LanScsiOpLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Release\LanScsiOpLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\LanScsiOpLib.lib" "$(OUTDIR)\LanScsiOpLib.bsc"
   ..\..\bin\libplace.cmd Release .\Release\LanScsiOpLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "LanscsiOpLib - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\LanScsiOpLib.lib" "$(OUTDIR)\LanScsiOpLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\LanScsiOpLib.obj"
	-@erase "$(INTDIR)\LanScsiOpLib.sbr"
	-@erase "$(INTDIR)\NDASOpLib.obj"
	-@erase "$(INTDIR)\NDASOpLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\LanScsiOpLib.bsc"
	-@erase "$(OUTDIR)\LanScsiOpLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

#CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "__NDASCHIP20_ALPHA_SUPPORT__" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LanScsiOpLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\LanScsiOpLib.sbr" \
	"$(INTDIR)\NDASOpLib.sbr"

"$(OUTDIR)\LanScsiOpLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\LanScsiOpLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\LanScsiOpLib.obj" \
	"$(INTDIR)\NDASOpLib.obj"

"$(OUTDIR)\LanScsiOpLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Debug\LanScsiOpLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\LanScsiOpLib.lib" "$(OUTDIR)\LanScsiOpLib.bsc"
   ..\..\bin\libplace.cmd Debug .\Debug\LanScsiOpLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 

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


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("LanScsiOpLib.dep")
!INCLUDE "LanScsiOpLib.dep"
!ELSE 
!MESSAGE Warning: cannot find "LanScsiOpLib.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "LanscsiOpLib - Win32 Release" || "$(CFG)" == "LanscsiOpLib - Win32 Debug"
SOURCE=.\LanScsiOpLib.c

"$(INTDIR)\LanScsiOpLib.obj"	"$(INTDIR)\LanScsiOpLib.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\NDASOpLib.c

"$(INTDIR)\NDASOpLib.obj"	"$(INTDIR)\NDASOpLib.sbr" : $(SOURCE) "$(INTDIR)"



!ENDIF 

