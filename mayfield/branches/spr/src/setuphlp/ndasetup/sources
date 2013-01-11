TARGETNAME=ndasetup
TARGETTYPE=PROGRAM
TARGETPATH=obj

INCLUDES=$(PSDK_INC_PATH);$(NDAS_INC_PATH)

MSC_WARNING_LEVEL=-W3 -WX

UMTYPE=windows
UMENTRY=wwinmain
#DLLENTRY=WinMain

C_DEFINES=/DUNICODE /D_UNICODE /DNDASUSER_EXPORTS
RCOPTIONS=/i $(NDAS_INC_PATH)
USECXX_FLAG=/Tp /Wp64

!IF $(FREEBUILD)
#USE_MSVCRT=1
USE_LIBCMT=1
!ELSE
USE_LIBCMT=1
DEBUG_CRTS=1
!ENDIF

MSC_WARNING_LEVEL=-W3 -WX
#MSC_OPTIMIZATION=/Oitb2
#MSC_OPTIMIZATION=/Od /Oi

PRECOMPILED_INCLUDE=stdafx.h
PRECOMPILED_CXX=1

BUILD_CONSUMES=ndupdate

TARGETLIBS= \
	$(PSDK_LIB_PATH)\kernel32.lib \
	$(PSDK_LIB_PATH)\user32.lib \
	$(PSDK_LIB_PATH)\comctl32.lib \
	$(PSDK_LIB_PATH)\ole32.lib \
	$(PSDK_LIB_PATH)\oleaut32.lib \
	$(PSDK_LIB_PATH)\urlmon.lib \
	$(PSDK_LIB_PATH)\wininet.lib \
	$(PSDK_LIB_PATH)\version.lib \
	$(PSDK_LIB_PATH)\comdlg32.lib \
	$(NDAS_LIB_PATH)\ndupdate.lib \
	$(PSDK_LIB_PATH)\shell32.lib

X_USE_WTL_71=1
USE_STATIC_ATL=1
#USE_ATL=1
ATL_MIN_CRT=1
# Be sure to include "xmatl.mk" after TAGETLIBS,
# as xmatl.mk will provide more TAGETLIBS
!INCLUDE "xmatl.mk"

SOURCES= \
	downloadbsc.cpp \
	msiproc.cpp \
	ndasetup.cpp \
	setupdlg.cpp \
	setuptask.cpp \
	winutil.cpp \
	ndasetup.rc \
	ndasetup_ver.rc 
#	ndasetup.loc.chs.rc \
#	ndasetup.loc.deu.rc \
#	ndasetup.loc.esn.rc \
#	ndasetup.loc.fra.rc \
#	ndasetup.loc.ita.rc \
#	ndasetup.loc.jpn.rc \
#	ndasetup.loc.kor.rc \
#	ndasetup.loc.ptg.rc

POST_BUILD_CMD=copy /y ndasetup.ini $(O)\ndasetup.ini
BINPLACE_FLAGS=$(O)\ndasetup.ini
