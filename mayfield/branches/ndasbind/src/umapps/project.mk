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
!	if [echo BUILDMSG: NDAS UMAPPS project.mk included]
!	endif
!	if [echo BUILDMSG: NDAS_UMAPPS_ROOT_PATH=$(NDAS_UMAPPS_ROOT_PATH) from %CD%]
!	endif
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
!	if [echo BUILDMSG: NDAS_UMAPPS_LIB_DEST=$(NDAS_UMAPPS_LIB_DEST)]
!	endif
!	if [echo BUILDMSG: NDAS_UMAPPS_LIB_DEST_PLT=$(NDAS_UMAPPS_LIB_DEST_PLT)]
!	endif
!	if [echo BUILDMSG: NDAS_UMAPPS_LIB_PATH=$(NDAS_UMAPPS_LIB_PATH)]
!	endif
!endif


!ifndef PSDK_INC_PATH
!error PSDK_INC_PATH is not defined. Set PSDK_INC_PATH or declare in config.inc
!if [echo BUILDMSG: PSDK_INC_PATH is not defined. Set PSDK_INC_PATH or declare in config.inc]
!endif
!endif

!if exists($(PSDK_INC_PATH))
!  ifdef XM_BUILD_VERBOSE
!    if [echo BUILDMSG: PSDK_INC_PATH=$(PSDK_INC_PATH)]
!    endif
!  endif
!else
!error PSDK_INC_PATH=$(PSDK_INC_PATH) not exists.
!endif

!ifndef PSDK_LIB_PATH
!error PSDK_LIB_PATH is not defined. Set PSDK_LIB_PATH or declare in config.inc
!if [echo BUILDMSG: PSDK_LIB_PATH is not defined. Set PSDK_LIB_PATH or declare in config.inc]
!endif
!endif

!if exists($(PSDK_LIB_PATH))
!  ifdef XM_BUILD_VERBOSE
!    if [echo BUILDMSG: PSDK_LIB_PATH=$(PSDK_LIB_PATH)]
!    endif
!  endif
!else
!error PSDK_LIB_PATH=$(PSDK_LIB_PATH) not exists.
!endif

!ifndef XM_VENDOR_PATH
!error XM_VENDOR_PATH is not defined. Set XM_VENDOR_PATH or declare in config.inc
!if [echo BUILDMSG: XM_VENDOR_PATH is not defined. Set XM_VENDOR_PATH or declare in config.inc]
!endif
!endif

!if exists($(XM_VENDOR_PATH))
!  ifdef XM_BUILD_VERBOSE
!    if [echo BUILDMSG: XM_VENDOR_PATH=$(XM_VENDOR_PATH)]
!    endif
!  endif
!else
!error XM_VENDOR_PATH=$(XM_VENDOR_PATH) not exists.
!endif
