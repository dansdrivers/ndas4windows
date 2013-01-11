# Microsoft Developer Studio Project File - Name="Lpx" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=Lpx - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Lpx.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Lpx.mak" CFG="Lpx - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Lpx - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "Lpx - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "Lpx - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f Lpx.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Lpx.exe"
# PROP BASE Bsc_Name "Lpx.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K free ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "Lpx.exe"
# PROP Bsc_Name "Lpx.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Lpx - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f Lpx.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Lpx.exe"
# PROP BASE Bsc_Name "Lpx.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\..\bin\ddkbuild -WNET2K checked ."
# PROP Rebuild_Opt "-cZ"
# PROP Target_File "Lpx.exe"
# PROP Bsc_Name "Lpx.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "Lpx - Win32 Release"
# Name "Lpx - Win32 Debug"

!IF  "$(CFG)" == "Lpx - Win32 Release"

!ELSEIF  "$(CFG)" == "Lpx - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\action.c
# End Source File
# Begin Source File

SOURCE=.\address.c
# End Source File
# Begin Source File

SOURCE=.\autodial.c
# End Source File
# Begin Source File

SOURCE=.\connect.c
# End Source File
# Begin Source File

SOURCE=.\connobj.c
# End Source File
# Begin Source File

SOURCE=.\devctx.c
# End Source File
# Begin Source File

SOURCE=.\dlc.c
# End Source File
# Begin Source File

SOURCE=.\event.c
# End Source File
# Begin Source File

SOURCE=.\framecon.c
# End Source File
# Begin Source File

SOURCE=.\framesnd.c
# End Source File
# Begin Source File

SOURCE=.\iframes.c
# End Source File
# Begin Source File

SOURCE=.\info.c
# End Source File
# Begin Source File

SOURCE=.\link.c
# End Source File
# Begin Source File

SOURCE=.\linktree.c
# End Source File
# Begin Source File

SOURCE=.\Lpx.c
# End Source File
# Begin Source File

SOURCE=.\LpxPacket.c
# End Source File
# Begin Source File

SOURCE=.\nbf.rc
# End Source File
# Begin Source File

SOURCE=.\nbfcnfg.c
# End Source File
# Begin Source File

SOURCE=.\nbfdebug.c
# End Source File
# Begin Source File

SOURCE=.\nbfdrvr.c
# End Source File
# Begin Source File

SOURCE=.\nbflog.c
# End Source File
# Begin Source File

SOURCE=.\nbfmac.c
# End Source File
# Begin Source File

SOURCE=.\nbfndis.c
# End Source File
# Begin Source File

SOURCE=.\nbfpnp.c
# End Source File
# Begin Source File

SOURCE=.\packet.c
# End Source File
# Begin Source File

SOURCE=.\rcv.c
# End Source File
# Begin Source File

SOURCE=.\rcveng.c
# End Source File
# Begin Source File

SOURCE=.\request.c
# End Source File
# Begin Source File

SOURCE=.\send.c
# End Source File
# Begin Source File

SOURCE=.\sendeng.c
# End Source File
# Begin Source File

SOURCE=.\SocketNbf.c
# End Source File
# Begin Source File

SOURCE=.\spnlckdb.c
# End Source File
# Begin Source File

SOURCE=.\testnbf.c
# End Source File
# Begin Source File

SOURCE=.\testtdi.c
# End Source File
# Begin Source File

SOURCE=.\timer.c
# End Source File
# Begin Source File

SOURCE=.\uframes.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Lpx.h
# End Source File
# Begin Source File

SOURCE=.\LpxProc.h
# End Source File
# Begin Source File

SOURCE=.\nbf.h
# End Source File
# Begin Source File

SOURCE=.\nbfcnfg.h
# End Source File
# Begin Source File

SOURCE=.\nbfconst.h
# End Source File
# Begin Source File

SOURCE=.\nbfhdrs.h
# End Source File
# Begin Source File

SOURCE=.\nbfmac.h
# End Source File
# Begin Source File

SOURCE=.\nbfprocs.h
# End Source File
# Begin Source File

SOURCE=.\nbftypes.h
# End Source File
# Begin Source File

SOURCE=.\precomp.h
# End Source File
# Begin Source File

SOURCE=..\inc\SocketLpx.h
# End Source File
# Begin Source File

SOURCE=.\SocketNbfProc.h
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
# Begin Source File

SOURCE=..\WshLpx\wshlpx.def
# End Source File
# End Target
# End Project
