# Microsoft Developer Studio Project File - Name="LfsFiltLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=LfsFiltLib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LfsFiltLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LfsFiltLib.mak" CFG="LfsFiltLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LfsFiltLib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "LfsFiltLib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "LfsFiltLib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f LfsFiltLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LfsFiltLib.exe"
# PROP BASE Bsc_Name "LfsFiltLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNETXP free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LfsFiltLib.exe"
# PROP Bsc_Name "LfsFiltLib.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "LfsFiltLib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f LfsFiltLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LfsFiltLib.exe"
# PROP BASE Bsc_Name "LfsFiltLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNETXP checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LfsFiltLib.exe"
# PROP Bsc_Name "LfsFiltLib.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "LfsFiltLib - Win32 Release"
# Name "LfsFiltLib - Win32 Debug"

!IF  "$(CFG)" == "LfsFiltLib - Win32 Release"

!ELSEIF  "$(CFG)" == "LfsFiltLib - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\fastioNames.c
# End Source File
# Begin Source File

SOURCE=.\fsFilterOperationNames.c
# End Source File
# Begin Source File

SOURCE=.\irpNames.c
# End Source File
# Begin Source File

SOURCE=.\nameLists.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\filespyLfsFiltLib.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\makefile
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
