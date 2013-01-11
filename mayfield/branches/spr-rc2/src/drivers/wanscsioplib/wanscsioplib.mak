# Microsoft Developer Studio Generated NMAKE File, Based on WanScsiOpLib.dsp
!IF "$(CFG)" == ""
CFG=WanScsiOpLib - Win32 Debug
!MESSAGE No configuration specified. Defaulting to WanScsiOpLib - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "WanscsiOpLib - Win32 Release" && "$(CFG)" != "WanscsiOpLib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WanScsiOpLib.mak" CFG="WanScsiOpLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WanscsiOpLib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "WanscsiOpLib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "WanscsiOpLib - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\WanScsiOpLib.lib" "$(OUTDIR)\WanScsiOpLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\WanScsiOpLib.obj"
	-@erase "$(INTDIR)\WanScsiOpLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\WanScsiOpLib.bsc"
	-@erase "$(OUTDIR)\WanScsiOpLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\WanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WanScsiOpLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\WanScsiOpLib.sbr"

"$(OUTDIR)\WanScsiOpLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\WanScsiOpLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\WanScsiOpLib.obj"

"$(OUTDIR)\WanScsiOpLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Release\WanScsiOpLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\WanScsiOpLib.lib" "$(OUTDIR)\WanScsiOpLib.bsc"
   ..\..\bin\libplace.cmd Release .\Release\WanScsiOpLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "WanscsiOpLib - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\WanScsiOpLib.lib" "$(OUTDIR)\WanScsiOpLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\WanScsiOpLib.obj"
	-@erase "$(INTDIR)\WanScsiOpLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\WanScsiOpLib.bsc"
	-@erase "$(OUTDIR)\WanScsiOpLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\WanScsiOpLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WanScsiOpLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\WanScsiOpLib.sbr"

"$(OUTDIR)\WanScsiOpLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\WanScsiOpLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\WanScsiOpLib.obj"

"$(OUTDIR)\WanScsiOpLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Debug\WanScsiOpLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\WanScsiOpLib.lib" "$(OUTDIR)\WanScsiOpLib.bsc"
   ..\..\bin\libplace.cmd Debug .\Debug\WanScsiOpLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("WanScsiOpLib.dep")
!INCLUDE "WanScsiOpLib.dep"
!ELSE 
!MESSAGE Warning: cannot find "WanScsiOpLib.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "WanscsiOpLib - Win32 Release" || "$(CFG)" == "WanscsiOpLib - Win32 Debug"
SOURCE=.\WanScsiOpLib.c

"$(INTDIR)\WanScsiOpLib.obj"	"$(INTDIR)\WanScsiOpLib.sbr" : $(SOURCE) "$(INTDIR)"



!ENDIF 

