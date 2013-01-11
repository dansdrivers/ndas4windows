#pragma once

//
// Defines the process-scope global variables
//

//
// There is only one single event handler instance
// per process in a DLL
//

typedef struct _PROCESS_DATA {
	OVERLAPPED overlapped;
} PROCESS_DATA, *PPROCESS_DATA;

extern PPROCESS_DATA _pProcessData;
