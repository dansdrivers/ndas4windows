#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__


#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')



NTSTATUS
NdasNtfsSecondaryCommonQueryQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct QueryQuota			queryQuota;

	PVOID						inputBuffer;
	ULONG						inputBufferLength;
	PVOID						outputBuffer;
	ULONG						outputBufferLength;
	ULONG						bufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );


	if (!FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT)) {

		return NtfsPostRequest( IrpContext, Irp );
	}



	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if (FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		queryQuota.Length			= irpSp->Parameters.QueryQuota.Length;
		queryQuota.SidList			= irpSp->Parameters.QueryQuota.SidList;
		queryQuota.SidListLength	= irpSp->Parameters.QueryQuota.SidListLength;
		queryQuota.StartSid			= irpSp->Parameters.QueryQuota.StartSid;

		inputBuffer					= queryQuota.SidList;
		inputBufferLength			= queryQuota.SidListLength;
		outputBuffer				= NtfsMapUserBuffer( Irp );
		outputBufferLength			= queryQuota.Length;
		bufferLength				= (inputBufferLength >= outputBufferLength) ? inputBufferLength : outputBufferLength;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_QUERY_QUOTA,
														  bufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_QUERY_QUOTA, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->QueryQuota.Length			= outputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.InputLength		= inputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.StartSidOffset	= (PCHAR)queryQuota.StartSid - (PCHAR)queryQuota.SidList;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength);

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
	
		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;
		
		if(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) {

			ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW );
			ASSERT( Irp->IoStatus.Information );
			ASSERT( Irp->IoStatus.Information <= outputBufferLength );
			ASSERT( outputBuffer );
			
			RtlCopyMemory( outputBuffer,
						   (_U8 *)(ndfsWinxpReplytHeader+1),
						   Irp->IoStatus.Information );
		}

try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}




NTSTATUS
NdasNtfsSecondaryCommonSetQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct SetQuota				setQuota;
	PVOID						inputBuffer;
	ULONG						inputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		setQuota.Length = irpSp->Parameters.SetQuota.Length;

		inputBuffer			= NtfsMapUserBuffer( Irp );
		inputBufferLength	= setQuota.Length;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_QUOTA,
														  inputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_QUOTA, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->SetQuota.Length	= inputBufferLength;
		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength) ;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
	
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

try_exit:  NOTHING;

	} finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}


NTSTATUS
NdasNtfsSecondaryCommonQueryVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct QueryVolume			queryVolume;
	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PVOID						outputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG						outputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		queryVolume.FsInformationClass	= irpSp->Parameters.QueryVolume.FsInformationClass;
		queryVolume.Length				= irpSp->Parameters.QueryVolume.Length;
		outputBufferLength				= queryVolume.Length;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_QUERY_VOLUME_INFORMATION,
														  outputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_QUERY_VOLUME_INFORMATION, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->QueryVolume.Length			   = outputBufferLength;
		ndfsWinxpRequestHeader->QueryVolume.FsInformationClass = queryVolume.FsInformationClass;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
	
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		if (status != STATUS_SUCCESS)
			DebugTrace( 0, Dbg, ("Status = %x, Irp->IoStatus.Information = %d, queryVolume.FsInformationClass =%d\n", 
								  status, Irp->IoStatus.Information, queryVolume.FsInformationClass) );
		
		if(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) {

			ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW );
			ASSERT( Irp->IoStatus.Information );
			ASSERT( Irp->IoStatus.Information <= outputBufferLength );
			ASSERT( outputBuffer );
			
			RtlCopyMemory( outputBuffer,
						   (_U8 *)(ndfsWinxpReplytHeader+1),
						   Irp->IoStatus.Information );
		
		}
		else
			ASSERT( ndfsWinxpReplytHeader->Information == 0 );

try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}



NTSTATUS
NdasNtfsSecondaryCommonSetVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct	SetVolume			setVolume;
	PVOID						inputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG						inputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		setVolume.FsInformationClass	= irpSp->Parameters.SetVolume.FsInformationClass;
		setVolume.Length				= irpSp->Parameters.SetVolume.Length;

		inputBufferLength				= setVolume.Length;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_VOLUME_INFORMATION,
														  inputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_VOLUME_INFORMATION, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->SetVolume.Length				= setVolume.Length;
		ndfsWinxpRequestHeader->SetVolume.FsInformationClass	= setVolume.FsInformationClass;

		ndfsWinxpRequestData = (_U8 *)( ndfsWinxpRequestHeader+1 );
		RtlCopyMemory( ndfsWinxpRequestData, inputBuffer, setVolume.Length );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
	
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;


