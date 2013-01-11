!IF 0

Copyright (c) 1989  Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.

!ENDIF

NT_UP=0

TARGETNAME=ntfs
TARGETTYPE=DRIVER
DRIVERTYPE=FS


#NTPROFILEINPUT=yes

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=..\AllocSup.c \
        ..\AttrData.c \
        ..\AttrSup.c  \
        ..\BitmpSup.c \
        ..\CacheSup.c \
        ..\CheckSup.c \
        ..\Cleanup.c  \
        ..\Close.c    \
        ..\ColatSup.c \
        ..\Create.c   \
        ..\DevCtrl.c  \
        ..\DevIoSup.c \
        ..\DirCtrl.c  \
        ..\Ea.c       \
        ..\FileInfo.c \
        ..\FilObSup.c \
        ..\Flush.c    \
        ..\FsCtrl.c   \
        ..\FspDisp.c  \
        ..\FstIoSup.c \
        ..\HashSup.c  \
        ..\IndexSup.c \
        ..\LockCtrl.c \
        ..\LogSup.c   \
        ..\McbSup.c   \
        ..\MftSup.c   \
        ..\NameSup.c  \
        ..\Ntfs.rc    \
        ..\NtfsData.c \
        ..\NtfsExp.c  \
        ..\NtfsInit.c \
        ..\ObjIdSup.c \
        ..\Pnp.c      \
        ..\PrefxSup.c \
        ..\Quota.c    \
        ..\QuotaSup.c \
        ..\Read.c     \
        ..\ReparSup.c \
        ..\ResrcSup.c \
        ..\RestrSup.c \
        ..\SecurSup.c \
        ..\SeInfo.c   \
        ..\Shutdown.c \
        ..\StrucSup.c \
        ..\UsnSup.c   \
        ..\VattrSup.c \
        ..\VerfySup.c \
        ..\ViewSup.c  \
        ..\VolInfo.c  \
        ..\WorkQue.c  \
        ..\Write.c

PRECOMPILED_INCLUDE=..\ntfsproc.h
PRECOMPILED_PCH=ntfsproc.pch
PRECOMPILED_OBJ=ntfsproc.obj

#INCLUDES=$(INCLUDES);..;$(PROJECT_ROOT)\ntos\inc;$(DRIVERS_INC_PATH)
#INCLUDES=$(INCLUDES);$(XP_SOURCE_ROOT)\base\ntos\inc
#INCLUDES=$(INCLUDES);$(XP_SOURCE_ROOT)\public\sdk\inc
#INCLUDES=$(INCLUDES);$(XP_SOURCE_ROOT)\public\internal\base\inc
#INCLUDES=$(INCLUDES);$(XP_SOURCE_ROOT)\public\internal\drivers\inc


INCLUDES=$(INCLUDES);..;$(IFSKIT_INC_PATH)
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\base\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\base\ntos\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\public\sdk\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\public\internal\base\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\public\internal\net\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\public\internal\sdktools\inc
INCLUDES=$(INCLUDES);$(XM_WXP_INC_PATH)\public\internal\drivers\inc

INCLUDES=$(INCLUDES);$(NDAS_INC_PATH)
INCLUDES=$(INCLUDES);$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel
