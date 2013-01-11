#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)


NTSTATUS
NdasFatSecondaryCommonWrite (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	struct Write				write;
	
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PCCB						ccb;
	BOOLEAN						fcbAcquired = FALSE;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
			return STATUS_FILE_CORRUPT_ERROR;
		}
	}

	if (irpSp->Parameters.Write.ByteOffset.QuadPart == FILE_WRITE_TO_END_OF_FILE && 
		irpSp->Parameters.Write.ByteOffset.HighPart == -1) {

		write.ByteOffset = fcb->Header.FileSize;

	} else {

		write.ByteOffset = irpSp->Parameters.Write.ByteOffset;
	}

	write.Key		= 0;
	write.Length	= irpSp->Parameters.Write.Length;

	if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
		
		ASSERT( (write.ByteOffset.QuadPart + write.Length) <= 
				((fcb->Header.AllocationSize.QuadPart + PAGE_SIZE - 1) & ~((LONGLONG) (PAGE_SIZE-1))) );

		return STATUS_SUCCESS;
	}

	ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ); 
	//ASSERT( !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_LAZY_WRITE ) );

	if ( (write.ByteOffset.QuadPart + write.Length) <= fcb->Header.FileSize.LowPart) {

		return STATUS_SUCCESS;
	}

	if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

		return STATUS_PENDING;

		ASSERT( FALSE );
		DebugTrace2( 0, Dbg, ("Can't wait in NdasFatSecondaryCommonWrite\n") );

		status = FatFsdPostRequest( IrpContext, Irp );

		DebugTrace2( -1, Dbg2, ("NdasFatSecondaryCommonWrite:  FatFsdPostRequest -> %08lx\n", status) );
		return status;
	}

	DebugTrace2( 0, Dbg, ("write.ByteOffset.QuadPart + write.Length > fcb->Header.AllocationSize.QuadPart = %d "
						  "ExIsResourceAcquiredSharedLite(fcb->Header.Resource) = %d\n",
						  ((write.ByteOffset.QuadPart + write.Length) > fcb->Header.AllocationSize.QuadPart),
						  ExIsResourceAcquiredSharedLite(fcb->Header.Resource)) );

	if ((write.ByteOffset.QuadPart + write.Length) > fcb->Header.AllocationSize.QuadPart) {

		FatAcquireExclusiveFcb( IrpContext, fcb );
		fcbAcquired = TRUE;
	
	} 	

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}


		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

		if (secondaryRequest == NULL) {

			FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_SET_INFORMATION, 0 );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

		ndfsWinxpRequestHeader->IrpFlags4   = 0;
		ndfsWinxpRequestHeader->IrpSpFlags = 0;

		ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
		ndfsWinxpRequestHeader->SetFile.Length					= sizeof( FILE_END_OF_FILE_INFORMATION );
		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;

		ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = write.ByteOffset.QuadPart + write.Length;


		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASFAT_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if (status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			status = STATUS_IO_DEVICE_ERROR;
			leave;
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = NTOHL(ndfsWinxpReplytHeader->Status4);
		Irp->IoStatus.Information = write.Length;

		if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg2, ("NdNtfsSecondaryCommonWrite: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );
			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
		
		} else
			ASSERT( ndfsWinxpReplytHeader->FileInformationSet );
	
		if (ndfsWinxpReplytHeader->FileInformationSet) {

			PNDFS_FAT_MCB_ENTRY	mcbEntry;
			ULONG			index;

			BOOLEAN			lookupResut;
			VBO				vcn;
			LBO				lcn;
			//LBO			startingLcn;
			ULONG			clusterCount;

			//DbgPrint( "w NTOHLL(ndfsWinxpReplytHeader->FileSize8) = %x\n", NTOHLL(ndfsWinxpReplytHeader->FileSize8) );

			if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) != fcb->Header.AllocationSize.QuadPart) {

				ASSERT( ExIsResourceAcquiredExclusiveLite(fcb->Header.Resource) );

				ASSERT( (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) > fcb->Header.AllocationSize.QuadPart );

				mcbEntry = (PNDFS_FAT_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

				for (index=0, vcn=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

					lookupResut = FatLookupMcbEntry( vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					
					if (lookupResut == TRUE && vcn < fcb->Header.AllocationSize.QuadPart) {

						ASSERT( lookupResut == TRUE );
						//ASSERT( startingLcn == lcn );
						ASSERT( vcn == mcbEntry->Vcn );
						ASSERT( lcn == (((LBO)mcbEntry->Lcn) << vcb->AllocationSupport.LogOfBytesPerSector) );
						ASSERT( clusterCount <= mcbEntry->ClusterCount );

						if (clusterCount < mcbEntry->ClusterCount) {

							FatAddMcbEntry ( vcb, 
											 &fcb->Mcb, 
											 (VBO)mcbEntry->Vcn, 
											 ((LBO)mcbEntry->Lcn) << vcb->AllocationSupport.LogOfBytesPerSector, 
											 (ULONG)mcbEntry->ClusterCount );

							lookupResut = FatLookupMcbEntry( vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

							ASSERT( lookupResut == TRUE );
							//ASSERT( startingLcn == lcn );
							ASSERT( vcn == mcbEntry->Vcn );
							ASSERT( lcn == (((LBO)mcbEntry->Lcn) << vcb->AllocationSupport.LogOfBytesPerSector) );
							ASSERT( clusterCount == mcbEntry->ClusterCount );
						}
					
					} else { 

						ASSERT( lookupResut == FALSE || lcn == 0 );

						FatAddMcbEntry ( vcb, 
										 &fcb->Mcb, 
										 (VBO)mcbEntry->Vcn, 
										 ((LBO)mcbEntry->Lcn) << vcb->AllocationSupport.LogOfBytesPerSector, 
										 (ULONG)mcbEntry->ClusterCount );
					}

					vcn += (ULONG)mcbEntry->ClusterCount;
				}

				ASSERT( vcn == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );

				fcb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);
				SetFlag( fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );		

				if (CcIsFileCached(fileObject)) {

					ASSERT( fileObject->SectionObjectPointer->SharedCacheMap != NULL );
					CcSetFileSizes( fileObject, (PCC_FILE_SIZES)&fcb->Header.AllocationSize );
				}
			}

			DebugTrace2(0, Dbg, ("write scb->Header.FileSize.LowPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
								 fcb->Header.FileSize.LowPart, fcb->Header.ValidDataLength.QuadPart) );

		}

#if DBG
		{
			BOOLEAN			lookupResut;
			VBO				vcn;
			LBO				lcn;
			//LCN				startingLcn;
			ULONG			clusterCount;

			vcn = 0;
			while (1) {

				lookupResut = FatLookupMcbEntry( vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
				if (lookupResut == FALSE || lcn == 0)
					break;

				vcn += clusterCount;
			}

			ASSERT( vcn == fcb->Header.AllocationSize.QuadPart );
		}

#endif

	} finally {
	
		if (secondarySessionResourceAcquired == TRUE)
				SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	return status;
}


NTSTATUS
NdasFatSecondaryCommonWrite2 (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp,
	IN ULONG		BytesToWrite
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	struct Write				write;
	
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PCCB						ccb;
	BOOLEAN						fcbAcquired = FALSE;

	BOOLEAN						writeToEof;
	PUCHAR						inputBuffer;
	ULONG						totalWriteLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
			return STATUS_FILE_CORRUPT_ERROR;
		}
	}

	if (irpSp->Parameters.Write.ByteOffset.QuadPart == FILE_WRITE_TO_END_OF_FILE && 
		irpSp->Parameters.Write.ByteOffset.HighPart == -1) {

		write.ByteOffset = fcb->Header.FileSize;

	} else {

		write.ByteOffset = irpSp->Parameters.Write.ByteOffset;
	}

	if (write.ByteOffset.QuadPart >= fcb->Header.ValidDataLength.QuadPart) {

		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = BytesToWrite;

		//DebugTrace2( -1, Dbg2, ("NtfsCommonCreate:  write.ByteOffset.QuadPart >= fcb->Header.FileSize.LowPart -> %08lx\n", Irp->IoStatus.Status) );

		return Irp->IoStatus.Status;
	}

	ASSERT( BytesToWrite );

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        DebugTrace2( 0, Dbg2, ("Can't wait in create\n") );

        status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace2( -1, Dbg2, ("NtfsCommonCreate:  FatFsdPostRequest -> %08lx\n", status) );
        return status;
    }

	write.Key		= 0;
	write.Length	= irpSp->Parameters.Write.Length;
	write.Length	= (fcb->Header.FileSize.LowPart - write.ByteOffset.LowPart) < BytesToWrite ? 
							(fcb->Header.FileSize.LowPart - write.ByteOffset.LowPart) : BytesToWrite;

	ASSERT( write.Length );

	ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ); 

	//FatAcquireSharedFcb( IrpContext, fcb );
	//fcbAcquired = TRUE;


	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );

			if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
	
				try_return( status = STATUS_FILE_LOCK_CONFLICT );
				
			} else {

				NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
		}

		inputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalWriteLength = 0;

		do {

			ULONG	inputBufferLength;
			UINT8		*ndfsWinxpRequestData;
			UINT64	primaryFileHandle;


			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdasFatSecondaryCommonWrite2: fileName = %wZ\n", &fileObject->FileName );

				totalWriteLength = write.Length;
				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->NonPaged->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->NonPaged->CcbQueue.Flink; 
					 ccbListEntry != &fcb->NonPaged->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->NonPaged->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			inputBufferLength = ((write.Length-totalWriteLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (write.Length-totalWriteLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_WRITE,
															  volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_WRITE, inputBufferLength );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_WRITE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(primaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Write.Length		= inputBufferLength;
			ndfsWinxpRequestHeader->Write.Key			= write.Key;
			ndfsWinxpRequestHeader->Write.ByteOffset	= write.ByteOffset.QuadPart + totalWriteLength;
			ndfsWinxpRequestHeader->Write.ForceWrite	= TRUE;


			DebugTrace2( 0, Dbg, ("ndfsWinxpRequestHeader->Write.ByteOffset = %I64d, ndfsWinxpRequestHeader->Write.Length = %d\n", 
								   ndfsWinxpRequestHeader->Write.ByteOffset, ndfsWinxpRequestHeader->Write.Length) );

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			if (inputBufferLength) {

				try {

					RtlCopyMemory( ndfsWinxpRequestData,
								   inputBuffer + totalWriteLength,
								   inputBufferLength );

				} except (EXCEPTION_EXECUTE_HANDLER) {

					DebugTrace2( 0, Dbg2, ("RedirectIrp: Exception - Input buffer is not valid\n") );
					
					status = GetExceptionCode();
					break;
				}
			}

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", ndfsWinxpRequestData );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if (status != STATUS_SUCCESS) {

				secondaryRequest = NULL;
				status = STATUS_IO_DEVICE_ERROR;
				leave;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
	
					try_return( status = STATUS_FILE_LOCK_CONFLICT );
				
				} else {

					NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {
			
				DebugTrace2( 0, Dbg, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

				if (totalWriteLength)
					status = STATUS_SUCCESS;
				else
					status = NTOHL(ndfsWinxpReplytHeader->Status4);

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			totalWriteLength += NTOHL(ndfsWinxpReplytHeader->Information32);

			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= inputBufferLength );
			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) != 0 );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalWriteLength < write.Length );

try_exit: NOTHING;

	} finally {

		if (!AbnormalTermination()) {
			
			if (status == STATUS_FILE_LOCK_CONFLICT) {

				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = status;
			
			} else {

				if (totalWriteLength) {

					if (totalWriteLength == write.Length) 
						Irp->IoStatus.Information = BytesToWrite;
					else
						Irp->IoStatus.Information = totalWriteLength;

					Irp->IoStatus.Status = STATUS_SUCCESS;
		
					ASSERT( Irp->IoStatus.Information == BytesToWrite );

				} else {
		
					Irp->IoStatus.Information = 0;
					Irp->IoStatus.Status = status;
				}
			}
		}

		DebugTrace2( 0, Dbg, ("write.ByteOffset.QuadPart = %I64x, write.Length = %x, totalWriteRequestLength = %x lastStatus = %x\n", 
								write.ByteOffset.QuadPart, write.Length, totalWriteLength, status) );

		if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE) && Irp->IoStatus.Status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg, ("write.ByteOffset.QuadPart = %I64x, write.Length = %x, totalWriteRequestLength = %x lastStatus = %x\n", 
								 write.ByteOffset.QuadPart, write.Length, totalWriteLength, status) );

			PrintIrp( Dbg, "RedirectIrpMajorWrite", NULL, Irp );
		}

		if (secondarySessionResourceAcquired == TRUE)
			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	return status;
}

