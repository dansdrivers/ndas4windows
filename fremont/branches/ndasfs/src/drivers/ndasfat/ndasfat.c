#include "FatProcs.h"

#include <stdio.h>

#if __NDAS_FAT__

#define Dbg		(DEBUG_TRACE_ALL)
#define Dbg2    (DEBUG_INFO_ALL)


ULONG gOsMajorVersion = 0;
ULONG gOsMinorVersion = 0;


NDFAT_SET_FILTER_TOKEN NdasFatSeFilterToken = NULL;
NDFAT_KE_ARE_APCS_DISABLED NdasFatKeAreApcsDisabled = NULL;
NDFAT_KE_ARE_ALL_APCS_DISABLED NdasFatKeAreAllApcsDisabled = NULL;
NDFAT_CC_MDL_WRITE_ABORT NdasFatCcMdlWriteAbort = NULL;
NDFAT_FSRTL_REGISTER_FILESYSTEM_FILTER_CALLBACKS NdasFatFsRtlRegisterFileSystemFilterCallbacks = NULL;
NDFAT_FSRTL_ARE_VOLUME_START_APPLICATIONS_COMPLETE NdasFatFsRtlAreVolumeStartupApplicationsComplete = NULL;
NDFAT_MM_DOES_FILE_HAVE_USER_WRITABLE_REFERENCES NdasFatMmDoesFileHaveUserWritableReferences = NULL;

VOID
VolDo_Reference (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	) 
{
    LONG result;


    result = InterlockedIncrement( &VolDo->ReferenceCount );
    ASSERT( result >= 0 );
	//DbgPrint("voldo->Reference = %d\n", VolDo->ReferenceCount);
}


VOID
VolDo_Dereference (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	) 
{
    LONG result;

    result = InterlockedDecrement( &VolDo->ReferenceCount );
    ASSERT( result >= 0 );

	//DbgPrint("voldo->Reference = %d\n", VolDo->ReferenceCount);

    if (result == 0) {

#if __NDAS_FAT_SECONDARY__

		ExDeleteResourceLite( &VolDo->RecoveryResource );
		ExDeleteResourceLite( &VolDo->Resource );
		ExDeleteResourceLite( &VolDo->CreateResource );
		ExDeleteResourceLite( &VolDo->SessionResource );

#endif

		//DebugTrace2( 0, SPYDEBUG_DISPLAY_ATTACHMENT_NAMES,
		//		("******** FilterDriver_Dereference: System Freed *******\n") );

		KeSetEvent( &VolDo->ReferenceZeroEvent, IO_DISK_INCREMENT, FALSE );		
	}
}

#if __NDAS_FAT_PRIMARY__

VOID
NdasFatPrimarySessionStopping (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	DebugTrace2( 0, Dbg2, ("NdasFatPrimarySessionStopping: VolDo = %p\n", VolDo) );

	KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = VolDo->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &VolDo->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_Stopping( primarySession );

		KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );

	return;
}


VOID
NdasFatPrimarySessionCancelStopping (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	DebugTrace2( 0, Dbg2, ("NdasFatPrimarySessionCancelStopping: VolDo = %p\n", VolDo) );

	KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = VolDo->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &VolDo->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_CancelStopping( primarySession );

		KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );

	return;
}


VOID
NdasFatPrimarySessionDisconnect (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	DebugTrace2( 0, Dbg2, ("NdasFatPrimarySessionDisconnect: VolDo = %p\n", VolDo) );

	KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = VolDo->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &VolDo->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_Disconnect( primarySession );

		KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );

	return;
}


VOID
NdasFatPrimarySessionSurpriseRemoval (
	IN PVOLUME_DEVICE_OBJECT	VolDo
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	DebugTrace2( 0, Dbg2, ("NdasFatPrimarySessionSurpriseRemoval: VolDo = %p\n", VolDo) );

	KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = VolDo->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &VolDo->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_Stopping( primarySession );

		KeAcquireSpinLock( &VolDo->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &VolDo->PrimarySessionQSpinLock, oldIrql );

	return;
}


#endif

