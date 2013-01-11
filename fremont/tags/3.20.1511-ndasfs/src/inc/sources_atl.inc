
# opt out MFC_INCLUDES to offset conflicts between atl71 and mfc42
MFC_INCLUDES=

INCLUDES=$(INCLUDES);$(ATL71_INC_PATH)\atl71

# By default we use only _ATL_STATIC_REGISTRY settings
# 1. define _ATL_STATIC_REGISTRY 
# 2. link against atls.lib or atlsd.lib
C_DEFINES=$(C_DEFINES) /D_ATL_STATIC_REGISTRY
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atls$(DCRT).lib

# atl71 requires VC7 symbols
VC7_SYMBOLS=1
