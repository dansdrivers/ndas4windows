
BROWSER_INFO=1
BROWSERFILE=$(TARGETNAME).bsc -n

!IFDEF INTERRUPT_SUPPORT
TARGETLIBS=$(DDK_LIB_PATH)\scsiport.lib

!ELSE
TARGETLIBS=$(DDK_LIB_PATH)\scsiport.lib \
           $(DDK_LIB_PATH)\scsiwmi.lib \
		   $(DDK_LIB_PATH)\ntoskrnl.lib \
		   $(DDK_LIB_PATH)\hal.lib

!ENDIF


!IF $(ALPHA) || $(AXP64) || $(IA64)
TARGETLIBS=$(TARGETLIBS) $(CRT_LIB_PATH)\libcntpr.lib
!ENDIF

!IFDEF INTERRUPT_SUPPORT

C_DEFINES=-DSCSIDBG_ENABLED=DBG -DUSE_RTL_BITMAP=1 -DDISABLE_ADAPTER_POWER=0 -DBUGBUG
C_DEFINES = -D__INTERRUPT__ $(C_DEFINES) 

!ELSE

!IFDEF COMPLETION
C_DEFINES = -D__COMPLETION__
!ENDIF

!ENDIF

INCLUDE=..

SOURCES=..\control.c  \
        ..\dispatch.c \
        ..\enable.c   \
        ..\enum.c     \
        ..\init.c     \
        ..\internal.c \
        ..\lock.c     \
        ..\map.c      \
        ..\pdo.c      \
        ..\pnp.c      \
        ..\pnpstop.c  \
        ..\port.c     \
        ..\power.c    \
        ..\prop.c     \
        ..\remove.c   \
        ..\utils.c    \
        ..\wmi.c      \
	..\lanscsiminiport.c	\
	..\Communication.c		\
	..\scsiport.rc			

PRECOMPILED_INCLUDE=..\port.h
PRECOMPILED_PCH=portpch.pch
PRECOMPILED_OBJ=portpch.obj
				