VOID
VolDoThreadProc (
	IN	PVOLUME_DEVICE_OBJECT	VolDo
	)
{
	BOOLEAN		volDoThreadTerminate = FALSE;


	DebugTrace2( 0, Dbg2, ("VolDoThreadProc: Start VolDo = %p\n", VolDo) );
	
	VolDo_Reference( VolDo );
	
	VolDo->Thread.Flags = VOLDO_THREAD_FLAG_INITIALIZING;

	ExAcquireFastMutex( &VolDo->FastMutex );		
	ClearFlag( VolDo->Thread.Flags, VOLDO_THREAD_FLAG_INITIALIZING );
	SetFlag( VolDo->Thread.Flags, VOLDO_THREAD_FLAG_START );
	ExReleaseFastMutex( &VolDo->FastMutex );
			
	KeSetEvent( &VolDo->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	volDoThreadTerminate = FALSE;
	
	while (volDoThreadTerminate == FALSE) {

		PKEVENT			events[2];
		LONG			eventCount;
		NTSTATUS		eventStatus;
		LARGE_INTEGER	timeOut;
		

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;
		events[eventCount++] = &VolDo->RequestEvent;

		timeOut.QuadPart = -NDFAT_VOLDO_THREAD_FLAG_TIME_OUT;

		eventStatus = KeWaitForMultipleObjects(	eventCount,
												events,
												WaitAny,
												Executive,
												KernelMode,
												TRUE,
												&timeOut,
												NULL );


		if (eventStatus == STATUS_TIMEOUT) {

			LARGE_INTEGER	currentTime;

			KeQuerySystemTime( &currentTime );

			if (FlagOn(VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_SHUTDOWN) || 
				!(FlagOn(VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_MOUNTED) /*&& !FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP)*/)) {
				
				continue;
			}

			if ((VolDo->NetdiskEnableMode == NETDISK_READ_ONLY && 
				 (VolDo->TryFlushOrPurgeTime.QuadPart > currentTime.QuadPart || 
				 (currentTime.QuadPart - VolDo->TryFlushOrPurgeTime.QuadPart) >= NDFAT_TRY_PURGE_DURATION)) ||

			    (VolDo->ReceiveWriteCommand == TRUE && 
				 (VolDo->TryFlushOrPurgeTime.QuadPart > currentTime.QuadPart || 
				 (currentTime.QuadPart - VolDo->TryFlushOrPurgeTime.QuadPart) >= NDFAT_TRY_FLUSH_DURATION))) {

				if (VolDo->NetdiskEnableMode != NETDISK_READ_ONLY && 
					(currentTime.QuadPart - VolDo->CommandReceiveTime.QuadPart) <=  NDFAT_TRY_FLUSH_DURATION /*&& 
					(currentTime.QuadPart - VolDo->TryFlushOrPurgeTime.QuadPart) <= (100*NDFAT_TRY_FLUSH_OR_PURGE_DURATION)*/) {

					continue;
				}

				do {
				
					HANDLE					eventHandle = NULL;

					HANDLE					fileHandle = NULL;
					ACCESS_MASK				desiredAccess;
					ULONG					attributes;
					OBJECT_ATTRIBUTES		objectAttributes;
					IO_STATUS_BLOCK			ioStatusBlock;
					LARGE_INTEGER			allocationSize;
					ULONG					fileAttributes;
					ULONG					shareAccess;
				    ULONG					createDisposition;
					ULONG					createOptions;
				    PVOID					eaBuffer;
					ULONG					eaLength;

					NTSTATUS				createStatus;
					NTSTATUS				fileSystemControlStatus;

					PIRP					topLevelIrp;
					PRIMARY_REQUEST_INFO	primaryRequestInfo;


					ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

					if (VolDo->NetdiskEnableMode == NETDISK_SECONDARY)
						break;

					DebugTrace2( 0, Dbg2, ("VolDoThreadProc: VolDo = %p, VolDo->NetdiskPartitionInformation.VolumeName = %wZ\n", 
											VolDo, &VolDo->NetdiskPartitionInformation.VolumeName) );

					desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
									| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

					ASSERT( desiredAccess == 0x0012019F );

					attributes  = OBJ_KERNEL_HANDLE;
					attributes |= OBJ_CASE_INSENSITIVE;

					InitializeObjectAttributes( &objectAttributes,
											    &VolDo->NetdiskPartitionInformation.VolumeName,
												attributes,
												NULL,
												NULL );
		
					allocationSize.LowPart  = 0;
					allocationSize.HighPart = 0;

					fileAttributes	  = 0;		
					shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
					createDisposition = FILE_OPEN;
					createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
					eaBuffer		  = NULL;
					eaLength		  = 0;

					RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

					createStatus = ZwCreateFile( &fileHandle,
												 desiredAccess,
												 &objectAttributes,
												 &ioStatusBlock,
												 &allocationSize,
												 fileAttributes,
												 shareAccess,
												 createDisposition,
												 createOptions,
												 eaBuffer,
												 eaLength );

					if (createStatus != STATUS_SUCCESS) 
						break;
						
					ASSERT( ioStatusBlock.Information == FILE_OPENED);
	
					createStatus = ZwCreateEvent( &eventHandle,
												  GENERIC_READ,
												  NULL,
												  SynchronizationEvent,
												  FALSE );

					if (createStatus != STATUS_SUCCESS) {

						ASSERT( NDASFAT_UNEXPECTED );
						ZwClose( fileHandle );
						break;
					}


					primaryRequestInfo.PrimaryTag				= 0xe2027482;
					primaryRequestInfo.PrimarySession			= NULL;
					primaryRequestInfo.NdfsWinxpRequestHeader	= NULL;

					topLevelIrp = IoGetTopLevelIrp();
					ASSERT( topLevelIrp == NULL );
					IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

					RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

					fileSystemControlStatus = ZwFsControlFile( fileHandle,
															   NULL, //&eventHandle,
															   NULL,
															   NULL,
															   &ioStatusBlock,
															   FSCTL_NDAS_FS_FLUSH_OR_PURGE,
															   NULL,
															   0,
															   NULL,
															   0 );

					DebugTrace2( 0, Dbg, ("VolDoThreadProc: VolDo = %p, createStatus = %x, ioStatusBlock = %x\n", 
											VolDo, fileSystemControlStatus, ioStatusBlock.Information) );

					if (fileSystemControlStatus == STATUS_PENDING) {
			
						LARGE_INTEGER			timeOut;
			
						timeOut.QuadPart = -3*NANO100_PER_SEC;

						ASSERT( FALSE );
						//fileSystemControlStatus = ZwWaitForSingleObject( eventHandle, TRUE,NULL /*, &timeOut*/ );
					}

					IoSetTopLevelIrp( topLevelIrp );

					if (fileSystemControlStatus != STATUS_SUCCESS)
						DebugTrace2( 0, Dbg2, ("VolDoThreadProc: VolDo = %p, fileSystemControlStatus = %x, ioStatusBlock = %x\n", 
												VolDo, fileSystemControlStatus, ioStatusBlock.Information) );

					ZwClose( eventHandle );
					ZwClose( fileHandle );
					
					break;
				
				} while (0);

				KeQuerySystemTime( &VolDo->TryFlushOrPurgeTime );
				VolDo->ReceiveWriteCommand = FALSE;
			}

			continue;
		}
		
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( eventCount < THREAD_WAIT_OBJECTS );
		
		if (!NT_SUCCESS( eventStatus ) || eventStatus >= eventCount) {

			ASSERT( NDASFAT_UNEXPECTED );
			SetFlag( VolDo->Thread.Flags, VOLDO_THREAD_FLAG_ERROR );
			volDoThreadTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			volDoThreadTerminate = TRUE;
			break;
		
		} else
			ASSERT( NDASFAT_BUG );
	}

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	ExAcquireFastMutex( &VolDo->FastMutex );

	SetFlag( VolDo->Thread.Flags, VOLDO_THREAD_FLAG_STOPED );

	ExReleaseFastMutex( &VolDo->FastMutex );

	DebugTrace2( 0, Dbg2, ("VolDoThreadProc: PsTerminateSystemThread VolDo = %p\n", VolDo) );
	
	ExAcquireFastMutex( &VolDo->FastMutex );
	SetFlag( VolDo->Thread.Flags, VOLDO_THREAD_FLAG_TERMINATED );
	ExReleaseFastMutex( &VolDo->FastMutex );
	
	VolDo_Dereference( VolDo );

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
}


#if (defined(__NDAS_FAT_PRIMARY__) || defined(__NDAS_FAT_VOLDO__))

NTSTATUS
RecvIt (
	IN PFILE_OBJECT					ConnectionFileObject,
	OUT PCHAR						RecvBuff, 
	IN  ULONG						RecvLen,
	OUT PULONG						TotalReceived,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
	IN  ULONG						RequestIdx,
	IN  PLARGE_INTEGER				TimeOut
	)
{
	INT			len = RecvLen;
	NTSTATUS	status;
	LONG		result;
	
	if (TotalReceived) {

		*TotalReceived = 0;
	}

	if (OverlappedData) {

		//	Asynchronous

		if (TotalReceived) {

			return STATUS_INVALID_PARAMETER;
		}

		status = LpxTdiV2Recv( ConnectionFileObject,
							   RecvBuff,
							   RecvLen,
							   0,
							   NULL,
							   OverlappedData,
							   RequestIdx,
							   NULL );


		if (status != STATUS_PENDING) {

#if 0
			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( !NT_SUCCESS(status) );

			KDPrintM( DBG_PROTO_ERROR, 
					 ("Can't recv! %02x:%02x:%02x:%02x:%02x:%02x\n",
					  LSS->NdasNodeAddress.Address[2],
					  LSS->NdasNodeAddress.Address[3],
					  LSS->NdasNodeAddress.Address[4],
					  LSS->NdasNodeAddress.Address[5],
					  LSS->NdasNodeAddress.Address[6],
					  LSS->NdasNodeAddress.Address[7]) );
#endif
		}

	} else {

		len = RecvLen;

		while (len > 0) {
		
			result = 0;

			status = LpxTdiV2Recv( ConnectionFileObject,
								   RecvBuff,
								   len,
								   0,
								   TimeOut,
								   NULL,
								   0,
								   &result );

			if (status != STATUS_SUCCESS) {

#if 0
				PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);
			
				KDPrintM( DBG_PROTO_ERROR, 
						 ("Can't recv! %02x:%02x:%02x:%02x:%02x:%02x\n",
						  LSS->NdasNodeAddress.Address[2],
						  LSS->NdasNodeAddress.Address[3],
						  LSS->NdasNodeAddress.Address[4],
						  LSS->NdasNodeAddress.Address[5],
						  LSS->NdasNodeAddress.Address[6],
						  LSS->NdasNodeAddress.Address[7]) );
#endif

				break;
			}

			//KDPrintM( DBG_PROTO_NOISE, ("len %d, result %d \n", len, result) );

			//	LstransReceive() must guarantee more than one byte is received
			//	when return SUCCESS

			len -= result;
			RecvBuff += result;
		}

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( len == 0 );
			
			if (TotalReceived) {
			
				*TotalReceived = RecvLen;
			}
		
		} else {

			if (TotalReceived) {

				*TotalReceived = RecvLen - len;
			}
		}
	}

	return status;
}

