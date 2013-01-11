!IF 0
-------------------------------------------------------------------------------------

Copyright (C) 2004 XIMETA, Inc.
All rights reserved.

-------------------------------------------------------------------------------------

Purpose:

Build makefile definitions for the project.

This project.mk is automatically included by build.exe (from DDK build)
when you run the build.exe in child directories.

This makefile provides some global variables, e.g.

- NDAS_INC_PATH
- NDAS_LIB_PATH
- NDAS_LIB_DEST
- NDAS_SRC_PATH
- NDAS_PUBLIC_INC_PATH
- NDAS_PUBLIC_LIB_PATH
- NDAS_PUBLIC_LIB_DEST

for every sub-project.
Also, it makes to run "binplace" after each build to place files
into the location defined in "placefil.txt".

Source tree root directory is declared as NDAS_SOURCE_TREE.
If you change the source tree root directory name, change this variable
to the appropriate value.

Revisions:

7/13/2004 cslee

  - Users can specify NDAS_SOURCE_TREE using an environmental variable.
  - Fixed bugs in the NDAS_SOURCE_TREE detection routine

6/15/2004 cslee - Initial implementation

-------------------------------------------------------------------------------------
!ENDIF

!if defined(BUILD_DEBUG)
!MESSAGE BUILDMSG: root\src\project.mk included
!endif

!ifdef XM_BUILD_VERBOSE
!message NDAS Project.mk included
!endif

!ifndef NDAS_DIR_TAG
NDAS_DIR_TAG=ndasdir.tag
!endif

