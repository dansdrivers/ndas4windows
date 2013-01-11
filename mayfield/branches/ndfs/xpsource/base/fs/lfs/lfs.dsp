# Microsoft Developer Studio Project File - Name="lfs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=lfs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "lfs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lfs.mak" CFG="lfs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lfs - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "lfs - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "lfs - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f lfs.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "lfs.exe"
# PROP BASE Bsc_Name "lfs.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\..\bin\ddkbuild -WNETXP free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "lfs.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "lfs - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f lfs.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "lfs.exe"
# PROP BASE Bsc_Name "lfs.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\..\bin\ddkbuild -WNETXP checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "lfs.exe"
# PROP Bsc_Name "mp\lfs.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "lfs - Win32 Release"
# Name "lfs - Win32 Debug"

!IF  "$(CFG)" == "lfs - Win32 Release"

!ELSEIF  "$(CFG)" == "lfs - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\cachesup.c
# End Source File
# Begin Source File

SOURCE=.\lbcbsup.c
# End Source File
# Begin Source File

SOURCE=.\lfsdata.c
# End Source File
# Begin Source File

SOURCE=.\logpgsup.c
# End Source File
# Begin Source File

SOURCE=.\logrcsup.c
# End Source File
# Begin Source File

SOURCE=.\lsnsup.c
# End Source File
# Begin Source File

SOURCE=.\querylog.c
# End Source File
# Begin Source File

SOURCE=.\registry.c
# End Source File
# Begin Source File

SOURCE=.\restart.c
# End Source File
# Begin Source File

SOURCE=.\rstrtsup.c
# End Source File
# Begin Source File

SOURCE=.\strucsup.c
# End Source File
# Begin Source File

SOURCE=.\sysinit.c
# End Source File
# Begin Source File

SOURCE=.\verfysup.c
# End Source File
# Begin Source File

SOURCE=.\write.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\lfsdata.h
# End Source File
# Begin Source File

SOURCE=.\lfsdisk.h
# End Source File
# Begin Source File

SOURCE=.\lfsprocs.h
# End Source File
# Begin Source File

SOURCE=.\lfsstruc.h
# End Source File
# Begin Source File

SOURCE=.\nodetype.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\dirs
# End Source File
# Begin Source File

SOURCE=.\lfs.dsp
# End Source File
# Begin Source File

SOURCE=.\lfsques.txt
# End Source File
# Begin Source File

SOURCE=.\mp\sources
# End Source File
# Begin Source File

SOURCE=.\up\sources
# End Source File
# Begin Source File

SOURCE=.\sources.inc
# End Source File
# End Target
# End Project
