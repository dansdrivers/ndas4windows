# driver project.mk

#
# temporary migration use only
#

# get the current project.mk directory

!if exists(.\project.mk)
NDAS_DRIVER_ROOT_PATH=.
!elseif exists(..\project.mk)
NDAS_DRIVER_ROOT_PATH=..
!elseif exists(..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..
!elseif exists(..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..
!elseif exists(..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..
!elseif exists(..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..
!elseif exists(..\..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\..\project.mk)
NDAS_DRIVER_ROOT_PATH=..\..\..\..\..\..\..\..\..\..
!else
!error NDAS Source Tree $(NDAS_DRIVER_ROOT_PATH) cannot be found!
!endif

!ifdef NDAS_VERBOSE
!	if [echo BUILDMSG: NDAS Driver Project.mk included]
!	endif
!	if [echo BUILDMSG: NDAS_DRIVER_ROOT_PATH=$(NDAS_DRIVER_ROOT_PATH) from %CD%]
!	endif
!endif

#
# global definitions from the project root
#
!include "$(NDAS_DRIVER_ROOT_PATH)\..\project.mk"

#
# local definitions
#
NDAS_DRIVER_SRC_PATH=$(NDAS_DRIVER_ROOT_PATH)
NDAS_DRIVER_INC_PATH=$(NDAS_DRIVER_SRC_PATH)\inc

!ifdef NDAS_VERBOSE
!	if [echo BUILDMSG: NDAS_DRIVER_LIB_DEST=$(NDAS_DRIVER_LIB_DEST)]
!	endif
!	if [echo BUILDMSG: NDAS_DRIVER_LIB_DEST_PLT=$(NDAS_DRIVER_LIB_DEST_PLT)]
!	endif
!	if [echo BUILDMSG: NDAS_DRIVER_LIB_PATH=$(NDAS_DRIVER_LIB_PATH)]
!	endif
!endif
