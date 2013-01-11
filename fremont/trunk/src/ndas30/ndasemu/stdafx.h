// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef UNICODE
#error "Disable UNICODE Mode"
#endif

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <time.h>

#include <WinSock2.h> // Prevent windows.h from include winsock.h

// TODO: reference additional headers your program requires here

#define HTONLL(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))

#define NTOHLL(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))

VOID
GetHostMacAddressOfSession (
	IN	UINT64 SessionId,
	OUT PUCHAR HostMacAddress
	);

#define DBG_LEVEL_NDAS_EMU	4
#define DBG_LEVEL_EMU_LOCK	4
