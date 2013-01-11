!IF 0

Supplemental declaration for WTL support

!ENDIF

!IFNDEF XM_VENDOR_PATH
XM_VENDOR_PATH=c:\devfiles
!ENDIF

!IFNDEF XM_VENDOR_PATH
!ERROR XM_VENDOR_PATH is not defined! Please define XM_VENDOR_PATH first
!ELSE
!IF [echo Declared XM_VENDOR_PATH=$(XM_VENDOR_PATH)]
!ENDIF
!ENDIF

#
# Hack: MFC_INCLUDES comes the very first include path 
#       with no other directive to modify than SDK_INC_PATH
MFC_INCLUDES=
# If you need MFC use MFC_INCLUDES=..\ext\mfc71\inc\mfc71\l.$(MFC_LANGUAGE)..\ext\mfc71\inc\mfc71;
#       
#
#! ifndef MFC_INCLUDES
#MFC_INCLUDES=$(SDK_INC_PATH)\mfc$(MFC_VER);$(MFC_DAO_INC)
#!  ifdef MFC_LANGUAGE
#MFC_INCLUDES=$(SDK_INC_PATH)\mfc$(MFC_VER)\l.$(MFC_LANGUAGE);$(MFC_INCLUDES)
#!  endif
#! endif

!IFDEF X_USE_WTL_71
USE_WTL=1
WTL_VER=71
WTL_INC_PATH=$(XM_VENDOR_PATH)\wtl71\inc

!IF [echo Declared WTL_INC_PATH=$(WTL_INC_PATH)]
!ENDIF

X_USE_ATL_71=1
!ENDIF

!IFDEF X_USE_ATL_71
ATL_VER=71
ATL_INC_PATH=$(XM_VENDOR_PATH)\atl71\inc
ATL_LIB_PATH=$(XM_VENDOR_PATH)\atl71\lib
!IF [echo Declared ATL_INC_PATH=$(ATL_INC_PATH)]
!ENDIF
!IF [echo Declared ATL_LIB_PATH=$(ATL_LIB_PATH)]
!ENDIF

!IFDEF USE_STATIC_ATL
!	IF "$(DDKBUILDENV)" == "chk"
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atlsd.lib
!	ELSE
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atls.lib
!	ENDIF
# Use of USE_STATIC_ATL requires atlmincrt.lib
ATL_MIN_CRT=1
!	IFDEF ATL_MIN_CRT
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atlmincrt.lib
!	ENDIF

!ELSE
!	IF [echo Using dynamic link library to ATL: ATL.LIB]
!	ENDIF
!ERROR Cannot support ATL Dynamic link library at this time.!
!ERROR Use USE_STATIC_ATL instead of USE_ATL
TARGETLIBS=$(TARGETLIBS) $(ATL_LIB_PATH)\atl.lib	
!ENDIF

!ENDIF



# Using mixing CRTs will emit warnings in linker
# Ignoring them here.
#
# atlsd.lib(XXX.obj) : warning LNK4231: /TMP incompatible with debugging
# information in 'XXX.obj'; delete and rebuild; linking object as if no debug info
#
LINK_LIB_IGNORE=$(LINK_LIB_IGNORE),4231
