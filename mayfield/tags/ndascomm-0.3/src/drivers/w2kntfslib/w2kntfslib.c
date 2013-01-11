//#include <ntifs.h>
//#include <stdlib.h>

#include "ntfsproc.h"
#undef CONSTANT_UNICODE_STRING

#include <filespy.h>
#include <fspyKern.h>

#include "W2KNtfsLib.h"


BOOLEAN
W2KNtfsLookupAllocation (
//    IN PIRP_CONTEXT IrpContext,
    IN PATTRIBUTE_RECORD_HEADER Attribute,
    IN OUT PSCB Scb,
    IN VCN Vcn,
    OUT PLCN Lcn,
    OUT PLONGLONG ClusterCount,
    OUT PVOID *RangePtr OPTIONAL,
    OUT PULONG RunIndex OPTIONAL
    );


NTSTATUS
W2KNtfsVerifyReadCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
//    PAGED_CODE();

    //
    //  Set the event so that our call will wake up.
    //

    KeSetEvent( (PKEVENT)Contxt, 0, FALSE );

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    //
    //  If we change this return value then NtfsIoCallSelf needs to reference the
    //  file object.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
W2KNtfsPerformVerifyDiskRead (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PVOID			Buffer,
    IN LONGLONG			Offset,
    IN ULONG			NumberOfBytesToRead
    )

/*++

Routine Description:

    This routine is used to read in a range of bytes from the disk.  It
    bypasses all of the caching and regular I/O logic, and builds and issues
    the requests itself.  It does this operation overriding the verify
    volume flag in the device object.

Arguments:

    Vcb - Supplies the Vcb denoting the device for this operation

    Buffer - Supplies the buffer that will recieve the results of this operation

    Offset - Supplies the offset of where to start reading

    NumberOfBytesToRead - Supplies the number of bytes to read, this must
        be in multiple of bytes units acceptable to the disk driver.

Return Value:

    None.

--*/

