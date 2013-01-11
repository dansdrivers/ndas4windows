#
# get the current project.mk directory
#

LOCAL_DIR_FILE=ndastools.dir
!if exists(.\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=.
!elseif exists(..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..
!elseif exists(..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..
!elseif exists(..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..
!elseif exists(..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..
!elseif exists(..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..
!elseif exists(..\..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..\..\..\..\..
!elseif exists(..\..\..\..\..\..\..\..\..\..\$(LOCAL_DIR_FILE))
LOCAL_PROJECT_PATH=..\..\..\..\..\..\..\..\..\..
!else
!error $(LOCAL_DIR_FILE) cannot be found!
!endif

#
# global definitions from the project root
#
!include "$(LOCAL_PROJECT_PATH)\..\project.mk"

!IFNDEF XM_VENDOR_PATH
XM_VENDOR_PATH=$(WNETBASE)\Supplement
!ENDIF

!IFNDEF PSDK_INC_PATH
PSDK_INC_PATH=$(XM_VENDOR_PATH)\psdk\include
!ENDIF
#obsolete not used anymore
#PSDK_INC_LONGPATH="C:\Program files\microsoft platform sdk\include"
!IFNDEF PSDK_LIB_PATH
PSDK_LIB_PATH=$(XM_VENDOR_PATH)\psdk\lib
!ENDIF

!IFNDEF WTL71_INC_PATH
WTL71_INC_PATH=$(XM_VENDOR_PATH)\wtl71\inc
!ENDIF

!IF "$(_BUILDARCH)" == "AMD64"
ATL71_INC_PATH=$(XM_VENDOR_PATH)\atl71\amd64\inc
ATL71_LIB_PATH=$(XM_VENDOR_PATH)\atl71\amd64\lib
CRT71_INC_PATH=$(XM_VENDOR_PATH)\crt71\amd64\inc\crt
CRT71_LIB_PATH=$(XM_VENDOR_PATH)\crt71\amd64\lib
#PSDK_LIB_PATH=$(PSDK_LIB_PATH)\AMD64
!ELSE
ATL71_INC_PATH=$(XM_VENDOR_PATH)\atl71\inc
ATL71_LIB_PATH=$(XM_VENDOR_PATH)\atl71\lib
CRT71_INC_PATH=$(XM_VENDOR_PATH)\crt71\inc\crt
CRT71_LIB_PATH=$(XM_VENDOR_PATH)\crt71\lib
!ENDIF

!if exists(.\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=.\atlsupp.inc
!elseif           exists(..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\atlsupp.inc
!elseif           exists(..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\atlsupp.inc
!elseif           exists(..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\..\..\..\..\atlsupp.inc
!elseif           exists(..\..\..\..\..\..\..\..\..\..\..\atlsupp.inc)
SOURCES_SUPPPORT_INCLUDE=..\..\..\..\..\..\..\..\..\..\..\atlsupp.inc
!endif

# !ELIF EXISTS(..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\atlsupp.inc
# !ELIF EXISTS(..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\..\..\..\..\atlsupp.inc
# !ELIF EXISTS(..\..\..\..\..\..\..\..\..\..\atlsupp.inc)
# ATLSUPP_INC_FILE=..\..\..\..\..\..\..\..\..\..\atlsupp.inc
# !ENDIF

!if exists(.\project.wrn)
PROJECT_WARNING_INCLUDE=.\project.wrn
!elseif           exists(..\project.wrn)
PROJECT_WARNING_INCLUDE=..\project.wrn
!elseif           exists(..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\project.wrn
!elseif           exists(..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\project.wrn
!elseif           exists(..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\..\..\..\..\project.wrn
!elseif           exists(..\..\..\..\..\..\..\..\..\..\..\project.wrn)
PROJECT_WARNING_INCLUDE=..\..\..\..\..\..\..\..\..\..\..\project.wrn
!endif

#!IF "$(__BUILDMACHINE__)" == "WinDDK"
__BUILDMACHINE__=$(COMPUTERNAME)
#!ENDIF

NO_NTDLL=1

#
# 'amd64mk.inc' forces to use WIN32_LIBS to include ntdll.lib
# which prevents to use Platform SDK libraries instead.
# As we apply the patch, the following definition 
# makes the patch effective.
#
# See README.txt for details
#
NO_NTDLL_IN_AMD64=1

!IFNDEF COMPILER_WARNINGS
# deprecate the default compiler warning flags
#COMPILER_WARNINGS=-FI$(SDK_INC_PATH)\warning.h $(PROJECT_COMPILER_WARNINGS)
COMPILER_WARNINGS=-FI$(PROJECT_WARNING_INCLUDE) $(PROJECT_COMPILER_WARNINGS)
!ENDIF


