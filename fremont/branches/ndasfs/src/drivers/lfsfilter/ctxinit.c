/*++

Copyright (c) 1999 - 2003  Microsoft Corporation

Module Name:

    ContextInit.c

Abstract:

    This is the main module of the kernel mode filter driver implementing
    the context sample.


Environment:

    Kernel mode


--*/

#include "LfsProc.h"

#if __NDAS_FS_MINI__

#if !__NDAS_FS_MINI__
#include "pch.h"
#endif

//
//  Global variables
//

CTX_GLOBAL_DATA Globals;


//
//  Local function prototypes
//

#if !__NDAS_FS_MINI__
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );
#endif

NTSTATUS
CtxUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    );

#if !__NDAS_FS_MINI__
VOID
CtxContextCleanup (
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    );

NTSTATUS
CtxInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

NTSTATUS
CtxInstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

VOID
CtxInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
CtxInstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

#endif

#if DBG

VOID
CtxInitializeDebugLevel (
    __in PUNICODE_STRING RegistryPath
    );

#endif

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#if !__NDAS_FS_MINI__ 
#pragma alloc_text(INIT, DriverEntry)
#else
#pragma alloc_text(INIT, CtxDriverEntry)
#endif

#if DBG
#pragma alloc_text(INIT, CtxInitializeDebugLevel)
#endif

#pragma alloc_text(PAGE, CtxUnload)
#pragma alloc_text(PAGE, CtxContextCleanup)
#pragma alloc_text(PAGE, CtxInstanceSetup)
#pragma alloc_text(PAGE, CtxInstanceQueryTeardown)
#pragma alloc_text(PAGE, CtxInstanceTeardownStart)
#pragma alloc_text(PAGE, CtxInstanceTeardownComplete)
#endif


//
//  Filter driver initialization and unload routines
//

#if !__NDAS_FS_MINI__

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )

#else

NTSTATUS
CtxDriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

#endif