{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;


    PAGED_CODE();

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  Build the irp for the operation and also set the overrride flag
    //
    //  Note that we may be at APC level, so do this asyncrhonously and
    //  use an event for synchronization normal request completion
    //  cannot occur at APC level.
    //
    Irp = IoBuildAsynchronousFsdRequest( IRP_MJ_READ,
                                         DeviceObject,
                                         Buffer,
                                         NumberOfBytesToRead,
                                         (PLARGE_INTEGER)&Offset,
                                         NULL );

    if ( Irp == NULL ) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SetFlag( IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME );

    //
    //  Set up the completion routine
    //

    IoSetCompletionRoutine( Irp,
                            W2KNtfsVerifyReadCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Call the device to do the write and wait for it to finish.
    //

    try {

        (VOID)IoCallDriver( DeviceObject, Irp );
        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

        //
        //  Grab the Status.
        //

        Status = Irp->IoStatus.Status;

    } finally {

        //
        //  If there is an MDL (or MDLs) associated with this I/O
        //  request, Free it (them) here.  This is accomplished by
        //  walking the MDL list hanging off of the IRP and deallocating
        //  each MDL encountered.
        //

        while (Irp->MdlAddress != NULL) {

            PMDL NextMdl;

            NextMdl = Irp->MdlAddress->Next;

            MmUnlockPages( Irp->MdlAddress );

            IoFreeMdl( Irp->MdlAddress );

            Irp->MdlAddress = NextMdl;
        }

        IoFreeIrp( Irp );
    }

    //
    //  If it doesn't succeed then raise the error
    //

    return Status;
}


BOOLEAN
W2KNtfsFindAttribute(
	IN	PVOID						Buffer,
    IN	ATTRIBUTE_TYPE_CODE			AttributeTypeCode,
	OUT	PATTRIBUTE_RECORD_HEADER	*Attribute
	)
{
    PFILE_RECORD_SEGMENT_HEADER fileRecord;
	PATTRIBUTE_RECORD_HEADER	attrHeader;
	ULONG						offset;

	
	fileRecord = (PFILE_RECORD_SEGMENT_HEADER)Buffer;
//	DbgPrint("fileRecord1->BytesAvailable = %d, filerecord1->SequenceNumber = %x\n", fileRecord1->BytesAvailable, fileRecord1->SequenceNumber);
					
	offset = fileRecord->FirstAttributeOffset;

	do {
		attrHeader = 
			(PATTRIBUTE_RECORD_HEADER)((CHAR *)fileRecord + offset);
		
//	DbgPrint("attrHeader1->RecordLength = %d, attrHeader1->TypeCode = %x, attrHeader1->FormCode\n", 
//			attrHeader->RecordLength, attrHeader->TypeCode, attrHeader->FormCode);

		if(attrHeader->TypeCode == AttributeTypeCode)
		{
			*Attribute = attrHeader;
			return TRUE;
		}

		if(attrHeader->TypeCode == $END)
			break;

		offset += attrHeader->RecordLength;
		if(offset >= fileRecord->BytesAvailable)
			break;
		
	}while(1);

	return FALSE;
}


BOOLEAN
W2kNtfsFlushMetaFile (
    IN PDEVICE_OBJECT		FSpyDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN ULONG				BufferLen,
	IN PCHAR				Buffer
	)
{
	PVCB						vcb;
	PVOLUME_DEVICE_OBJECT		volDo;
	PFILESPY_DEVICE_EXTENSION	devExt;
	NTSTATUS					status;	
	BOOLEAN						found;
	PFILE_RECORD_SEGMENT_HEADER fileRecord;
	PATTRIBUTE_RECORD_HEADER	attribute;
	LARGE_INTEGER				validDataLength;
	LARGE_INTEGER				mftOffset;
	BOOLEAN						result1 = 0, result2 = 0, result3 = 0;
	FILE_REFERENCE				fileReference;
	PATTRIBUTE_RECORD_HEADER	mftBitmapAttribute;
				


//	baseDeviceObject 
//		= ((PFILESPY_DEVICE_EXTENSION) (FSpyDeviceObject->DeviceExtension))->BaseVolumeDeviceObject;
	volDo = (PVOLUME_DEVICE_OBJECT)baseDeviceObject;
	vcb = &volDo->Vcb;

	devExt = FSpyDeviceObject->DeviceExtension;
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("FSpyDeviceObject = %p, devExt = %p\n", FSpyDeviceObject, devExt));
	ASSERT(Buffer);

	if(Buffer == NULL)
		return FALSE;

	status = W2KNtfsPerformVerifyDiskRead (
				devExt->DiskDeviceObject,
				(PVOID)Buffer,
				(LONGLONG)LlBytesFromClusters(vcb, vcb->MftStartLcn),
				(LONG)BufferLen
				);

	if(!NT_SUCCESS(status)) {
		DbgPrint("read fail\n");
		return FALSE;
	}
					
	if(vcb->BitmapScb && vcb->BitmapScb->FileObject)
		result1 = CcPurgeCacheSection (
					&vcb->BitmapScb->NonpagedScb->SegmentObject,
					NULL,
					0,
					FALSE
					);
					
	if(vcb->MftBitmapScb && vcb->MftBitmapScb->FileObject)
		result2 = CcPurgeCacheSection (
					&vcb->MftBitmapScb->NonpagedScb->SegmentObject,
					NULL,
					0,
					FALSE
					);

	/* 0 - 3 can' be purged */
	NtfsSetSegmentNumber(&fileReference, 0, 4);
	mftOffset.QuadPart = NtfsFullSegmentNumber( &fileReference );
	mftOffset.QuadPart = LlBytesFromFileRecords( vcb, mftOffset.QuadPart );
				
	if(vcb->MftScb && vcb->MftScb->FileObject)
		result3 = CcPurgeCacheSection (
					&vcb->MftScb->NonpagedScb->SegmentObject,
					&mftOffset,
					(ULONG)vcb->MftScb->Header.FileSize.QuadPart,
					FALSE
					);

	fileRecord = (PFILE_RECORD_SEGMENT_HEADER)Buffer;

//	if(devExt->MftLsn.QuadPart == fileRecord->Lsn.QuadPart)
//		goto DO_NOTHING;

//	devExt->MftLsn.QuadPart = fileRecord->Lsn.QuadPart;

	found = W2KNtfsFindAttribute(Buffer, $DATA, &attribute);
	if(!found) {
		DbgPrint("attribute fail\n");
		//ASSERT(FALSE);
		return FALSE;
	}

	if (NtfsIsAttributeResident( attribute )) {
		// DbgPrint("attrHeader1->Form.Resident.ValueLength = %x\n",
		//	attrHeader1->Form.Resident.ValueLength);
		ASSERT(FALSE);		// do more
		return FALSE;
	} else
	{
		validDataLength.QuadPart = attribute->Form.Nonresident.ValidDataLength;
	}

	if(vcb->MftScb && validDataLength.QuadPart != vcb->MftScb->Header.ValidDataLength.QuadPart)
	{
		VCN		vcn;
		BOOLEAN	result0 = 0, result1 = 0, result2 = 0, 
				result3 = 0, result4 = 0, result5 = 0;
						

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
			("attribute->Form.Nonresident.AllocatedLength = %x\n",
			attribute->Form.Nonresident.AllocatedLength / 512));
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
			("attribute->Form.Nonresident.ValidDataLength = %x\n",
			attribute->Form.Nonresident.ValidDataLength / 512));
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
			("attribute->Form.Nonresident.TotalAllocated = %x\n",
			attribute->Form.Nonresident.TotalAllocated / 512));					
