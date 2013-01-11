/*++

Copyright (c) 1998-1999 Microsoft Corporation

Module Name:

    fspyTx.c

Abstract:

    This module contains the support routines for the KTM transactions.
    This feature is only available in windows VISTA and later.

Environment:

    Kernel mode

--*/

#include "LfsProc.h"

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#include <ntifs.h>
#include <stdio.h>
#include "filespy.h"
#include "fspyKern.h"
#include "namelookup.h"


#ifdef ALLOC_PRAGMA

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)
#pragma alloc_text(INIT, SpyCreateKtmResourceManager)
#pragma alloc_text(PAGE, SpyCloseKtmResourceManager)
#pragma alloc_text(PAGE, SpyLogTransactionNotify)
#endif

#endif

//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                 KTM transaction support routines                     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

NTSTATUS
SpyCreateKtmResourceManager (
    VOID
    )
/*++

Routine Description

    This routine will create a global resource manager (and a transaction manager)
    for Filespy.

Arguments

    None.

Return Value

    Returns the status of this operation.

--*/
{
    OBJECT_ATTRIBUTES objAttr;
    NTSTATUS status;
    GUID rmGuid;

    if (!IS_VISTA_OR_LATER()) {

        return STATUS_SUCCESS;
    }

    ASSERT( NULL != gSpyDynamicFunctions.CreateTransactionManager );
    ASSERT( NULL != gSpyDynamicFunctions.CreateResourceManager );
    ASSERT( NULL != gSpyDynamicFunctions.EnableTmCallbacks );
    ASSERT( NULL == gKtmTransactionManagerHandle );
    ASSERT( NULL == gKtmResourceManagerHandle );
    ASSERT( NULL == gKtmResourceManager );

    try {

        //
        //  Create a Transaction Manager
        //  The TM is global across Filespy
        //

        InitializeObjectAttributes( &objAttr,
                                    NULL,
                                    OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL );

        status = (gSpyDynamicFunctions.CreateTransactionManager)(
                                            &gKtmTransactionManagerHandle,
                                            TRANSACTIONMANAGER_ALL_ACCESS,
                                            &objAttr,
                                            NULL,
                                            TRANSACTION_MANAGER_VOLATILE,
                                            0 );

        if (!NT_SUCCESS( status )) {

            leave;
        }

        //
        //  Build the resource manager guid.
        //

        status = ExUuidCreate( &rmGuid );

        if (!NT_SUCCESS( status )) {

            leave;
        }

        //
        //  Initialize the object attributes and create a Resource Manager.
        //  The RM is global across Filespy.
        //

        InitializeObjectAttributes( &objAttr,
                                    NULL,
                                    OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL );

        status = (gSpyDynamicFunctions.CreateResourceManager)(
                                            &gKtmResourceManagerHandle,
                                            RESOURCEMANAGER_ALL_ACCESS,
                                            gKtmTransactionManagerHandle,
                                            &rmGuid,
                                            &objAttr,
                                            RESOURCE_MANAGER_VOLATILE,
                                            NULL );

        if (!NT_SUCCESS( status )) {

            leave;
        }

        //
        //  Grab the RM object from the created handle.
        //

        status = ObReferenceObjectByHandle( gKtmResourceManagerHandle,
                                            0,
                                            NULL,
                                            KernelMode,
                                            (PVOID *)&gKtmResourceManager,
                                            NULL );

        if (!NT_SUCCESS( status )) {

            leave;
        }

        //
        //  Enable the KTM notification callback.
        //

        status = (gSpyDynamicFunctions.EnableTmCallbacks)( gKtmResourceManager,
                                                           SpyKtmNotification,
                                                           NULL );

        if (!NT_SUCCESS( status )) {

            leave;
        }

    } finally {

        if (!NT_SUCCESS( status )) {

            SpyCloseKtmResourceManager();
        }
    }

    return status;
}

VOID
SpyCloseKtmResourceManager (
    VOID
    )
/*++

Routine Description

    This routine will close the global transaction manager and the
    resource manager.

Arguments

    None.

Return Value

    None.

--*/
{
    PAGED_CODE();

    if (gKtmResourceManager) {

        ObDereferenceObject( gKtmResourceManager );
        gKtmResourceManager = NULL;
    }

    if (gKtmResourceManagerHandle) {

        ZwClose( gKtmResourceManagerHandle );
        gKtmResourceManagerHandle = NULL;
    }

    if (gKtmTransactionManagerHandle) {

        ZwClose( gKtmTransactionManagerHandle );
        gKtmTransactionManagerHandle = NULL;
    }
}

NTSTATUS
SpyEnlistInTransaction (
    __in PKTRANSACTION Transaction,
    __in PKRESOURCEMANAGER KtmResourceManager,
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT FileObject
    )
