# Microsoft Developer Studio Project File - Name="NDInst" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=NDInst - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NDInst.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "NDInst - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NDInst_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\inc" /I "..\..\..\lanscsisystemv2\src\inc" /D "NDEBUG" /D "_UNICODE" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_USRDLL" /D "NDINST_EXPORTS" /FR /FD $(OEM_DEFINES) /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" $(OEM_DEFINES)
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib nddevice.lib ndfilter.lib ndlog.lib ndnetcomp.lib WSnmp32.lib NetDiskUILib.lib /nologo /dll /machine:I386 /libpath:"..\..\lib\fre_w2k_x86\i386" /libpath:"$(WNETBASE)\lib\w2k\i386" /libpath:"..\..\..\lanscsisystemv2\lib\Release"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NDInst_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\inc" /I "..\..\..\lanscsisystemv2\src\inc" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "_USRDLL" /D "NDINST_EXPORTS" /FR /FD /GZ $(OEM_DEFINES) /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" $(OEM_DEFINES)
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib nddevice.lib ndfilter.lib ndlog.lib WSnmp32.lib ndnetcomp.lib NetDiskUILib.lib shell32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept /libpath:"..\..\lib\chk_w2k_x86\i386" /libpath:"$(WNETBASE)\lib\w2k\i386" /libpath:"..\..\..\lanscsisystemv2\lib\Debug"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
TargetPath=.\Debug\NDInst.dll
TargetName=NDInst
SOURCE="$(InputPath)"
PostBuild_Cmds=copy /y $(TargetPath) ..\..\..\installer\NetDiskSetup\Binary	copy /y Debug\$(TargetName).pdb \symbols\local	..\..\bin\exeplace Debug $(TargetPath) Debug\$(TargetName).pdb
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "NDInst - Win32 Release"
# Name "NDInst - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ActivateWarnDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\FndApp.c
# End Source File
# Begin Source File

SOURCE=.\InstallTipDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\MultilangRes.cpp
# End Source File
# Begin Source File

SOURCE=.\NDInst.cpp
# End Source File
# Begin Source File

SOURCE=.\NDInst.def

!IF  "$(CFG)" == "NDInst - Win32 Release"

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\NDInstDebug.def

!IF  "$(CFG)" == "NDInst - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\RebootFlag.cpp
# End Source File
# Begin Source File

SOURCE=.\SvcQuery.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ActivateWarnDlg.h
# End Source File
# Begin Source File

SOURCE=.\FndApp.h
# End Source File
# Begin Source File

SOURCE=.\InstallTipDlg.h
# End Source File
# Begin Source File

SOURCE=.\MsiProgressBar.h
# End Source File
# Begin Source File

SOURCE=.\MultilangRes.h
# End Source File
# Begin Source File

SOURCE=.\NDInst.h
# End Source File
# Begin Source File

SOURCE=.\RebootFlag.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SvcQuery.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\NDInst.rc

!IF  "$(CFG)" == "NDInst - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputPath=.\NDInst.rc

"Release/NDInst.res" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	rc /l 0x409 /fo"Release/NDInst.res" /d "NDEBUG" $(OEM_DEFINES)  $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputPath=.\NDInst.rc

"Debug/NDInst.res" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	rc /l 0x409 /fo"Debug/NDInst.res" /d "_DEBUG" $(OEM_DEFINES) $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\pddkbld.cmd
# End Source File
# Begin Source File

SOURCE=.\ReadMe.txt

!IF  "$(CFG)" == "NDInst - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\setpsdk.cmd
# End Source File
# Begin Source File

SOURCE=.\SOURCES

!IF  "$(CFG)" == "NDInst - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NDInst - Win32 Debug"

!ENDIF 

# End Source File
# End Target
# End Project
