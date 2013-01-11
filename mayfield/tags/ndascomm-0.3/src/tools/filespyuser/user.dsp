# Microsoft Developer Studio Project File - Name="User" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=User - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "User.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "User.mak" CFG="User - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "User - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "User - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "User - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f User.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "User.exe"
# PROP BASE Bsc_Name "User.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f "User.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "User.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "User - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f User.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "User.exe"
# PROP BASE Bsc_Name "User.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\bin\ddkbuild -WNETXP checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "User.exe"
# PROP Bsc_Name "PurgeFilterUser.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "User - Win32 Release"
# Name "User - Win32 Debug"

!IF  "$(CFG)" == "User - Win32 Release"

!ELSEIF  "$(CFG)" == "User - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\fspyLog.c
# End Source File
# Begin Source File

SOURCE=.\fspyUser.c
# End Source File
# Begin Source File

SOURCE=.\fspyUser.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\fspyLog.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\makefile
# End Source File
# Begin Source File

SOURCE=..\Inf\PurgeFilter.inf
# End Source File
# Begin Source File

SOURCE=..\Inf\PurgeFilterInstall.cmd
# End Source File
# Begin Source File

SOURCE=..\Inf\PurgeFilterUninstall.cmd
# End Source File
# Begin Source File

SOURCE=..\Inf\PurgeFilterW2k.inf
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# Begin Source File

SOURCE=..\Inf\W2kPurgeFilterInstall.cmd
# End Source File
# Begin Source File

SOURCE=..\Inf\W2kPurgeFilterUninstall.cmd
# End Source File
# End Target
# End Project
