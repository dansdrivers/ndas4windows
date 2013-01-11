#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasscsi"

#if DBG

NDASSCSI_DBG_FLAGS NdasScsiDebugLevel	=	0x00000000						|
											NDASSCSI_DBG_LURN_NDASR_ERROR	|
											NDASSCSI_DBG_LURN_NDASR_INFO	|
											NDASSCSI_DBG_LURN_IDE_ERROR		|
											NDASSCSI_DBG_LURN_IDE_INFO		;

#define DEBUG_BUFFER_LENGTH 256

UCHAR	DebugBufferWithFunction[DEBUG_BUFFER_LENGTH + 1];

VOID
NdasscsiPrintWithFunction (
	IN PCCHAR	DebugMessage,
	IN PCCHAR	FunctionName,
	IN UINT32	LineNumber
	)
{
	_snprintf( DebugBufferWithFunction, 
			   DEBUG_BUFFER_LENGTH, 
			   "[%s:%04d] %s", FunctionName, LineNumber, DebugMessage );

	NDASSCSI_ASSERT( strlen(DebugBufferWithFunction) <= DEBUG_BUFFER_LENGTH );

	DbgPrint(DebugBufferWithFunction);
}

#endif
