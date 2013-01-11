/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#if _MSC_VER <= 1000
#error "Out of date Compiler"
#endif

#pragma once

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
#include <WinSock2.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <tchar.h>
#include "..\Inc\ndasscsi.h"
#include "..\inc\binparams.h"
#include "..\inc\lsprotospec.h"
#include "..\inc\lsprotoidespec.h"
#include "..\inc\hash.h"
#include "..\inc\socketlpx.h"
#include "ndasemustruc.h"

//
//	Normal user password
//

#define HASH_KEY_USER			0x1F4A50731530EABB


//
//	Super user password
//

#define HASH_KEY_SUPER		0x3e2b321a4750131e


//
//	Port numbers
//

#define	NDASDEV_LISTENPORT_NUMBER	10000
#define	BROADCAST_SOURCEPORT_NUMBER	10001
#define BROADCAST_DESTPORT_NUMBER	(BROADCAST_SOURCEPORT_NUMBER+1)

//////////////////////////////////////////////////////////////////////////
//
//	structures
//
typedef struct _GENERAL_PURPOSE_LOCK {
	BOOL	Acquired;
	ULONG	Counter;
	UINT64	SessionId;
} GENERAL_PURPOSE_LOCK, *PGENERAL_PURPOSE_LOCK;

typedef struct _PROM_DATA_OLD {
	UINT32 MaxConnTime;
	UINT32 MaxRetTime;
	UINT64 UserPasswd;
	UINT64 SuperPasswd;
} PROM_DATA_OLD, *PPROM_DATA_OLD;

typedef struct _RAM_DATA {

	// protect General purpose locks.
	HANDLE					LockMutex;

	// General purpose locks
	GENERAL_PURPOSE_LOCK	GPLocks[4];

} RAM_DATA, *PRAM_DATA;