/*++

Routine Description:

    This is the initialization routine for this filter driver. It registers
    itself with the filter manager and initializes all its global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    NTSTATUS status;

    //
    //  Filters callback routines
    //

    FLT_OPERATION_REGISTRATION callbacks[] = {

        { IRP_MJ_CREATE,
          FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
          CtxPreCreate,
          CtxPostCreate },

        { IRP_MJ_CLEANUP,
          FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
          CtxPreCleanup,
          NULL },

        { IRP_MJ_CLOSE,
          FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
          CtxPreClose,
          NULL },

        { IRP_MJ_SET_INFORMATION,
          FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
          CtxPreSetInfo,
          CtxPostSetInfo },

        { IRP_MJ_OPERATION_END }
    };

    const FLT_CONTEXT_REGISTRATION contextRegistration[] = {

        { FLT_INSTANCE_CONTEXT,
          0,
          CtxContextCleanup,
          CTX_INSTANCE_CONTEXT_SIZE,
          CTX_INSTANCE_CONTEXT_TAG },

        { FLT_FILE_CONTEXT,
          0,
          CtxContextCleanup,
          CTX_FILE_CONTEXT_SIZE,
          CTX_FILE_CONTEXT_TAG },

        { FLT_STREAM_CONTEXT,
          0,
          CtxContextCleanup,
          CTX_STREAM_CONTEXT_SIZE,
          CTX_STREAM_CONTEXT_TAG },

        { FLT_STREAMHANDLE_CONTEXT,
          0,
          CtxContextCleanup,
          CTX_STREAMHANDLE_CONTEXT_SIZE,
          CTX_STREAMHANDLE_CONTEXT_TAG },

        { FLT_CONTEXT_END }
    };

    //
    // Filters registration data structure
    //

#if !__NDAS_FS_MINI__ 
    FLT_REGISTRATION filterRegistration = {
#else
    FLT_REGISTRATION CtxFilterRegistration = {
#endif

        sizeof( FLT_REGISTRATION ),                     //  Size
        FLT_REGISTRATION_VERSION,                       //  Version
        0,                                              //  Flags
        contextRegistration,                            //  Context
        callbacks,                                      //  Operation callbacks
        CtxUnload,                                      //  Filters unload routine
        CtxInstanceSetup,                               //  InstanceSetup routine
        CtxInstanceQueryTeardown,                       //  InstanceQueryTeardown routine
        CtxInstanceTeardownStart,                       //  InstanceTeardownStart routine
        CtxInstanceTeardownComplete,                    //  InstanceTeardownComplete routine
        NULL, NULL, NULL                                //  Unused naming support callbacks
    };


    RtlZeroMemory( &Globals, sizeof( Globals ) );

#if DBG

    //
    //  Initialize global debug level
    //

    CtxInitializeDebugLevel( RegistryPath );

#else

    UNREFERENCED_PARAMETER( RegistryPath );

#endif

    DebugTrace( DEBUG_TRACE_LOAD_UNLOAD,
                ("[Ctx]: Driver being loaded\n") );

#if __NDAS_FS_MINI__ 

	UNREFERENCED_PARAMETER( DriverObject );
	UNREFERENCED_PARAMETER( RegistryPath );
	
	status = STATUS_SUCCESS;

#else

    //
    //  Register with the filter manager
    //

    status = FltRegisterFilter( DriverObject,
                                &filterRegistration,
                                &Globals.Filter );

    if (!NT_SUCCESS( status )) {

        return status;
    }

    //
    //  Start filtering I/O
    //

    status = FltStartFiltering( Globals.Filter );

    if (!NT_SUCCESS( status )) {

        FltUnregisterFilter( Globals.Filter );
    }

    DebugTrace( DEBUG_TRACE_LOAD_UNLOAD,
                ("[Ctx]: Driver loaded complete (Status = 0x%08X)\n",
                status) );

#endif

    return status;
}

#if DBG

VOID
CtxInitializeDebugLevel (
    __in PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine tries to read the filter DebugLevel parameter from
    the registry.  This value will be found in the registry location
    indicated by the RegistryPath passed in.

Arguments:

    RegistryPath - The path key passed to the driver during DriverEntry.

Return Value:

    None.

--*/
{
    OBJECT_ATTRIBUTES attributes;
    HANDLE driverRegKey;
    NTSTATUS status;
    ULONG resultLength;
    UNICODE_STRING valueName;
    UCHAR buffer[sizeof( KEY_VALUE_PARTIAL_INFORMATION ) + sizeof( LONG )];

    Globals.DebugLevel = DEBUG_TRACE_ERROR;
#if __NDAS_FS_MINI__ 
	Globals.DebugLevel |= DEBUG_TRACE_INSTANCES;
	Globals.DebugLevel |= DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS;
	Globals.DebugLevel |= DEBUG_INFO_CREATE;
	//Globals.DebugLevel |= 0xFFFFFFFF;
#endif

    //
    //  Open the desired registry key
    //

    InitializeObjectAttributes( &attributes,
                                RegistryPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    status = ZwOpenKey( &driverRegKey,
                        KEY_READ,
                        &attributes );

    if (NT_SUCCESS( status )) {

        //
        // Read the DebugFlags value from the registry.
        //

        RtlInitUnicodeString( &valueName, L"DebugLevel" );

        status = ZwQueryValueKey( driverRegKey,
                                  &valueName,
                                  KeyValuePartialInformation,
                                  buffer,
                                  sizeof(buffer),
                                  &resultLength );

        if (NT_SUCCESS( status )) {

            Globals.DebugLevel = *((PULONG) &(((PKEY_VALUE_PARTIAL_INFORMATION) buffer)->Data));
        }
    }

    //
    //  Close the registry entry
    //

    ZwClose( driverRegKey );
}

#endif

NTSTATUS
CtxUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this filter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unloaded indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns the final status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    DebugTrace( DEBUG_TRACE_LOAD_UNLOAD,
                ("[Ctx]: Unloading driver\n") );


    FltUnregisterFilter( Globals.Filter );
    Globals.Filter = NULL;

    return STATUS_SUCCESS;
}

VOID
CtxContextCleanup (
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
    PCTX_INSTANCE_CONTEXT instanceContext;
    PCTX_FILE_CONTEXT fileContext;
    PCTX_STREAM_CONTEXT streamContext;
    PCTX_STREAMHANDLE_CONTEXT streamHandleContext;

    PAGED_CODE();

    switch(ContextType) {

    case FLT_INSTANCE_CONTEXT:

        instanceContext = (PCTX_INSTANCE_CONTEXT) Context;

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Cleaning up instance context for volume %wZ (Context = %p)\n",
                     &instanceContext->VolumeName,
                     Context) );

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)
		if (instanceContext->DiskDeviceObject) {

			ObDereferenceObject( instanceContext->DiskDeviceObject );
		}

		if (instanceContext->DeviceObject) {

			ObDereferenceObject( instanceContext->DeviceObject );
		}

		if (instanceContext->VolumeProperties) {

			ExFreePoolWithTag( instanceContext->VolumeProperties, CTX_VOLUME_PROPERTY_TAG );
		}
#endif

        //
        //  Here the filter should free memory or synchronization objects allocated to
        //  objects within the instance context. The instance context itself should NOT
        //  be freed. It will be freed by Filter Manager when the ref count on the
        //  context falls to zero.
        //

        CtxFreeUnicodeString( &instanceContext->VolumeName );

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Instance context cleanup complete.\n") );

        break;


    case FLT_FILE_CONTEXT:

        fileContext = (PCTX_FILE_CONTEXT) Context;

        DebugTrace( DEBUG_TRACE_FILE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Cleaning up file context for file %wZ (FileContext = %p)\n",
                     &fileContext->FileName,
                     fileContext) );


        //
        //  Free the file name
        //

        if (fileContext->FileName.Buffer != NULL) {

            CtxFreeUnicodeString(&fileContext->FileName);
        }

        DebugTrace( DEBUG_TRACE_FILE_CONTEXT_OPERATIONS,
                    ("[Ctx]: File context cleanup complete.\n") );

        break;

    case FLT_STREAM_CONTEXT:

        streamContext = (PCTX_STREAM_CONTEXT) Context;

        DebugTrace( DEBUG_TRACE_STREAM_CONTEXT_OPERATIONS,
                    ("[Ctx]: Cleaning up stream context for file %wZ (StreamContext = %p) \n\tCreateCount = %x \n\tCleanupCount = %x, \n\tCloseCount = %x\n",
                     &streamContext->FileName,
                     streamContext,
                     streamContext->CreateCount,
                     streamContext->CleanupCount,
                     streamContext->CloseCount) );

        //
        //  Delete the resource and memory the memory allocated for the resource
        //

        if (streamContext->Resource != NULL) {

            ExDeleteResourceLite( streamContext->Resource );
            CtxFreeResource( streamContext->Resource );
        }

        //
        //  Free the file name
        //

        if (streamContext->FileName.Buffer != NULL) {

            CtxFreeUnicodeString(&streamContext->FileName);
        }

        DebugTrace( DEBUG_TRACE_STREAM_CONTEXT_OPERATIONS,
                    ("[Ctx]: Stream context cleanup complete.\n") );

        break;

    case FLT_STREAMHANDLE_CONTEXT:

        streamHandleContext = (PCTX_STREAMHANDLE_CONTEXT) Context;

        DebugTrace( DEBUG_TRACE_STREAMHANDLE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Cleaning up stream handle context for file %wZ (StreamContext = %p)\n",
                     &streamHandleContext->FileName,
                     streamHandleContext) );

        //
        //  Delete the resource and memory the memory allocated for the resource
        //

        if (streamHandleContext->Resource != NULL) {

            ExDeleteResourceLite( streamHandleContext->Resource );
            CtxFreeResource( streamHandleContext->Resource );
        }

        //
        //  Free the file name
        //

        if (streamHandleContext->FileName.Buffer != NULL) {

            CtxFreeUnicodeString(&streamHandleContext->FileName);
        }

        DebugTrace( DEBUG_TRACE_STREAMHANDLE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Stream handle context cleanup complete.\n") );

        break;

    }

}

//
//  Instance setup/teardown routines.
//

NTSTATUS
CtxInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    PCTX_INSTANCE_CONTEXT instanceContext = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG volumeNameLength;

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	ULONG					propertyLengthReturned; 

	UNICODE_STRING			ntfs;
	UNICODE_STRING			ndasNtfs;
	UNICODE_STRING			fat;
	UNICODE_STRING			ndasFat;

	PNETDISK_PARTITION		netdiskPartition;
	NETDISK_ENABLE_MODE		netdiskEnableMode;

#endif

    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

#if __NDAS_FS_MINI__

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance setup started FltObjects = %p\n", FltObjects) );

#endif

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance setup started (Volume = %p, Instance = %p)\n",
                 FltObjects->Volume,
                 FltObjects->Instance) );


    //
    //  Allocate and initialize the context for this volume
    //


    //
    //  Allocate the instance context
    //

    DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                ("[Ctx]: Allocating instance context (Volume = %p, Instance = %p)\n",
                 FltObjects->Volume,
                 FltObjects->Instance) );

    status = FltAllocateContext( FltObjects->Filter,
                                 FLT_INSTANCE_CONTEXT,
                                 CTX_INSTANCE_CONTEXT_SIZE,
                                 NonPagedPool,
                                 &instanceContext );

    if (!NT_SUCCESS( status )) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Failed to allocate instance context (Volume = %p, Instance = %p, Status = 0x%x)\n",
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );

        goto CtxInstanceSetupCleanup;
    }

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	RtlZeroMemory( instanceContext, sizeof(CTX_INSTANCE_CONTEXT) );

	status = FltGetVolumeProperties( FltObjects->Volume,
									 NULL,
									 0,
									 &propertyLengthReturned ); 


	DebugTrace( DEBUG_TRACE_INSTANCES,
				("[MiniSpy]: IRP_MJ_VOLUME_MOUNT FltGetVolumeProperties, status = %x, propertyLengthReturned = %d\n",
				 status,
				 propertyLengthReturned) );

	instanceContext->VolumeProperties = ExAllocatePoolWithTag( PagedPool, propertyLengthReturned, CTX_VOLUME_PROPERTY_TAG );

	status = FltGetVolumeProperties( FltObjects->Volume,
									 instanceContext->VolumeProperties,
									 propertyLengthReturned,
									 &propertyLengthReturned ); 

	DebugTrace( DEBUG_TRACE_INSTANCES,
				("[MiniSpy]: FltGetVolumeProperties, status = %x, propertyLengthReturned = %d\n",
				 status,
				 propertyLengthReturned) );

	if( !NT_SUCCESS( status )) {

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}

	DebugTrace( DEBUG_TRACE_INSTANCES,
				("[MiniSpy]: FltGetVolumeProperties, DeviceType = %d "
				 "FileSystemDriverName = %wZ\n"
				 "FileSystemDeviceName = %wZ "
				 "RealDeviceName = %wZ\n",
				 instanceContext->VolumeProperties->DeviceType,
				 &instanceContext->VolumeProperties->FileSystemDriverName,
				 &instanceContext->VolumeProperties->FileSystemDeviceName,
				 &instanceContext->VolumeProperties->RealDeviceName) );

	RtlInitUnicodeString( &ntfs, L"\\Ntfs" );
	RtlInitUnicodeString( &ndasNtfs, NDAS_NTFS_DEVICE_NAME );
	RtlInitUnicodeString( &fat, L"\\Fat" );
	RtlInitUnicodeString( &ndasFat, NDAS_FAT_DEVICE_NAME );

	if (!(RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)		||
		  RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)	||
		  RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &fat, TRUE)		||
		  RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE))) {

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}

	status = FltGetDeviceObject( FltObjects->Volume, &instanceContext->DeviceObject );

	if (!NT_SUCCESS(status)) {

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}
	
	status = FltGetDiskDeviceObject( FltObjects->Volume, &instanceContext->DiskDeviceObject );

	if (!NT_SUCCESS(status)) {

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}

	DebugTrace( DEBUG_TRACE_INSTANCES,
				("DeviceObject = %p, DiskDeviceObject = %p\n", instanceContext->DeviceObject, instanceContext->DiskDeviceObject) );


	if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE) && 
		!GlobalLfs.NdasFatRwSupport && !GlobalLfs.NdasFatRoSupport ||
		RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE) && 
		!GlobalLfs.NdasNtfsRwSupport && !GlobalLfs.NdasNtfsRoSupport) {

		NDAS_ASSERT( FALSE );
		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}

	if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

		status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
												GlobalLfs.NdasFatRwIndirect ? TRUE : FALSE,
												TRUE,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
												&netdiskPartition,
												&netdiskEnableMode );
	
	} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

		status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
												GlobalLfs.NdasNtfsRwIndirect ? TRUE : FALSE,
												TRUE,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
												&netdiskPartition,
												&netdiskEnableMode );
	
	} else {

		status = NetdiskManager_PreMountVolume( GlobalLfs.NetdiskManager,
												FALSE,
												TRUE,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.DeviceObject,
												instanceContext->DiskDeviceObject, //pIrpSp->Parameters.MountVolume.Vpb->RealDevice,
												&netdiskPartition,
												&netdiskEnableMode );
	}


	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("NetdiskManager_IsNetdiskPartition status = %x\n", status) );

	if (!NT_SUCCESS(status)) {

		if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE) ||
			RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

			NDAS_ASSERT( FALSE );
			status = STATUS_FLT_DO_NOT_ATTACH;
			goto CtxInstanceSetupCleanup;
		}

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	} 

	if (status == STATUS_VOLUME_MOUNTED) {

		NDAS_ASSERT( GlobalLfs.NdasFsMiniMode == TRUE );
		NDAS_ASSERT( netdiskEnableMode == NETDISK_READ_ONLY );

		status = STATUS_FLT_DO_NOT_ATTACH;
		goto CtxInstanceSetupCleanup;
	}

	NDAS_ASSERT( netdiskEnableMode != NETDISK_READ_ONLY );

	switch (netdiskEnableMode) {

	case NETDISK_READ_ONLY:

		if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &fat, TRUE)) {

			if (GlobalLfs.NdasFatRoSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );
	
				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}			
			
		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

			if (!GlobalLfs.NdasFatRoSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );
	
				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}			

		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)) {

			if (GlobalLfs.NdasNtfsRoSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );

				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}
			
		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

			if (!GlobalLfs.NdasNtfsRoSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );

				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}
			
		} else {

			NDAS_ASSERT( FALSE );
		}

		break;

	case NETDISK_SECONDARY:
	case NETDISK_PRIMARY:
	case NETDISK_SECONDARY2PRIMARY:

		if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &fat, TRUE)) {

			if (GlobalLfs.NdasFatRwSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );
	
				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}			
			
		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

			if (!GlobalLfs.NdasFatRwSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );
	
				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}			

		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)) {

			if (GlobalLfs.NdasNtfsRwSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );

				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
				goto CtxInstanceSetupCleanup;
			}
			
		} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

			if (!GlobalLfs.NdasNtfsRwSupport) {

				NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
												netdiskPartition,
												netdiskEnableMode,
												FALSE,
												0,
												NULL,
												NULL );

				NDAS_ASSERT( FALSE );
				status = STATUS_FLT_DO_NOT_ATTACH;
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

	LfsReference( &GlobalLfs );

    ExInitializeFastMutex( &instanceContext->LfsDeviceExt.FastMutex );
	instanceContext->LfsDeviceExt.ReferenceCount		= 1;

	InitializeListHead( &instanceContext->LfsDeviceExt.LfsQListEntry );

	instanceContext->LfsDeviceExt.Flags = LFS_DEVICE_FLAG_INITIALIZING;

	instanceContext->LfsDeviceExt.FileSpyDeviceObject	= NULL;
	instanceContext->LfsDeviceExt.InstanceContext		= instanceContext;

	FltReferenceContext( instanceContext->LfsDeviceExt.InstanceContext );

	instanceContext->LfsDeviceExt.NetdiskPartition		= netdiskPartition;
	instanceContext->LfsDeviceExt.NetdiskEnabledMode	= netdiskEnableMode;

	instanceContext->LfsDeviceExt.FilteringMode	= LFS_NO_FILTERING;

	instanceContext->LfsDeviceExt.DiskDeviceObject			= instanceContext->DiskDeviceObject;
	instanceContext->LfsDeviceExt.MountVolumeDeviceObject	= instanceContext->DiskDeviceObject;

    SPY_LOG_PRINT( SPYDEBUG_ERROR,
                   ("FileSpy!CtxInstanceSetup: instanceContext->LfsDeviceExt.DiskDeviceObject = %p\n",
					instanceContext->LfsDeviceExt.DiskDeviceObject) );

	ExInterlockedInsertTailList( &GlobalLfs.LfsDeviceExtQueue,
								 &instanceContext->LfsDeviceExt.LfsQListEntry,
								 &GlobalLfs.LfsDeviceExtQSpinLock );


	if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ntfs, TRUE)) {

		instanceContext->LfsDeviceExt.FileSystemType = LFS_FILE_SYSTEM_NTFS;

	} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasNtfs, TRUE)) {

		if (GlobalLfs.NdasNtfsRwIndirect) {

			SetFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT );
		}

		instanceContext->LfsDeviceExt.FileSystemType = LFS_FILE_SYSTEM_NDAS_NTFS;
	
	} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &fat, TRUE)) {

		instanceContext->LfsDeviceExt.FileSystemType = LFS_FILE_SYSTEM_FAT;

	} else if (RtlEqualUnicodeString(&instanceContext->VolumeProperties->FileSystemDeviceName, &ndasFat, TRUE)) {

		if (GlobalLfs.NdasFatRwIndirect) {

			SetFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT );
		}

		instanceContext->LfsDeviceExt.FileSystemType = LFS_FILE_SYSTEM_NDAS_FAT;
	
	} else {

		NDAS_ASSERT( FALSE ); 
	}

	NetdiskManager_PostMountVolume( GlobalLfs.NetdiskManager,
									instanceContext->LfsDeviceExt.NetdiskPartition,
									instanceContext->LfsDeviceExt.NetdiskEnabledMode,
									TRUE,
									instanceContext->LfsDeviceExt.FileSystemType,		
									&instanceContext->LfsDeviceExt,
									&instanceContext->LfsDeviceExt.NetdiskPartitionInformation );

	switch (netdiskEnableMode) {

	case NETDISK_READ_ONLY:

		instanceContext->LfsDeviceExt.FilteringMode = LFS_READONLY;

		break;

	case NETDISK_SECONDARY:

		instanceContext->LfsDeviceExt.FilteringMode = LFS_SECONDARY;

		break;

	case NETDISK_PRIMARY:
	case NETDISK_SECONDARY2PRIMARY:

		instanceContext->LfsDeviceExt.FilteringMode = LFS_PRIMARY;

		break;
	
	default:

		ASSERT( LFS_BUG );

		break;
	}

	SetFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTING );

	ASSERT( instanceContext->DiskDeviceObject->Vpb->DeviceObject );

	instanceContext->LfsDeviceExt.Vpb						= instanceContext->LfsDeviceExt.DiskDeviceObject->Vpb; // Vpb will be changed in IrpSp Why ?
	instanceContext->LfsDeviceExt.BaseVolumeDeviceObject	= instanceContext->LfsDeviceExt.Vpb->DeviceObject;

	switch (instanceContext->LfsDeviceExt.FilteringMode) {

	case LFS_READONLY: {
		
		//
		//	We don't support cache purging yet.
		//

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsFsControlMountVolumeComplete: READONLY newDevExt->LfsDeviceExt = %p "
											"newDevExt->LfsDeviceExt.FileSystemType = %d\n",
											 &instanceContext->LfsDeviceExt, instanceContext->LfsDeviceExt.FileSystemType) );
		
		instanceContext->LfsDeviceExt.AttachedToDeviceObject = instanceContext->DeviceObject; //NULL; //instanceContext->NLExtHeader.AttachedToDeviceObject;

		NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
											instanceContext->LfsDeviceExt.NetdiskPartition,
											instanceContext->LfsDeviceExt.NetdiskEnabledMode,
											status,
											instanceContext->LfsDeviceExt.AttachedToDeviceObject );

		status = SpyFsControlReadonlyMountVolumeComplete( &instanceContext->LfsDeviceExt );

		break;
	}

	case LFS_PRIMARY: {

		instanceContext->LfsDeviceExt.AttachedToDeviceObject = instanceContext->DeviceObject; //NULL; //instanceContext->NLExtHeader.AttachedToDeviceObject;

		ASSERT( instanceContext->LfsDeviceExt.AttachedToDeviceObject );

		NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
											instanceContext->LfsDeviceExt.NetdiskPartition,
											instanceContext->LfsDeviceExt.NetdiskEnabledMode,
											status,
											instanceContext->LfsDeviceExt.AttachedToDeviceObject );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsFsControlMountVolumeComplete: LFS_PRIMARY newDevExt->LfsDeviceExt = %p "
											"newDevExt->LfsDeviceExt.FileSystemType = %d\n",
											 &instanceContext->LfsDeviceExt, instanceContext->LfsDeviceExt.FileSystemType) );

		status = STATUS_SUCCESS;
		break;
	}

	case LFS_SECONDARY: {

		instanceContext->LfsDeviceExt.AttachedToDeviceObject = instanceContext->DeviceObject; //NULL; //instanceContext->NLExtHeader.AttachedToDeviceObject;
					
		NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
											instanceContext->LfsDeviceExt.NetdiskPartition,
											instanceContext->LfsDeviceExt.NetdiskEnabledMode,
											status,
											instanceContext->LfsDeviceExt.AttachedToDeviceObject );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("CtxInstanceSetup: LFS_SECONDARY newDevExt->LfsDeviceExt = %p "
											"newDevExt->LfsDeviceExt.FileSystemType = %d\n",
											 &instanceContext->LfsDeviceExt, instanceContext->LfsDeviceExt.FileSystemType) );

		status = SpyFsControlSecondaryMountVolumeComplete( &instanceContext->LfsDeviceExt );

		if (instanceContext->LfsDeviceExt.FilteringMode == LFS_SECONDARY_TO_PRIMARY) {
		
			NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
									   instanceContext->LfsDeviceExt.NetdiskPartition,
									   &instanceContext->LfsDeviceExt.NetdiskEnabledMode );
		}

		break;
	}

	default:

		NDAS_ASSERT( FALSE );

		NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
											instanceContext->LfsDeviceExt.NetdiskPartition,
											instanceContext->LfsDeviceExt.NetdiskEnabledMode,
											status,
											NULL );

		status = STATUS_SUCCESS;

		break;
	}

    //
    //  We completed initialization of this device object, so now
    //  clear the initializing flag.
    //

	if (status == STATUS_SUCCESS) {

		ClearFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INITIALIZING );
		ClearFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTING );

		SetFlag( instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED );

	} else {

		NTSTATUS status2;


		NDAS_ASSERT( FALSE );

		//
		//	Queue an event to notify user applications
		//

		XevtQueueVolumeInvalidOrLocked( -1,
										instanceContext->LfsDeviceExt.NetdiskPartitionInformation.NetdiskInformation.SlotNo,
										instanceContext->LfsDeviceExt.NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo );

		//
		//	Try to unplug
		//

		status2 = NetdiskManager_UnplugNetdisk( GlobalLfs.NetdiskManager,
												instanceContext->LfsDeviceExt.NetdiskPartition,
												instanceContext->LfsDeviceExt.NetdiskEnabledMode );

		ASSERT( NT_SUCCESS(status2) );
	}

#endif

    //
    //  Get the NT volume name length
    //

    status = FltGetVolumeName( FltObjects->Volume, NULL, &volumeNameLength );

    if( !NT_SUCCESS( status ) &&
        (status != STATUS_BUFFER_TOO_SMALL) ) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Unexpected failure in FltGetVolumeName. (Volume = %p, Instance = %p, Status = 0x%x)\n",
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );

        goto CtxInstanceSetupCleanup;
    }

    //
    //  Allocate a string big enough to take the volume name
    //

    instanceContext->VolumeName.MaximumLength = (USHORT) volumeNameLength;
    status = CtxAllocateUnicodeString( &instanceContext->VolumeName );

    if( !NT_SUCCESS( status )) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Failed to allocate volume name string. (Volume = %p, Instance = %p, Status = 0x%x)\n",
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );

        goto CtxInstanceSetupCleanup;
    }

    //
    //  Get the NT volume name
    //

    status = FltGetVolumeName( FltObjects->Volume, &instanceContext->VolumeName, &volumeNameLength );

    if( !NT_SUCCESS( status ) ) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Unexpected failure in FltGetVolumeName. (Volume = %p, Instance = %p, Status = 0x%x)\n",
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );

        goto CtxInstanceSetupCleanup;
    }


    instanceContext->Instance = FltObjects->Instance;
    instanceContext->Volume = FltObjects->Volume;

    //
    //  Set the instance context.
    //

    DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                ("[Ctx]: Setting instance context %p for volume %wZ (Volume = %p, Instance = %p)\n",
                 instanceContext,
                 &instanceContext->VolumeName,
                 FltObjects->Volume,
                 FltObjects->Instance) );

    status = FltSetInstanceContext( FltObjects->Instance,
                                    FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                    instanceContext,
                                    NULL );

    if( !NT_SUCCESS( status )) {

        DebugTrace( DEBUG_TRACE_INSTANCES | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Failed to set instance context for volume %wZ (Volume = %p, Instance = %p, Status = 0x%08X)\n",
                     &instanceContext->VolumeName,
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );
        goto CtxInstanceSetupCleanup;
    }


