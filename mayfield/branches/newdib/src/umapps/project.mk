# driver project.mk

#
# temporary migration use only
#

# get the current project.mk directory

!if exists(.\project.mk)
NDAS_UMAPPS_ROOT_PATH=.
!elseif exists(..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..
!elseif exists(..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..
!elseif exists(..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..
!elseif exists(..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..
!elseif exists(..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..
!elseif exists(..\..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\..\project.mk)
NDAS_UMAPPS_ROOT_PATH=..\..\..\..\..\..\..\..\..\..
!else
!error NDAS Source Tree $(NDAS_UMAPPS_ROOT_PATH) cannot be found!
!endif

!ifdef XM_BUILD_VERBOSE
!message BUILDMSG: NDAS UMAPPS project.mk included
!message BUILDMSG: NDAS_UMAPPS_ROOT_PATH=$(NDAS_UMAPPS_ROOT_PATH) from %CD%
!endif

#
# global definitions from the project root
#
!include "$(NDAS_UMAPPS_ROOT_PATH)\..\project.mk"

#
# local definitions
#
NDAS_UMAPPS_SRC_PATH=$(NDAS_UMAPPS_ROOT_PATH)
NDAS_UMAPPS_INC_PATH=$(NDAS_UMAPPS_SRC_PATH)\inc

!ifdef XM_BUILD_VERBOSE
!message BUILDMSG: NDAS_UMAPPS_LIB_DEST=$(NDAS_UMAPPS_LIB_DEST)
!message BUILDMSG: NDAS_UMAPPS_LIB_DEST_PLT=$(NDAS_UMAPPS_LIB_DEST_PLT)
!message BUILDMSG: NDAS_UMAPPS_LIB_PATH=$(NDAS_UMAPPS_LIB_PATH)
!endif


!ifndef PSDK_INC_PATH
!message BUILDMSG: PSDK_INC_PATH is not defined. Set PSDK_INC_PATH or declare in config.inc
!error PSDK_INC_PATH is not defined. Set PSDK_INC_PATH or declare in config.inc
!endif

!if exists($(PSDK_INC_PATH))
!  ifdef XM_BUILD_VERBOSE
!message BUILDMSG: PSDK_INC_PATH=$(PSDK_INC_PATH)
!  endif
!else
!error PSDK_INC_PATH=$(PSDK_INC_PATH) not exists.
!endif

!ifndef PSDK_LIB_PATH
!error PSDK_LIB_PATH is not defined. Set PSDK_LIB_PATH or declare in config.inc
!if [echo BUILDMSG: PSDK_LIB_PATH is not defined. Set PSDK_LIB_PATH or declare in config.inc]
!endif
!endif

!ifdef XM_BUILD_VERBOSE
!message BUILDMSG: Target Architecture: $(_BUILDARCH)
!endif

!if "$(_BUILDARCH)" == "AMD64"
PSDK_LIB_PATH=$(PSDK_LIB_PATH)\AMD64
!endif

!if exists($(PSDK_LIB_PATH))
!  ifdef XM_BUILD_VERBOSE
!    message BUILDMSG: PSDK_LIB_PATH=$(PSDK_LIB_PATH)
!  endif
!else
!error PSDK_LIB_PATH=$(PSDK_LIB_PATH) not exists.
!endif

!ifndef XM_VENDOR_PATH
!error XM_VENDOR_PATH is not defined. Set XM_VENDOR_PATH or declare in config.inc
!message BUILDMSG: XM_VENDOR_PATH is not defined. Set XM_VENDOR_PATH or declare in config.inc
!endif

!if exists($(XM_VENDOR_PATH))
!  ifdef XM_BUILD_VERBOSE
!    message BUILDMSG: XM_VENDOR_PATH=$(XM_VENDOR_PATH)
!  endif
!else
!error XM_VENDOR_PATH=$(XM_VENDOR_PATH) not exists.
!endif
