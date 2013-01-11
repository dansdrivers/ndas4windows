
C_DEFINES=/DUNICODE /D_UNICODE
INCLUDES=..;$(NDAS_INC_PATH);$(NDAS_PUBLIC_INC_PATH);$(PSDK_INC_PATH)
RCOPTIONS=/i $(PROJECT_PATH)\inc /i $(NDAS_SDK_INC_PATH)
USECXX_FLAG=/Tp

!IF $(FREEBUILD)
USE_MSVCRT=1
MSC_OPTIMIZATION=/Ox
!ELSE
USE_LIBCMT=1
DEBUG_CRTS=1
!ENDIF

TARGETLIBS= \
	$(SDK_LIB_PATH)\kernel32.lib \
	$(SDK_LIB_PATH)\user32.lib \
	$(SDK_LIB_PATH)\setupapi.lib \
	$(SDK_LIB_PATH)\ws2_32.lib \
	$(NDAS_LIB_PATH)\liblpxtrans.lib

MSC_WARNING_LEVEL=-W3 -WX -Wp64

SOURCES=\
	..\task.cpp \
	..\ndasheartbeat.cpp \
	..\ndashear.cpp