CtxInstanceSetupCleanup:

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	if (status != STATUS_SUCCESS) {

		if (instanceContext->DiskDeviceObject) {

			ObDereferenceObject( instanceContext->DiskDeviceObject );
			instanceContext->DiskDeviceObject = NULL;
		}

		if (instanceContext->DeviceObject) {

			ObDereferenceObject( instanceContext->DeviceObject );
			instanceContext->DeviceObject = NULL;
		}

		if (instanceContext->VolumeProperties) {

			ExFreePoolWithTag( instanceContext->VolumeProperties, CTX_VOLUME_PROPERTY_TAG );
			instanceContext->VolumeProperties = NULL;
		}

		if (FlagOn(instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INITIALIZING)) {

			LfsDeviceExt_Dereference( &instanceContext->LfsDeviceExt );
		}
	}

#endif

    //
    //  If FltAllocateContext suceeded then we MUST release the context,
    //  irrespective of whether FltSetInstanceContext suceeded or not.
    //
    //  FltAllocateContext increments the ref count by one.
    //  A successful FltSetInstanceContext increments the ref count by one
    //  and also associates the context with the file system object
    //
    //  FltReleaseContext decrements the ref count by one.
    //
    //  When FltSetInstanceContext succeeds, calling FltReleaseContext will
    //  leave the context with a ref count of 1 corresponding to the internal
    //  reference to the context from the file system structures
    //
    //  When FltSetInstanceContext fails, calling FltReleaseContext will
    //  leave the context with a ref count of 0 which is correct since
    //  there is no reference to the context from the file system structures
    //

    if ( instanceContext != NULL ) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Releasing instance context %p (Volume = %p, Instance = %p)\n",
                     instanceContext,
                     FltObjects->Volume,
                     FltObjects->Instance) );

        FltReleaseContext( instanceContext );
    }


    if (NT_SUCCESS( status )) {

        DebugTrace( DEBUG_TRACE_INSTANCES,
                    ("[Ctx]: Instance setup complete (Volume = %p, Instance = %p). Filter will attach to the volume.\n",
                     FltObjects->Volume,
                     FltObjects->Instance) );
    } else {

        DebugTrace( DEBUG_TRACE_INSTANCES,
                    ("[Ctx]: Instance setup complete (Volume = %p, Instance = %p). Filter will not attach to the volume.\n",
                     FltObjects->Volume,
                     FltObjects->Instance) );
    }

    return status;
}


