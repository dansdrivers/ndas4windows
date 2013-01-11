!if defined(BUILD_DEBUG)
!MESSAGE BUILDMSG: $(GLOBAL_PROJECT_ROOT)\project_mk.inc included
!endif

!IFNDEF XM_VENDOR_PATH
!ERROR XM_VENDOR_PATH is not defined! Please define XM_VENDOR_PATH first
!ELSE
!if defined(BUILD_DEBUG)
!MESSAGE BUILDMSG: XM_VENDOR_PATH=$(XM_VENDOR_PATH)
!endif
!ENDIF

# ------------------------------------------------------------
# PREPROCESSING
# ------------------------------------------------------------

!IF "$(USE_ATL: =)" == "1" || "$(USE_STATIC_ATL: =)" == "1"
!IF "$(ATL_VER: =)" == "71"
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

VC7_SYMBOLS=1

!ENDIF
!ENDIF

#------------------------------------------------------------
#++ PREPROCESSING
#------------------------------------------------------------

#------------------------------------------------------------
#-- END OF PRE-PROCESSING
#------------------------------------------------------------

# include actual ddk i386mk.inc
!INCLUDE $(NTMAKEENV)\$(TARGET_DIRECTORY)mk.inc

#!message BUILDMSG: INCPATH0=$(INCPATH0)
#!message BUILDMSG: INCPATH1=$(INCPATH1)


#------------------------------------------------------------
#++ POST-PROCESSING
#------------------------------------------------------------


!IF "$(USE_ATL: =)" == "1" || "$(USE_STATIC_ATL: =)" == "1"
!IF "$(ATL_VER: =)" == "71"

# We will redefine ADDITIONAL_ATL_LIBS as empty to
# remove atlthunk.lib from the libs
# atlthunk.lib is not required for ATL 7.1 and we do not have it.
# remove atlthunk.lib
ADDITIONAL_ATL_LIBS=

# ATL 7.1 DLL support is not available
!ifdef USE_ATL
!undef USE_ATL
!message BUILDMSG: WARNING: use USE_STATIC_ATL instead of USE_ATL
USE_STATIC_ATL=1
!endif

! ifdef USE_STATIC_ATL
!  ifdef DEBUG_CRTS
ATL_DEFINES=/D_ATL_STATIC_REGISTRY
ATL_LIBS=$(ATL_LIB_PATH)\atlsd.lib
!  else
ATL_LIBS=$(ATL_LIB_PATH)\atls.lib
!  endif
! else
ATL_DEFINES=/D_ATL_DLL
ATL_LIBS=$(ATL_LIB_PATH)\atl.lib
! endif

! ifdef ATL_MIN_CRT
ATL_LIBS=$(ATL_LIBS) $(ATL_LIB_PATH)\atlmincrt.lib
! endif

!ENDIF

!ENDIF

#------------------------------------------------------------
#-- END OF POST-PROCESSING
#------------------------------------------------------------

!IF 0

!MESSAGE BUILDMSG: ATL_LIBS=$(ATL_LIBS)
!MESSAGE BUILDMSG: ADDITIONAL_ATL_LIBS=$(ADDITIONAL_ATL_LIBS)
!MESSAGE BUILDMSG: USE_ATL="$(USE_ATL)"
!MESSAGE BUILDMSG: ATL_VER="$(ATL_VER)"

!ENDIF
