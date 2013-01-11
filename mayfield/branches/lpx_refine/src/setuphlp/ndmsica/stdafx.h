#define STRICT
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <crtdbg.h>
#include <msiquery.h>

#ifdef __cplusplus
#define NC_EXTERN_C extern "C"
#else
#define NC_EXTERN_C
#endif

#define NC_DLLSPEC NC_EXTERN_C __declspec(dllexport)
#define NC_CALLSPEC __stdcall

