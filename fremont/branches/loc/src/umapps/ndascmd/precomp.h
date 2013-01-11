#if MSC_VER > 10
#pragma once
#endif

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <crtdbg.h>

#include <ndas/ndasuser.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

extern HINSTANCE AppResourceInstance;
