# Microsoft Developer Studio Project File - Name="NDNetCompLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=NDNetCompLib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NDNetCompLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NDNetCompLib.mak" CFG="NDNetCompLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NDNetCompLib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "NDNetCompLib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "NDNetCompLib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f NDNetCompLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "NDNetCompLib.exe"
# PROP BASE Bsc_Name "NDNetCompLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\fre_w2k_x86\i386\NDNetComp.lib"
# PROP Bsc_Name "NDNetComp.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "NDNetCompLib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f NDNetCompLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "NDNetCompLib.exe"
# PROP BASE Bsc_Name "NDNetCompLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K chk ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\chk_w2k_x86\i386\NDNetComp.lib"
# PROP Bsc_Name "NDNetComp.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "NDNetCompLib - Win32 Release"
# Name "NDNetCompLib - Win32 Debug"

!IF  "$(CFG)" == "NDNetCompLib - Win32 Release"

!ELSEIF  "$(CFG)" == "NDNetCompLib - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\implinc.cpp
# End Source File
# Begin Source File

SOURCE=.\NDNetComp.cpp
# End Source File
# Begin Source File

SOURCE=.\snetcfg.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\NDNetComp.h
# End Source File
# Begin Source File

SOURCE=.\NDSetup.h
# End Source File
# Begin Source File

SOURCE=.\pch.h
# End Source File
# Begin Source File

SOURCE=.\snetcfg.h
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
