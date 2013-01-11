# Microsoft Developer Studio Project File - Name="LfsFilter" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=LfsFilter - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LfsFilter.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LfsFilter.mak" CFG="LfsFilter - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LfsFilter - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "LfsFilter - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "LfsFilter - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f LfsFilter.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LfsFilter.exe"
# PROP BASE Bsc_Name "LfsFilter.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNETXP free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LfsFilt.exe"
# PROP Bsc_Name "LfsFilt.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "LfsFilter - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f LfsFilter.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LfsFilter.exe"
# PROP BASE Bsc_Name "LfsFilter.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNETXP checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "LfsFilt.exe"
# PROP Bsc_Name "LfsFilt.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "LfsFilter - Win32 Release"
# Name "LfsFilter - Win32 Debug"

!IF  "$(CFG)" == "LfsFilter - Win32 Release"

!ELSEIF  "$(CFG)" == "LfsFilter - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FastIoDispatch.c
# End Source File
# Begin Source File

SOURCE=.\filespy.c
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

SOURCE=.\Lfs.c
# End Source File
# Begin Source File

SOURCE=.\LfsDbg.c
# End Source File
# Begin Source File

SOURCE=.\LfsDGSvrCli.c
# End Source File
# Begin Source File

SOURCE=.\LfsLib.c
# End Source File
# Begin Source File

SOURCE=.\LfsProto.c
# End Source File
# Begin Source File

SOURCE=.\LfsTable.c
# End Source File
# Begin Source File

SOURCE=.\md5.c
# End Source File
# Begin Source File

SOURCE=.\MemoryMap.c
# End Source File
# Begin Source File

SOURCE=.\NetdiskManager.c
# End Source File
# Begin Source File

SOURCE=.\Primary.c
# End Source File
# Begin Source File

SOURCE=.\PrimarySession.c
# End Source File
# Begin Source File

SOURCE=.\ReadOnly.c
# End Source File
# Begin Source File

SOURCE=.\Secondary.c
# End Source File
# Begin Source File

SOURCE=.\SecondaryRedirectIrp.c
# End Source File
# Begin Source File

SOURCE=.\SecondaryThread.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CheckUp.h
# End Source File
# Begin Source File

SOURCE=.\FastIoDispatch.h
# End Source File
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

SOURCE=..\inc\driver\LanscsiBus.h
# End Source File
# Begin Source File

SOURCE=.\Lfs.h
# End Source File
# Begin Source File

SOURCE=..\inc\driver\LfsCtl.h
# End Source File
# Begin Source File

SOURCE=.\LfsDbg.h
# End Source File
# Begin Source File

SOURCE=.\LfsDGSvrCli.h
# End Source File
# Begin Source File

SOURCE=..\inc\LfsFilterPublic.h
# End Source File
# Begin Source File

SOURCE=.\LfsMessageHeader.h
# End Source File
# Begin Source File

SOURCE=.\LfsProc.h
# End Source File
# Begin Source File

SOURCE=.\LfsSystemProc.h
# End Source File
# Begin Source File

SOURCE=.\LfsTable.h
# End Source File
# Begin Source File

SOURCE=.\md5.h
# End Source File
# Begin Source File

SOURCE=.\NdfsProtocolHeader.h
# End Source File
# Begin Source File

SOURCE=.\NdftProtocolHeader.h
# End Source File
# Begin Source File

SOURCE=.\NetdiskManager.h
# End Source File
# Begin Source File

SOURCE=.\Primary.h
# End Source File
# Begin Source File

SOURCE=.\Secondary.h
# End Source File
# Begin Source File

SOURCE=.\Table.h
# End Source File
# Begin Source File

SOURCE=.\Win2kHeader.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# End Group
# Begin Source File

SOURCE=.\filespy.rc
# End Source File
# Begin Source File

SOURCE=.\makefile
# End Source File
# Begin Source File

SOURCE=.\print.txt
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
