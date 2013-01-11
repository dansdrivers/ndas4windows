#ifndef _DEBUG_PRINT_H  
#define _DEBUG_PRINT_H  
  
  
#ifdef __cplusplus  
extern "C" {  
#endif  

#define DEBUG_BUFFER_LENGTH 256

VOID
DbgPrintA(
		  IN PCHAR	DebugMessage,
		  ...
		  );
VOID
DbgPrintW(
		  IN PWCHAR	DebugMessage,
		  ...
		  );

#ifdef UNICODE
#define DbgPrint	DbgPrintW
#else
#define DbgPrint	DbgPrintA
#endif

extern ULONG	DebugPrintLevel;

#ifdef _DEBUG	

	#define DebugPrint(_l_, _x_)	\
	if(_l_ <= DebugPrintLevel) {	\
		DbgPrint _x_;				\
	}
#else
	#define DebugPrint(_l_, _x_)
#endif

LPTSTR 
GetLastErrorText(
				 LPTSTR	lpszBuf, 
				 DWORD	dwSize 
				 );  

void 
PrintErrorCode(
			   LPTSTR	strPrefix, 
			   DWORD	ErrorCode
			   );

#ifdef __cplusplus  
}  
#endif  
  
#endif 