NTSTATUS
CtxInstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance query teardown started (Instance = %p)\n",
                 FltObjects->Instance) );


    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance query teadown ended (Instance = %p)\n",
                 FltObjects->Instance) );
    return STATUS_SUCCESS;
}


VOID
CtxInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is been deleted.

Return Value:

    None.

--*/
{
#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)
	NTSTATUS				status;
	PCTX_INSTANCE_CONTEXT	instanceContext = NULL;
#endif

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance teardown start started (Instance = %p)\n",
                 FltObjects->Instance) );


    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance teardown start ended (Instance = %p)\n",
                 FltObjects->Instance) );

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	status = FltGetInstanceContext( FltObjects->Instance, &instanceContext );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	if (status == STATUS_SUCCESS) {

		NDAS_ASSERT( FlagOn(instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) ||
					   FlagOn(instanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED) ||
					   (instanceContext->LfsDeviceExt.Flags & ~LFS_DEVICE_FLAG_INDIRECT) == LFS_DEVICE_FLAG_MOUNTED );

		if (instanceContext->LfsDeviceExt.Flags == LFS_DEVICE_FLAG_MOUNTED) {

			DebugTrace( DEBUG_TRACE_INSTANCES,
	                    ("[Ctx]: Instance teardown called LFS_DEVICE_FLAG_MOUNTED lfsDeviceExt = %p\n", instanceContext->LfsDeviceExt) );
		
			LfsDismountVolume( &instanceContext->LfsDeviceExt );
		}

		LfsCleanupMountedDevice( &instanceContext->LfsDeviceExt );

		if (instanceContext->DiskDeviceObject) {

			ObDereferenceObject( instanceContext->DiskDeviceObject );
			instanceContext->DiskDeviceObject = NULL;
		}

		if (instanceContext->DeviceObject) {

			ObDereferenceObject( instanceContext->DeviceObject );
			instanceContext->DeviceObject = NULL;
		}

		if (instanceContext->VolumeProperties) {

			ExFreePoolWithTag( instanceContext->VolumeProperties, CTX_VOLUME_PROPERTY_TAG );
			instanceContext->VolumeProperties = NULL;
		}

		FltReleaseContext( instanceContext );
	}

