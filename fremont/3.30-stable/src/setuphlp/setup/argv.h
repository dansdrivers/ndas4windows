#pragma once
#include <shellapi.h>
#include <windows.h>

LPSTR* CommandLineToArgvA(LPCSTR lpCmdLine, int* pNumArgs);

#ifdef UNICODE
#define CommandLineToArgv CommandLineToArgvW
#else
#define CommandLineToArgv CommandLineToArgvA
#endif

