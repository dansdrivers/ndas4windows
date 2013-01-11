# Microsoft Developer Studio Generated NMAKE File, Based on LanscsiLib.dsp
!IF "$(CFG)" == ""
CFG=LanscsiLib - Win32 Debug
!MESSAGE No configuration specified. Defaulting to LanscsiLib - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "LanscsiLib - Win32 Release" && "$(CFG)" != "LanscsiLib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LanscsiLib.mak" CFG="LanscsiLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LanscsiLib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "LanscsiLib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "LanscsiLib - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\LanscsiLib.lib" "$(OUTDIR)\LanscsiLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\Instdrv.obj"
	-@erase "$(INTDIR)\Instdrv.sbr"
	-@erase "$(INTDIR)\LanscsiLib.obj"
	-@erase "$(INTDIR)\LanscsiLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\LanscsiLib.bsc"
	-@erase "$(OUTDIR)\LanscsiLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "%XPBASE%\inc" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanscsiLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LanscsiLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\Instdrv.sbr" \
	"$(INTDIR)\LanscsiLib.sbr"

"$(OUTDIR)\LanscsiLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\LanscsiLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Instdrv.obj" \
	"$(INTDIR)\LanscsiLib.obj"

"$(OUTDIR)\LanscsiLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Release\LanscsiLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\LanscsiLib.lib" "$(OUTDIR)\LanscsiLib.bsc"
   ..\..\bin\libplace.cmd Release .\Release\LanscsiLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "LanscsiLib - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\LanscsiLib.lib" "$(OUTDIR)\LanscsiLib.bsc"


CLEAN :
	-@erase "$(INTDIR)\Instdrv.obj"
	-@erase "$(INTDIR)\Instdrv.sbr"
	-@erase "$(INTDIR)\LanscsiLib.obj"
	-@erase "$(INTDIR)\LanscsiLib.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\LanscsiLib.bsc"
	-@erase "$(OUTDIR)\LanscsiLib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "%XPBASE%\inc" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\LanscsiLib.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LanscsiLib.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\Instdrv.sbr" \
	"$(INTDIR)\LanscsiLib.sbr"

"$(OUTDIR)\LanscsiLib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\LanscsiLib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Instdrv.obj" \
	"$(INTDIR)\LanscsiLib.obj"

"$(OUTDIR)\LanscsiLib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

TargetPath=.\Debug\LanscsiLib.lib
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\LanscsiLib.lib" "$(OUTDIR)\LanscsiLib.bsc"
   ..\..\bin\libplace.cmd Debug .\Debug\LanscsiLib.lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("LanscsiLib.dep")
!INCLUDE "LanscsiLib.dep"
!ELSE 
!MESSAGE Warning: cannot find "LanscsiLib.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "LanscsiLib - Win32 Release" || "$(CFG)" == "LanscsiLib - Win32 Debug"
SOURCE=.\Instdrv.c

"$(INTDIR)\Instdrv.obj"	"$(INTDIR)\Instdrv.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\LanscsiLib.c

"$(INTDIR)\LanscsiLib.obj"	"$(INTDIR)\LanscsiLib.sbr" : $(SOURCE) "$(INTDIR)"



!ENDIF 

