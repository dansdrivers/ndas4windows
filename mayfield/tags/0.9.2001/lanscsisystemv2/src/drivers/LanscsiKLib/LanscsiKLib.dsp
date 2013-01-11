# Microsoft Developer Studio Project File - Name="LanscsiKLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=LanscsiKLib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LanscsiKLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LanscsiKLib.mak" CFG="LanscsiKLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LanscsiKLib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "LanscsiKLib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "LanscsiKLib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f LanscsiKLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LanscsiKLib.exe"
# PROP BASE Bsc_Name "LanscsiKLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LanscsiKLib.exe"
# PROP Bsc_Name "LanscsiKLib.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "LanscsiKLib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f LanscsiKLib.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LanscsiKLib.exe"
# PROP BASE Bsc_Name "LanscsiKLib.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LanscsiKLib.exe"
# PROP Bsc_Name "LanscsiKLib.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "LanscsiKLib - Win32 Release"
# Name "LanscsiKLib - Win32 Debug"

!IF  "$(CFG)" == "LanscsiKLib - Win32 Release"

!ELSEIF  "$(CFG)" == "LanscsiKLib - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\KDebug.c
# End Source File
# Begin Source File

SOURCE=.\LSCcb.c
# End Source File
# Begin Source File

SOURCE=.\LSLurn.c
# End Source File
# Begin Source File

SOURCE=.\LSLurnAssoc.c
# End Source File
# Begin Source File

SOURCE=.\LSLurnIde.c
# End Source File
# Begin Source File

SOURCE=.\LSProto.c
# End Source File
# Begin Source File

SOURCE=.\LSProtoIde.c
# End Source File
# Begin Source File

SOURCE=.\LSTransport.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\inc\kernel\KDebug.h
# End Source File
# Begin Source File

SOURCE=..\inc\LanScsi.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSCcb.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSKLib.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSLurn.h
# End Source File
# Begin Source File

SOURCE=.\LSLurnAssoc.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSLurnIDE.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSProto.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSProtoIde.h
# End Source File
# Begin Source File

SOURCE=..\inc\LSProtoIdeSpec.h
# End Source File
# Begin Source File

SOURCE=..\inc\LSProtoSpec.h
# End Source File
# Begin Source File

SOURCE=..\inc\kernel\LSTransport.h
# End Source File
# Begin Source File

SOURCE=.\ver.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
