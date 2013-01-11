#pragma once

//
// Defines the process-scope global variables
//

//
// There is only one single event handler instance
// per process in a DLL
//

typedef struct _PROCESS_DATA {
	UCHAR Reserved[4];
} PROCESS_DATA, *PPROCESS_DATA;

BOOL InitProcessData();
VOID CleanupProcessData();

VOID LockProcessData();
VOID UnlockProcessData();

PPROCESS_DATA GetProcessData();

