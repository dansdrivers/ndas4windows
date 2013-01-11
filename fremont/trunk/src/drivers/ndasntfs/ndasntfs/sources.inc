TARGETNAME=ndasntfs
TARGETTYPE=DRIVER
DRIVERTYPE=FS

INCLUDES=..;$(PROJECT_ROOT)\ntos\inc;$(DRIVERS_INC_PATH)

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

AMD64_WARNING_LEVEL=/W3 /Wp64 -wd4296 /WX
# -Wp64 will not work with ntddk.h for w2k,wxp headers
386_WARNING_LEVEL=/W3 /WX

MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX /W3

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

INCLUDES=$(INCLUDES);..\..\inc\wxp
INCLUDES=$(INCLUDES);$(NDAS_INC_PATH)
INCLUDES=$(INCLUDES);$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel
