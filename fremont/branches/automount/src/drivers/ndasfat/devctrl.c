/*++

Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    DevCtrl.c

Abstract:

    This module implements the File System Device Control routines for Fat
    called by the dispatch driver.


--*/

#include "FatProcs.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_DEVCTRL)
#if __NDAS_FAT__
#define Dbg2                             (DEBUG_INFO_DEVCTRL)
#endif

//
//  Local procedure prototypes
//

NTSTATUS
FatDeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatCommonDeviceControl)
#pragma alloc_text(PAGE, FatFsdDeviceControl)
#endif


NTSTATUS
FatFsdDeviceControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Device control operations

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;

#if	__NDAS_FAT__
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation( Irp );
#endif
    PAGED_CODE();




#if	__NDAS_FAT__

	if ((PVOID)FatControlDeviceObject == VolumeDeviceObject) {

		Status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return Status;
	}

	if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && FatData.DiskFileSystemDeviceObject == (PDEVICE_OBJECT)VolumeDeviceObject) {

		switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

		case IOCTL_REGISTER_NDFS_CALLBACK: {

			PNDFS_CALLBACK	inputBuffer			= (PNDFS_CALLBACK)Irp->AssociatedIrp.SystemBuffer;
			ULONG			inputBufferLength	= irpSp->Parameters.DeviceIoControl.InputBufferLength;


			if ( inputBufferLength != sizeof( VolumeDeviceObject->NdfsCallback ) ) {

				Status = Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				break;
			} 
			
			DebugTrace2( 0, Dbg2, ("NtfsFsdDispatch: IOCTL_REGISTER_NDFS_CALLBACK, size = %d\n", inputBuffer->Size) );		

			RtlCopyMemory( &VolumeDeviceObject->NdfsCallback, inputBuffer, sizeof(VolumeDeviceObject->NdfsCallback) );

			//ASSERT( FALSE );
			Status = Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			
			break;
		}
	
		case IOCTL_UNREGISTER_NDFS_CALLBACK:

			DebugTrace2( 0, Dbg2, ("NtfsFsdDispatch: IOCTL_UNREGISTER_NDFS_CALLBACK\n") );		

			RtlZeroMemory( &VolumeDeviceObject->NdfsCallback, sizeof(VolumeDeviceObject->NdfsCallback) );

			Status = Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;

			break;

		case IOCTL_SHUTDOWN: {
			
			PLIST_ENTRY				vcbListEntry;

			DebugTrace2( 0, Dbg2, ("FatFsdDeviceControl: IOCTL_SHUTDOWN\n") );		

	        for (vcbListEntry = FatData.VcbQueue.Flink;
			     vcbListEntry != &FatData.VcbQueue;
				 vcbListEntry = vcbListEntry->Flink) {

				PVCB					vcb = NULL;
#if __NDAS_FAT_PRIMARY__
				PLIST_ENTRY				primarySessionListEntry;
#endif
				PVOLUME_DEVICE_OBJECT	volDo = NULL;


				DebugTrace2( 0, Dbg2, ("NtfsFsdDispatch: volDo = %p, KeGetCurrentIrql() = %d\n", volDo, KeGetCurrentIrql()) );		

				vcb = CONTAINING_RECORD(vcbListEntry, VCB, VcbLinks);
				volDo = CONTAINING_RECORD( vcb, VOLUME_DEVICE_OBJECT, Vcb );

				DebugTrace2( 0, Dbg2, ("FatFsdDeviceControl: ND_FAT_DEVICE_FLAG_SHUTDOWN\n") );		
				SetFlag( volDo->NdFatFlags, ND_FAT_DEVICE_FLAG_SHUTDOWN );

		
#if __NDAS_FAT_PRIMARY__

				for (primarySessionListEntry = volDo->PrimarySessionQueue.Flink;
					 primarySessionListEntry != &volDo->PrimarySessionQueue; ) {

					PPRIMARY_SESSION primarySession;

					primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
					primarySessionListEntry = primarySessionListEntry->Flink;
					PrimarySession_Reference( primarySession );
		
					PrimarySession_FileSystemShutdown( primarySession );

					RemoveEntryList( &primarySession->ListEntry );
					InitializeListHead(  &primarySession->ListEntry );
					PrimarySession_Dereference( primarySession );
				}
#endif
			}

			Status = Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			DebugTrace2( 0, Dbg2, ("NtfsFsdDispatch: IOCTL_SHUTDOWN return\n") );		

			break;
		}

		default:
				
			//ASSERT( FALSE );
			Status = Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
			Irp->IoStatus.Information = 0;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return Status;
	}

	//if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && 
	//	IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode == IOCTL_INSERT_PRIMARY_SESSION) {

    //    return NtfsFsdDispatchSwitch( NULL, Irp, FALSE );
	//}