//		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
//			("devExt->MftLsn.QuadPart = %I64x, fileRecord->Lsn.QuadPart = %I64x\n",
//			devExt->MftLsn.QuadPart, fileRecord->Lsn.QuadPart));
	
		vcb->MftScb->Header.AllocationSize.QuadPart 
			= attribute->Form.Nonresident.AllocatedLength;
		vcb->MftScb->Header.ValidDataLength.QuadPart 
			= attribute->Form.Nonresident.ValidDataLength;
		vcb->MftScb->Header.FileSize.QuadPart
			= attribute->Form.Nonresident.FileSize;
			
		vcb->MftScb->TotalAllocated
			= vcb->MftScb->Header.AllocationSize.QuadPart;
						
		CcSetFileSizes( vcb->MftScb->FileObject,
			(PCC_FILE_SIZES)&vcb->MftScb->Header.AllocationSize
			);
											
		vcn = attribute->Form.Nonresident.LowestVcn;
		
		do {
			LCN			lcn;
			LONGLONG	clusterCount;
	
		
			W2KNtfsLookupAllocation (
				attribute,
				vcb->MftScb,
				vcn,
				&lcn,
				&clusterCount,
				NULL,
				NULL
				);
						
			vcn += clusterCount;
		} while(vcn < attribute->Form.Nonresident.HighestVcn);
	}

	found = W2KNtfsFindAttribute(Buffer, $BITMAP, &mftBitmapAttribute);
	if(!found) {
		DbgPrint("mftBitmapAttribute fail\n");
		ASSERT(FALSE);
		return FALSE;
	}

	if (NtfsIsAttributeResident( mftBitmapAttribute )) {
		// DbgPrint("attrHeader1->Form.Resident.ValueLength = %x\n",
		//	attrHeader1->Form.Resident.ValueLength);
		ASSERT(FALSE);		// do more
	} else
	{
		validDataLength.QuadPart = attribute->Form.Nonresident.ValidDataLength;
	}

	if(vcb->MftBitmapScb && validDataLength.QuadPart != vcb->MftBitmapScb->Header.ValidDataLength.QuadPart)
	{
		VCN		mftBitmapVcn;
		BOOLEAN	result0 = 0, result1 = 0, result2 = 0, 
				result3 = 0, result4 = 0, result5 = 0;
							

		vcb->MftBitmapScb->Header.AllocationSize.QuadPart 
			= mftBitmapAttribute->Form.Nonresident.AllocatedLength;
		vcb->MftBitmapScb->Header.ValidDataLength.QuadPart 
			= mftBitmapAttribute->Form.Nonresident.ValidDataLength;
		vcb->MftBitmapScb->Header.FileSize.QuadPart
			= mftBitmapAttribute->Form.Nonresident.FileSize;
		
		vcb->MftBitmapScb->TotalAllocated
			= vcb->MftBitmapScb->Header.AllocationSize.QuadPart;
						
		CcSetFileSizes( vcb->MftBitmapScb->FileObject,
			(PCC_FILE_SIZES)&vcb->MftBitmapScb->Header.AllocationSize
		);
					
		mftBitmapVcn = mftBitmapAttribute->Form.Nonresident.LowestVcn;
		do {
			LCN			mftBitmapLcn;
			LONGLONG	mftBitmapVcnClusterCount;
		
			
			W2KNtfsLookupAllocation (
				mftBitmapAttribute,
				vcb->MftBitmapScb,
				mftBitmapVcn,
				&mftBitmapLcn,
				&mftBitmapVcnClusterCount,
				NULL,
				NULL
				);
			
			mftBitmapVcn += mftBitmapVcnClusterCount;	
		} while(mftBitmapVcn < mftBitmapAttribute->Form.Nonresident.HighestVcn);
	}

	return TRUE;
}


