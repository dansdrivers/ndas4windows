/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    txNames.c

Abstract:

    This module contains functions used to generate names for Transaction
    notificatoins.


Environment:

    User mode

--*/

#include "LfsProc.h"

#include <ntifs.h>
#if !__NDAS_FS__
#include <strsafe.h>
#endif
#include "filespyLib.h"

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

VOID
GetTxNotifyName (
    __in ULONG TransactionNotification,
    __out_bcount(TxNotifyNameSize) PCHAR TxNotifyName,
    __in ULONG TxNotifyNameSize
    )
/*++

Routine Description:

    This routine translates the given transaction notification code into a 
    printable string which is returned.

Arguments:

    TransactionNotification - the transaction notification code to translate.
    TxNotifyName - a buffer at least OPERATION_NAME_BUFFER_SIZE characters long
      that receives the name.
    TxNotifyNameSize - the TxNotifyName buffer size in bytes.

Return Value:

    None.

--*/
{
    PCHAR txNotifyString;
    CHAR nameBuf[OPERATION_NAME_BUFFER_SIZE];

	UNREFERENCED_PARAMETER( TxNotifyNameSize );

    switch (TransactionNotification) {

        case TRANSACTION_NOTIFY_PREPREPARE:
            txNotifyString = "TX_PREPREPARE";
            break;

        case TRANSACTION_NOTIFY_PREPARE:
            txNotifyString = "TX_PREPARE";
            break;

        case TRANSACTION_NOTIFY_COMMIT:
            txNotifyString = "TX_COMMIT";
            break;

        case TRANSACTION_NOTIFY_ROLLBACK:
            txNotifyString = "TX_ROLLBACK";
            break;

        case TRANSACTION_NOTIFY_PREPREPARE_COMPLETE:
            txNotifyString = "TX_PREPREPARE_COMPLETE";
            break;
            
        case TRANSACTION_NOTIFY_PREPARE_COMPLETE:
            txNotifyString = "TX_PREPARE_COMPLETE";
            break;

        case TRANSACTION_NOTIFY_COMMIT_COMPLETE:
            txNotifyString = "TX_COMMIT_COMPLETE";
            break;

        case TRANSACTION_NOTIFY_ROLLBACK_COMPLETE:
            txNotifyString = "TX_ROLLBACK_COMPLETE";
            break;

        case TRANSACTION_NOTIFY_RECOVER:
            txNotifyString = "TX_RECOVER";
            break;

        case TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT:
            txNotifyString = "TX_SINGLE_PHASE_COMMIT";
            break;

        case TRANSACTION_NOTIFY_DELEGATE_COMMIT:
            txNotifyString = "TX_DELEGATE_COMMIT";
            break;

        case TRANSACTION_NOTIFY_RECOVER_QUERY:
            txNotifyString = "TX_RECOVER_QUERY";
            break;

        case TRANSACTION_NOTIFY_ENLIST_PREPREPARE:
            txNotifyString = "TX_ENLIST_PREPREPARE";
            break;

        case TRANSACTION_NOTIFY_LAST_RECOVER:
            txNotifyString = "TX_LAST_RECOVER";
            break;

        case TRANSACTION_NOTIFY_INDOUBT:
            txNotifyString = "TX_INDOUBT";
            break;

        case TRANSACTION_NOTIFY_PROPAGATE_PULL:
            txNotifyString = "TX_PROPAGATE_PULL";
            break;

        case TRANSACTION_NOTIFY_PROPAGATE_PUSH:
            txNotifyString = "TX_PROPAGATE_PUSH";
            break;

        case TRANSACTION_NOTIFY_MARSHAL:
            txNotifyString = "TX_MARSHAL";
            break;

        case TRANSACTION_NOTIFY_ENLIST_MASK:
            txNotifyString = "TX_ENLIST_MASK";
            break;

        case TRANSACTION_NOTIFY_SAVEPOINT:
            txNotifyString = "TX_SAVEPOINT";
            break;

        case TRANSACTION_NOTIFY_SAVEPOINT_COMPLETE:
            txNotifyString = "TX_SAVEPOINT_COMPLETE";
            break;

        case TRANSACTION_NOTIFY_CLEAR_SAVEPOINT:
            txNotifyString = "TX_CLEAR_SAVEPOINT";
            break;

        case TRANSACTION_NOTIFY_CLEAR_ALL_SAVEPOINTS:
            txNotifyString = "TX_CLEAR_ALL_SAVEPOINTS";
            break;

        case TRANSACTION_NOTIFY_ROLLBACK_SAVEPOINT:
            txNotifyString = "TX_ROLLBACK_SAVEPOINT";
            break;

        case TRANSACTION_NOTIFY_RM_DISCONNECTED:
            txNotifyString = "TX_RM_DISCONNECTED";
            break;

        case TRANSACTION_NOTIFY_TM_ONLINE:
            txNotifyString = "TX_TM_ONLINE";
            break;

        case TRANSACTION_NOTIFY_COMMIT_REQUEST:
            txNotifyString = "TX_COMMIT_REQUEST";
            break;

        case TRANSACTION_NOTIFY_PROMOTE:
            txNotifyString = "TX_PROMOTE";
            break;

        case TRANSACTION_NOTIFY_PROMOTE_NEW:
            txNotifyString = "TX_PROMOTE_NEW";
            break;

        default:
#if __NDAS_FS__
			sprintf( nameBuf, "Unknown transaction notification (%u)", TransactionNotification );
#else
            StringCbPrintfA(nameBuf,sizeof(nameBuf),"Unknown transaction notification (%u)",TransactionNotification);


#endif
            txNotifyString = nameBuf;
    }

#if __NDAS_FS__
	strcpy(TxNotifyName,txNotifyString);
#else
    StringCbCopyA(TxNotifyName, TxNotifyNameSize, txNotifyString);


#endif
}

#endif