NTSTATUS
RecvMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	RecvNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	)
{
	NTSTATUS		status;
	UINT32			remainDataLength;
	UINT32			chooseDataLength;
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

	chooseDataLength = NdasFcChooseSendSize( RecvNdasFcStatistics, BufferLength );

	startTime = NdasCurrentTime();

	remainDataLength = BufferLength;

	do {

		status = RecvIt( ConnectionFileObject,
						 Buffer + BufferLength - remainDataLength,
						 (remainDataLength < chooseDataLength) ? remainDataLength : chooseDataLength,
						 NULL,
						 NULL,
						 0,
						 TimeOut );

		if (status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg2, ("Error when Recv data\n") );

			return status;
		}

		if (remainDataLength > chooseDataLength) {

			remainDataLength -= chooseDataLength;
			
		} else {

			remainDataLength = 0;
		}

	} while (remainDataLength);

	endTime = NdasCurrentTime();

	NdasFcUpdateSendSize( RecvNdasFcStatistics, 
						  chooseDataLength, 
						  BufferLength,
						  startTime, 
						  endTime );

	return STATUS_SUCCESS;
}

NTSTATUS
SendIt (
	IN PFILE_OBJECT					ConnectionFileObject,
	IN  PCHAR						SendBuff, 
	IN  UINT32						SendLen,
	OUT PULONG						TotalSent,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
	IN  ULONG						RequestIdx,
	IN  PLARGE_INTEGER				TimeOut
	)
{
	NTSTATUS	status;
	UINT32		result;

	if (TotalSent) {

		*TotalSent = 0;
	}

	if (OverlappedData) {

		//	Asynchronous

		if (TotalSent) {

			NDAS_ASSERT( FALSE );
			return STATUS_INVALID_PARAMETER;
		}

		status = LpxTdiV2Send( ConnectionFileObject,
							   SendBuff,
							   SendLen,
							   0,
							   NULL,
							   OverlappedData,
							   RequestIdx,
							   NULL );

#if 0
		if (status != STATUS_PENDING) {

			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( !NT_SUCCESS(status) );
			
			KDPrintM( DBG_PROTO_ERROR, 
					  ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
					   LSS->NdasNodeAddress.Address[2],
					   LSS->NdasNodeAddress.Address[3],
					   LSS->NdasNodeAddress.Address[4],
					   LSS->NdasNodeAddress.Address[5],
					   LSS->NdasNodeAddress.Address[6],
					   LSS->NdasNodeAddress.Address[7]) );
		}
#endif

	} else {

		//	Synchronous

		result = 0;

		status = LpxTdiV2Send( ConnectionFileObject,
							   SendBuff,
							   SendLen,
							   0,
							   TimeOut,
							   NULL,
							   0,
							   &result );

		if (result == SendLen) {
		
			if (TotalSent) {

				*TotalSent = SendLen;
			}

		} else {

			//PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( result == 0 );

			if (TotalSent) {

				*TotalSent = 0;
			}

			status = STATUS_PORT_DISCONNECTED;
			
#if 0
			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, 
						  ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
							LSS->NdasNodeAddress.Address[2],
							LSS->NdasNodeAddress.Address[3],
							LSS->NdasNodeAddress.Address[4],
							LSS->NdasNodeAddress.Address[5],
							LSS->NdasNodeAddress.Address[6],
							LSS->NdasNodeAddress.Address[7]) );
#endif
		}
	}

	return status;
}

NTSTATUS
SendMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	SendNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	)
{
	NTSTATUS		status;
	UINT32			remainDataLength;
	UINT32			chooseDataLength;
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

	chooseDataLength = NdasFcChooseSendSize( SendNdasFcStatistics, BufferLength );

	startTime = NdasCurrentTime();

	remainDataLength = BufferLength;

	do {

		status = SendIt( ConnectionFileObject,
						 Buffer + BufferLength - remainDataLength,
						 (remainDataLength < chooseDataLength) ? remainDataLength : chooseDataLength,
						 NULL,
						 NULL,
						 0,
						 TimeOut );

		if (status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg2, ("Error when Send data\n") );

			return status;
		}

		if (remainDataLength > chooseDataLength) {

			remainDataLength -= chooseDataLength;
			
		} else {

			remainDataLength = 0;
		}

	} while (remainDataLength);

	endTime = NdasCurrentTime();

	NdasFcUpdateSendSize( SendNdasFcStatistics, 
						  chooseDataLength, 
						  BufferLength,
						  startTime, 
						  endTime );

	return STATUS_SUCCESS;
}

