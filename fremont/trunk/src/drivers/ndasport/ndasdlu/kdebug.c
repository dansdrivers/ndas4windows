#include "ndasport.h"
#include "trace.h"
#include "ndasdlu.h"
#include "ndasdluioctl.h"
#include "constants.h"
#include "utils.h"

#include <initguid.h>
#include "ndasdluguid.h"

#include "lpxtdi.h"

#pragma warning(disable:4995)

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "KDebug"

#define DEBUG_BUFFER_LENGTH 256

ULONG	KDebugLevel = 1;
ULONG	KDebugMask = DBG_DEFAULT_MASK;

UCHAR	MiniBuffer[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	MiniBufferSentinal = NULL;

UCHAR	MiniBufferWithLocation[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	MiniBufferWithLocationSentinal = NULL;

VOID
_LSKDebugPrint (
   IN PCCHAR	DebugMessage,
   ...
	)
{
    va_list ap;

    va_start( ap, DebugMessage );

	_vsnprintf( MiniBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap );

	ASSERTMSG( "_KDebugPrint overwrote sentinal byte",
				((MiniBufferSentinal == NULL) || (*MiniBufferSentinal == 0xff)) );

	if (MiniBufferSentinal) {
	
		*MiniBufferSentinal = 0xff;
	}

	DbgPrint( MiniBuffer );

    va_end( ap );
}

PCHAR
_LSKDebugPrintVa (
		IN PCCHAR	DebugMessage,
		...
){
	va_list ap;

	va_start( ap, DebugMessage );

	_vsnprintf( MiniBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap );

	ASSERTMSG("_KDebugPrint overwrote sentinal byte",
			  ((MiniBufferSentinal == NULL) || (*MiniBufferSentinal == 0xff)));

	if (MiniBufferSentinal) {

		*MiniBufferSentinal = 0xff;
	}

	va_end( ap );

	return (PCHAR)MiniBuffer;
}

VOID
_LSKDebugPrintWithLocation(
   IN PCCHAR	DebugMessage,
   PCCHAR		ModuleName,
   UINT32		LineNumber,
   PCCHAR		FunctionName
   )
{
	_snprintf(MiniBufferWithLocation, DEBUG_BUFFER_LENGTH, 
		"[%s:%04d] %s : %s", ModuleName, LineNumber, FunctionName, DebugMessage);

	ASSERTMSG("_KDebugPrintWithLocation overwrote sentinal byte",
		((MiniBufferWithLocationSentinal == NULL) || (*MiniBufferWithLocationSentinal == 0xff)));

	if(MiniBufferWithLocationSentinal) {
		*MiniBufferWithLocationSentinal = 0xff;
	}

	DbgPrint(MiniBufferWithLocation);
}