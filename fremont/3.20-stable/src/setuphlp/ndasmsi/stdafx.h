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

//
// We do not use __declspec(dllexport)
// Use ndasmsi.def instead
// Using both causes warnings in x64 builds
//
//#define NC_DLLSPEC NC_EXTERN_C __declspec(dllexport)
#define NC_DLLSPEC
#define NC_CALLSPEC __stdcall

