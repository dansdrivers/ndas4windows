# Microsoft Developer Studio Project File - Name="LanscsiBus" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=LanscsiBus - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LanscsiBus.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LanscsiBus.mak" CFG="LanscsiBus - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LanscsiBus - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "LanscsiBus - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "LanscsiBus - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f LanscsiBus.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LanscsiBus.exe"
# PROP BASE Bsc_Name "LanscsiBus.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "ndasbus.exe"
# PROP Bsc_Name "ndasbus.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "LanscsiBus - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f LanscsiBus.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LanscsiBus.exe"
# PROP BASE Bsc_Name "LanscsiBus.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "ndasbus.exe"
# PROP Bsc_Name "ndasbus.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "LanscsiBus - Win32 Release"
# Name "LanscsiBus - Win32 Debug"

!IF  "$(CFG)" == "LanscsiBus - Win32 Release"

!ELSEIF  "$(CFG)" == "LanscsiBus - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\busenum.c
# End Source File
# Begin Source File

SOURCE=.\buspdo.c
# End Source File
# Begin Source File

SOURCE=.\LanscsiBus.c
# End Source File
# Begin Source File

SOURCE=.\NdasComm.c
# End Source File
# Begin Source File

SOURCE=.\pnp.c
# End Source File
# Begin Source File

SOURCE=.\power.c
# End Source File
# Begin Source File

SOURCE=.\register.c
# End Source File
# Begin Source File

SOURCE=.\Wmi.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\busenum.h
# End Source File
# Begin Source File

SOURCE=.\driver.h
# End Source File
# Begin Source File

SOURCE=.\LanscsiBus.h
# End Source File
# Begin Source File

SOURCE=.\LanscsiBusProc.h
# End Source File
# Begin Source File

SOURCE=..\inc\lsbusioctl.h
# End Source File
# Begin Source File

SOURCE=..\inc\lurdesc.h
# End Source File
# Begin Source File

SOURCE=.\ver.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\busenum.bmf
# End Source File
# Begin Source File

SOURCE=..\NewLanscsiBus\busenum.mof
# End Source File
# Begin Source File

SOURCE=.\busenum.rc
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
