!IFDEF USING_WDK
MFC_INCLUDES=
INCLUDES=$(INCLUDES);$(ATL_INC_ROOT)\crt\atl71
!ELSE
# opt out MFC_INCLUDES to offset conflicts between atl71 and mfc42
MFC_INCLUDES=
INCLUDES=$(INCLUDES);$(ATL71_INC_PATH)\atl71
!ENDIF

# By default we use only _ATL_STATIC_REGISTRY settings
# 1. define _ATL_STATIC_REGISTRY 
# 2. link against atls.lib or atlsd.lib
C_DEFINES=$(C_DEFINES) /D_ATL_STATIC_REGISTRY

!  ifdef DEBUG_CRTS
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atlsd.lib
!  else
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atls.lib
!  endif

!  if !defined(BUFFER_OVERFLOW_CHECKS)
BUFFER_OVERFLOW_CHECKS=1
!  endif

# atl71 requires VC7 symbols
VC7_SYMBOLS=1

!IFDEF USING_WDK
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atlthunk.lib
!ENDIF