#endif
}


VOID
CtxInstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is been deleted.

Return Value:

    None.

--*/
{
    PCTX_INSTANCE_CONTEXT instanceContext;
    NTSTATUS status;

    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance teardown complete started (Instance = %p)\n",
                 FltObjects->Instance) );

    DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                ("[Ctx]: Getting instance context (Volume = %p, Instance = %p)\n",
                 FltObjects->Volume,
                 FltObjects->Instance) );

    status = FltGetInstanceContext( FltObjects->Instance,
                                    &instanceContext );

    if (NT_SUCCESS( status )) {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Instance teardown for volume %wZ (Volume = %p, Instance = %p, InstanceContext = %p)\n",
                     &instanceContext->VolumeName,
                     FltObjects->Volume,
                     FltObjects->Instance,
                     instanceContext) );


        //
        //  Here the filter may perform any teardown of its own structures associated
        //  with this instance.
        //
        //  The filter should not free memory or synchronization objects allocated to
        //  objects within the instance context. That should be performed in the
        //  cleanup callback for the instance context
        //

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS,
                    ("[Ctx]: Releasing instance context %p for volume %wZ (Volume = %p, Instance = %p)\n",
                     instanceContext,
                     &instanceContext->VolumeName,
                     FltObjects->Volume,
                     FltObjects->Instance) );

        FltReleaseContext( instanceContext );
    } else {

        DebugTrace( DEBUG_TRACE_INSTANCE_CONTEXT_OPERATIONS | DEBUG_TRACE_ERROR,
                    ("[Ctx]: Failed to get instance context (Volume = %p, Instance = %p Status = 0x%x)\n",
                     FltObjects->Volume,
                     FltObjects->Instance,
                     status) );
    }

    DebugTrace( DEBUG_TRACE_INSTANCES,
                ("[Ctx]: Instance teardown complete ended (Instance = %p)\n",
                 FltObjects->Instance) );
}

#endif