try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}

NTSTATUS
NdasNtfsSecondaryCommonQuerySecurityInfo (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP Irp
	)
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct QuerySecurity		querySecurity;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PVOID						outputBuffer = Irp->UserBuffer;
	ULONG						outputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		querySecurity.Length				= irpSp->Parameters.QuerySecurity.Length;
		querySecurity.SecurityInformation	= irpSp->Parameters.QuerySecurity.SecurityInformation;

		outputBufferLength = querySecurity.Length;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_QUERY_SECURITY,
														  outputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
										NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_QUERY_SECURITY, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->QuerySecurity.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QuerySecurity.SecurityInformation	= querySecurity.SecurityInformation;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		if(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) {

			ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW );
			ASSERT( Irp->IoStatus.Information );
			ASSERT( Irp->IoStatus.Information <= outputBufferLength );
			ASSERT( outputBuffer );

			RtlCopyMemory( outputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				Irp->IoStatus.Information );
		}

try_exit:  NOTHING;
	} finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}



NTSTATUS
NdasNtfsSecondaryCommonSetSecurityInfo (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP Irp
	)
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct SetSecurity			setSecurity;
	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;

	ULONG						securityLength = 0;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );


	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		setSecurity.SecurityDescriptor  = irpSp->Parameters.SetSecurity.SecurityDescriptor;
		setSecurity.SecurityInformation = irpSp->Parameters.SetSecurity.SecurityInformation;

		status = SeQuerySecurityDescriptorInfo( &setSecurity.SecurityInformation,
												NULL,
												&securityLength,
												&setSecurity.SecurityDescriptor );

		DebugTrace( 0, Dbg, ("NdasNtfsSecondaryCommonSetSecurityInfo: The length of the security desc:%lu\n",securityLength) );

		if( (!securityLength && status == STATUS_BUFFER_TOO_SMALL ) ||
			(securityLength &&  status != STATUS_BUFFER_TOO_SMALL ))
		{
			ASSERT(NDASNTFS_UNEXPECTED);

			NtfsRaiseStatus( IrpContext, status, NULL, NULL );
		}


		inputBufferLength = securityLength;


		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_SECURITY,
														  inputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
										NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_SECURITY, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->SetSecurity.Length					= inputBufferLength;
		ndfsWinxpRequestHeader->SetSecurity.SecurityInformation		= setSecurity.SecurityInformation;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		status = SeQuerySecurityDescriptorInfo( &setSecurity.SecurityInformation,
												(PSECURITY_DESCRIPTOR)ndfsWinxpRequestData,
												&securityLength,
												&setSecurity.SecurityDescriptor );

		if(status != STATUS_SUCCESS) {

			ASSERT(NDASNTFS_UNEXPECTED);
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

			try_return( status );
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;


try_exit:  NOTHING;
	} finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}

