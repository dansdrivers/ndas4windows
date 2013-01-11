#include	"StdAfx.h"

HANDLE		hServerStopEvent = NULL;
HANDLE		hUpdateEvent = NULL;
UCHAR		ucCurrentNetdisk[6];
UCHAR		ucVersion;
BOOL		bNewNetdiskArrived = FALSE;
UINT		uiLot = 0;