

!IF 0

Copyright (c) 1989  Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.


Author:

    Steve Wood (stevewo) 12-Apr-1990

NOTE:   Commented description of this file is in $(BASEDIR)\bak\bin\sources.tpl

!ENDIF

TARGETTYPE=DRIVER

!IFDEF SOCKETNBF
TARGETLIBS=$(DDK_LIB_PATH)\tdi.lib \
           $(DDK_LIB_PATH)\ndis.lib
!ELSE
TARGETLIBS=$(BASEDIR)\public\sdk\lib\*\tdi.lib \
           $(BASEDIR)\public\sdk\lib\*\ndis.lib

!ENDIF

!IFDEF SOCKETNBF
C_DEFINES = -D__SOCKET_NBF__ -DNDIS40 -D_PNP_POWER_ -DNDIS_POOL_TAGGING=1 -DPKT_LOG=0
!IFDEF LPX
C_DEFINES = -D__LPX__ -D__VERSION_CONTROL__ $(C_DEFINES)
!ENDIF
!ELSE
C_DEFINES = -DRASAUTODIAL -DNDIS40 -D_PNP_POWER_ -DNDIS_POOL_TAGGING=1 -DPKT_LOG=0
!ENDIF

!IFDEF XP
C_DEFINES = -D__XP__ $(C_DEFINES)
!ENDIF

MSC_WARNING_LEVEL=/W2 /WX

INCLUDES=..

SOURCES=..\action.c \
        ..\address.c \
        ..\autodial.c \
        ..\connect.c \
        ..\connobj.c \
        ..\devctx.c \
        ..\dlc.c \
        ..\event.c \
        ..\framecon.c \
        ..\framesnd.c \
        ..\iframes.c \
        ..\info.c \
        ..\link.c \
        ..\linktree.c \
        ..\nbf.rc \
        ..\nbfcnfg.c \
        ..\nbfdrvr.c \
        ..\nbfdebug.c \
        ..\nbfmac.c \
        ..\nbfndis.c \
        ..\nbfpnp.c    \
        ..\packet.c \
        ..\rcv.c \
        ..\rcveng.c \
        ..\request.c \
        ..\send.c \
        ..\sendeng.c \
        ..\spnlckdb.c \
        ..\timer.c \
        ..\uframes.c \
		    ..\nbflog.c \
		    ..\SocketNbf.c \
		    ..\Lpx.c \
		    ..\Lpxpacket.c

PRECOMPILED_INCLUDE=..\precomp.h
PRECOMPILED_PCH=precomp.pch
PRECOMPILED_OBJ=precomp.obj

!IFNDEF 386_WARNING_LEVEL
386_WARNING_LEVEL=/W3
!ENDIF

