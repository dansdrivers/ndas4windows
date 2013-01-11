// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef UNICODE
#error "Disable UNICODE Mode"
#endif

#define _CRT_SECURE_NO_DEPRECATE

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

// TODO: reference additional headers your program requires here

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <time.h>

#include <WinSock2.h> // Prevent windows.h from include winsock.h

#include "..\inc\socketLpx.h"
#include "..\inc\lsprotospec.h"
#include "..\inc\binparams.h"
#include "..\inc\lsprotoidespec.h"
#include "..\inc\hdreg.h"

#include "ndascli.h"


#define DBG_LEVEL_NDAS_CLI	4
#define DBG_LEVEL_CLI_CMD	4
