!IFDEF		OEM_BUILD

C_DEFINES=-DOEM_BUILD $(C_DEFINES)

#-----------------------------------------------------------------------------
# OEM BUILD SPECIFIC SETTINGS
#-----------------------------------------------------------------------------
!IF "$(OEM_BUILD)" == "iomega"
#-----------------------------------------------------------------------------

C_DEFINES=-DOEM_IOMEGA $(C_DEFINES)
OEM_BRAND=iomega

#-----------------------------------------------------------------------------
!ELSEIF "$(OEM_BUILD)" == "moritani"
#-----------------------------------------------------------------------------

C_DEFINES=-DOEM_MORITANI $(C_DEFINES)
OEM_BRAND=moritani

#-----------------------------------------------------------------------------
!ELSEIF "$(OEM_BUILD)" == "gennetworks"
#-----------------------------------------------------------------------------

C_DEFINES=-DOEM_GENNETWORKS $(C_DEFINES)
OEM_BRAND=gennetworks

#-----------------------------------------------------------------------------
!ELSEIF "$(OEM_BUILD)" == "logitec"
#-----------------------------------------------------------------------------

C_DEFINES=-DOEM_LOGITEC $(C_DEFINES)
OEM_BRAND=logitec

#-----------------------------------------------------------------------------
!ELSE
!ERROR No valid OEM brand name specified. Currently supports "iomega", "moritani", "gennetworks" and "logitec" only!
!ENDIF
#-----------------------------------------------------------------------------
#ENDOF OEM_COMPANY

OEM_BASEDIR=..\..\..\oem\$(OEM_BRAND)

# Assumed libraries are not customized
!IF "$(TARGETTYPE)" != "DRIVER_LIBRARY" && "$(TARGETTYPE)" != "LIBRARY"
#
# if we directly specify final target directory as TARGETPATH,
# files in it will be deleted if we change the TARGETPATH
#
#TARGETPATH=..\..\..\oem\$(OEM_BRAND)\branded\sys\$(BUILD_ALT_DIR)
!ENDIF

#!IF "$(NETDISK_BINPLACE_TYPE)" == "exe"
#_NTTREE=$(OEM_BASEDIR)\branded\exe\$(BUILD_ALT_DIR)
#!ELSE
#_NTTREE=$(OEM_BASEDIR)\branded\sys\$(BUILD_ALT_DIR)
#!ENDIF

!ENDIF
#ENDOF OEM_BUILD
