#ifndef _DEBUG_PRINT_H_
#define _DEBUG_PRINT_H_

#define DEBUG_BUFFER_LENGTH 256

void PrintErrorCode(LPTSTR strPrefix, DWORD	ErrorCode);

VOID
DbgPrint(
		 IN PCHAR	DebugMessage,
		 ...
		);

extern ULONG	DebugPrintLevel;

#ifdef _DEBUG	

	#define DebugPrint(_l_, _x_)	\
	if(_l_ <= DebugPrintLevel) {	\
		DbgPrint _x_;				\
	}
#else
	#define DebugPrint(_l_, _x_)
#endif


void PrintLastError() ;


#endif