NTSTATUS
NdasFatSecondaryCommonWrite3 (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	struct Write				write;
	
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PCCB						ccb;
	BOOLEAN						fcbAcquired = FALSE;

	BOOLEAN						writeToEof;
	PUCHAR						inputBuffer;
	ULONG						totalWriteLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );
	ASSERT (!FlagOn(Irp->Flags, IRP_PAGING_IO));
	
	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );

			status = STATUS_FILE_CORRUPT_ERROR;
	        FatCompleteRequest( IrpContext, Irp, status );
			return status;
		}
	}

	writeToEof = (irpSp->Parameters.Write.ByteOffset.QuadPart == FILE_WRITE_TO_END_OF_FILE && 
				  irpSp->Parameters.Write.ByteOffset.HighPart == -1);

	write.ByteOffset	= irpSp->Parameters.Write.ByteOffset;
	write.Key			= irpSp->Parameters.Write.Key;
	write.Length		= irpSp->Parameters.Write.Length;

	ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ); 

	//FatAcquireSharedFcb( IrpContext, fcb );
	//fcbAcquired = TRUE;


	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );

			if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
	
				try_return( status = STATUS_FILE_LOCK_CONFLICT );
				
			} else {

				NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
		}

		inputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalWriteLength = 0;

		do {

			ULONG	inputBufferLength;
			UINT8		*ndfsWinxpRequestData;
			UINT64	primaryFileHandle;

			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdasFatSecondaryCommonWrite2: fileName = %wZ\n", &fileObject->FileName );

				totalWriteLength = write.Length;
				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->NonPaged->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->NonPaged->CcbQueue.Flink; 
					 ccbListEntry != &fcb->NonPaged->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->NonPaged->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			inputBufferLength = ((write.Length-totalWriteLength) <= volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize) 
									? (write.Length-totalWriteLength) : volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_WRITE,
															  volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_WRITE, inputBufferLength );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_WRITE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(primaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Write.Length		= inputBufferLength;
			ndfsWinxpRequestHeader->Write.Key			= write.Key;
			if (writeToEof)
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart;
			else
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart + totalWriteLength;
			
			ndfsWinxpRequestHeader->Write.ForceWrite	= TRUE;


			DebugTrace2( 0, Dbg, ("ndfsWinxpRequestHeader->Write.ByteOffset = %I64d, ndfsWinxpRequestHeader->Write.Length = %d\n", 
								   ndfsWinxpRequestHeader->Write.ByteOffset, ndfsWinxpRequestHeader->Write.Length) );

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			if (inputBufferLength) {

				try {

					RtlCopyMemory( ndfsWinxpRequestData,
								   inputBuffer + totalWriteLength,
								   inputBufferLength );

				} except (EXCEPTION_EXECUTE_HANDLER) {

					DebugTrace2( 0, Dbg2, ("RedirectIrp: Exception - Input buffer is not valid\n") );
					
					status = GetExceptionCode();
					break;
				}
			}

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", ndfsWinxpRequestData );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if (status != STATUS_SUCCESS) {

				secondaryRequest = NULL;
				status = STATUS_IO_DEVICE_ERROR;
				leave;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
	
					try_return( status = STATUS_FILE_LOCK_CONFLICT );
				
				} else {

					NDAS_BUGON( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {
			
				DebugTrace2( 0, Dbg, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

				if (totalWriteLength)
					status = STATUS_SUCCESS;
				else
					status = NTOHL(ndfsWinxpReplytHeader->Status4);

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			totalWriteLength += NTOHL(ndfsWinxpReplytHeader->Information32);

			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= inputBufferLength );
			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) != 0 );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalWriteLength < write.Length );

try_exit: NOTHING;

	} finally {

		if (!AbnormalTermination()) {
			
			if (totalWriteLength) {

				Irp->IoStatus.Information = totalWriteLength;
				Irp->IoStatus.Status = STATUS_SUCCESS;
		
			} else {
		
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = status;
			}
		}

		DebugTrace2( 0, Dbg, ("write.ByteOffset.QuadPart = %I64x, write.Length = %x, totalWriteRequestLength = %x lastStatus = %x\n", 
								write.ByteOffset.QuadPart, write.Length, totalWriteLength, status) );

		if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE) && Irp->IoStatus.Status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg, ("write.ByteOffset.QuadPart = %I64x, write.Length = %x, totalWriteRequestLength = %x lastStatus = %x\n", 
								 write.ByteOffset.QuadPart, write.Length, totalWriteLength, status) );

			PrintIrp( Dbg, "RedirectIrpMajorWrite", NULL, Irp );
		}

		if (secondarySessionResourceAcquired == TRUE)
			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	FatCompleteRequest( IrpContext, Irp, status );
	return status;
}

#endif