#if 0

#define MAX_TRANSMIT_DATA	(64*1024)

NTSTATUS
SendMessage (
	PFILE_OBJECT	ConnectionFileObject,
	UINT8				*Buf, 
	LONG			Size,
	PULONG			TotalSent
	)
{
	NTSTATUS	status;
	LONG		result;
	ULONG		remaining;
	ULONG		onceReqSz;


	ASSERT( Size > 0 );
	
	for (remaining = Size; remaining > 0; ) {

		onceReqSz = (remaining < MAX_TRANSMIT_DATA) ? remaining : MAX_TRANSMIT_DATA;

		status = LpxTdiV2Send( ConnectionFileObject,
							   Buf,
							   onceReqSz,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (status != STATUS_SUCCESS) {

		    DebugTrace2( 0, DEBUG_INFO_ALL, ("SendMessage: sending failed.\n") );
			break;
		}

		NDAS_ASSERT( result >= 0 );

		remaining -= result;
		((PCHAR)Buf) += result;
	}

	if (remaining) {

		DebugTrace2( 0, DEBUG_INFO_ALL, ("SendMessage: unexpected data length sent. remaining:%lu\n", remaining) );

		NDAS_ASSERT( !NT_SUCCESS(status) );
	}

	if (TotalSent != NULL)
		*TotalSent = Size-remaining;

	return status;
}


NTSTATUS
RecvMessage (
	PFILE_OBJECT	ConnectionFileObject,
	UINT8				*Buf, 
	LONG			Size,
	PULONG			Received
	)
{
	NTSTATUS	status;
	ULONG		result;
	ULONG		remaining;
	ULONG		onceReqSz;


	ASSERT( Size > 0 );

	//
	//	send a packet
	//
	
	for (remaining = Size; remaining > 0;  ) {

		onceReqSz = (remaining < MAX_TRANSMIT_DATA) ? remaining:MAX_TRANSMIT_DATA;

		status = LpxTdiV2Recv( ConnectionFileObject,
							   Buf,
							   onceReqSz,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (status != STATUS_SUCCESS) {

			DebugTrace2( 0, DEBUG_INFO_ALL, ("RecvMessage: Receiving failed.\n") );
			break;
		}

		NDAS_ASSERT( result > 0 );

		remaining -= result;
		((PCHAR)Buf) += result;
	}

	if (remaining) {

		DebugTrace2( 0, DEBUG_INFO_ALL, ("RecvMessage: unexpected data length sent. remaining:%lu\n", remaining) );
		
		NDAS_ASSERT( !NT_SUCCESS(status) );
	}

	if (Received != NULL)
		*Received = Size-remaining;

	return status;
}

#endif

#endif

#if __NDAS_FAT_DBG__

VOID
PrintIrp (
	ULONGLONG				DebugLevel,
	PCHAR					Where,
	PVOID					VolDo,
	PIRP					Irp
	)
{
#if DBG
	
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	UNICODE_STRING		nullName;
	UCHAR				minorFunction;
    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName (
		irpSp->MajorFunction,
		irpSp->MinorFunction,
		irpSp->Parameters.FileSystemControl.FsControlCode,
		irpMajorString,
		irpMinorString
		);

	RtlInitUnicodeString(&nullName, L"fileObject == NULL");
	
	if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) 
		minorFunction = (UCHAR)((irpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2);
	else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && irpSp->MinorFunction == 0)
		minorFunction = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);
	else
		minorFunction = irpSp->MinorFunction;

	ASSERT(Irp->RequestorMode == KernelMode || Irp->RequestorMode == UserMode);

	if ( KeGetCurrentIrql() < DISPATCH_LEVEL) {
	
		DebugTrace2( 0, DebugLevel,
			("%s %p Irql:%d Irp:%p %s %s (%u:%u) %08x %02x ",
				(Where) ? Where : "", VolDo,
				KeGetCurrentIrql(), 
				Irp, irpMajorString, irpMinorString, irpSp->MajorFunction, minorFunction,
				Irp->Flags, irpSp->Flags));
				/*"%s %c%c%c%c%c ", */
				/*(Irp->RequestorMode == KernelMode) ? "KernelMode" : "UserMode",
		        (Irp->Flags & IRP_PAGING_IO) ? '*' : ' ',
				(Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
				(Irp->Flags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
				BooleanFlagOn(Irp->Flags,IRP_NOCACHE) ? 'N' : ' ',
				(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' ',*/
		DebugTrace2( 0, DebugLevel,
			("file: %p  %08x %p %wZ %d\n",
				fileObject, 
				fileObject ? fileObject->Flags : 0,
				fileObject ? fileObject->RelatedFileObject : NULL,
				fileObject ? &fileObject->FileName : &nullName,
				fileObject ? fileObject->FileName.Length : 0
				));
	}

#else
	
	UNREFERENCED_PARAMETER(DebugLevel);
	UNREFERENCED_PARAMETER(Where);
	UNREFERENCED_PARAMETER(VolDo);
	UNREFERENCED_PARAMETER(Irp);

#endif

	return;
}


CHAR UnknownIrpMinor[] = "Unknown Irp minor code (%u)";


VOID
GetIrpName (
    IN UCHAR MajorCode,
    IN UCHAR MinorCode,
    IN ULONG FsctlCode,
    OUT PCHAR MajorCodeName,
    OUT PCHAR MinorCodeName
    )
