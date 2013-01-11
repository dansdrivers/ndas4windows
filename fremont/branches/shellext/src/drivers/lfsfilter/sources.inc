MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX /W3

INCLUDES=$(INCLUDES);..

SOURCES=..\filespy.c					\
        ..\fspyLib.c					\
        ..\fspyCtx.c					\
        ..\fspyHash.c					\
		..\Lfs.c						\
		..\Primary.c					\
		..\PrimarySession.c				\
		..\LfsTable.c					\
		..\LfsLib.c						\
		..\Secondary.c					\
		..\SecondaryRedirectIrp.c		\
		..\SecondaryThread.c			\
		..\ReadOnly.c					\
		..\ReadonlyRedirectIrp.c		\
		..\LfsDbg.c						\
		..\MemoryMap.c					\
		..\FastIoDispatch.c				\
		..\LfsProto.c					\
		..\LfsDGSvrCli.c				\
		..\md5.c						\
		..\NetdiskManager.c				\
		..\PrimarySessionThread.c		\
		..\PrimarySessionDispatch.c		\
		..\PrimarySessionDispatchWinxpRequest.c \
		..\lfsfilt.rc

SOURCES=$(SOURCES)						\
		..\irpNames.c					\
		..\nameLists.c					\
		..\fastioNames.c				\
		..\fsFilterOperationNames.c

SOURCES=$(SOURCES)					\
		..\namelookup.c	

SOURCES=$(SOURCES)					\
		..\fSpyTx.c					\
		..\txNames.c	
		

PRECOMPILED_INCLUDE=..\lfsproc.h
PRECOMPILED_PCH=lfsproc.pch
PRECOMPILED_OBJ=lfsproc.obj
		
