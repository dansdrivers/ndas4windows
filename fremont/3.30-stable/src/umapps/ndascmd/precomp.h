#if MSC_VER > 10
#pragma once
#endif

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <crtdbg.h>

#pragma warning(disable: 4201)
// winioctl.h(1878) : warning C4201: nonstandard extension used : 
//   nameless struct/union
#include <winioctl.h>
#pragma warning(default: 4201)

#include <ndas/ndasuser.h>
#include <ndas/ndasfs.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

extern HINSTANCE AppResourceInstance;