!ifndef NDAS_ROOT
!if exists(.\$(NDAS_DIR_TAG))
NDAS_ROOT=.
!elseif exists(..\$(NDAS_DIR_TAG))
NDAS_ROOT=..
!elseif exists(..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..
!elseif exists(..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..
!elseif exists(..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..
!elseif exists(..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..
!elseif exists(..\..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\..\$(NDAS_DIR_TAG))
NDAS_ROOT=..\..\..\..\..\..\..\..\..\..
!else
!error NDAS Source Tree $(NDAS_DIR_TAG) cannot be found!
!endif
!endif

!if exists($(NDAS_ROOT)\src\config.inc)
!include "$(NDAS_ROOT)\src\config.inc"
!endif

PUBLISH_CMD=cscript.exe //nologo $(NDAS_ROOT)\bin\publish.js

!ifdef XM_BUILD_VERBOSE
!message BUILDMSG: NDAS_ROOT=$(NDAS_ROOT)
!endif

TARGET_PLATFORM_W2K=w2k
TARGET_PLATFORM_WXP=wxp
TARGET_PLATFORM_WNET=wnet

!if "$(DDK_TARGET_OS)" == "Win2K"
TARGET_PLATFORM=$(TARGET_PLATFORM_W2K)
!elseif "$(DDK_TARGET_OS)" == "WinXP"
TARGET_PLATFORM=$(TARGET_PLATFORM_WXP)
!elseif "$(DDK_TARGET_OS)" == "WinNET"
TARGET_PLATFORM=$(TARGET_PLATFORM_WNET)
!else
!	if [echo ERROR: Cannot recognize build target OS: DDK_TARGET_OS=$(DDK_TARGET_OS)]
!	endif
!endif


NDAS_TOOLS=$(NDAS_ROOT)\bin

# If you use * instead of $(TARGET_DIRECTORY),
# publish command won't expand it to i386, or amd64, etc.
# e.g. NDAS_LIB_PATH=$(NDAS_LIB_PATH)\$(BUILD_ALT_DIR)\*
#      -> * will not be expanded

NDAS_SRC_PATH=$(NDAS_ROOT)\src
NDAS_INC_PATH=$(NDAS_SRC_PATH)\inc
NDAS_PUBLIC_INC_PATH=$(NDAS_SRC_PATH)\inc\public

NDAS_LIB_DEST=$(NDAS_ROOT)\lib\$(DDKBUILDENV)
NDAS_LIB_PATH=$(NDAS_LIB_DEST)\$(TARGET_DIRECTORY)

NDAS_PUBLIC_LIB_DEST=$(NDAS_ROOT)\lib\public\$(DDKBUILDENV)
NDAS_PUBLIC_LIB_PATH=$(NDAS_PUBLIC_LIB_DEST)\$(TARGET_DIRECTORY)

NDAS_DRIVER_LIB_DEST=$(NDAS_ROOT)\lib\$(DDKBUILDENV)\kernel
NDAS_DRIVER_LIB_DEST_PLT=$(NDAS_DRIVER_LIB_DEST)\$(TARGET_PLATFORM)\$(TARGET_DIRECTORY)
NDAS_DRIVER_LIB_PATH=$(NDAS_DRIVER_LIB_DEST)\$(TARGET_DIRECTORY)

NDAS_USER_LIB_DEST=$(NDAS_LIB_DEST)
NDAS_USER_LIB_PATH=$(NDAS_DRIVER_LIB_DEST)\$(TARGET_DIRECTORY)

NDAS_DRIVER_W2K_LIB_PATH=$(NDAS_DRIVER_LIB_DEST)\$(DDKBUILDENV)\$(TARGET_PLATFORM_W2K)\$(TARGET_DIRECTORY)
NDAS_DRIVER_WXP_LIB_PATH=$(NDAS_DRIVER_LIB_DEST)\$(DDKBUILDENV)\$(TARGET_PLATFORM_WXP)\$(TARGET_DIRECTORY)
NDAS_DRIVER_WNET_LIB_PATH=$(NDAS_DRIVER_LIB_DEST)\$(DDKBUILDENV)\$(TARGET_PLATFORM_WNET)\$(TARGET_DIRECTORY)

GLOBAL_PROJECT_ROOT=$(NDAS_SRC_PATH)

# we are using binplace!
!UNDEF NO_BINPLACE
#!UNDEF NTDBGFILES
# we don't have symchk.exe
BUILD_NO_SYMCHK=1
#USE_MAPSYM=1

BINPLACE_PLACEFILE=$(NDAS_ROOT)\placefil.txt
!IF "$(DDKBUILDENV)" == "fre"
_NTTREE=$(NDAS_ROOT)\publish\fre\$(TARGET_DIRECTORY)
!ELSE
_NTTREE=$(NDAS_ROOT)\publish\chk\$(TARGET_DIRECTORY)
!ENDIF

#
#!INCLUDE "$(NDAS_INC_PATH)\oem.mk"
#

!IFDEF OFFICIAL_BUILD
!IFNDEF __BUILDMACHINE__
__BUILDMACHINE__=$(COMPUTERNAME)
!ENDIF
!ELSE
__BUILDMACHINE__=""
!ENDIF

# Followings will be placed later
!IF 0
!IF $(FREEBUILD)
NTDEBUG=ntsdnodbg
NTDEBUGTYPE=both
USE_PDB=1
!ELSE
NTDEBUG=ntsd
NTDEBUGTYPE=both
USE_PDB=1
!ENDIF
!ENDIF

#!IFNDEF COFFBASE_TXT_FILE
COFFBASE_TXT_FILE=$(NDAS_SRC_PATH)\coffbase.txt
#!ENDIF

#
# ATL and WTL version
#
ATL_VER=71
WTL_VER=71

!if "$(_BUILDARCH)" == "AMD64"

ATL_INC_PATH=$(XM_VENDOR_PATH)\atl71\amd64\inc
ATL_LIB_PATH=$(XM_VENDOR_PATH)\atl71\amd64\lib
ATL_INC_ROOT=$(ATL_INC_PATH)
WTL_INC_PATH=$(XM_VENDOR_PATH)\wtl71\inc

!else

ATL_INC_PATH=$(XM_VENDOR_PATH)\atl71\inc
ATL_LIB_PATH=$(XM_VENDOR_PATH)\atl71\lib
ATL_INC_ROOT=$(ATL_INC_PATH)
WTL_INC_PATH=$(XM_VENDOR_PATH)\wtl71\inc

!endif

!ifndef PSDK_INC_PATH
!message BUILDMSG: Warning! PSDK_INC_PATH is not defined. Set PSDK_INC_PATH or declare in config.inc
!endif

!ifndef PSDK_LIB_PATH
!message BUILDMSG: Warning! PSDK_LIB_PATH is not defined. Set PSDK_LIB_PATH or declare in config.inc
!endif

!if "$(_BUILDARCH)" == "AMD64"
PSDK_LIB_PATH=$(PSDK_LIB_PATH)\AMD64
!endif

!ifndef XM_VENDOR_PATH
!message BUILDMSG: Warning! XM_VENDOR_PATH is not defined. Set XM_VENDOR_PATH or declare in config.inc
!endif

!  ifdef XM_BUILD_VERBOSE
!    message BUILDMSG: PSDK_INC_PATH=$(PSDK_INC_PATH)
!    message BUILDMSG: PSDK_LIB_PATH=$(PSDK_LIB_PATH)
!    message BUILDMSG: XM_VENDOR_PATH=$(XM_VENDOR_PATH)
!  endif
