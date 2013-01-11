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

#if WINVER >= 0x0501

VOID
GetFsFilterOperationName (
    __in UCHAR FsFilterOperation,
    __out_bcount(FsFilterOperationNameSize) PCHAR FsFilterOperationName,
    __in ULONG FsFilterOperationNameSize
    )
/*++

Routine Description:

    This routine translates the given FsFilter Operation code into a printable
    string which is returned.

Arguments:

    FsFilterOperation - the FsFilter operation code to translate
    FsFilterOperationName - a buffer at least OPERATION_NAME_BUFFER_SIZE
                characters long that receives the name.

Return Value:

    None.

--*/
{
    PCHAR operationString;
    CHAR nameBuf[OPERATION_NAME_BUFFER_SIZE];

	UNREFERENCED_PARAMETER( FsFilterOperationNameSize );

    switch (FsFilterOperation) {

        case FS_FILTER_ACQUIRE_FOR_CC_FLUSH:
            operationString = "ACQUIRE_FOR_CC_FLUSH";
            break;

        case FS_FILTER_RELEASE_FOR_CC_FLUSH:
            operationString = "RELEASE_FOR_CC_FLUSH";
            break;

        case FS_FILTER_ACQUIRE_FOR_SECTION_SYNCHRONIZATION:
            operationString = "ACQUIRE_FOR_SECTION_SYNC";
            break;

        case FS_FILTER_RELEASE_FOR_SECTION_SYNCHRONIZATION:
            operationString = "RELEASE_FOR_SECTION_SYNC";
            break;

        case FS_FILTER_ACQUIRE_FOR_MOD_WRITE:
            operationString = "ACQUIRE_FOR_MOD_WRITE";
            break;

        case FS_FILTER_RELEASE_FOR_MOD_WRITE:
            operationString = "RELEASE_FOR_MOD_WRITE";
            break;

        default:
#if __NDAS_FS__
			sprintf(nameBuf,"Unknown FsFilter operation (%u)",FsFilterOperation);
#else
            StringCbPrintfA(nameBuf,sizeof(nameBuf),"Unknown FsFilter operation (%u)",FsFilterOperation);
#endif
            operationString = nameBuf;
    }

#if __NDAS_FS__
	strcpy(FsFilterOperationName,operationString);
#else
    StringCbCopyA(FsFilterOperationName,FsFilterOperationNameSize,operationString);
#endif
}

#endif

