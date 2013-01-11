# Microsoft Developer Studio Project File - Name="frhed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=frhed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "frhed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "frhed.mak" CFG="frhed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "frhed - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "frhed - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "frhed"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "frhed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../.."
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Od /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /FR /Yu"precomp.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 version.lib comctl32.lib wininet.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib shlwapi.lib WS2_32.lib ndascomm.lib /nologo /subsystem:windows /machine:I386 /out:"Release/frhed.exe" /libpath:"..\..\..\..\lib\chk\i386" /ALIGN:16384
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "frhed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FR /Yu"precomp.h" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 imagehlp.lib version.lib comctl32.lib wininet.lib shlwapi.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib WS2_32.lib ndascomm.lib /nologo /subsystem:windows /incremental:no /debug /machine:I386 /pdbtype:sept /libpath:"..\..\..\..\lib\chk\i386"
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "frhed - Win32 Release"
# Name "frhed - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\diagbox.cpp
# End Source File
# Begin Source File

SOURCE=.\gktools.cpp
# End Source File
# Begin Source File

SOURCE=.\gtools.cpp
# End Source File
# Begin Source File

SOURCE=.\hexwnd.cpp
# End Source File
# Begin Source File

SOURCE=.\ido.cpp
# End Source File
# Begin Source File

SOURCE=.\ids.cpp
# End Source File
# Begin Source File

SOURCE=.\idt.cpp
# End Source File
# Begin Source File

SOURCE=.\InvokeHtmlHelp.cpp
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\PDrive95.cpp
# End Source File
# Begin Source File

SOURCE=.\PDriveNDAS.cpp
# End Source File
# Begin Source File

SOURCE=.\PDriveNT.cpp
# End Source File
# Begin Source File

SOURCE=.\PhysicalDrive.cpp
# End Source File
# Begin Source File

SOURCE=.\PMemoryBlock.cpp
# End Source File
# Begin Source File

SOURCE=.\precomp.cpp
# ADD CPP /Yc"precomp.h"
# End Source File
# Begin Source File

SOURCE=.\toolbar.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BinTrans.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\BinTrans.h
# End Source File
# Begin Source File

SOURCE=.\compat.h
# End Source File
# Begin Source File

SOURCE=.\gktools.h
# End Source File
# Begin Source File

SOURCE=.\gtools.h
# End Source File
# Begin Source File

SOURCE=.\hexwnd.h
# End Source File
# Begin Source File

SOURCE=.\ido.h
# End Source File
# Begin Source File

SOURCE=.\ids.h
# End Source File
# Begin Source File

SOURCE=.\idt.h
# End Source File
# Begin Source File

SOURCE=.\ntdiskspec.h
# End Source File
# Begin Source File

SOURCE=.\PDrive95.h
# End Source File
# Begin Source File

SOURCE=.\PDriveNDAS.h
# End Source File
# Begin Source File

SOURCE=.\PDriveNT.h
# End Source File
# Begin Source File

SOURCE=.\PhysicalDrive.h
# End Source File
# Begin Source File

SOURCE=.\PMemoryBlock.h
# End Source File
# Begin Source File

SOURCE=.\precomp.h
# End Source File
# Begin Source File

SOURCE=..\RAWIO32\RAWIO32.h
# End Source File
# Begin Source File

SOURCE=.\Simparr.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\Simparr.h
# End Source File
# Begin Source File

SOURCE=.\toolbar.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\Script1.rc
# End Source File
# Begin Source File

SOURCE=.\Toolbar.bmp
# End Source File
# End Group
# Begin Group "Text Files"

# PROP Default_Filter "txt"
# Begin Source File

SOURCE=.\Bugs.txt
# End Source File
# Begin Source File

SOURCE=.\History.txt
# End Source File
# Begin Source File

SOURCE=.\Readme.txt
# End Source File
# Begin Source File

SOURCE=.\Todo.txt
# End Source File
# End Group
# End Target
# End Project
