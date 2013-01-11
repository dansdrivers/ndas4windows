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

!IF 0
TARGETNAME=ntfs
!ELSE
TARGETNAME=ndasntfs
!ENDIF
TARGETTYPE=DRIVER
DRIVERTYPE=FS

!IF 0
INCLUDES=..;$(PROJECT_ROOT)\ntos\inc;$(DRIVERS_INC_PATH)
!ENDIF

NTPROFILEINPUT=yes

!IF 0
C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

!ENDIF

MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

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

INCLUDES=$(INCLUDES);..
INCLUDES=$(INCLUDES);..\..\inc\wxp
INCLUDES=$(INCLUDES);$(NDAS_INC_PATH)
INCLUDES=$(INCLUDES);$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel
