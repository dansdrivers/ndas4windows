/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    MiniSpy.c

Abstract:

    This is the main module for the MiniSpy mini-filter.

Environment:

    Kernel mode

--*/

#include "LfsProc.h"

#if __NDAS_FS_MINI__

#if !__NDAS_FS_MINI__
#include "mspyKern.h"
#include <stdio.h>
#endif

//
//  Global variables
//

MINISPY_DATA MiniSpyData;
NTSTATUS StatusToBreakOn = 0;

//---------------------------------------------------------------------------
//  Function prototypes
//---------------------------------------------------------------------------

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );


NTSTATUS
SpyMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    );

NTSTATUS
SpyConnect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    );

VOID
SpyDisconnect(
    __in_opt PVOID ConnectionCookie
    );

NTSTATUS
MiniSpyEnlistInTransaction (
    __in PCFLT_RELATED_OBJECTS FltObjects
    );

//---------------------------------------------------------------------------
//  Assign text sections for each routine.
//---------------------------------------------------------------------------

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT, DriverEntry)
    #pragma alloc_text(PAGE, SpyFilterUnload)
    #pragma alloc_text(PAGE, SpyQueryTeardown)
    #pragma alloc_text(PAGE, SpyConnect)
    #pragma alloc_text(PAGE, SpyDisconnect)
    #pragma alloc_text(PAGE, SpyMessage)
#endif

//---------------------------------------------------------------------------
//                      ROUTINES
//---------------------------------------------------------------------------

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine is called when a driver first loads.  Its purpose is to
    initialize global state and then register with FltMgr to start filtering.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.
    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Status of the operation.

--*/
{
    PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;
    NTSTATUS status;

#if __NDAS_FS_MINI__

#if DBG
	DbgPrint( "Minispy DriverEntry %s %s\n", __DATE__, __TIME__ );
	DbgPrint( "IRP_MJ_MDL_READ %d, %x\n", (CHAR)IRP_MJ_MDL_READ, IRP_MJ_MDL_READ );
#endif

#endif

	if (IS_WINDOWSVISTA())
		return STATUS_UNSUCCESSFUL;

    try {

        //
        // Initialize global data structures.
        //

        MiniSpyData.LogSequenceNumber = 0;
        MiniSpyData.MaxRecordsToAllocate = DEFAULT_MAX_RECORDS_TO_ALLOCATE;
        MiniSpyData.RecordsAllocated = 0;
        MiniSpyData.NameQueryMethod = DEFAULT_NAME_QUERY_METHOD;

        MiniSpyData.DriverObject = DriverObject;

#if __NDAS_FS_MINI__

        InitializeListHead( &MiniSpyData.OutputBufferList );
        KeInitializeSpinLock( &MiniSpyData.OutputBufferLock );

        ExInitializeNPagedLookasideList( &MiniSpyData.FreeBufferList,
                                         NULL,
                                         NULL,
                                         0,
                                         RECORD_SIZE,
                                         SPY_TAG,
                                         0 );

#endif

#if MINISPY_LONGHORN

        //
        //  Dynamically import FilterMgr APIs for transaction support
        //

        MiniSpyData.PFltSetTransactionContext = FltGetRoutineAddress( "FltSetTransactionContext" );
        MiniSpyData.PFltGetTransactionContext = FltGetRoutineAddress( "FltGetTransactionContext" );
        MiniSpyData.PFltEnlistInTransaction = FltGetRoutineAddress( "FltEnlistInTransaction" );

#endif

        //
        // Read the custom parameters for MiniSpy from the registry
        //

        SpyReadDriverParameters(RegistryPath);

        //
        //  Now that our global configuration is complete, register with FltMgr.
        //

        status = FltRegisterFilter( DriverObject,
                                    &FilterRegistration,
                                    &MiniSpyData.Filter );

        if (!NT_SUCCESS( status )) {

           leave;
        }


        status  = FltBuildDefaultSecurityDescriptor( &sd,
                                                     FLT_PORT_ALL_ACCESS );

        if (!NT_SUCCESS( status )) {
            leave;
        }

        RtlInitUnicodeString( &uniString, MINISPY_PORT_NAME );

        InitializeObjectAttributes( &oa,
                                    &uniString,
                                    OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    sd );

        status = FltCreateCommunicationPort( MiniSpyData.Filter,
                                             &MiniSpyData.ServerPort,
                                             &oa,
                                             NULL,
                                             SpyConnect,
                                             SpyDisconnect,
                                             SpyMessage,
                                             1 );

        FltFreeSecurityDescriptor( sd );

        if (!NT_SUCCESS( status )) {
            leave;
        }

#if __NDAS_FS_MINI__

		status = FileSpyDriverEntry( DriverObject, RegistryPath );

		if (!NT_SUCCESS( status )) {
			leave;
		}

		status = CtxDriverEntry( DriverObject, RegistryPath );

		if (!NT_SUCCESS( status )) {
			leave;
		}

#endif

        //
        //  We are now ready to start filtering
        //

        status = FltStartFiltering( MiniSpyData.Filter );

    } finally {

        if (!NT_SUCCESS( status ) ) {

#if __NDAS_FS_MINI__
			FileSpyDriverUnload( DriverObject );
#endif
             if (NULL != MiniSpyData.ServerPort) {
                 FltCloseCommunicationPort( MiniSpyData.ServerPort );
             }

             if (NULL != MiniSpyData.Filter) {
                 FltUnregisterFilter( MiniSpyData.Filter );
             }

#if __NDAS_FS_MINI__
             ExDeleteNPagedLookasideList( &MiniSpyData.FreeBufferList );
#endif
        }
    }

    return status;
}

NTSTATUS
SpyConnect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
/*++

Routine Description

    This is called when user-mode connects to the server
    port - to establish a connection

Arguments

    ClientPort - This is the pointer to the client port that
        will be used to send messages from the filter.
    ServerPortCookie - unused
    ConnectionContext - unused
    SizeofContext   - unused
    ConnectionCookie - unused

Return Value

    STATUS_SUCCESS - to accept the connection
--*/
{

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext);
    UNREFERENCED_PARAMETER( ConnectionCookie );

    ASSERT( MiniSpyData.ClientPort == NULL );
    MiniSpyData.ClientPort = ClientPort;
    return STATUS_SUCCESS;
}


VOID
SpyDisconnect(
    __in_opt PVOID ConnectionCookie
   )
/*++

Routine Description

    This is called when the connection is torn-down. We use it to close our handle to the connection

Arguments

    ConnectionCookie - unused

Return value

    None
--*/
{

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ConnectionCookie );

    //
    //  Close our handle
    //

    FltCloseClientPort( MiniSpyData.Filter, &MiniSpyData.ClientPort );
}

NTSTATUS
SpyFilterUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is called when a request has been made to unload the filter.  Unload
    requests from the Operation System (ex: "sc stop minispy" can not be
    failed.  Other unload requests may be failed.

    You can disallow OS unload request by setting the
    FLTREGFL_DO_NOT_SUPPORT_SERVICE_STOP flag in the FLT_REGISTARTION
    structure.

Arguments:

    Flags - Flags pertinent to this operation

Return Value:

    Always success

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

#if __NDAS_FS_MINI__
	FileSpyDriverUnload( MiniSpyData.DriverObject );
#endif

    //
    //  Close the server port. This will stop new connections.
    //

    FltCloseCommunicationPort( MiniSpyData.ServerPort );

    FltUnregisterFilter( MiniSpyData.Filter );

#if __NDAS_FS_MINI__
    SpyEmptyOutputBufferList();
    ExDeleteNPagedLookasideList( &MiniSpyData.FreeBufferList );
#endif

    return STATUS_SUCCESS;
}


NTSTATUS
SpyQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This allows our filter to be manually detached from a volume.

Arguments:

    FltObjects - Contains pointer to relevant objects for this operation.
        Note that the FileObject field will always be NULL.

    Flags - Flags pertinent to this operation

Return Value:

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    PAGED_CODE();
    return STATUS_SUCCESS;
}


NTSTATUS
SpyMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
/*++

Routine Description:

    This is called whenever a user mode application wishes to communicate
    with this minifilter.

Arguments:

    ConnectionCookie - unused

    OperationCode - An identifier describing what type of message this
        is.  These codes are defined by the MiniFilter.
    InputBuffer - A buffer containing input data, can be NULL if there
        is no input data.
    InputBufferSize - The size in bytes of the InputBuffer.
    OutputBuffer - A buffer provided by the application that originated
        the communication in which to store data to be returned to this
        application.
    OutputBufferSize - The size in bytes of the OutputBuffer.
    ReturnOutputBufferSize - The size in bytes of meaningful data
        returned in the OutputBuffer.

Return Value:

    Returns the status of processing the message.

--*/
{
    MINISPY_COMMAND command;
    NTSTATUS status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ConnectionCookie );

    //
    //                      **** PLEASE READ ****
    //
    //  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
    //  manager has already done a ProbedForRead (on InputBuffer) and
    //  ProbedForWrite (on OutputBuffer) which guarentees they are valid
    //  addresses based on the access (user mode vs. kernel mode).  The
    //  minifilter does not need to do their own probe.
    //
    //  The filter manager is NOT doing any alignment checking on the pointers.
    //  The minifilter must do this themselves if they care (see below).
    //
    //  The minifilter MUST continue to use a try/except around any access to
    //  these buffers.
    //

    if ((InputBuffer != NULL) &&
        (InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE,Command) +
                             sizeof(MINISPY_COMMAND)))) {

        try  {

            //
            //  Probe and capture input message: the message is raw user mode
            //  buffer, so need to protect with exception handler
            //

            command = ((PCOMMAND_MESSAGE) InputBuffer)->Command;

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return GetExceptionCode();
        }

        switch (command) {

            case GetMiniSpyLog:

                //
                //  Return as many log records as can fit into the OutputBuffer
                //

                if ((OutputBuffer == NULL) || (OutputBufferSize == 0)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                //
                //  We want to validate that the given buffer is POINTER
                //  aligned.  But if this is a 64bit system and we want to
                //  support 32bit applications we need to be careful with how
                //  we do the check.  Note that the way SpyGetLog is written
                //  it actually does not care about alignment but we are
                //  demonstrating how to do this type of check.
                //

#if defined(_WIN64)
                if (IoIs32bitProcess( NULL )) {

                    //
                    //  Validate alignment for the 32bit process on a 64bit
                    //  system
                    //

                    if (!IS_ALIGNED(OutputBuffer,sizeof(ULONG))) {

                        status = STATUS_DATATYPE_MISALIGNMENT;
                        break;
                    }

                } else {
#endif

                    if (!IS_ALIGNED(OutputBuffer,sizeof(PVOID))) {

                        status = STATUS_DATATYPE_MISALIGNMENT;
                        break;
                    }

#if defined(_WIN64)
                }
#endif
                //
                //  Get the log record.
                //

                status = MiniSpyGetLog( OutputBuffer,
                                    OutputBufferSize,
                                    ReturnOutputBufferLength );
                break;


            case GetMiniSpyVersion:

                //
                //  Return version of the MiniSpy filter driver.  Verify
                //  we have a valid user buffer including valid
                //  alignment
                //

                if ((OutputBufferSize < sizeof( MINISPYVER )) ||
                    (OutputBuffer == NULL)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                //
                //  Validate Buffer alignment.  If a minifilter cares about
                //  the alignment value of the buffer pointer they must do
                //  this check themselves.  Note that a try/except will not
                //  capture alignment faults.
                //

                if (!IS_ALIGNED(OutputBuffer,sizeof(ULONG))) {

                    status = STATUS_DATATYPE_MISALIGNMENT;
                    break;
                }

                //
                //  Protect access to raw user-mode output buffer with an
                //  exception handler
                //

                try {

                    ((PMINISPYVER)OutputBuffer)->Major = MINISPY_MAJ_VERSION;
                    ((PMINISPYVER)OutputBuffer)->Minor = MINISPY_MIN_VERSION;

                } except( EXCEPTION_EXECUTE_HANDLER ) {

                      return GetExceptionCode();
                }

                *ReturnOutputBufferLength = sizeof( MINISPYVER );
                status = STATUS_SUCCESS;
                break;

            default:
                status = STATUS_INVALID_PARAMETER;
                break;
        }

    } else {

        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}

typedef struct _NDASFS_COMPLETION_CONTEXT {

	PNETDISK_PARTITION		NetdiskPartition;
	NETDISK_ENABLE_MODE		NetdiskEnableMode;	

} NDASFS_COMPLETION_CONTEXT, *PNDASFS_COMPLETION_CONTEXT;

//---------------------------------------------------------------------------
//              Operation filtering routines
//---------------------------------------------------------------------------


FLT_PREOP_CALLBACK_STATUS
SpyPreOperationCallback (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine receives ALL pre-operation callbacks for this filter.  It then
    tries to log information about the given operation.  If we are able
    to log information then we will call our post-operation callback  routine.

    NOTE:  This routine must be NON-PAGED because it can be called on the
           paging path.

Arguments:

    Data - Contains information about the given operation.

    FltObjects - Contains pointers to the various objects that are pertinent
        to this operation.

    CompletionContext - This receives the address of our log buffer for this
        operation.  Our completion routine then receives this buffer address.

Return Value:

    Identifies how processing should continue for this operation

--*/
{
    FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK; //assume we are NOT going to call our completion routine
    PMINI_RECORD_LIST recordList;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    UNICODE_STRING defaultName;
    PUNICODE_STRING nameToUse;
    NTSTATUS status;
#if MINISPY_NOT_W2K
    WCHAR name[MINI_MAX_NAME_SPACE/sizeof(WCHAR)];
#endif

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	if (Data->Iopb->MajorFunction == IRP_MJ_VOLUME_MOUNT) {

		PFLT_VOLUME_PROPERTIES	VolumeProperties = NULL;
		ULONG					propertyLengthReturned; 

		UNICODE_STRING			ntfs;
		UNICODE_STRING			ndasNtfs;
		UNICODE_STRING			fat;
		UNICODE_STRING			ndasFat;

		PNETDISK_PARTITION		netdiskPartition;
		NETDISK_ENABLE_MODE		netdiskEnableMode;

		PDEVICE_OBJECT			deviceObject = NULL;
		PDEVICE_OBJECT			diskDeviceObject = NULL;
		

	    DebugTrace( DEBUG_TRACE_INSTANCES,
		            ("[MiniSpy]: Instance setup started FltObjects = %p\n", FltObjects) );

	    DebugTrace( DEBUG_TRACE_INSTANCES,
		            ("[MiniSpy]: Instance setup started (Volume = %p, Instance = %p)\n",
			         FltObjects->Volume,
				     FltObjects->Instance) );

		status = FltGetVolumeProperties( FltObjects->Volume,
										 NULL,
										 0,
										 &propertyLengthReturned ); 


		DebugTrace( DEBUG_TRACE_INSTANCES,
					("[MiniSpy]: IRP_MJ_VOLUME_MOUNT FltGetVolumeProperties, status = %x, propertyLengthReturned = %d\n",
					 status,
					 propertyLengthReturned) );

		VolumeProperties = ExAllocatePoolWithTag( PagedPool, propertyLengthReturned, CTX_VOLUME_PROPERTY_TAG );

		status = FltGetVolumeProperties( FltObjects->Volume,
										 VolumeProperties,
										 propertyLengthReturned,
										 &propertyLengthReturned ); 

		DebugTrace( DEBUG_TRACE_INSTANCES,
					("[MiniSpy]: FltGetVolumeProperties, status = %x, propertyLengthReturned = %d\n",
					 status,
					 propertyLengthReturned) );

		if( !NT_SUCCESS( status )) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		}

		DebugTrace( DEBUG_TRACE_INSTANCES,
					("[MiniSpy]: FltGetVolumeProperties, DeviceType = %d "
					 "FileSystemDriverName = %wZ\n"
					 "FileSystemDeviceName = %wZ "
					 "RealDeviceName = %wZ\n",
					 VolumeProperties->DeviceType,
					 &VolumeProperties->FileSystemDriverName,
					 &VolumeProperties->FileSystemDeviceName,
					 &VolumeProperties->RealDeviceName) );

		RtlInitUnicodeString( &ntfs, L"\\Ntfs" );
		RtlInitUnicodeString( &ndasNtfs, NDAS_NTFS_DEVICE_NAME );
		RtlInitUnicodeString( &fat, L"\\Fat" );
		RtlInitUnicodeString( &ndasFat, NDAS_FAT_DEVICE_NAME );

		if (!(RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)		||
			  RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)	||
			  RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &fat, TRUE)		||
			  RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE))) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		}

		status = FltGetDeviceObject( FltObjects->Volume, &deviceObject );

		if (!NT_SUCCESS(status)) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		}
	
		status = FltGetDiskDeviceObject( FltObjects->Volume, &diskDeviceObject );

		if (!NT_SUCCESS(status)) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		}

		DebugTrace( DEBUG_TRACE_INSTANCES,
					("DeviceObject = %p, DiskDeviceObject = %p\n", deviceObject, diskDeviceObject) );


		if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE) && !GlobalLfs.NdasFatRwSupport && !GlobalLfs.NdasFatRoSupport ||
			RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE) && !GlobalLfs.NdasNtfsRwSupport && !GlobalLfs.NdasNtfsRoSupport) {

			Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
			Data->IoStatus.Information = 0;

			returnStatus = FLT_PREOP_COMPLETE;
			goto CtxInstanceSetupCleanup;
		}

		if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

			status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
													GlobalLfs.NdasFatRwIndirect ? TRUE : FALSE,
													TRUE,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
													&netdiskPartition,
													&netdiskEnableMode );
	
		} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

			status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
													GlobalLfs.NdasNtfsRwIndirect ? TRUE : FALSE,
													TRUE,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
													&netdiskPartition,
													&netdiskEnableMode );
	
		} else {

			status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
													FALSE,
													TRUE,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
													diskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
													&netdiskPartition,
													&netdiskEnableMode );
		}	

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("NetdiskManager_IsNetdiskPartition status = %x\n", status) );

		if (!NT_SUCCESS(status)) {

			if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE) ||
				RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

				Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
				Data->IoStatus.Information = 0;

				returnStatus = FLT_PREOP_COMPLETE;
				goto CtxInstanceSetupCleanup;
			}

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		} 

		if (status == STATUS_VOLUME_MOUNTED) {

			NDAS_ASSERT( GlobalLfs.NdasFsMiniMode == TRUE );
			NDAS_ASSERT( netdiskEnableMode == NETDISK_READ_ONLY );

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			goto CtxInstanceSetupCleanup;
		}

		NDAS_ASSERT( netdiskEnableMode != NETDISK_READ_ONLY );

		switch (netdiskEnableMode) {

		case NETDISK_READ_ONLY:

			if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &fat, TRUE)) {

				if (GlobalLfs.NdasFatRoSupport) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );
	
					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}			
			
			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

				if (!GlobalLfs.NdasFatRoSupport) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );
	
					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}			

			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)) {

				if (GlobalLfs.NdasNtfsRoSupport) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );

					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}
			
			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

				if (!GlobalLfs.NdasNtfsRoSupport) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );

					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}
			
			} else {

				NDAS_ASSERT( FALSE );
			}

			break;

		case NETDISK_SECONDARY:
		case NETDISK_PRIMARY:
		case NETDISK_SECONDARY2PRIMARY:

			if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &fat, TRUE)) {

				if (GlobalLfs.NdasFatRwSupport &&
					FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );
	
					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}			
			
			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

				if (!(GlobalLfs.NdasFatRwSupport &&
					  FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE))) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );
	
					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}			

			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)) {

				if (GlobalLfs.NdasNtfsRwSupport &&
					FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );

					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}
			
			} else if (RtlEqualUnicodeString(&VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

				if (!(GlobalLfs.NdasNtfsRwSupport &&
					  FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE))) {

					NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
													netdiskPartition,
													netdiskEnableMode,
													FALSE,
													0,
													NULL,
													NULL );

					Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
					Data->IoStatus.Information = 0;

					returnStatus = FLT_PREOP_COMPLETE;
					goto CtxInstanceSetupCleanup;
				}
			
			} else {

				NDAS_ASSERT( FALSE );
			}
	
			break;

		default:

			NDAS_ASSERT( FALSE );
			break;
		}

#if 1

		*CompletionContext = ExAllocatePoolWithTag( PagedPool, sizeof(NDASFS_COMPLETION_CONTEXT), CTX_COMPLETION_CONTEXT_TAG );
		((PNDASFS_COMPLETION_CONTEXT)(*CompletionContext))->NetdiskPartition = netdiskPartition;
		((PNDASFS_COMPLETION_CONTEXT)(*CompletionContext))->NetdiskEnableMode = netdiskEnableMode;

		returnStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

#else
		NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
										netdiskPartition,
										netdiskEnableMode,
										FALSE,
										0,
										NULL,
										NULL );


		*CompletionContext = NULL;
		returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

#endif

		goto CtxInstanceSetupCleanup;

CtxInstanceSetupCleanup:

		if (diskDeviceObject) {

			ObDereferenceObject( diskDeviceObject );
		}

		if (deviceObject) {

			ObDereferenceObject( deviceObject );
		}

		if (VolumeProperties) {

			ExFreePoolWithTag( VolumeProperties, CTX_VOLUME_PROPERTY_TAG );
		}

		return returnStatus;
	}

	do {

		PCTX_INSTANCE_CONTEXT	instanceContext = NULL;
		KIRQL					oldIrql2 = KeGetCurrentIrql();

		if (FltObjects->Instance == NULL) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			break;
		}

		status = FltGetInstanceContext( FltObjects->Instance, &instanceContext );

		if (!(Data->Iopb->MajorFunction == IRP_MJ_CREATE					||
			  Data->Iopb->MajorFunction == IRP_MJ_CLOSE						||
			  Data->Iopb->MajorFunction == IRP_MJ_READ						||
			  Data->Iopb->MajorFunction == IRP_MJ_WRITE						||
			  Data->Iopb->MajorFunction == IRP_MJ_QUERY_INFORMATION			||
			  Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION			||
			  Data->Iopb->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION	||
			  Data->Iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL			||
			  Data->Iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL		||
			  Data->Iopb->MajorFunction == IRP_MJ_CLEANUP)) {

			DebugTrace( DEBUG_TRACE_CREATE, 
						("[MiniSpy]: SpyPreOperationCallback instanceContext = %p status = %x Data->Iopb->MajorFunction = %x fileObject: %p fileName: %wZ\n",
						 instanceContext, status, Data->Iopb->MajorFunction, FltObjects->FileObject, &FltObjects->FileObject->FileName) );
		}

		if (IRP_MJ_CREATE <= Data->Iopb->MajorFunction && 
			Data->Iopb->MajorFunction <= IRP_MJ_MAXIMUM_FUNCTION &&
			Data->Iopb->MajorFunction != IRP_MJ_SHUTDOWN) {

			if (instanceContext->LfsDeviceExt.FilteringMode == LFS_READONLY) {

				returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
				//returnStatus = NdasFsReadonlyPreOperationCallback( FltObjects, instanceContext, Data );

			} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_PRIMARY ||
					   instanceContext->LfsDeviceExt.FilteringMode == LFS_SECONDARY_TO_PRIMARY) {

				returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
				returnStatus = NdasFsGeneralPreOperationCallback( FltObjects, instanceContext, Data );

			} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_FILE_CONTROL) {

				returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	
			} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_SECONDARY) {

				if (!FlagOn(instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) &&
					(instanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
					 instanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

					PFILE_OBJECT		fileObject = Data->Iopb->TargetFileObject;

					returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

					if (Data->Iopb->MajorFunction == IRP_MJ_CREATE) {

						if ((fileObject->FileName.Length == (sizeof(REMOUNT_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
							RtlEqualMemory(fileObject->FileName.Buffer, REMOUNT_VOLUME_FILE_NAME, fileObject->FileName.Length)) ||
							(fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
							 RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length))) {

							//PrintIrp( LFS_DEBUG_LFS_TRACE, "TOUCH/REMOUNT_VOLUME_FILE_NAME", &devExt->LfsDeviceExt, Irp );

							Data->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
							Data->IoStatus.Information = 0;

							returnStatus = FLT_PREOP_COMPLETE;
						}
					}

					if (returnStatus != FLT_PREOP_COMPLETE) {

						returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
						returnStatus = NdasFsGeneralPreOperationCallback( FltObjects, instanceContext, Data );
					}

				} else {

					returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
					returnStatus = NdasFsSecondaryPreOperationCallback( FltObjects, instanceContext, Data );
				}

			} else {

				NDAS_ASSERT( FALSE );
				returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			}

			NDAS_ASSERT( oldIrql2 == KeGetCurrentIrql() );

			DebugTrace( DEBUG_TRACE_CREATE, ("[MiniSpy]: returnStatus = %x\n", returnStatus) );

		} else if (Data->Iopb->MajorFunction == IRP_MJ_SHUTDOWN) {

			ExAcquireFastMutex( &GlobalLfs.FastMutex );

			if (GlobalLfs.ShutdownOccured == TRUE) {

				ExReleaseFastMutex( &GlobalLfs.FastMutex );
		
			} else {

				KIRQL			oldIrql;
				PLIST_ENTRY		lfsDeviceExtListEntry;
				LARGE_INTEGER	interval;

		
				GlobalLfs.ShutdownOccured = TRUE;
				ExReleaseFastMutex( &GlobalLfs.FastMutex );

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("file system type = %d\n", instanceContext->LfsDeviceExt.FileSystemType) );

				KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );

				for (lfsDeviceExtListEntry = GlobalLfs.LfsDeviceExtQueue.Flink;
					 lfsDeviceExtListEntry != &GlobalLfs.LfsDeviceExtQueue;
					 lfsDeviceExtListEntry = lfsDeviceExtListEntry->Flink) {

					PLFS_DEVICE_EXTENSION volumeLfsDeviceExt;

					volumeLfsDeviceExt = CONTAINING_RECORD( lfsDeviceExtListEntry, LFS_DEVICE_EXTENSION, LfsQListEntry );
					SetFlag( volumeLfsDeviceExt->Flags, LFS_DEVICE_FLAG_SHUTDOWN );

					if (volumeLfsDeviceExt->FilteringMode == LFS_FILE_CONTROL			&& 
						FlagOn(volumeLfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED)		&& 
						(volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
						 volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

						NTSTATUS			deviceControlStatus;
						IO_STATUS_BLOCK		ioStatusBlock;
						ULONG				ioControlCode;
						ULONG				inputBufferLength;
						ULONG				outputBufferLength;


						ioControlCode		= IOCTL_SHUTDOWN; 
						inputBufferLength	= 0;
						outputBufferLength	= 0;

						RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

						KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );

						deviceControlStatus = LfsFilterDeviceIoControl( volumeLfsDeviceExt->BaseVolumeDeviceObject,
																		ioControlCode,
																		NULL,
																		inputBufferLength,
																		NULL,
																		outputBufferLength,
																		NULL );

						KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );
					}
				}

				KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );

				NetdiskManager_FileSystemShutdown( GlobalLfs.NetdiskManager );
				Primary_FileSystemShutdown( GlobalLfs.Primary );

			   interval.QuadPart = (2 * DELAY_ONE_SECOND);      //for primarySession to close files 
				KeDelayExecutionThread( KernelMode, FALSE, &interval );		
			}

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
		
		} else if (Data->Iopb->MajorFunction == IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION ||
				   Data->Iopb->MajorFunction == IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION ||
				   Data->Iopb->MajorFunction == IRP_MJ_ACQUIRE_FOR_MOD_WRITE ||
				   Data->Iopb->MajorFunction == IRP_MJ_RELEASE_FOR_MOD_WRITE ||
				   Data->Iopb->MajorFunction == IRP_MJ_ACQUIRE_FOR_CC_FLUSH ||
				   Data->Iopb->MajorFunction == IRP_MJ_RELEASE_FOR_CC_FLUSH) {

			//Data->IoStatus.Status = STATUS_SUCCESS;
			//Data->IoStatus.Information = 0;

			//returnStatus = FLT_PREOP_COMPLETE;

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
		
		} else if (Data->Iopb->MajorFunction == IRP_MJ_VOLUME_MOUNT) {

			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

		} else if (Data->Iopb->MajorFunction == IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE ||
				   Data->Iopb->MajorFunction == IRP_MJ_NETWORK_QUERY_OPEN) {
	
		   returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

		} else if (Data->Iopb->MajorFunction == IRP_MJ_MDL_READ_COMPLETE ||
				   Data->Iopb->MajorFunction == IRP_MJ_MDL_WRITE_COMPLETE) {
	
		   returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

		} else {

			NDAS_ASSERT( FALSE );
			returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
		}

		if (instanceContext) {

			FltReleaseContext( instanceContext );
		}

	} while(0);

	return returnStatus;

#endif

    //
    //  Try and get a log record
    //

    recordList = MiniSpyNewRecord();

    if (recordList) {

        //
        //  We got a log record, if there is a file object, get its name.
        //
        //  NOTE: By default, we use the query method
        //  FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP
        //  because MiniSpy would like to get the name as much as possible, but
        //  can cope if we can't retrieve a name.  For a debugging type filter,
        //  like Minispy, this is reasonable, but for most production filters
        //  who need names reliably, they should query the name at times when it
        //  is known to be safe and use the query method
        //  FLT_FILE_NAME_QUERY_DEFAULT.
        //

        if (FltObjects->FileObject != NULL) {

            status = FltGetFileNameInformation( Data,
                                                FLT_FILE_NAME_NORMALIZED |
                                                MiniSpyData.NameQueryMethod,
                                                &nameInfo );

        } else {

            //
            //  Can't get a name when there's no file object
            //

            status = STATUS_UNSUCCESSFUL;
        }

        //
        //  Use the name if we got it else use a default name
        //

        if (NT_SUCCESS( status )) {

            nameToUse = &nameInfo->Name;

            //
            //  Parse the name if requested
            //

            if (FlagOn( MiniSpyData.DebugFlags, SPY_DEBUG_PARSE_NAMES )) {

                status = FltParseFileNameInformation( nameInfo );
                ASSERT(NT_SUCCESS(status));
            }

        } else {

#if MINISPY_NOT_W2K
            NTSTATUS lstatus;
            PFLT_FILE_NAME_INFORMATION lnameInfo;

            //
            //  If we couldn't get the "normalized" name try and get the
            //  "opened" name
            //

            if (FltObjects->FileObject != NULL) {

                //
                //  Get the opened name
                //

                lstatus = FltGetFileNameInformation( Data,
                                                     FLT_FILE_NAME_OPENED |
                                                            FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,
                                                     &lnameInfo );


                if (NT_SUCCESS(lstatus)) {

#pragma prefast(suppress:__WARNING_BANNED_API_USAGE, "reviewed and safe usage")
                    (VOID)_snwprintf( name,
                                      sizeof(name)/sizeof(WCHAR),
                                      L"<%08x> %wZ",
                                      status,
                                      &lnameInfo->Name );

                    FltReleaseFileNameInformation( lnameInfo );

                } else {

                    //
                    //  If that failed report both NORMALIZED status and
                    //  OPENED status
                    //

#pragma prefast(suppress:__WARNING_BANNED_API_USAGE, "reviewed and safe usage")
                    (VOID)_snwprintf( name,
                                      sizeof(name)/sizeof(WCHAR),
                                      L"<NO NAME: NormalizeStatus=%08x OpenedStatus=%08x>",
                                      status,
                                      lstatus );
                }

            } else {

#pragma prefast(suppress:__WARNING_BANNED_API_USAGE, "reviewed and safe usage")
                (VOID)_snwprintf( name,
                                  sizeof(name)/sizeof(WCHAR),
                                  L"<NO NAME>" );

            }

            RtlInitUnicodeString( &defaultName, name );
            nameToUse = &defaultName;
#else
            //
            //  We were unable to get the String safe routine to work on W2K
            //  Do it the old safe way
            //

            RtlInitUnicodeString( &defaultName, L"<NO NAME>" );
            nameToUse = &defaultName;
#endif //MINISPY_NOT_W2K


#if DBG
            //
            //  Debug support to break on certain errors.
            //

            if (FltObjects->FileObject != NULL) {
                NTSTATUS retryStatus;

                if ((StatusToBreakOn != 0) && (status == StatusToBreakOn)) {

                    DbgBreakPoint();
                }

                retryStatus = FltGetFileNameInformation( Data,
                                                         FLT_FILE_NAME_NORMALIZED |
                                                             MiniSpyData.NameQueryMethod,
                                                         &nameInfo );
                ASSERT(retryStatus == status);
            }
#endif

        }

        //
        //  Store the name
        //

        SpySetRecordName( &(recordList->LogRecord), nameToUse );

        //
        //  Release the name information structure (if defined)
        //

        if (NULL != nameInfo) {

            FltReleaseFileNameInformation( nameInfo );
        }

        //
        //  Set all of the operation information into the record
        //

        SpyLogPreOperationData( Data, FltObjects, recordList );

        //
        //  Pass the record to our completions routine and return that
        //  we want our completion routine called.
        //

        if (Data->Iopb->MajorFunction == IRP_MJ_SHUTDOWN) {

            //
            //  Since completion callbacks are not supported for
            //  this operation, do the completion processing now
            //

            SpyPostOperationCallback( Data,
                                      FltObjects,
                                      recordList,
                                      0 );

            returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

        } else {

            *CompletionContext = recordList;
            returnStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
        }
    }

    return returnStatus;
}


FLT_POSTOP_CALLBACK_STATUS
SpyPostOperationCallback (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine receives ALL post-operation callbacks.  This will take
    the log record passed in the context parameter and update it with
    the completion information.  It will then insert it on a list to be
    sent to the usermode component.

    NOTE:  This routine must be NON-PAGED because it can be called at DPC level

Arguments:

    Data - Contains information about the given operation.

    FltObjects - Contains pointers to the various objects that are pertinent
        to this operation.

    CompletionContext - Pointer to the RECORD_LIST structure in which we
        store the information we are logging.  This was passed from the
        pre-operation callback

    Flags - Contains information as to why this routine was called.

Return Value:

    Identifies how processing should continue for this operation

--*/
{
    PMINI_RECORD_LIST recordList;
    PMINI_RECORD_LIST reparseRecordList = NULL;
    PMINI_LOG_RECORD reparseLogRecord;
    PFLT_TAG_DATA_BUFFER tagData;
    ULONG copyLength;

    UNREFERENCED_PARAMETER( FltObjects );

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	do {

		PCTX_INSTANCE_CONTEXT	instanceContext = NULL;
		KIRQL					oldIrql2 = KeGetCurrentIrql();
		NTSTATUS				status;

		if (Data->Iopb->MajorFunction == IRP_MJ_VOLUME_MOUNT) {

			DebugTrace( DEBUG_TRACE_INSTANCES, ("[MiniSpy]: Data->IoStatus.Status = %x\n", Data->IoStatus.Status) );

			NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
											((PNDASFS_COMPLETION_CONTEXT)CompletionContext)->NetdiskPartition,
											((PNDASFS_COMPLETION_CONTEXT)CompletionContext)->NetdiskEnableMode,
											FALSE,
											0,
											NULL,
											NULL );

			ExFreePoolWithTag( CompletionContext, CTX_COMPLETION_CONTEXT_TAG );

			break;
		}

		if (FltObjects->Instance == NULL) {

			break;
		}

		if (!(Data->Iopb->MajorFunction == IRP_MJ_PNP																	&&
			  Data->Iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE
			  ||
			  Data->Iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL													&&
			  (Data->Iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || Data->Iopb->MinorFunction == IRP_MN_KERNEL_CALL)	&&
			  Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_DISMOUNT_VOLUME)) {

			NDAS_ASSERT( FALSE );
			break;
		} 

		status = FltGetInstanceContext( FltObjects->Instance, &instanceContext );

		if (instanceContext->LfsDeviceExt.FilteringMode == LFS_READONLY) {

			NdasFsReadonlyPostOperationCallback( FltObjects, instanceContext, Data );

		} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_PRIMARY ||
				   instanceContext->LfsDeviceExt.FilteringMode == LFS_SECONDARY_TO_PRIMARY) {

			NdasFsGeneralPostOperationCallback( FltObjects, instanceContext, Data );

		} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_FILE_CONTROL) {

		} else if (instanceContext->LfsDeviceExt.FilteringMode == LFS_SECONDARY) {

			if (!FlagOn(instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) &&
				(instanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				 instanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

				NdasFsGeneralPostOperationCallback( FltObjects, instanceContext, Data );
	
			} else {

				NdasFsSecondaryPostOperationCallback( FltObjects, instanceContext, Data );
			}

		} else {

			NDAS_ASSERT( FALSE );
		}

		NDAS_ASSERT( oldIrql2 == KeGetCurrentIrql() );

		if (instanceContext) {

			FltReleaseContext( instanceContext );
		}

	} while (0);

	return FLT_POSTOP_FINISHED_PROCESSING;

#endif


    recordList = (PMINI_RECORD_LIST)CompletionContext;

    //
    //  If our instance is in the process of being torn down don't bother to
    //  log this record, free it now.
    //

    if (FlagOn(Flags,FLTFL_POST_OPERATION_DRAINING)) {

        MiniSpyFreeRecord( recordList );
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    //
    //  Set completion information into the record
    //

    SpyLogPostOperationData( Data, recordList );

    //
    //  Log reparse tag information if specified.
    //

    if (tagData = Data->TagData) {

        reparseRecordList = MiniSpyNewRecord();

        if (reparseRecordList) {

            //
            //  only copy the DATA portion of the information
            //

            RtlCopyMemory( &reparseRecordList->LogRecord.Data,
                           &recordList->LogRecord.Data,
                           sizeof(RECORD_DATA) );

            reparseLogRecord = &reparseRecordList->LogRecord;

            copyLength = FLT_TAG_DATA_BUFFER_HEADER_SIZE + tagData->TagDataLength;

            if(copyLength > MINI_MAX_NAME_SPACE) {

                copyLength = MINI_MAX_NAME_SPACE;
            }

            //
            //  Copy reparse data
            //

            RtlCopyMemory(
                &reparseRecordList->LogRecord.Name[0],
                tagData,
                copyLength
                );

            reparseLogRecord->RecordType |= RECORD_TYPE_FILETAG;
            reparseLogRecord->Length += (ULONG) ROUND_TO_SIZE( copyLength, sizeof( PVOID ) );
        }
    }

    //
    //  Send the logged information to the user service.
    //

    MiniSpyLog( recordList );

    if (reparseRecordList) {

        MiniSpyLog( reparseRecordList );
    }

    //
    //  For creates within a transaction enlist in the transaction
    //  if we haven't already done.
    //

    if ((FltObjects->Transaction != NULL) &&
        (Data->Iopb->MajorFunction == IRP_MJ_CREATE) &&
        (Data->IoStatus.Status == STATUS_SUCCESS)) {

        //
        //  Enlist in the transaction.
        //

        MiniSpyEnlistInTransaction( FltObjects );
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS
MiniSpyEnlistInTransaction (
    __in PCFLT_RELATED_OBJECTS FltObjects
    )
{

#if MINISPY_LONGHORN

    PMINISPY_TRANSACTION_CONTEXT transactionContext=NULL;
    PMINI_RECORD_LIST recordList;
    NTSTATUS status;
    static ULONG Sequence=1;

    //
    //  This code is only built in the Longhorn environment, but
    //  we need to ensure this binary still runs down-level.  Return
    //  at this point if the transaction dynamic imports were not found.
    //
    //  If we find FltGetTransactionContext, we assume the other
    //  transaction APIs are also present.
    //

    if (NULL == MiniSpyData.PFltGetTransactionContext) {

        return STATUS_SUCCESS;
    }

    //
    //  Try to get our context for this transaction. If we get
    //  one we have already enlisted in this transaction.
    //

    status = (*MiniSpyData.PFltGetTransactionContext)( FltObjects->Instance,
                                                       FltObjects->Transaction,
                                                       &transactionContext );

    if (NT_SUCCESS( status )) {

        FltReleaseContext( transactionContext );
        return STATUS_SUCCESS;
    }

    //
    //  If the context does not exist create a new one, else return the error
    //  status to the caller.
    //

    if (status != STATUS_NOT_FOUND) {

        return status;
    }

    //
    //  Allocate a transaction context.
    //

    status = FltAllocateContext( FltObjects->Filter,
                                 FLT_TRANSACTION_CONTEXT,
                                 sizeof(MINISPY_TRANSACTION_CONTEXT),
                                 PagedPool,
                                 &transactionContext );

    if (!NT_SUCCESS( status )) {

        return status;
    }

    //
    //  Set the context into the transaction
    //

    RtlZeroMemory(transactionContext, sizeof(MINISPY_TRANSACTION_CONTEXT));
    transactionContext->Count = Sequence++;

    ASSERT( MiniSpyData.PFltSetTransactionContext );

    status = (*MiniSpyData.PFltSetTransactionContext)( FltObjects->Instance,
                                                       FltObjects->Transaction,
                                                       FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                                       transactionContext,
                                                       NULL );

    if (!NT_SUCCESS( status )) {

        FltReleaseContext( transactionContext );    //this will free the context
        return status;
    }

    //
    //  Enlist on this transaction for notifications.
    //

    ASSERT( MiniSpyData.PFltEnlistInTransaction );

    status = (*MiniSpyData.PFltEnlistInTransaction)( FltObjects->Instance,
                                                     FltObjects->Transaction,
                                                     transactionContext,
                                                     FLT_MAX_TRANSACTION_NOTIFICATIONS );

    //
    //  If the enlistment failed we need to delete the context and remove
    //  our count
    //

    if (!NT_SUCCESS( status )) {

        FltDeleteContext( transactionContext );
        FltReleaseContext( transactionContext );

        return status;
    }

    //
    //  The operation succeeded, remove our count
    //

    FltReleaseContext( transactionContext );

    //
    //  Log a record that a new transaction has started.
    //

    recordList = MiniSpyNewRecord();

    if (recordList) {

        MiniSpyLogTransactionNotify( FltObjects, recordList, 0 );

        //
        //  Send the logged information to the user service.
        //

        MiniSpyLog( recordList );
    }

#endif // MINISPY_LONGHORN

#if __NDAS_FS_MINI__
	UNREFERENCED_PARAMETER( FltObjects );
#endif

    return STATUS_SUCCESS;
}


#if MINISPY_LONGHORN

NTSTATUS
SpyKtmNotificationCallback (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PFLT_CONTEXT TransactionContext,
    __in ULONG TransactionNotification
    )
{
    PMINI_RECORD_LIST recordList;

	UNREFERENCED_PARAMETER( TransactionContext );

    //
    //  Try and get a log record
    //

    recordList = MiniSpyNewRecord();

    if (recordList) {

        MiniSpyLogTransactionNotify( FltObjects, recordList, TransactionNotification );

        //
        //  Send the logged information to the user service.
        //

        MiniSpyLog( recordList );
    }

    return STATUS_SUCCESS;
}

#endif // MINISPY_LONGHORN

VOID
SpyDeleteTxfContext (
    __inout PMINISPY_TRANSACTION_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
    UNREFERENCED_PARAMETER( Context );
    UNREFERENCED_PARAMETER( ContextType );

    ASSERT(FLT_TRANSACTION_CONTEXT == ContextType);
    ASSERT(Context->Count != 0);
}

#endif