/*++

Routine Description

    This routine creates an enlistment for the specified resource manager
    and the transactoin. Return success if the transaction has already been
    enlisted for the specified device object.

Arguments

    Transaction - Pointer to the transaction object.

    KtmResourceManager - Pointer to the resource manager object.

    DeviceObject - Pointer to the device object that Filespy attached to the
        file system filter stack for the file object involved in the
        transaction.

    FileObject - The pointer to the file object bound to the transaction.

Return Value

    Returns the status of this operation.

--*/
{
    PKENLISTMENT enlistmentObject;
    HANDLE enlistmentHandle = NULL;
    OBJECT_ATTRIBUTES objAttr;
    PLIST_ENTRY entry;
    PFILESPY_TRANSACTION_CONTEXT txData;
    PFILESPY_DEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS status;

    UNREFERENCED_PARAMETER( DeviceExtension );

    if (!IS_VISTA_OR_LATER()) {

        return STATUS_SUCCESS;
    }

    ASSERT( NULL != gSpyDynamicFunctions.CreateEnlistment );

    //
    //  First check in the device extension if we have already enlisted in
    //  this specified transaction. Return success if it has been done.
    //

    ExAcquireFastMutex( &DeviceExtension->TxListLock );

    entry = DeviceExtension->TxListHead.Flink;

    while (entry != &DeviceExtension->TxListHead) {

        txData = CONTAINING_RECORD( entry, FILESPY_TRANSACTION_CONTEXT, List );

        if (txData->Transaction == Transaction) {

            //
            //  Return success if we already enlisted in this transaction.
            //

            ExReleaseFastMutex( &DeviceExtension->TxListLock );

            return STATUS_SUCCESS;
        }

        entry = entry->Flink;
    }

    //
    //  Allocate a transaction context and save it in the device extension.
    //

    txData = ExAllocateFromNPagedLookasideList( &gTransactionList );

    if (txData == NULL) {

        ExReleaseFastMutex( &DeviceExtension->TxListLock );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    txData->Transaction = Transaction;
    txData->DeviceObject = DeviceObject;
    txData->FileObject = FileObject;

    InsertTailList( &DeviceExtension->TxListHead, &txData->List );

    ExReleaseFastMutex( &DeviceExtension->TxListLock );

    //
    //  Create an enlistment using the global RM created earlier.
    //

    InitializeObjectAttributes ( &objAttr,
                                 NULL,
                                 OBJ_KERNEL_HANDLE,
                                 NULL,
                                 NULL );

    status = (gSpyDynamicFunctions.CreateEnlistment)(
                                    &enlistmentHandle,
                                    KernelMode,
                                    ENLISTMENT_ALL_ACCESS,
                                    &objAttr,
                                    KtmResourceManager,
                                    Transaction,
                                    0,
                                    TRANSACTION_NOTIFY_PREPREPARE |
                                    TRANSACTION_NOTIFY_PREPARE |
                                    TRANSACTION_NOTIFY_COMMIT |
                                    TRANSACTION_NOTIFY_ROLLBACK,
                                    txData );

    if (NT_SUCCESS( status )) {

        status = ObReferenceObjectByHandle( enlistmentHandle,
                                            0,
                                            NULL,
                                            KernelMode,
                                            &enlistmentObject,
                                            NULL );

        if (!NT_SUCCESS(status)) {

            ASSERT(!"Filespy: ObReferenceObjectByHandle failed for an enlistment object");

            ZwClose( enlistmentHandle );
        }
    }

    //
    //  Free the transaction context on error.
    //

    if (!NT_SUCCESS( status )) {

        ExAcquireFastMutex( &DeviceExtension->TxListLock );
        RemoveEntryList( &txData->List );
        ExReleaseFastMutex( &DeviceExtension->TxListLock );

        ExFreeToNPagedLookasideList( &gTransactionList,
                                     txData );
    }

    return status;
}

NTSTATUS
SpyCheckTransaction (
    __in PIRP Irp
    )
/*++

Routine Description

    This routine is called after the file system return a status success for
    IRP_MJ_CREATE. It checks if the file object is bound to a transaction after
    the create, and enlists in the transaction using the RM created earlier
    if it has not been done yet.

Arguments

    Irp - Pointer to the I/o request packet to be checked.

Return Value

    Returns the status of this operation.

--*/
{
    PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation( Irp );
    NTSTATUS status = STATUS_SUCCESS;

    if (!IS_VISTA_OR_LATER()) {

        return STATUS_SUCCESS;
    }

    ASSERT( NULL != gSpyDynamicFunctions.GetTransactionParameterBlock );

    //
    //  Only need to check a transaction for a file object in PostCreate.
    //

    if ((IrpStack->MajorFunction == IRP_MJ_CREATE) &&
        (IrpStack->FileObject != NULL)) {

        PTXN_PARAMETER_BLOCK transParms;

        transParms = (gSpyDynamicFunctions.GetTransactionParameterBlock)( IrpStack->FileObject );

        //
        //  If the file object is bound to a transaction, enlist in it.
        //

        if ((transParms != NULL) &&
            (transParms->TransactionObject != NULL)) {

            status = SpyEnlistInTransaction( transParms->TransactionObject,
                                             gKtmResourceManager,
                                             IrpStack->DeviceObject,
                                             IrpStack->FileObject );
        }
    }

    return status;
}

NTSTATUS
SpyLogTransactionNotify (
    __in PFILESPY_TRANSACTION_CONTEXT txData,
    __in ULONG TransactionNotification
    )
/*++

Routine Description:

    This routine will log the transacation notification information.

Arguments:

    txData - Pointer to the transaction context data.

    TransactionNotification - Transaction notification code.

Return Value:

    The function value is the status of the operation.

--*/
{

    PDEVICE_OBJECT deviceObject = txData->DeviceObject;
    PFILE_OBJECT fileObject = txData->FileObject;

    PAGED_CODE();

    ASSERT( IS_FILESPY_DEVICE_OBJECT( deviceObject ) );

    //
    //  We will log the transaction record only if the logging of the
    //  involved device object is enabled.
    //

    if (SHOULD_LOG( deviceObject )) {

        PRECORD_LIST recordList = SpyNewRecord(0);

        if (recordList != NULL) {

            //
            //  Log the necessary information
            //

            PRECORED_TRANSACTION recordTx = &recordList->LogRecord.Record.RecordTx;

            SetFlag( recordList->LogRecord.RecordType, RECORD_TYPE_TRANSACTION );

            recordTx->TransactionNotification = TransactionNotification;
            recordTx->Transaction = (TX_ID)txData->Transaction;
            recordTx->FileObject = (FILE_ID)fileObject;
            recordTx->DeviceObject = (DEVICE_ID)deviceObject;
            recordTx->ProcessId = (FILE_ID)PsGetCurrentProcessId();
            recordTx->ThreadId = (FILE_ID)PsGetCurrentThreadId();

            KeQuerySystemTime( &recordTx->OriginatingTime );

            //
            //  Note there can be multiple file objects bound to a transaction,
            //  but TxContext only contains one of the file objects that triggers
            //  the enlistment. Here we query the file name for that particular
            //  file object.
            //

            SpySetName( recordList,
                        deviceObject,
                        fileObject,
                        NLFL_ONLY_CHECK_CACHE,
                        NULL );

            //
            //  Add RecordList to our gOutputBufferList so that it gets up to
            //  the user
            //

            SpyLog( recordList );
        }
    }

    return STATUS_SUCCESS;
}

VOID
SpyDumpTransactionNotify (
    __in PFILESPY_TRANSACTION_CONTEXT txData,
    __in ULONG TransactionNotification
    )
/*++

Routine Description:

    This routine is for debugging and prints out a string to the
    debugger specifying what transaction notification is being seen.

Arguments:

    txData - Pointer to the transaction context data.

    TransactionNotification - Transaction notification code.

Return Value:

    None.

--*/
{
    CHAR notificationString[OPERATION_NAME_BUFFER_SIZE];


    GetTxNotifyName( TransactionNotification,
                     notificationString,
                     sizeof(notificationString) );

    DbgPrint( "FILESPY: Transaction (%p) notificatoin %s\n",
              txData->Transaction,
              notificationString );
}

NTSTATUS
SpyIsAttachedToNtfs (
    __in PDEVICE_OBJECT DeviceObject,
    __out PBOOLEAN AttachToNtfs
    )
/*++

Routine Description:

    This routine determines if this device object is attached to a NTFS
    file system stack.

Arguments:

    DeviceObject - Pointer to device object Filespy attached to the file system
        filter stack.

    AttachToNtfs - Pointer to receive the result.

Return Value:

    The function value is the status of the operation.

--*/
{
    PNAME_CONTROL driverName;
    NTSTATUS status;

    if (!IS_WINDOWSXP_OR_LATER()) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    //  Get the base file system device object.
    //

    ASSERT( NULL != gSpyDynamicFunctions.GetDeviceAttachmentBaseRef );
    DeviceObject = (gSpyDynamicFunctions.GetDeviceAttachmentBaseRef)( DeviceObject );

    status = NLAllocateNameControl( &driverName, &gFileSpyNameBufferLookasideList );

    if (!NT_SUCCESS( status )) {

        return status;
    }

    driverName->Name.Length = 0;

    try {

        //
        //  Get the name of driver.
        //

        status = NLGetObjectName( DeviceObject->DriverObject, driverName );

        if (!NT_SUCCESS(status)) {

            leave;
        }

        if (driverName->Name.Length == 0) {

            *AttachToNtfs = FALSE;
            leave;
        }

        //
        //  Compare to "\\FileSystem\\Ntfs"
        //

        *AttachToNtfs = RtlEqualUnicodeString( &driverName->Name,
                                               &gNtfsDriverName,
                                               TRUE );

    } finally {

        //
        //  Remove the reference added by IoGetDeviceAttachmentBaseRef.
        //

        ObDereferenceObject( DeviceObject );

        NLFreeNameControl( driverName, &gFileSpyNameBufferLookasideList );
    }

    return status;
}

#endif