/*++

Routine Description:

    This routine translates the given Irp codes into printable strings which
    are returned.  This guarantees to routine valid strings in each buffer.
    The MinorCode string may be a NULL string (not a null pointer).

Arguments:

    MajorCode - the IRP Major code of the operation
    MinorCode - the IRP Minor code of the operation
    FsctlCode - if this is an IRP_MJ_FILE_SYSTEM_CONTROL/IRP_MN_USER_FS_REQUEST
                operation then this is the FSCTL code whose name is also
                translated.  This name is returned as part of the MinorCode
                string.
    MajorCodeName - a string buffer at least OPERATION_NAME_BUFFER_SIZE
                characters long that receives the major code name.
    MinorCodeName - a string buffer at least OPERATION_NAME_BUFFER_SIZE
                characters long that receives the minor/fsctl code name.

Return Value:

    None.

--*/
{
    PCHAR irpMajorString;
    PCHAR irpMinorString = "";
    CHAR nameBuf[OPERATION_NAME_BUFFER_SIZE];

    switch (MajorCode) {
        case IRP_MJ_CREATE:
            irpMajorString = "IRP_MJ_CREATE";
            break;
        case IRP_MJ_CREATE_NAMED_PIPE:
            irpMajorString = "IRP_MJ_CREATE_NAMED_PIPE";
            break;
        case IRP_MJ_CLOSE:
            irpMajorString = "IRP_MJ_CLOSE";
            break;
        case IRP_MJ_READ:
            irpMajorString = "IRP_MJ_READ";
            switch (MinorCode) {
                case IRP_MN_NORMAL:
                    irpMinorString = "IRP_MN_NORMAL";
                    break;
                case IRP_MN_DPC:
                    irpMinorString = "IRP_MN_DPC";
                    break;
                case IRP_MN_MDL:
                    irpMinorString = "IRP_MN_MDL";
                    break;
                case IRP_MN_COMPLETE:
                    irpMinorString = "IRP_MN_COMPLETE";
                    break;
                case IRP_MN_COMPRESSED:
                    irpMinorString = "IRP_MN_COMPRESSED";
                    break;
                case IRP_MN_MDL_DPC:
                    irpMinorString = "IRP_MN_MDL_DPC";
                    break;
                case IRP_MN_COMPLETE_MDL:
                    irpMinorString = "IRP_MN_COMPLETE_MDL";
                    break;
                case IRP_MN_COMPLETE_MDL_DPC:
                    irpMinorString = "IRP_MN_COMPLETE_MDL_DPC";
                    break;
                default:
                    sprintf( nameBuf, UnknownIrpMinor, MinorCode );
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_WRITE:
            irpMajorString = "IRP_MJ_WRITE";
            switch (MinorCode) {
                case IRP_MN_NORMAL:
                    irpMinorString = "IRP_MN_NORMAL";
                    break;
                case IRP_MN_DPC:
                    irpMinorString = "IRP_MN_DPC";
                    break;
                case IRP_MN_MDL:
                    irpMinorString = "IRP_MN_MDL";
                    break;
                case IRP_MN_COMPLETE:
                    irpMinorString = "IRP_MN_COMPLETE";
                    break;
                case IRP_MN_COMPRESSED:
                    irpMinorString = "IRP_MN_COMPRESSED";
                    break;
                case IRP_MN_MDL_DPC:
                    irpMinorString = "IRP_MN_MDL_DPC";
                    break;
                case IRP_MN_COMPLETE_MDL:
                    irpMinorString = "IRP_MN_COMPLETE_MDL";
                    break;
                case IRP_MN_COMPLETE_MDL_DPC:
                    irpMinorString = "IRP_MN_COMPLETE_MDL_DPC";
                    break;
                default:
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_QUERY_INFORMATION:
            irpMajorString = "IRP_MJ_QUERY_INFORMATION";
            break;
        case IRP_MJ_SET_INFORMATION:
            irpMajorString = "IRP_MJ_SET_INFORMATION";
            break;
        case IRP_MJ_QUERY_EA:
            irpMajorString = "IRP_MJ_QUERY_EA";
            break;
        case IRP_MJ_SET_EA:
            irpMajorString = "IRP_MJ_SET_EA";
            break;
        case IRP_MJ_FLUSH_BUFFERS:
            irpMajorString = "IRP_MJ_FLUSH_BUFFERS";
            break;
        case IRP_MJ_QUERY_VOLUME_INFORMATION:
            irpMajorString = "IRP_MJ_QUERY_VOLUME_INFORMATION";
            break;
        case IRP_MJ_SET_VOLUME_INFORMATION:
            irpMajorString = "IRP_MJ_SET_VOLUME_INFORMATION";
            break;
        case IRP_MJ_DIRECTORY_CONTROL:
            irpMajorString = "IRP_MJ_DIRECTORY_CONTROL";
            switch (MinorCode) {
                case IRP_MN_QUERY_DIRECTORY:
                    irpMinorString = "IRP_MN_QUERY_DIRECTORY";
                    break;
                case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
                    irpMinorString = "IRP_MN_NOTIFY_CHANGE_DIRECTORY";
                    break;
                default:
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_FILE_SYSTEM_CONTROL:
            irpMajorString = "IRP_MJ_FILE_SYSTEM_CONTROL";
            switch (MinorCode) {
                case IRP_MN_USER_FS_REQUEST:
                    switch (FsctlCode) {
                        case FSCTL_REQUEST_OPLOCK_LEVEL_1:
                            irpMinorString = "FSCTL_REQUEST_OPLOCK_LEVEL_1";
                            break;
                        case FSCTL_REQUEST_OPLOCK_LEVEL_2:
                            irpMinorString = "FSCTL_REQUEST_OPLOCK_LEVEL_2";
                            break;
                        case FSCTL_REQUEST_BATCH_OPLOCK:
                            irpMinorString = "FSCTL_REQUEST_BATCH_OPLOCK";
                            break;
                        case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
                            irpMinorString = "FSCTL_OPLOCK_BREAK_ACKNOWLEDGE";
                            break;
                        case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
                            irpMinorString = "FSCTL_OPBATCH_ACK_CLOSE_PENDING";
                            break;
                        case FSCTL_OPLOCK_BREAK_NOTIFY:
                            irpMinorString = "FSCTL_OPLOCK_BREAK_NOTIFY";
                            break;
                        case FSCTL_LOCK_VOLUME:
                            irpMinorString = "FSCTL_LOCK_VOLUME";
                            break;
                        case FSCTL_UNLOCK_VOLUME:
                            irpMinorString = "FSCTL_UNLOCK_VOLUME";
                            break;
                        case FSCTL_DISMOUNT_VOLUME:
                            irpMinorString = "FSCTL_DISMOUNT_VOLUME";
                            break;
                        case FSCTL_IS_VOLUME_MOUNTED:
                            irpMinorString = "FSCTL_IS_VOLUME_MOUNTED";
                            break;
                        case FSCTL_IS_PATHNAME_VALID:
                            irpMinorString = "FSCTL_IS_PATHNAME_VALID";
                            break;
                        case FSCTL_MARK_VOLUME_DIRTY:
                            irpMinorString = "FSCTL_MARK_VOLUME_DIRTY";
                            break;
                        case FSCTL_QUERY_RETRIEVAL_POINTERS:
                            irpMinorString = "FSCTL_QUERY_RETRIEVAL_POINTERS";
                            break;
                        case FSCTL_GET_COMPRESSION:
                            irpMinorString = "FSCTL_GET_COMPRESSION";
                            break;
                        case FSCTL_SET_COMPRESSION:
                            irpMinorString = "FSCTL_SET_COMPRESSION";
                            break;
                        case FSCTL_MARK_AS_SYSTEM_HIVE:
                            irpMinorString = "FSCTL_MARK_AS_SYSTEM_HIVE";
                            break;
                        case FSCTL_OPLOCK_BREAK_ACK_NO_2:
                            irpMinorString = "FSCTL_OPLOCK_BREAK_ACK_NO_2";
                            break;
                        case FSCTL_INVALIDATE_VOLUMES:
                            irpMinorString = "FSCTL_INVALIDATE_VOLUMES";
                            break;
                        case FSCTL_QUERY_FAT_BPB:
                            irpMinorString = "FSCTL_QUERY_FAT_BPB";
                            break;
                        case FSCTL_REQUEST_FILTER_OPLOCK:
                            irpMinorString = "FSCTL_REQUEST_FILTER_OPLOCK";
                            break;
                        case FSCTL_FILESYSTEM_GET_STATISTICS:
                            irpMinorString = "FSCTL_FILESYSTEM_GET_STATISTICS";
                            break;
                        case FSCTL_GET_NTFS_VOLUME_DATA:
                            irpMinorString = "FSCTL_GET_NTFS_VOLUME_DATA";
                            break;
                        case FSCTL_GET_NTFS_FILE_RECORD:
                            irpMinorString = "FSCTL_GET_NTFS_FILE_RECORD";
                            break;
                        case FSCTL_GET_VOLUME_BITMAP:
                            irpMinorString = "FSCTL_GET_VOLUME_BITMAP";
                            break;
                        case FSCTL_GET_RETRIEVAL_POINTERS:
                            irpMinorString = "FSCTL_GET_RETRIEVAL_POINTERS";
                            break;
                        case FSCTL_MOVE_FILE:
                            irpMinorString = "FSCTL_MOVE_FILE";
                            break;
                        case FSCTL_IS_VOLUME_DIRTY:
                            irpMinorString = "FSCTL_IS_VOLUME_DIRTY";
                            break;
                        case FSCTL_ALLOW_EXTENDED_DASD_IO:
                            irpMinorString = "FSCTL_ALLOW_EXTENDED_DASD_IO";
                            break;
                        case FSCTL_FIND_FILES_BY_SID:
                            irpMinorString = "FSCTL_FIND_FILES_BY_SID";
                            break;
                        case FSCTL_SET_OBJECT_ID:
                            irpMinorString = "FSCTL_SET_OBJECT_ID";
                            break;
                        case FSCTL_GET_OBJECT_ID:
                            irpMinorString = "FSCTL_GET_OBJECT_ID";
                            break;
                        case FSCTL_DELETE_OBJECT_ID:
                            irpMinorString = "FSCTL_DELETE_OBJECT_ID";
                            break;
                        case FSCTL_SET_REPARSE_POINT:
                            irpMinorString = "FSCTL_SET_REPARSE_POINT";
                            break;
                        case FSCTL_GET_REPARSE_POINT:
                            irpMinorString = "FSCTL_GET_REPARSE_POINT";
                            break;
                        case FSCTL_DELETE_REPARSE_POINT:
                            irpMinorString = "FSCTL_DELETE_REPARSE_POINT";
                            break;
                        case FSCTL_ENUM_USN_DATA:
                            irpMinorString = "FSCTL_ENUM_USN_DATA";
                            break;
                        case FSCTL_SECURITY_ID_CHECK:
                            irpMinorString = "FSCTL_SECURITY_ID_CHECK";
                            break;
                        case FSCTL_READ_USN_JOURNAL:
                            irpMinorString = "FSCTL_READ_USN_JOURNAL";
                            break;
                        case FSCTL_SET_OBJECT_ID_EXTENDED:
                            irpMinorString = "FSCTL_SET_OBJECT_ID_EXTENDED";
                            break;
                        case FSCTL_CREATE_OR_GET_OBJECT_ID:
                            irpMinorString = "FSCTL_CREATE_OR_GET_OBJECT_ID";
                            break;
                        case FSCTL_SET_SPARSE:
                            irpMinorString = "FSCTL_SET_SPARSE";
                            break;
                        case FSCTL_SET_ZERO_DATA:
                            irpMinorString = "FSCTL_SET_ZERO_DATA";
                            break;
                        case FSCTL_QUERY_ALLOCATED_RANGES:
                            irpMinorString = "FSCTL_QUERY_ALLOCATED_RANGES";
                            break;
                        case FSCTL_SET_ENCRYPTION:
                            irpMinorString = "FSCTL_SET_ENCRYPTION";
                            break;
                        case FSCTL_ENCRYPTION_FSCTL_IO:
                            irpMinorString = "FSCTL_ENCRYPTION_FSCTL_IO";
                            break;
                        case FSCTL_WRITE_RAW_ENCRYPTED:
                            irpMinorString = "FSCTL_WRITE_RAW_ENCRYPTED";
                            break;
                        case FSCTL_READ_RAW_ENCRYPTED:
                            irpMinorString = "FSCTL_READ_RAW_ENCRYPTED";
                            break;
                        case FSCTL_CREATE_USN_JOURNAL:
                            irpMinorString = "FSCTL_CREATE_USN_JOURNAL";
                            break;
                        case FSCTL_READ_FILE_USN_DATA:
                            irpMinorString = "FSCTL_READ_FILE_USN_DATA";
                            break;
                        case FSCTL_WRITE_USN_CLOSE_RECORD:
                            irpMinorString = "FSCTL_WRITE_USN_CLOSE_RECORD";
                            break;
                        case FSCTL_EXTEND_VOLUME:
                            irpMinorString = "FSCTL_EXTEND_VOLUME";
                            break;
                        case FSCTL_QUERY_USN_JOURNAL:
                            irpMinorString = "FSCTL_QUERY_USN_JOURNAL";
                            break;
                        case FSCTL_DELETE_USN_JOURNAL:
                            irpMinorString = "FSCTL_DELETE_USN_JOURNAL";
                            break;
                        case FSCTL_MARK_HANDLE:
                            irpMinorString = "FSCTL_MARK_HANDLE";
                            break;
                        case FSCTL_SIS_COPYFILE:
                            irpMinorString = "FSCTL_SIS_COPYFILE";
                            break;
                        case FSCTL_SIS_LINK_FILES:
                            irpMinorString = "FSCTL_SIS_LINK_FILES";
                            break;
                        case FSCTL_HSM_MSG:
                            irpMinorString = "FSCTL_HSM_MSG";
                            break;
                        case FSCTL_HSM_DATA:
                            irpMinorString = "FSCTL_HSM_DATA";
                            break;
                        case FSCTL_RECALL_FILE:
                            irpMinorString = "FSCTL_RECALL_FILE";
                            break;
#if WINVER >= 0x0501                            
                        case FSCTL_READ_FROM_PLEX:
                            irpMinorString = "FSCTL_READ_FROM_PLEX";
                            break;
                        case FSCTL_FILE_PREFETCH:
                            irpMinorString = "FSCTL_FILE_PREFETCH";
                            break;
#endif                            
                        default:
                            sprintf(nameBuf,"Unknown FSCTL (%u)",MinorCode);
                            irpMinorString = nameBuf;
                            break;
                    }

                    sprintf(nameBuf,"%s (USER)",irpMinorString);
                    irpMinorString = nameBuf;
                    break;

                case IRP_MN_MOUNT_VOLUME:
                    irpMinorString = "IRP_MN_MOUNT_VOLUME";
                    break;
                case IRP_MN_VERIFY_VOLUME:
                    irpMinorString = "IRP_MN_VERIFY_VOLUME";
                    break;
                case IRP_MN_LOAD_FILE_SYSTEM:
                    irpMinorString = "IRP_MN_LOAD_FILE_SYSTEM";
                    break;
                case IRP_MN_TRACK_LINK:
                    irpMinorString = "IRP_MN_TRACK_LINK";
                    break;
                default:
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_DEVICE_CONTROL:
            irpMajorString = "IRP_MJ_DEVICE_CONTROL";
            switch (MinorCode) {
                case 0:
                    irpMinorString = "User request";
                    break;
                case IRP_MN_SCSI_CLASS:
                    irpMinorString = "IRP_MN_SCSI_CLASS";
                    break;
                default:
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_INTERNAL_DEVICE_CONTROL:
            irpMajorString = "IRP_MJ_INTERNAL_DEVICE_CONTROL";
            break;
        case IRP_MJ_SHUTDOWN:
            irpMajorString = "IRP_MJ_SHUTDOWN";
            break;
        case IRP_MJ_LOCK_CONTROL:
            irpMajorString = "IRP_MJ_LOCK_CONTROL";
            switch (MinorCode) {
                case IRP_MN_LOCK:
                    irpMinorString = "IRP_MN_LOCK";
                    break;
                case IRP_MN_UNLOCK_SINGLE:
                    irpMinorString = "IRP_MN_UNLOCK_SINGLE";
                    break;
                case IRP_MN_UNLOCK_ALL:
                    irpMinorString = "IRP_MN_UNLOCK_ALL";
                    break;
                case IRP_MN_UNLOCK_ALL_BY_KEY:
                    irpMinorString = "IRP_MN_UNLOCK_ALL_BY_KEY";
                    break;
                default:
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_CLEANUP:
            irpMajorString = "IRP_MJ_CLEANUP";
            break;
        case IRP_MJ_CREATE_MAILSLOT:
            irpMajorString = "IRP_MJ_CREATE_MAILSLOT";
            break;
        case IRP_MJ_QUERY_SECURITY:
            irpMajorString = "IRP_MJ_QUERY_SECURITY";
            break;
        case IRP_MJ_SET_SECURITY:
            irpMajorString = "IRP_MJ_SET_SECURITY";
            break;
        case IRP_MJ_POWER:
            irpMajorString = "IRP_MJ_POWER";
            switch (MinorCode) {
                case IRP_MN_WAIT_WAKE:
                    irpMinorString = "IRP_MN_WAIT_WAKE";
                    break;
                case IRP_MN_POWER_SEQUENCE:
                    irpMinorString = "IRP_MN_POWER_SEQUENCE";
                    break;
                case IRP_MN_SET_POWER:
                    irpMinorString = "IRP_MN_SET_POWER";
                    break;
                case IRP_MN_QUERY_POWER:
                    irpMinorString = "IRP_MN_QUERY_POWER";
                    break;
                default :
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_SYSTEM_CONTROL:
            irpMajorString = "IRP_MJ_SYSTEM_CONTROL";
            switch (MinorCode) {
                case IRP_MN_QUERY_ALL_DATA:
                    irpMinorString = "IRP_MN_QUERY_ALL_DATA";
                    break;
                case IRP_MN_QUERY_SINGLE_INSTANCE:
                    irpMinorString = "IRP_MN_QUERY_SINGLE_INSTANCE";
                    break;
                case IRP_MN_CHANGE_SINGLE_INSTANCE:
                    irpMinorString = "IRP_MN_CHANGE_SINGLE_INSTANCE";
                    break;
                case IRP_MN_CHANGE_SINGLE_ITEM:
                    irpMinorString = "IRP_MN_CHANGE_SINGLE_ITEM";
                    break;
                case IRP_MN_ENABLE_EVENTS:
                    irpMinorString = "IRP_MN_ENABLE_EVENTS";
                    break;
                case IRP_MN_DISABLE_EVENTS:
                    irpMinorString = "IRP_MN_DISABLE_EVENTS";
                    break;
                case IRP_MN_ENABLE_COLLECTION:
                    irpMinorString = "IRP_MN_ENABLE_COLLECTION";
                    break;
                case IRP_MN_DISABLE_COLLECTION:
                    irpMinorString = "IRP_MN_DISABLE_COLLECTION";
                    break;
                case IRP_MN_REGINFO:
                    irpMinorString = "IRP_MN_REGINFO";
                    break;
                case IRP_MN_EXECUTE_METHOD:
                    irpMinorString = "IRP_MN_EXECUTE_METHOD";
                    break;
                default :
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        case IRP_MJ_DEVICE_CHANGE:
            irpMajorString = "IRP_MJ_DEVICE_CHANGE";
            break;
        case IRP_MJ_QUERY_QUOTA:
            irpMajorString = "IRP_MJ_QUERY_QUOTA";
            break;
        case IRP_MJ_SET_QUOTA:
            irpMajorString = "IRP_MJ_SET_QUOTA";
            break;
        case IRP_MJ_PNP:
            irpMajorString = "IRP_MJ_PNP";
            switch (MinorCode) {
                case IRP_MN_START_DEVICE:
                    irpMinorString = "IRP_MN_START_DEVICE";
                    break;
                case IRP_MN_QUERY_REMOVE_DEVICE:
                    irpMinorString = "IRP_MN_QUERY_REMOVE_DEVICE";
                    break;
                case IRP_MN_REMOVE_DEVICE:
                    irpMinorString = "IRP_MN_REMOVE_DEVICE";
                    break;
                case IRP_MN_CANCEL_REMOVE_DEVICE:
                    irpMinorString = "IRP_MN_CANCEL_REMOVE_DEVICE";
                    break;
                case IRP_MN_STOP_DEVICE:
                    irpMinorString = "IRP_MN_STOP_DEVICE";
                    break;
                case IRP_MN_QUERY_STOP_DEVICE:
                    irpMinorString = "IRP_MN_QUERY_STOP_DEVICE";
                    break;
                case IRP_MN_CANCEL_STOP_DEVICE:
                    irpMinorString = "IRP_MN_CANCEL_STOP_DEVICE";
                    break;
                case IRP_MN_QUERY_DEVICE_RELATIONS:
                    irpMinorString = "IRP_MN_QUERY_DEVICE_RELATIONS";
                    break;
                case IRP_MN_QUERY_INTERFACE:
                    irpMinorString = "IRP_MN_QUERY_INTERFACE";
                    break;
                case IRP_MN_QUERY_CAPABILITIES:
                    irpMinorString = "IRP_MN_QUERY_CAPABILITIES";
                    break;
                case IRP_MN_QUERY_RESOURCES:
                    irpMinorString = "IRP_MN_QUERY_RESOURCES";
                    break;
                case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
                    irpMinorString = "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
                    break;
                case IRP_MN_QUERY_DEVICE_TEXT:
                    irpMinorString = "IRP_MN_QUERY_DEVICE_TEXT";
                    break;
                case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
                    irpMinorString = "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
                    break;
                case IRP_MN_READ_CONFIG:
                    irpMinorString = "IRP_MN_READ_CONFIG";
                    break;
                case IRP_MN_WRITE_CONFIG:
                    irpMinorString = "IRP_MN_WRITE_CONFIG";
                    break;
                case IRP_MN_EJECT:
                    irpMinorString = "IRP_MN_EJECT";
                    break;
                case IRP_MN_SET_LOCK:
                    irpMinorString = "IRP_MN_SET_LOCK";
                    break;
                case IRP_MN_QUERY_ID:
                    irpMinorString = "IRP_MN_QUERY_ID";
                    break;
                case IRP_MN_QUERY_PNP_DEVICE_STATE:
                    irpMinorString = "IRP_MN_QUERY_PNP_DEVICE_STATE";
                    break;
                case IRP_MN_QUERY_BUS_INFORMATION:
                    irpMinorString = "IRP_MN_QUERY_BUS_INFORMATION";
                    break;
                case IRP_MN_DEVICE_USAGE_NOTIFICATION:
                    irpMinorString = "IRP_MN_DEVICE_USAGE_NOTIFICATION";
                    break;
                case IRP_MN_SURPRISE_REMOVAL:
                    irpMinorString = "IRP_MN_SURPRISE_REMOVAL";
                    break;
                case IRP_MN_QUERY_LEGACY_BUS_INFORMATION:
                    irpMinorString = "IRP_MN_QUERY_LEGACY_BUS_INFORMATION";
                    break;
                default :
                    sprintf(nameBuf,UnknownIrpMinor,MinorCode);
                    irpMinorString = nameBuf;
            }
            break;

        default:
            sprintf(nameBuf,"Unknown Irp major code (%u)",MajorCode);
            irpMajorString = nameBuf;
    }

    strcpy(MajorCodeName,irpMajorString);
    strcpy(MinorCodeName,irpMinorString);
}

#endif


ULONG
CaculateAllocationSize (
	PVOLUME_DEVICE_OBJECT	VolDo,
	PFCB					FcbOrDcb,
	ULONG					FileSize
	)
{
	ULONG ApproximateClusterCount;
    ULONG TargetAllocation = 0;
    ULONG Multiplier;
    ULONG BytesPerCluster;
    ULONG ClusterAlignedFileSize;
	//
    //  We are going to try and allocate a bigger chunk than
    //  we actually need in order to maximize FastIo usage.
    //
    //  The multiplier is computed as follows:
    //
    //
    //            (FreeDiskSpace            )
    //  Mult =  ( (-------------------------) / 32 ) + 1
    //            (FileSize - AllocationSize)
    //
    //          and max out at 32.
    //
    //  With this formula we start winding down chunking
    //  as we get near the disk space wall.
    //
    //  For instance on an empty 1 MEG floppy doing an 8K
    //  write, the multiplier is 6, or 48K to allocate.
    //  When this disk is half full, the multipler is 3,
    //  and when it is 3/4 full, the mupltiplier is only 1.
    //
    //  On a larger disk, the multiplier for a 8K read will
    //  reach its maximum of 32 when there is at least ~8 Megs
    //  available.
    //

    //
    //  Small write performance note, use cluster aligned
    //  file size in above equation.
    //

    //
    //  We need to carefully consider what happens when we approach
    //  a 2^32 byte filesize.  Overflows will cause problems.
    //

    BytesPerCluster = 1 << VolDo->Vcb.AllocationSupport.LogOfBytesPerCluster;

    //
    //  This can overflow if the target filesize is in the last cluster.
    //  In this case, we can obviously skip over all of this fancy
    //  logic and just max out the file right now.
    //

    ClusterAlignedFileSize = (FileSize + (BytesPerCluster - 1)) & ~(BytesPerCluster - 1);

	//DbgPrint("ClusterAlignedFileSize = %x\n", ClusterAlignedFileSize);
	return ClusterAlignedFileSize;

    if (ClusterAlignedFileSize != 0) {

		//
	    //  This actually has a chance but the possibility of overflowing
		//  the numerator is pretty unlikely, made more unlikely by moving
	    //  the divide by 32 up to scale the BytesPerCluster. However, even if it does the
		//  effect is completely benign.
	    //
		//  FAT32 with a 64k cluster and over 2^21 clusters would do it (and
	    //  so forth - 2^(16 - 5 + 21) == 2^32).  Since this implies a partition
		//  of 32gb and a number of clusters (and cluster size) we plan to
	    //  disallow in format for FAT32, the odds of this happening are pretty
		//  low anyway.
	    //
    
		Multiplier = ((VolDo->Vcb.AllocationSupport.NumberOfFreeClusters *
			              (BytesPerCluster >> 5)) /
				          (ClusterAlignedFileSize -
					      FcbOrDcb->Header.AllocationSize.LowPart)) + 1;
    
	    if (Multiplier > 32) { Multiplier = 32; }
    
		Multiplier *= (ClusterAlignedFileSize - FcbOrDcb->Header.AllocationSize.LowPart);

	    TargetAllocation = FcbOrDcb->Header.AllocationSize.LowPart + Multiplier;
    
		//
	    //  We know that TargetAllocation is in whole clusters, so simply
		//  checking if it wrapped is correct.  If it did, we fall back
	    //  to allocating up to the maximum legal size.
		//
    
	    if (TargetAllocation < FcbOrDcb->Header.AllocationSize.LowPart) {
    
			TargetAllocation = ~BytesPerCluster + 1;
			Multiplier = TargetAllocation - FcbOrDcb->Header.AllocationSize.LowPart;
		}
    }

	return TargetAllocation;
}


#endif