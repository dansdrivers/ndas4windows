#include "ntkrnlapi.h"

#include <scsi.h>
#include <stdio.h>
#include <stdarg.h>

#include "debug.h"

#define DEBUG_BUFFER_LENGTH 256

LONG	NbDebugLevel = 0;
UCHAR	NbBuffer[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	NbBufferSentinal = NULL;
UCHAR	NbBufferWithLocation[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	NbBufferWithLocationSentinal = NULL;


VOID
_NbDebugPrintE(
   IN PCCHAR	DebugMessage,
   ...
   )


{
    va_list ap;

    va_start(ap, DebugMessage);
	_vsnprintf(NbBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	ASSERTMSG("_KDebugPrint overwrote sentinal byte",
		((NbBufferSentinal == NULL) || (*NbBufferSentinal == 0xff)));

	if(NbBufferSentinal) {
		*NbBufferSentinal = 0xff;
	}

#ifdef __ENABLE_LOADER__
	ScsiDebugPrint(0, NbBuffer);
#else
	DbgPrint(NbBuffer);
#endif

    va_end(ap);
} 
