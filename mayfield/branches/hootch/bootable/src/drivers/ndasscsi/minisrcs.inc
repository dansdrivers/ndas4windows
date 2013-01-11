# We do not support interrupt after NDASDRV 3.07 version.
# INTERRUPT_SUPPORT1=interruptsupport
COMPLETION=completion

!IFDEF COMPLETION
C_DEFINES=-D__COMPLETION__
!ENDIF

INCLUDES=$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel;$(NDAS_INC_PATH)
RCOPTIONS=/i $(NDAS_INC_PATH)

#PRECOMPILED_INCLUDE=port.h
MSC_WARNING_LEVEL=-W3 -WX -Wp64 

!IF $(FREEBUILD)
MSC_OPTIMIZATION=/O2
!ELSE
!ENDIF

SOURCES=ndasscsi.c \
		ndsccomp.c \
		ndscioctl.c \
        ndasscsi.rc

!IF $(ALPHA) || $(AXP64) || $(IA64)
TARGETLIBS=$(TARGETLIBS) $(CRT_LIB_PATH)\libcntpr.lib
!ENDIF

#C_DEFINES=$(C_DEFINES) -D__NDFS__