VOID
W2kNtfsFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		FSpyDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp,
	IN ULONG				BufferLen,
	IN PCHAR				Buffer
	)
{
	PVCB					vcb;
//	PDEVICE_OBJECT			baseDeviceObject;
    PVOLUME_DEVICE_OBJECT	volDo;
	PFILE_OBJECT			fileObject;
				

//	baseDeviceObject 
//		= ((PFILESPY_DEVICE_EXTENSION) (FSpyDeviceObject->DeviceExtension))->BaseVolumeDeviceObject;
	volDo = (PVOLUME_DEVICE_OBJECT)baseDeviceObject;
	vcb = &volDo->Vcb;
		
	fileObject = IrpSp->FileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
		("fileObject->FileName = %Z\n",
			&fileObject->FileName)); 
			
//	if (FlagOn( vcb->VcbState, VCB_STATE_MOUNT_READ_ONLY ))
	{
		PSCB			scb;
		TYPE_OF_OPEN	typeOfOpen = UnopenedFileObject;	
		PCCB			ccb;
				
				
		ccb = (PCCB)fileObject->FsContext2;
		ASSERT(ccb);

		typeOfOpen = ccb->TypeOfOpen;

		scb = (PSCB)fileObject->FsContext;
		ASSERT(scb != NULL);

		if ((UserDirectoryOpen == typeOfOpen) &&
			!FlagOn( scb->ScbState, SCB_STATE_VIEW_INDEX )) 
		{
			BOOLEAN flushResult;
			BOOLEAN	scbResult0 = 0;
				

			flushResult = W2kNtfsFlushMetaFile (
								FSpyDeviceObject,
								baseDeviceObject,
								BufferLen,
								Buffer
								);

			if(flushResult != TRUE) {
				DbgPrint("flushResult = %d\n", flushResult);
				return;
			}

										
			if(scb->NonpagedScb && scb->FileObject)
				scbResult0 = CcPurgeCacheSection (
//								scb->FileObject->SectionObjectPointer,
								&scb->NonpagedScb->SegmentObject,
								NULL,
								0,
								FALSE
								);
			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
							("scbResult0 = %d\n", scbResult0));
		}
	}
	
	return;
}