#include "NtfsProc.h"


#ifdef __ND_NTFS_SECONDARY__

#define BugCheckFileId                   (NTFS_BUG_CHECK_WRITE)

#define Dbg                              (DEBUG_TRACE_WRITE)
#define Dbg2                             (DEBUG_INFO_WRITE)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('cfdN')




NTSTATUS
NdNtfsSecondaryCommonWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
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
	PSCB						scb;
	PCCB						ccb;
	BOOLEAN						scbAcquired = FALSE;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );
		
	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

		if (FlagOn( scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {
	
			ASSERT( FALSE );
			NtfsRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else {
					
			ASSERT( FlagOn(ccb->NdNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );
			
			return STATUS_FILE_CORRUPT_ERROR;
		}
	}

	if (irpSp->Parameters.Write.ByteOffset.QuadPart == FILE_WRITE_TO_END_OF_FILE && 
		irpSp->Parameters.Write.ByteOffset.HighPart == -1) {

		write.ByteOffset = scb->Header.FileSize;

	} else {

		write.ByteOffset = irpSp->Parameters.Write.ByteOffset;
	}

	write.Key		= 0;
	write.Length	= irpSp->Parameters.Write.Length;

	if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
		
		ASSERT( (write.ByteOffset.QuadPart + write.Length) <= 
				((scb->Header.AllocationSize.QuadPart + PAGE_SIZE - 1) & ~((LONGLONG) (PAGE_SIZE-1))) );

		return STATUS_SUCCESS;
	}

	ASSERT( FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) ); 
	ASSERT( !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_LAZY_WRITE ) );

	if ( (write.ByteOffset.QuadPart + write.Length) <= scb->Header.FileSize.QuadPart) {

		return STATUS_SUCCESS;
	}

	if ((write.ByteOffset.QuadPart + write.Length) > scb->Header.AllocationSize.QuadPart) {

		NtfsAcquireExclusiveScb( IrpContext, scb );
		scbAcquired = TRUE;
	}	

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}


		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

		if(secondaryRequest == NULL) {

			NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_SET_INFORMATION, 0 );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		ndfsWinxpRequestHeader->IrpTag   = (_U32)Irp;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

		ndfsWinxpRequestHeader->IrpFlags   = 0;
		ndfsWinxpRequestHeader->IrpSpFlags = 0;

		ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
		ndfsWinxpRequestHeader->SetFile.Length					= sizeof( FILE_END_OF_FILE_INFORMATION );
		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;

		ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = write.ByteOffset.QuadPart + write.Length;


		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			status = STATUS_IO_DEVICE_ERROR;
			leave;
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = write.Length;

		if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

			DebugTrace( 0, Dbg2, ("NdNtfsSecondaryCommonWrite: ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );
			ASSERT( ndfsWinxpReplytHeader->Information == 0 );
		
		} else
			ASSERT( ndfsWinxpReplytHeader->FileInformationSet );
	
		if (ndfsWinxpReplytHeader->FileInformationSet) {

			PNDFS_MCB_ENTRY	mcbEntry;
			ULONG			index;

			BOOLEAN			lookupResut;
			VCN				vcn;
			LCN				lcn;
			LCN				startingLcn;
			LONGLONG		clusterCount;

			if (ndfsWinxpReplytHeader->AllocationSize != scb->Header.AllocationSize.QuadPart) {

				ASSERT( NtfsIsExclusiveScb(scb) );

				ASSERT( ndfsWinxpReplytHeader->AllocationSize > scb->Header.AllocationSize.QuadPart );

				mcbEntry = (PNDFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

				for (index=0, vcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

					lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					
					if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, scb->Header.AllocationSize.QuadPart)) {

						ASSERT( lookupResut == TRUE );
						ASSERT( startingLcn == lcn );
						ASSERT( vcn == mcbEntry->Vcn );
						ASSERT( lcn == mcbEntry->Lcn );
						ASSERT( clusterCount <= mcbEntry->ClusterCount );

						if (clusterCount < mcbEntry->ClusterCount) {

							NtfsAddNtfsMcbEntry( &scb->Mcb, 
												 mcbEntry->Vcn, 
												 mcbEntry->Lcn, 
												 (LONGLONG)mcbEntry->ClusterCount, 
												 FALSE );

							lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );

							ASSERT( lookupResut == TRUE );
							ASSERT( startingLcn == lcn );
							ASSERT( vcn == mcbEntry->Vcn );
							ASSERT( lcn == mcbEntry->Lcn );
							ASSERT( clusterCount == mcbEntry->ClusterCount );
						}
					
					} else { 

						ASSERT( lookupResut == FALSE || lcn == UNUSED_LCN );

						NtfsAddNtfsMcbEntry( &scb->Mcb, 
											 mcbEntry->Vcn, 
											 mcbEntry->Lcn, 
											 (LONGLONG)mcbEntry->ClusterCount, 
											 FALSE );
					}

					vcn += mcbEntry->ClusterCount;
				}

				ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == ndfsWinxpReplytHeader->AllocationSize );

				scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;
				SetFlag( scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );		

				if (CcIsFileCached(fileObject)) {

					ASSERT( fileObject->SectionObjectPointer->SharedCacheMap != NULL );
					NtfsSetBothCacheSizes( fileObject,
										   (PCC_FILE_SIZES)&scb->Header.AllocationSize,
										   scb );
				}
			}

			DebugTrace(0, Dbg, ("write scb->Header.FileSize.QuadPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
								 scb->Header.FileSize.QuadPart, scb->Header.ValidDataLength.QuadPart) );
		}

#if DBG
		{
			BOOLEAN			lookupResut;
			VCN				vcn;
			LCN				lcn;
			LCN				startingLcn;
			LONGLONG		clusterCount;

			vcn = 0;
			while (1) {

				lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
				if (lookupResut == FALSE || lcn == UNUSED_LCN)
					break;

				vcn += clusterCount;
			}

			ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == scb->Header.AllocationSize.QuadPart );
		}

#endif



	} finally {
	
		if (secondarySessionResourceAcquired == TRUE)
				SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );

		if (scbAcquired) {
             NtfsReleaseScb( IrpContext, scb );
        }

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	return status;
}


#endif