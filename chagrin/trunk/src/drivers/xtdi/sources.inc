#TARGETNAME=libxtdi
#TARGETPATH=obj
#TARGETTYPE=DRIVER_LIBRARY
#BUILD_PRODUCES=libxtdi

INCLUDES=$(NDAS_INC_PATH)

C_DEFINES=$(C_DEFINES)

!IF $(FREEBUILD)
MSC_OPTIMIZATION=/Ox
LINK_TIME_CODE_GENERATION=1
!ELSE
!ENDIF

SOURCES=\
	..\xtdi.c \
	..\xtdilpx.c

PASS1_PUBLISH=\
	{$(O)\$(TARGETNAME).lib=$(NDAS_DRIVER_LIB_PATH)\$(TARGETNAME).lib} \
	{..\xtdi.h=$(NDAS_INC_PATH)\xtdi.h} \
	{..\xtdilpx.h=$(NDAS_INC_PATH)\xtdilpx.h}

!IFDEF USE_WPP

#
# WPP Settings
#
!IF $(_NT_TARGET_VERSION) <= 0x500
WPP_TEMPLATE=km-w2k.tpl
!ELSE
WPP_TEMPLATE=km-default.tpl
!ENDIF
RUN_WPP=$(SOURCES) -km -gen:{$(WPP_TEMPLATE)}*.tmh -func:xTdiTrace(FLAG,LEVEL,MSG,...)

!ENDIF
