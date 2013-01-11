#ifndef gktools_h
#define gktools_h

#include <ndas/ndascomm.h>

BOOL WINAPI GetDllExportNames( LPCSTR pszFilename, ULONG* lpulOffset, ULONG* lpulSize );
BOOL WINAPI GetDllImportNames( LPCSTR pszFilename, ULONG* lpulOffset, ULONG* lpulSize );

struct s_MEMORY_ENCODING;

typedef void (WINAPI* LPFNEncodeMemoryFunction)( s_MEMORY_ENCODING* p );

typedef struct s_MEMORY_ENCODING
{
	LPBYTE lpbMemory;
	DWORD dwSize;
	LPCSTR lpszArguments;
	BOOL bEncode;
	LPFNEncodeMemoryFunction fpEncodeFunc;
} MEMORY_CODING, *LPMEMORY_CODING;

typedef struct _MEMORY_CODING_DESCRIPTION
{
	LPCSTR lpszDescription;
	LPFNEncodeMemoryFunction fpEncodeFunc;
} MEMORY_CODING_DESCRIPTION, *LPMEMORY_CODING_DESCRIPTION;

EXTERN_C LPMEMORY_CODING_DESCRIPTION WINAPI GetMemoryCodings();
typedef LPMEMORY_CODING_DESCRIPTION (WINAPI* LPFNGetMemoryCodings)();

#include "PhysicalDrive.h"
extern PartitionInfo* SelectedPartitionInfo;
extern NDASCOMM_CONNECTION_INFO NdasConnectionInfo;

BOOL WINAPI GetMemoryCoding( HINSTANCE hInstance, HWND hParent, LPMEMORY_CODING p, LPCSTR lpszDlls );
BOOL WINAPI GetDriveNameDialog( HINSTANCE hInstance, HWND hParent );
BOOL WINAPI GetAddressDialog( HINSTANCE hInstance, HWND hParent );


BOOL WINAPI GotoTrackDialog( HINSTANCE hInstance, HWND hParent );

#endif // gktools_h
