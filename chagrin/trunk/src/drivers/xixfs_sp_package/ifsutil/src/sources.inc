

TARGETNAME=ifsutil
TARGETPATH=obj
TARGETTYPE=DYNLINK

!IF "$(_BUILDARCH)" == "AMD64" 

TARGETLIBS=	\
	$(DDK_LIB_PATH)\kernel32.lib	\
	$(XM_WXP_INC_PATH)\public\internal\ds\lib\amd64\rsa32.lib	\
	$(DDK_LIB_PATH)\advapi32.lib	\
	$(DDK_LIB_PATH)\user32.lib		\
	$(DDK_LIB_PATH)\cfgmgr32.lib	\
	$(DDK_LIB_PATH)\setupapi.lib	\
	$(DDK_LIB_PATH)\ntdll.lib		\
	..\..\..\ulib\src\$(ALT_PROJECT)\$(O)\ulib.lib

!ELSE IF "$(_BUILDARCH)" == "64"

TARGETLIBS=	\
	$(DDK_LIB_PATH)\kernel32.lib	\
	$(XM_WXP_INC_PATH)\public\internal\ds\lib\ia64\rsa32.lib	\
	$(DDK_LIB_PATH)\advapi32.lib	\
	$(DDK_LIB_PATH)\user32.lib		\
	$(DDK_LIB_PATH)\cfgmgr32.lib	\
	$(DDK_LIB_PATH)\setupapi.lib	\
	$(DDK_LIB_PATH)\ntdll.lib		\
	..\..\..\ulib\src\$(ALT_PROJECT)\$(O)\ulib.lib
	
!ELSE

TARGETLIBS=	\
	$(DDK_LIB_PATH)\kernel32.lib	\
	$(DDK_LIB_PATH)\rsa32sdk2.lib	\
	$(DDK_LIB_PATH)\advapi32.lib	\
	$(DDK_LIB_PATH)\user32.lib		\
	$(DDK_LIB_PATH)\cfgmgr32.lib	\
	$(DDK_LIB_PATH)\setupapi.lib	\
	$(DDK_LIB_PATH)\ntdll.lib		\
	..\..\..\ulib\src\$(ALT_PROJECT)\$(O)\ulib.lib

!ENDIF



	
INCLUDES= \
	..;\
	..\..\inc;\
	..\..\..\ulib\inc; \
	$(XM_WXP_INC_PATH)\public\sdk\inc;	\
	$(XM_WXP_INC_PATH)\public\ddk\inc;	\
	$(XM_WXP_INC_PATH)\public\internal\base\inc;	\
	$(PSDK_INC_PATH);\
	$(XM_WXP_INC_PATH)\public\internal\ds\inc\crypto; \
	$(DDK_INC_PATH);	
	
	


!IF "$(_BUILDARCH)" == "AMD64" || "$(_BUILDARCH)" == "64"

!ELSE

DLLENTRY=InitializeIfsUtil

!ENDIF


NO_SAFESEH=1


USE_MSVCRT=1





PRECOMPILED_INCLUDE= ..\pch.cxx

MSC_WARNING_LEVEL=/W3 /WX




SOURCES=\
	..\autoentr.cxx    \
	..\autoreg.cxx     \
	..\bigint.cxx      \
	..\bootreg.c       \
	..\cache.cxx       \
	..\cannedsd.cxx    \
	..\dcache.cxx      \
	..\digraph.cxx     \
	..\drive.cxx       \
	..\ifssys.cxx      \
	..\ifsutil.cxx     \
	..\intstack.cxx    \
	..\mldcopy.cxx     \
	..\mpmap.cxx       \
	..\numset.cxx      \
	..\rcache.cxx      \
	..\rwcache.cxx     \
	..\secrun.cxx      \
	..\smsdtect.cxx    \
	..\spaset.cxx      \
	..\supera.cxx      \
	..\tlink.cxx       \
	..\volume.cxx      \
	..\ifsutil.rc


	



C_DEFINES=-DUNICODE=1 -DSTRICT=1 

#
# This allow W2K utils to run under NT4 for temporary use.  Not intend for release.
#
!IF defined(UTILS_RUN_ON_NT4)
C_DEFINES=$(C_DEFINES) /DRUN_ON_NT4
!ENDIF

!if $(FREEBUILD)
C_DEFINES=$(C_DEFINES) /DDBG=0
!else
C_DEFINES=$(C_DEFINES) /DDBG=1
!IFNDEF NOMEMLEAK
C_DEFINES=$(C_DEFINES) /DMEMLEAK
!ENDIF
!IFDEF STACK_TRACE
C_DEFINES=$(C_DEFINES) /DSTACK_TRACE
!ENDIF
!ENDIF



UMTYPE=console

DLLDEF=

W32_SB=1
SOURCES_USED=..\sources.inc

BROWSER_INFO=1
BROWSERFILE=$(TARGETNAME).bsc -n



