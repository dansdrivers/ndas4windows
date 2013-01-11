# Microsoft Developer Studio Project File - Name="Filter" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=Filter - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Filter.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Filter.mak" CFG="Filter - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Filter - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "Filter - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "Filter - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f Filter.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Filter.exe"
# PROP BASE Bsc_Name "Filter.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET checked ."
# PROP Rebuild_Opt "/cZ"
# PROP Target_File "Filter.exe"
# PROP Bsc_Name "PurgeFilter.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Filter - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f Filter.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Filter.exe"
# PROP BASE Bsc_Name "Filter.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "Filter.exe"
# PROP Bsc_Name "PurgeFilter.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "Filter - Win32 Release"
# Name "Filter - Win32 Debug"

!IF  "$(CFG)" == "Filter - Win32 Release"

!ELSEIF  "$(CFG)" == "Filter - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\filespy.c
# End Source File
# Begin Source File

SOURCE=.\filespy.rc
# End Source File
# Begin Source File

SOURCE=.\fspyCtx.c
# End Source File
# Begin Source File

SOURCE=.\fspyHash.c
# End Source File
# Begin Source File

SOURCE=.\fspyLib.c
# End Source File
# Begin Source File

SOURCE=.\RmtHookDevice.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\filespy.h
# End Source File
# Begin Source File

SOURCE=.\fspydef.h
# End Source File
# Begin Source File

SOURCE=.\fspyKern.h
# End Source File
# Begin Source File

SOURCE=.\RmtHookDevice.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\Filter.dsp
# End Source File
# Begin Source File

SOURCE=.\makefile
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
