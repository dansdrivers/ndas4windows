#ifndef _NDAS_NIF_H_INCLUDED
#define _NDAS_NIF_H_INCLUDED
#pragma once
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NDASNIF_LINKAGE
#define NDASNIF_LINKAGE __declspec(dllimport)
#endif

#define NDAS_NIF_NAME_LENGTH 30

typedef struct _NDAS_NIF_V1_ENTRYW
{
	DWORD_PTR Flags; /* reserved */
	LPWSTR Name;
	LPWSTR DeviceId;
	LPWSTR WriteKey;
	LPWSTR Description;
} NDAS_NIF_V1_ENTRYW, *PNDAS_NIF_V1_ENTRYW;

typedef struct _NDAS_NIF_V1_ENTRYA
{
	DWORD_PTR Flags; /* reserved */
	LPSTR Name;
	LPSTR DeviceId;
	LPSTR WriteKey;
	LPSTR Description;
} NDAS_NIF_V1_ENTRYA, *PNDAS_NIF_V1_ENTRYA;

#ifdef UNICODE
#define NDAS_NIF_V1_ENTRY  NDAS_NIF_V1_ENTRYW
#define PNDAS_NIF_V1_ENTRY PNDAS_NIF_V1_ENTRYW
#else
#define NDAS_NIF_V1_ENTRY  NDAS_NIF_V1_ENTRYA
#define PNDAS_NIF_V1_ENTRY PNDAS_NIF_V1_ENTRYA
#endif

/* Description
 *
 * NdasNifImport function reads a NIF file and returns the entries.
 *
 * Parameters
 *
 * FileName [in] 
 * 	   Pointer to a null-terminated string that names the file to
 *     be opened.
 *
 * lpEntryCount [out]
 *     Pointer to a DWORD value to receive the count of the entries in the
 *     returned buffer
 *
 * Return Values
 *  
 * If the function succeeds, the return value is a pointer to the buffer
 * containing the array of NDAS_NIF_ENTRY structure up the the value
 * returned to lpEntryCount. The caller is responsible to free the returned
 * buffer with LocalFree.
 *
 * If the function fails, the return value is null. To get extended error
 * information, call GetLastError.
 */

NDASNIF_LINKAGE
HRESULT
NdasNifImportW(
	LPCWSTR FileName, 
	LPDWORD lpEntryCount,
	NDAS_NIF_V1_ENTRYW** ppEntry);

NDASNIF_LINKAGE
HRESULT
NdasNifImportA(
	LPCSTR FileName, 
	LPDWORD lpEntryCount,
	NDAS_NIF_V1_ENTRYA** ppEntry);

#ifdef UNICODE
#define NdasNifImport NdasNifImportW
#else
#define NdasNifImport NdasNifImportA
#endif

NDASNIF_LINKAGE
HRESULT
WINAPI
NdasNifExportW(
	LPCWSTR FileName,
	DWORD EntryCount,
	CONST NDAS_NIF_V1_ENTRYW* pEntry);

NDASNIF_LINKAGE
HRESULT
WINAPI
NdasNifExportA(
	LPCSTR FileName,
	DWORD EntryCount,
	CONST NDAS_NIF_V1_ENTRYA* pEntry);

#ifdef UNICODE
#define NdasNifExport NdasNifExportW
#else
#define NdasNifExport NdasNifExportA
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _NDAS_NIF_H_INCLUDED */

