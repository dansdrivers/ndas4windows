# Microsoft Developer Studio Project File - Name="NDFilterLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=NDFilterLib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NDFilterLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NDFilterLib.mak" CFG="NDFilterLib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NDFilterLib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "NDFilterLib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "NDFilterLib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "objfre_w2k_x86\i386"
# PROP BASE Intermediate_Dir "objfre_w2k_x86\i386"
# PROP BASE Cmd_Line "NMAKE /f NDFilterLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "NDFilterLib.exe"
# PROP BASE Bsc_Name "NDFilterLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "objfre_w2k_x86\i386"
# PROP Intermediate_Dir "objfre_w2k_x86\i386"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\fre_w2k_x86\i386\NDFilter.lib"
# PROP Bsc_Name "NDFilter.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "NDFilterLib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "objchk_w2k_x86\i386"
# PROP BASE Intermediate_Dir "objchk_w2k_x86\i386"
# PROP BASE Cmd_Line "NMAKE /f NDFilterLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "NDFilterLib.exe"
# PROP BASE Bsc_Name "NDFilterLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "objchk_w2k_x86\i386"
# PROP Intermediate_Dir "objchk_w2k_x86\i386"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "..\..\lib\chk_w2k_x86\i386\NDFilter.lib"
# PROP Bsc_Name "NDFilter.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "NDFilterLib - Win32 Release"
# Name "NDFilterLib - Win32 Debug"

!IF  "$(CFG)" == "NDFilterLib - Win32 Release"

!ELSEIF  "$(CFG)" == "NDFilterLib - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\NDFilter.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\inc\NDFilter.h
# End Source File
# Begin Source File

SOURCE=..\inc\ROIOCTLCMD.H
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
