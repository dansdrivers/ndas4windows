!IF 0

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

!ENDIF

C_DEFINES=/DUNICODE /D_UNICODE
INCLUDES=..\;..\..\inc;$(NDAS_INC_PATH);$(PSDK_INC_PATH)
USE_NATIVE_EH=1
USE_STL=1

PRECOMPILED_INCLUDE=..\stdafx.h
PRECOMPILED_CXX=1

SOURCES=\
	..\pnpdevinst.cpp \
	..\svcinst.cpp \
	..\netcomp.cpp \
	..\findinf.cpp \
	..\setupsup.cpp \
	..\xdi.cpp