NTSTATUS
NdasNtfsSecondaryCommonQueryEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct QueryEa				queryEa;
	PVOID						inputBuffer;
	ULONG						inputBufferLength;
	PVOID						outputBuffer = NtfsMapUserBuffer (Irp );
	ULONG						outputBufferLength;

	ULONG						bufferLength;
	ULONG						returnedDataSize;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );


	if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

		return NtfsPostRequest( IrpContext, Irp );
	}


	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		queryEa.EaIndex			= irpSp->Parameters.QueryEa.EaIndex;
		queryEa.EaList			= irpSp->Parameters.QueryEa.EaList;
		queryEa.EaListLength	= irpSp->Parameters.QueryEa.EaListLength;
		queryEa.Length			= irpSp->Parameters.QueryEa.Length;

		inputBuffer				= queryEa.EaList;
		outputBufferLength		= queryEa.Length;

		if(inputBuffer != NULL) {

			PFILE_GET_EA_INFORMATION	fileGetEa = (PFILE_GET_EA_INFORMATION)inputBuffer;

			inputBufferLength = 0;
		
			while(fileGetEa->NextEntryOffset) {

				inputBufferLength += fileGetEa->NextEntryOffset;
				fileGetEa = (PFILE_GET_EA_INFORMATION)((_U8 *)fileGetEa + fileGetEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_GET_EA_INFORMATION) - sizeof(CHAR) + fileGetEa->EaNameLength);
		}
		else
			inputBufferLength = 0;

		DebugTrace( 0, Dbg,
			("NdasNtfsSecondaryCommonQueryEa: BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED) = %d queryEa.EaIndex = %d queryEa.EaList = %p queryEa.Length = %d, inputBufferLength = %d\n",
			 BooleanFlagOn(irpSp->Flags, SL_INDEX_SPECIFIED), queryEa.EaIndex, queryEa.EaList, queryEa.EaListLength, inputBufferLength) );

		bufferLength = (inputBufferLength >= outputBufferLength) ? inputBufferLength : outputBufferLength;

		ASSERT( bufferLength <= volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize );

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_QUERY_EA,
														  bufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_QUERY_EA, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->QueryEa.Length			= queryEa.Length;
		ndfsWinxpRequestHeader->QueryEa.EaIndex			= queryEa.EaIndex;
		ndfsWinxpRequestHeader->QueryEa.EaListLength	= queryEa.EaListLength;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory( ndfsWinxpRequestData, inputBuffer, inputBufferLength );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize) {

			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)(ndfsWinxpReplytHeader+1);

			while(fileFullEa->NextEntryOffset) {

			DebugTrace( 0, Dbg, ("getEa scb->FullPathName = %Z, fileFullea->EaName = %ws\n", &ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((_U8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			DebugTrace( 0, Dbg, ("getEa scb->FullPathName = %Z, fileFullea->EaName = %ws\n", &ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );

			ASSERT( returnedDataSize <= ADD_ALIGN8(queryEa.Length) );
			ASSERT( outputBuffer );

			RtlCopyMemory( outputBuffer,
						   (_U8 *)(ndfsWinxpReplytHeader+1),
						   (returnedDataSize < queryEa.Length) ? returnedDataSize : queryEa.Length );
		}

try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}



NTSTATUS
NdasNtfsSecondaryCommonSetEa (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct SetEa				setEa;
	PVOID						inputBuffer = NtfsMapUserBuffer( Irp );
	ULONG						inputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		setEa.Length =	irpSp->Parameters.SetEa.Length;

		if(inputBuffer != NULL) {

			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)inputBuffer;

			inputBufferLength = 0;

			while(fileFullEa->NextEntryOffset) {

				DebugTrace( 0, Dbg, ("getEa scb->FullPathName = %Z, fileFullea->EaName = %ws\n", &ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((_U8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			DebugTrace( 0, Dbg, ("getEa scb->FullPathName = %Z, fileFullea->EaName = %ws\n", &ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );
		
			inputBufferLength += (sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength);
		}
		else
			inputBufferLength = 0;

		DebugTrace( 0, Dbg,
					("NdasNtfsSecondaryCommonSetEa: Ea is set setEa->Length = %d, inputBufferLength = %d\n",
					 setEa.Length, inputBufferLength));

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_EA,
														  inputBufferLength );

		if(secondaryRequest == NULL) {

			status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			try_return( status );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_EA, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->SetEa.Length = setEa.Length;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory( ndfsWinxpRequestData, inputBuffer, inputBufferLength );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;


try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}


NTSTATUS
NdasNtfsSecondaryCommonQueryInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
{
	NTSTATUS					status;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PSCB						scb;
	PCCB						ccb;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct QueryFile			queryFile;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PVOID						outputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG						outputBufferLength;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );


	if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

		status = NtfsPostRequest( IrpContext, Irp );

		return status;
	}

	if(volDo->Secondary == NULL) {

		status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		return status;
	}

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		typeOfOpen = NtfsDecodeFileObject( IrpContext, fileObject, &vcb, &fcb, &scb, &ccb, TRUE );
		ASSERT( scb );

		if(FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

			try_return( status = STATUS_FILE_CORRUPT_ERROR );
		}

		queryFile.FileInformationClass	= irpSp->Parameters.QueryFile.FileInformationClass;
		queryFile.Length				= irpSp->Parameters.QueryFile.Length;
		outputBufferLength				= queryFile.Length;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_QUERY_INFORMATION,
														  outputBufferLength );

		if(secondaryRequest == NULL) {

			try_return( status = STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
									    NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_QUERY_INFORMATION, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, ccb->PrimaryFileHandle );

		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if(status != STATUS_SUCCESS) {

			secondaryRequest = NULL;
			try_return( status = STATUS_IO_DEVICE_ERROR );
		}

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );

			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;
		
		if(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) {

			ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW );
			ASSERT( Irp->IoStatus.Information );
			ASSERT( Irp->IoStatus.Information <= outputBufferLength );
			ASSERT( outputBuffer );
			
			RtlCopyMemory( outputBuffer,
						   (_U8 *)(ndfsWinxpReplytHeader+1),
						   Irp->IoStatus.Information );
		}

try_exit:  NOTHING;
    } finally {

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}

	return status;
}


#endif