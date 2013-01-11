# Microsoft Developer Studio Project File - Name="NDDeviceLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=NDDeviceLib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NDDeviceLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NDDeviceLib.mak" CFG="NDDeviceLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NDDeviceLib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "NDDeviceLib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "NDDeviceLib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line ""
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "install.exe"
# PROP BASE Bsc_Name "install.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\fre_w2k_x86\i386\NDDevice.lib"
# PROP Bsc_Name "NDDevice.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "NDDeviceLib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f FiltInstallLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "install.exe"
# PROP BASE Bsc_Name "install.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\chk_w2k_x86\i386\NDDevice.lib"
# PROP Bsc_Name "NDDevice.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "NDDeviceLib - Win32 Release"
# Name "NDDeviceLib - Win32 Debug"

!IF  "$(CFG)" == "NDDeviceLib - Win32 Release"

!ELSEIF  "$(CFG)" == "NDDeviceLib - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\NDDevice.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\NDDevice.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\MAKEFILE
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
