/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    irpName.c

Abstract:

    This module contains functions used to generate names for IRPs


Environment:

    User mode


--*/

#include "LfsProc.h"

#include <ntifs.h>
#if !__NDAS_FS__
#include <strsafe.h>
#endif
#include "filespyLib.h"


VOID
GetFastioName (
    __in FASTIO_TYPE FastIoCode,
    __out_bcount(FastIoNameSize) PCHAR FastIoName,
    __in ULONG FastIoNameSize
    )
/*++

Routine Description:

    This routine translates the given FastIO code into a printable string which
    is returned.

Arguments:

    FastIoCode - the FastIO code to translate
    FastioName - a buffer at least OPERATION_NAME_BUFFER_SIZE characters long
                 that receives the fastIO name.

Return Value:

    None.

--*/
{
    PCHAR fastIoString;
    CHAR nameBuf[OPERATION_NAME_BUFFER_SIZE];

	UNREFERENCED_PARAMETER( FastIoNameSize );

    switch (FastIoCode) {

        case CHECK_IF_POSSIBLE:
            fastIoString = "CHECK_IF_POSSIBLE";
            break;

        case READ:
            fastIoString = "READ";
            break;

        case WRITE:
            fastIoString = "WRITE";
            break;

        case QUERY_BASIC_INFO:
            fastIoString = "QUERY_BASIC_INFO";
            break;

        case QUERY_STANDARD_INFO:
            fastIoString = "QUERY_STANDARD_INFO";
            break;

        case LOCK:
            fastIoString = "LOCK";
            break;

        case UNLOCK_SINGLE:
            fastIoString = "UNLOCK_SINGLE";
            break;

        case UNLOCK_ALL:
            fastIoString = "UNLOCK_ALL";
            break;

        case UNLOCK_ALL_BY_KEY:
            fastIoString = "UNLOCK_ALL_BY_KEY";
            break;

        case DEVICE_CONTROL:
            fastIoString = "DEVICE_CONTROL";
            break;

        case DETACH_DEVICE:
            fastIoString = "DETACH_DEVICE";
            break;

        case QUERY_NETWORK_OPEN_INFO:
            fastIoString = "QUERY_NETWORK_OPEN_INFO";
            break;

        case MDL_READ:
            fastIoString = "MDL_READ";
            break;

        case MDL_READ_COMPLETE:
            fastIoString = "MDL_READ_COMPLETE";
            break;

        case MDL_WRITE:
            fastIoString = "MDL_WRITE";
            break;

        case MDL_WRITE_COMPLETE:
            fastIoString = "MDL_WRITE_COMPLETE";
            break;

        case READ_COMPRESSED:
            fastIoString = "READ_COMPRESSED";
            break;

        case WRITE_COMPRESSED:
            fastIoString = "WRITE_COMPRESSED";
            break;

        case MDL_READ_COMPLETE_COMPRESSED:
            fastIoString = "MDL_READ_COMPLETE_COMPRESSED";
            break;

        case PREPARE_MDL_WRITE:
            fastIoString = "PREPARE_MDL_WRITE";
            break;

        case MDL_WRITE_COMPLETE_COMPRESSED:
            fastIoString = "MDL_WRITE_COMPLETE_COMPRESSED";
            break;

        case QUERY_OPEN:
            fastIoString = "QUERY_OPEN";
            break;

        default:
#if __NDAS_FS__
			sprintf(nameBuf,"Unknown FastIO operation (%u)",FastIoCode);
#else
            StringCbPrintfA( nameBuf, sizeof(nameBuf), "Unknown FastIO operation (%u)", FastIoCode );
#endif
            fastIoString = nameBuf;
    }

#if __NDAS_FS__
	strcpy(FastIoName,fastIoString);
#else
    StringCbCopyA( FastIoName, FastIoNameSize, fastIoString );
#endif
}

