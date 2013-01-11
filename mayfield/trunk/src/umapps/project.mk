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
!message BUILDMSG: NDAS_UMAPPS_ROOT_PATH=$(NDAS_UMAPPS_ROOT_PATH)
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


