TARGETNAME=lfs
TARGETPATH=obj
TARGETTYPE=LIBRARY

INCLUDES=..;..\..\..\ntos\inc;$(IFSKIT_INC_PATH)

C_DEFINES=$(C_DEFINES) -D_NTDRIVER_

!IFDEF BUILD_FOR_3_51
C_DEFINES= $(C_DEFINES) -D_NTIFS_
!ENDIF

MSC_WARNING_LEVEL=/W3 /WX

SOURCES=..\CacheSup.c  \
        ..\LbcbSup.c   \
        ..\LfsData.c   \
        ..\LogPgSup.c  \
        ..\LogRcSup.c  \
        ..\LsnSup.c    \
        ..\QueryLog.c  \
        ..\Registry.c  \
        ..\Restart.c   \
        ..\RstrtSup.c  \
        ..\StrucSup.c  \
        ..\SysInit.c   \
        ..\VerfySup.c  \
        ..\Write.c

PRECOMPILED_INCLUDE=..\lfsprocs.h
PRECOMPILED_PCH=lfsprocs.pch
PRECOMPILED_OBJ=lfsprocs.obj

DRIVERTYPE=FS

INCLUDES=$(INCLUDES);..
INCLUDES=$(INCLUDES);..\..\inc\wxp

