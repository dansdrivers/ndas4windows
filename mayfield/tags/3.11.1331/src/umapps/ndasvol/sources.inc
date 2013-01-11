#TARGETNAME=<should be set by sources>
#TARGETPATH=<should be set by sources>
#TARGETTYPE=<should be set by sources>

SOURCES=\
	..\devrel.cpp \
	..\cmquery.cpp \
	..\winioctlhelper.cpp \
	..\ndasvol.cpp

C_DEFINES=/DUNICODE /D_UNICODE /DNDASVOL_IMP
MSC_WARNING_LEVEL=-W4 -WX -wd4201 -wd4127

INCLUDES=..;$(PSDK_INC_PATH);$(NDAS_PUBLIC_INC_PATH);$(NDAS_INC_PATH)

PASS1_PUBLISH= {$(O)\$(TARGETNAME).lib=$(NDAS_LIB_PATH)\$(TARGETNAME).lib}