#endif

    DebugTrace(+1, Dbg, "FatFsdDeviceControl\n", 0);

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

    try {

#if __NDAS_FAT__

		if (IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_DEVICE_CONTROL && 
			IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode == IOCTL_INSERT_PRIMARY_SESSION) {

			IrpContext = FatCreateIrpContext( Irp, FALSE );

		} else {

			IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ));
		}

#else
        IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ));
#endif

        Status = FatCommonDeviceControl( IrpContext, Irp );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdDeviceControl -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


NTSTATUS
FatCommonDeviceControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for doing Device control operations called
    by both the fsd and fsp threads

Arguments:

    Irp - Supplies the Irp to process

    InFsp - Indicates if this is the fsp thread or someother thread

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    KEVENT WaitEvent;
    PVOID CompletionContext = NULL;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

#if __NDAS_FAT_PRIMARY__

	if (IrpContext->MajorFunction == IRP_MJ_DEVICE_CONTROL && 
		IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode == IOCTL_INSERT_PRIMARY_SESSION) {

		PVOLUME_DEVICE_OBJECT	VolDo = CONTAINING_RECORD( IoGetCurrentIrpStackLocation(Irp)->DeviceObject, 
														   VOLUME_DEVICE_OBJECT, 
														   DeviceObject );
		PSESSION_INFORMATION	inputBuffer = (PSESSION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
		ULONG					inputBufferLength = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.InputBufferLength;
		ULONG					outputBufferLength;
		PULONG					outputBuffer;
		PPRIMARY_SESSION		primarySession;

		if (inputBufferLength != sizeof(SESSION_INFORMATION)) {

			FatCompleteRequest( IrpContext, Irp, Status = STATUS_INVALID_PARAMETER );
			return Status;
		} 

		if (FlagOn(VolDo->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT)) {

			FatCompleteRequest( IrpContext, Irp, Status = STATUS_INVALID_DEVICE_REQUEST );
			return Status;
		}

		outputBufferLength = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.OutputBufferLength;
		outputBuffer = (PULONG)Irp->AssociatedIrp.SystemBuffer;

		primarySession = PrimarySession_Create( IrpContext, VolDo, inputBuffer, NULL );

		ASSERT( primarySession );

		Irp->IoStatus.Information = sizeof( primarySession );
		FatCompleteRequest( IrpContext, Irp, Status = STATUS_SUCCESS );

		return Status;
	}

#endif

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonDeviceControl\n", 0);
    DebugTrace( 0, Dbg, "Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "MinorFunction = %08lx\n", IrpSp->MinorFunction);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatCommonDeviceControl -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  A few IOCTLs actually require some intervention on our part
    //

#if __NDAS_FAT__

#define IOCTL_VOLUME_BASE   ((ULONG) 'V')
#define MOUNTDEVCONTROLTYPE  ((ULONG) 'M')

	DebugTrace2( 0, Dbg2, 
				 ("FatCommonDeviceControl: deviceType = %d, function = %d IOCTL_VOLUME_BASE = %d, MOUNTDEVCONTROLTYPE = %d\n",
				   DEVICE_TYPE_FROM_CTL_CODE(IrpSp->Parameters.DeviceIoControl.IoControlCode), 
				   (UCHAR)((IrpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2), IOCTL_VOLUME_BASE, MOUNTDEVCONTROLTYPE) );

	if (CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb)->NetdiskEnableMode == NETDISK_SECONDARY)
		SetFlag( IrpContext->NdFatFlags, ND_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );

#endif

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES:

#if __NDAS_FAT_WIN2K_SUPPORT__

		if (!IS_WINDOWSXP_OR_LATER()) {

			ASSERT( FALSE );
			IoSkipCurrentIrpStackLocation( Irp );
			break;
		}

#endif

        //
        //  This is sent by the Volume Snapshot driver (Lovelace).
        //  We flush the volume, and hold all file resources
        //  to make sure that nothing more gets dirty. Then we wait
        //  for the IRP to complete or cancel.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
        FatAcquireExclusiveVolume( IrpContext, Vcb );

        FatFlushAndCleanVolume( IrpContext,
                                Irp,
                                Vcb,
                                FlushWithoutPurge );

        KeInitializeEvent( &WaitEvent, NotificationEvent, FALSE );
        CompletionContext = &WaitEvent;

        //
        //  Get the next stack location, and copy over the stack location
        //

        IoCopyCurrentIrpStackLocationToNext( Irp );

        //
        //  Set up the completion routine
        //

        IoSetCompletionRoutine( Irp,
                                FatDeviceControlCompletionRoutine,
                                CompletionContext,
                                TRUE,
                                TRUE,
                                TRUE );
        break;

#ifndef NTDDI_VERSION

#define IOCTL_DISK_COPY_DATA            CTL_CODE(IOCTL_DISK_BASE, 0x0019, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define SL_FORCE_DIRECT_WRITE           0x10

#endif

    case IOCTL_DISK_COPY_DATA:

        //
        //  We cannot allow this IOCTL to be sent unless the volume is locked,
        //  since this IOCTL allows direct writing of data to the volume.
        //  We do allow kernel callers to force access via a flag.  A handle that
        //  issued a dismount can send this IOCTL as well.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_FLAG_LOCKED ) &&
            !FlagOn( IrpSp->Flags, SL_FORCE_DIRECT_WRITE ) &&
            !FlagOn( Ccb->Flags, CCB_FLAG_COMPLETE_DISMOUNT )) {

            FatCompleteRequest( IrpContext,
                                Irp,
                                STATUS_ACCESS_DENIED );

            DebugTrace(-1, Dbg, "FatCommonDeviceControl -> %08lx\n", STATUS_ACCESS_DENIED);
            return STATUS_ACCESS_DENIED;
        }

        break;

    case IOCTL_SCSI_PASS_THROUGH:
    case IOCTL_SCSI_PASS_THROUGH_DIRECT:

        //
        //  If someone is issuing a format unit command underneath us, then make
        //  sure we mark the device as needing verification when they close their
        //  handle.
        //

        if ((!FlagOn( IrpSp->FileObject->Flags, FO_FILE_MODIFIED ) ||
             !FlagOn( Ccb->Flags, CCB_FLAG_SENT_FORMAT_UNIT )) &&
            (IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof( SCSI_PASS_THROUGH ) &&
            (Irp->AssociatedIrp.SystemBuffer != NULL))) {

            PSCSI_PASS_THROUGH Spt = Irp->AssociatedIrp.SystemBuffer;

            if (Spt->Cdb[0] == SCSIOP_FORMAT_UNIT) {

                SetFlag( Ccb->Flags, CCB_FLAG_SENT_FORMAT_UNIT );
                SetFlag( IrpSp->FileObject->Flags, FO_FILE_MODIFIED );
            }
        }

        //
        //  Fall through as we do not need to know the outcome of this operation.
        //

    default:

        //
        //  FAT doesn't need to see this on the way back, so skip ourselves.
        //

        IoSkipCurrentIrpStackLocation( Irp );
        break;
    }

    //
    //  Send the request.
    //

    Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

    if (Status == STATUS_PENDING && CompletionContext) {

        KeWaitForSingleObject( &WaitEvent,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL );

        Status = Irp->IoStatus.Status;
    }

    //
    //  If we had a context, the IRP remains for us and we will complete it.
    //  Handle it appropriately.
    //

    if (CompletionContext) {

        //
        //  Release all the resources that we held because of a
        //  VOLSNAP_FLUSH_AND_HOLD. 
        //

        ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES );

        FatReleaseVolume( IrpContext, Vcb );

        //
        //  If we had no context, the IRP will complete asynchronously.
        //

    } else {

        Irp = NULL;
    }

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatCommonDeviceControl -> %08lx\n", Status);

    return Status;
}


//
//  Local support routine
//

NTSTATUS
FatDeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    PKEVENT Event = (PKEVENT) Contxt;
    
    //
    //  If there is an event, this is a synch request. Signal and
    //  let I/O know this isn't done yet.
    //

    if (Event) {

        KeSetEvent( Event, 0, FALSE );
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    return STATUS_SUCCESS;
}

