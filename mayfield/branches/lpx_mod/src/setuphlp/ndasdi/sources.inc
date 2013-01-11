!IF 0

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

!ENDIF

C_DEFINES=/DUNICODE /D_UNICODE
INCLUDES=..\;..\..\inc;$(NDAS_INC_PATH);$(PSDK_INC_PATH)
USECXX_FLAG=/Tp /EHsc
USER_C_FLAGS=/Wp64

#MSC_WARNING_LEVEL=-W3 -WX
#MSC_OPTIMIZATION=/Oitb2

PRECOMPILED_INCLUDE=..\stdafx.h
PRECOMPILED_CXX=1

!IF $(FREEBUILD)
USE_MSVCRT=1
!ELSE
USE_LIBCMT=1
DEBUG_CRTS=1
!ENDIF

SOURCES=\
	..\pnpdevinst.cpp \
	..\svcinst.cpp \
	..\netcomp.cpp \
	..\findinf.cpp \
	..\setupsup.cpp
