#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"


NTSTATUS
XixFsReadLockState(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint64			LotNumber,
	IN uint32			LotSize,
	IN uint32			SectorSize,
	IN uint8			*HostId,
	OUT uint8			*pLockOwnerId,
	OUT uint8			*pLockOwnerMac,
	OUT uint32			*LotState,
	OUT	uint64			*LockAcquireTime
	);


BOOLEAN
XixFsAreYouHaveLotLock(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint8		* LockOwnerMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint8		* LockOwnerId
);


BOOLEAN
XixFsIHaveLotLock(
	IN uint8	* HostMac,
	IN uint64	 LotNumber,
	IN uint8	* DiskId,
	IN uint32	 PartitionId
);


NTSTATUS
XixFsLotUnLockRoutine(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber
	);



NTSTATUS
XixFsLotLockRoutine(
	BOOLEAN 		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	BOOLEAN			CheckLotLock
	);




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsReadLockState)


#pragma alloc_text(PAGE, XixFsLotUnLockRoutine)
#pragma alloc_text(PAGE, XixFsLotLockRoutine)
#pragma alloc_text(PAGE, XixFsLotLock)
#pragma alloc_text(PAGE, XixFsLotUnLock)
#pragma alloc_text(PAGE, XixFsGetLockState)
#pragma alloc_text(PAGE, XixFsCheckAndLock)
#endif



NTSTATUS
XixFsReadLockState(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint64			LotNumber,
	IN uint32			LotSize,
	IN uint32			SectorSize,
	IN uint8			*HostId,
	OUT uint8			*pLockOwnerId,
	OUT uint8			*pLockOwnerMac,
	OUT uint32			*LotState,
	OUT	uint64			*LockAcquireTime
	)
{
	NTSTATUS			RC;
	NTSTATUS			STATUS;
	uint8 				*buffer = NULL;
	PXIDISK_LOCK		LotLock;
	LARGE_INTEGER		Offset;
	uint32				Size = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsReadLockState \n"));

	ASSERT(DeviceObject);
	
	Size = SectorSize;


	buffer = (PUCHAR) ExAllocatePool(NonPagedPool, Size);
	if(!buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(buffer, Size);

	try{
		RC = XixFsRawReadLotLockInfo(DeviceObject, 
									LotSize, 
									LotNumber, 
									buffer, 
									Size,
									SectorSize
									);

		if(!NT_SUCCESS(RC))
		{
			try_return(RC);
		}

		LotLock = (PXIDISK_LOCK)buffer;

		RtlCopyMemory(pLockOwnerId, LotLock->LockHostSignature, 16);
		// Changed by ILGU HONG
		//	chesung suggest
		//RtlCopyMemory(pLockOwnerMac, LotLock->LockHostMacAddress, 6);
		RtlCopyMemory(pLockOwnerMac, LotLock->LockHostMacAddress, 32);
		*LockAcquireTime = LotLock->LockAcquireTime;


		if(LotLock->LockState == XIDISK_LOCK_RELEASED){
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Lock is Free \n"));

			*LotState = FCB_FILE_LOCK_INVALID;
		}
		else if(LotLock->LockState == XIDISK_LOCK_ACQUIRED){
			if(RtlCompareMemory(HostId ,pLockOwnerId, 16) != 16){
				DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("Already Lock is set by other! \n"));

				*LotState = FCB_FILE_LOCK_OTHER_HAS;
			}else{
				DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("Already Lock has! \n"));

				*LotState = FCB_FILE_LOCK_HAS;
			}
		}else {
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("First Time lock acq! \n"));

			*LotState = FCB_FILE_LOCK_INVALID;
		}

	}finally{
		if(buffer) ExFreePool(buffer);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsReadLockState \n"));
	return RC;
}










NTSTATUS
XixFsLotUnLockRoutine(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber
	)
{
	int					lockstate;
	NTSTATUS			RC;
	uint8				*buffer = NULL;
	uint32				Size = 0;
	PXIDISK_LOCK		Lock = NULL;
	LARGE_INTEGER		Offset;



	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XifsdLotUnLockRoutine \n"));


	

	Size = VCB->SectorSize;


	buffer = (PUCHAR) ExAllocatePool(NonPagedPool, Size);

	if(!buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(buffer, Size);	

	try{
		if(VCB->SectorSize > SECTORSIZE_512){
			RC = XixFsRawReadLotLockInfo(DeviceObject, 
										VCB->LotSize, 
										LotNumber, 
										buffer, 
										Size,
										Size
										);

			if(!NT_SUCCESS(RC))
			{
				try_return(RC);
			}			
			
		}

		Lock = (PXIDISK_LOCK)buffer;
		Lock->LockAcquireTime = XixGetSystemTime().QuadPart;
		RtlCopyMemory(Lock->LockHostSignature,VCB->HostId,16);
		// Changed by ILGU HONG
		//	chesung suggest
		//RtlCopyMemory(Lock->LockHostMacAddress, VCB->HostMac, 6);
		RtlCopyMemory(Lock->LockHostMacAddress, VCB->HostMac, 32);

		Lock->LockState = XIDISK_LOCK_RELEASED;


		RC = XixFsRawWriteLotLockInfo(DeviceObject, 
										VCB->LotSize, 
										LotNumber, 
										buffer, 
										Size,
										Size
										);

		if(!NT_SUCCESS(RC))
		{
			try_return(RC);
		}


		RC = STATUS_SUCCESS;

	}finally{
		if(buffer) ExFreePool(buffer);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XifsdLotUnLockRoutine \n"));
	return RC;
}


NTSTATUS
XixFsLotLockRoutine(
	BOOLEAN 		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	BOOLEAN			CheckLotLock
	)
{
	NTSTATUS		RC;
	int				lockstate;
	char *			buffer = NULL;
	// Changed by ILGU HONG
	//	chesung suggest
	//uint8			LotOwnerMac[6];
	uint8			LotOwnerMac[32];
	uint8			LotOwnerId[16];
	uint64			LotOwnerAcqTime = 0;

	uint8			tmpLotOwnerMac[32];
	uint8			tmpLotOwnerId[16];
	uint64			tmpLotOwnerAcqTime = 0;


	PXIDISK_LOCK		Lock;
	LARGE_INTEGER		Offset;
	uint32				Size = 0;
	BOOLEAN				bRetry = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsLotLockRoutine \n"));

	Size = VCB->SectorSize;

	
	buffer = (PUCHAR) ExAllocatePool(NonPagedPool, Size);

	if(!buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}



	try{

		if(CheckLotLock){

			RC = XixFsReadLockState(
					DeviceObject, 
					LotNumber,
					VCB->LotSize ,
					VCB->SectorSize,
					VCB->HostId,
					LotOwnerId,
					LotOwnerMac,
					&lockstate,
					&LotOwnerAcqTime
					);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
		}else{
			lockstate = FCB_FILE_LOCK_INVALID;
		}


		if(lockstate != FCB_FILE_LOCK_INVALID)
		{
			if(lockstate == FCB_FILE_LOCK_OTHER_HAS) {

				XixFsNdasUnLock(DeviceObject);

				if(XixFsAreYouHaveLotLock(
					Wait,
					VCB->HostMac,
					LotOwnerMac,
					LotNumber,
					VCB->DiskId,
					VCB->PartitionId,
					LotOwnerId
					))
				{

					RC= STATUS_UNSUCCESSFUL;
					try_return(RC);
				}

				RC = XixFsNdasLock(DeviceObject);
				
				if(!NT_SUCCESS(RC)){
					RC= STATUS_UNSUCCESSFUL;
					try_return(RC);
				}

				if(CheckLotLock){
					RC = XixFsReadLockState(
							DeviceObject, 
							LotNumber,
							VCB->LotSize ,
							VCB->SectorSize,
							VCB->HostId,
							tmpLotOwnerId,
							tmpLotOwnerMac,
							&lockstate,
							&tmpLotOwnerAcqTime
							);

					if(!NT_SUCCESS(RC)){
						RC= STATUS_UNSUCCESSFUL;
						try_return(RC);
					}

					if( (RtlCompareMemory(LotOwnerId, tmpLotOwnerId, 16) != 16)  
						|| (RtlCompareMemory(LotOwnerMac, tmpLotOwnerMac, 32) != 32)  
						|| (LotOwnerAcqTime != tmpLotOwnerAcqTime) )
					{
						RC= STATUS_UNSUCCESSFUL;
						try_return(RC);
					}

				}



			}

			RC = XixFsLotUnLockRoutine(
				VCB,
				DeviceObject,
				LotNumber
				);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}

			lockstate = FCB_FILE_LOCK_INVALID;
			
		}

		RtlZeroMemory(buffer, Size);	


		if(VCB->SectorSize > SECTORSIZE_512){
			RC = XixFsRawReadLotLockInfo(DeviceObject, 
										VCB->LotSize, 
										LotNumber, 
										buffer, 
										Size, 
										VCB->SectorSize
										);

			if(!NT_SUCCESS(RC))
			{
				try_return(RC);
			}			
			
		}

		Lock = (PXIDISK_LOCK)buffer;
		Lock->LockAcquireTime = XixGetSystemTime().QuadPart;
		RtlCopyMemory(Lock->LockHostSignature,VCB->HostId, 16);
		// Changed by ILGU HONG
		//	chesung suggest
		//RtlCopyMemory(Lock->LockHostMacAddress,VCB->HostMac,6);
		RtlCopyMemory(Lock->LockHostMacAddress,VCB->HostMac,32);
		Lock->LockState = XIDISK_LOCK_ACQUIRED;


		RC = XixFsRawWriteLotLockInfo(DeviceObject, 
										VCB->LotSize, 
										LotNumber, 
										buffer, 
										Size, 
										VCB->SectorSize
										);

		if(!NT_SUCCESS(RC))
		{
			try_return(RC);
		}


		RC = STATUS_SUCCESS;


	}finally{
		if(buffer)ExFreePool(buffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsLotLockRoutine \n"));

	return RC;
}

VOID
XixFsRefAuxLotLock(
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo
)
{
	ASSERT(AuxLotInfo->RefCount > 0 );
	InterlockedIncrement(&(AuxLotInfo->RefCount));
	return;
}


VOID
XixFsDeRefAuxLotLock(
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo
)
{
	//XifsdDebugTarget = DEBUG_TARGET_LOCK;
	ASSERT(AuxLotInfo->RefCount > 0 );
	if(InterlockedDecrement(&(AuxLotInfo->RefCount)) == 0)
	{
		
		if(AuxLotInfo->RefCount == 0){
		
		// Changed by ILGU HONG
		/*
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsDeRefAuxLotLock: Delete DiskId(0x%02x:%02x:%02x:%02x:%02x:%02x) PartitionId(%ld) LotNum(%I64d)\n",
			AuxLotInfo->DiskId[0],
			AuxLotInfo->DiskId[1],
			AuxLotInfo->DiskId[2],
			AuxLotInfo->DiskId[3],
			AuxLotInfo->DiskId[4],
			AuxLotInfo->DiskId[5],
			AuxLotInfo->PartitionId,
			AuxLotInfo->LotNumber
			));
		*/
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsDeRefAuxLotLock: Delete DiskId(0x%02x:%02x:%02x:%02x:%02x:%02x) PartitionId(%ld) LotNum(%I64d)\n",
			AuxLotInfo->DiskId[10],
			AuxLotInfo->DiskId[11],
			AuxLotInfo->DiskId[12],
			AuxLotInfo->DiskId[13],
			AuxLotInfo->DiskId[14],
			AuxLotInfo->DiskId[15],
			AuxLotInfo->PartitionId,
			AuxLotInfo->LotNumber
			));

			ExFreePool(AuxLotInfo);
		}
	}

	//XifsdDebugTarget = 0;
	return;
}

NTSTATUS
XixFsAuxLotLock(
	BOOLEAN  		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	BOOLEAN			Check,
	BOOLEAN			Retry
)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	PLIST_ENTRY					pAuxLotLockList = NULL;
	BOOLEAN						bNewEntry = FALSE;
	uint32						Status = 0;

	//XifsdDebugTarget = DEBUG_TARGET_LOCK;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsAuxLotLock \n"));

	pAuxLotLockList = XiGlobalData.XifsAuxLotLockList.Flink;
	// Check Aux List
	ExAcquireFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

	while(pAuxLotLockList != &(XiGlobalData.XifsAuxLotLockList))
	{
		AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIFS_AUXI_LOT_LOCK_INFO, AuxLink);
		//Changed by ILGU HONG
		/*
		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 6) == 6) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) &&
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			XixFsRefAuxLotLock(AuxLotInfo);
			break;
		}
		*/
		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 16) == 16) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) &&
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			XixFsRefAuxLotLock(AuxLotInfo);
			break;
		}
		AuxLotInfo = NULL;
		pAuxLotLockList = pAuxLotLockList->Flink;
	}
	
	ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

	
	if(AuxLotInfo == NULL){
		AuxLotInfo = (PXIFS_AUXI_LOT_LOCK_INFO)ExAllocatePoolWithTag(NonPagedPool, sizeof(XIFS_AUXI_LOT_LOCK_INFO), TAG_BUFFER);
		if(!AuxLotInfo){
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(AuxLotInfo, sizeof(XIFS_AUXI_LOT_LOCK_INFO));
		// Changed by ILGU HONG
		//	chesung suggest
		//RtlCopyMemory(AuxLotInfo->DiskId, VCB->DiskId, 6);
		//RtlCopyMemory(AuxLotInfo->HostMac, VCB->HostMac, 6);
		RtlCopyMemory(AuxLotInfo->DiskId, VCB->DiskId, 16);
		RtlCopyMemory(AuxLotInfo->HostMac, VCB->HostMac, 32);
		RtlCopyMemory(AuxLotInfo->HostId, VCB->HostId, 16);
		AuxLotInfo->PartitionId = VCB->PartitionId;
		AuxLotInfo->LotNumber = LotNumber;
		AuxLotInfo->HasLock = FCB_FILE_LOCK_INVALID;
		AuxLotInfo->RefCount = 2;
	
		// Changed by ILGU HONG
		/*
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsAuxLotLock: New DiskId(0x%02x:%02x:%02x:%02x:%02x:%02x) PartitionId(%ld) LotNum(%I64d)\n",
			AuxLotInfo->DiskId[0],
			AuxLotInfo->DiskId[1],
			AuxLotInfo->DiskId[2],
			AuxLotInfo->DiskId[3],
			AuxLotInfo->DiskId[4],
			AuxLotInfo->DiskId[5],
			AuxLotInfo->PartitionId,
			AuxLotInfo->LotNumber
			));
		*/

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsAuxLotLock: New DiskId(0x%02x:%02x:%02x:%02x:%02x:%02x) PartitionId(%ld) LotNum(%I64d)\n",
			AuxLotInfo->DiskId[10],
			AuxLotInfo->DiskId[11],
			AuxLotInfo->DiskId[12],
			AuxLotInfo->DiskId[13],
			AuxLotInfo->DiskId[14],
			AuxLotInfo->DiskId[15],
			AuxLotInfo->PartitionId,
			AuxLotInfo->LotNumber
			));

		InitializeListHead(&(AuxLotInfo->AuxLink));
		
		bNewEntry = TRUE;

		ExAcquireFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));
		InsertHeadList(&(XiGlobalData.XifsAuxLotLockList), &(AuxLotInfo->AuxLink));
		ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

		
	}

	RC = XixFsLotLock(Wait,
						VCB,
						DeviceObject,
						LotNumber,
						&Status,
						Check, 
						Retry
						);



	AuxLotInfo->HasLock = Status;

	XixFsDeRefAuxLotLock(AuxLotInfo);
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsAuxLotLock \n"));
	//XifsdDebugTarget = 0;
	return RC;	
}


NTSTATUS
XixFsAuxLotUnLock(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber
)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	PLIST_ENTRY					pAuxLotLockList = NULL;
	BOOLEAN						bNewEntry = FALSE;
	uint32						Status = 0;

	//XifsdDebugTarget = DEBUG_TARGET_LOCK;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsAuxLotUnLock \n"));

	pAuxLotLockList = XiGlobalData.XifsAuxLotLockList.Flink;
	// Check Aux List
	ExAcquireFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

	while(pAuxLotLockList != &(XiGlobalData.XifsAuxLotLockList))
	{
		AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIFS_AUXI_LOT_LOCK_INFO, AuxLink);
		// Changed by ILGU HONG
		/*
		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 6) == 6) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) &&
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			XixFsRefAuxLotLock(AuxLotInfo);
			break;
		}
		*/
		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 16) == 16) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) &&
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			XixFsRefAuxLotLock(AuxLotInfo);
			break;
		}

		AuxLotInfo = NULL;
		pAuxLotLockList = pAuxLotLockList->Flink;
	}
	ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));


	RC = XixFsLotUnLock(VCB,DeviceObject,LotNumber,&Status);

	if(AuxLotInfo){
		AuxLotInfo->HasLock = FCB_FILE_LOCK_INVALID;
	}

	XixFsDeRefAuxLotLock(AuxLotInfo);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsAuxLotUnLock \n"));
	//XifsdDebugTarget = 0;
	return STATUS_SUCCESS;	
}



VOID
XixFsCleanUpAuxLockLockInfo(
		PXIFS_VCB		VCB
)
{
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	PLIST_ENTRY					pAuxLotLockList = NULL;
	
	//XifsdDebugTarget = DEBUG_TARGET_LOCK;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsCleanUpAuxLockLockInfo \n"));


	pAuxLotLockList = XiGlobalData.XifsAuxLotLockList.Flink;

	ExAcquireFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));
	while(pAuxLotLockList != &(XiGlobalData.XifsAuxLotLockList))
	{
		AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIFS_AUXI_LOT_LOCK_INFO, AuxLink);
		// Changed by ILGU HONG
		/*
		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 6) == 6) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) )
		{
			pAuxLotLockList = pAuxLotLockList->Flink;
			RemoveEntryList(&(AuxLotInfo->AuxLink));
			XixFsDeRefAuxLotLock(AuxLotInfo);
		}else{
			pAuxLotLockList = pAuxLotLockList->Flink;
		}
		*/

		if((RtlCompareMemory(VCB->DiskId, AuxLotInfo->DiskId, 16) == 16) && 
			(VCB->PartitionId == AuxLotInfo->PartitionId) )
		{
			pAuxLotLockList = pAuxLotLockList->Flink;
			RemoveEntryList(&(AuxLotInfo->AuxLink));
			XixFsDeRefAuxLotLock(AuxLotInfo);
		}else{
			pAuxLotLockList = pAuxLotLockList->Flink;
		}

	}
	ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));
	
	//XifsdDebugTarget = 0;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsCleanUpAuxLockLockInfo \n"));
}





NTSTATUS
XixFsLotLock(
	BOOLEAN  		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	uint32			*Status,
	BOOLEAN			Check,
	BOOLEAN			Retry
	)
{
	NTSTATUS		RC;
	uint32			TryCount = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsLotLock \n"));

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("XixFsLotLock LotNumber(%I64d).\n", LotNumber));


	try{
retry:
		RC = XixFsNdasLock(DeviceObject);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_LEVEL_ALL, 
						("\n!!!Fail XixFsLotLock LotNumber(%I64d).\n", LotNumber));
			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		RC = XixFsLotLockRoutine(Wait,VCB, DeviceObject, LotNumber, Check);
		
		XixFsNdasUnLock(DeviceObject);
		
		if((!NT_SUCCESS(RC)) && (Retry)&& (TryCount < 3)){
			TryCount++;
			goto retry;
		}

	}finally{

	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsLotLock \n"));

	if(NT_SUCCESS(RC)){
		*Status = FCB_FILE_LOCK_HAS;
	}else{
		*Status = FCB_FILE_LOCK_INVALID;
	}

	return RC;
}


NTSTATUS
XixFsLotUnLock(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	uint32			*Status
	)
{
	NTSTATUS		RC;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsLotUnLock \n"));
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("XixFsLotUnLock LotNumber(%I64d).\n", LotNumber));

	try{
		RC = XixFsNdasLock(DeviceObject);
		if(!NT_SUCCESS(RC)){
			RC =  STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		RC = XixFsLotUnLockRoutine(VCB, DeviceObject, LotNumber);
		XixFsNdasUnLock(DeviceObject);
	}finally{

	}



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsLotUnLock \n"));

	
	*Status = FCB_FILE_LOCK_INVALID;
	

	return RC;
}

















NTSTATUS
XixFsGetLockState(
	IN PXIFS_VCB VCB,
	IN PDEVICE_OBJECT DeviceObject,
	IN uint64 		LotNumber,
	OUT uint32 * 	State
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	uint8			LockOwnerId[16];
	// Changed by ILGU HONG
	//	chesung suggest
	//uint8 			LockOwnerMac[6];
	uint8 			LockOwnerMac[32];
	int32			LockState;
	uint64			LockOwnerAcqTime = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsGetLockState \n"));

	try{
		RC = XixFsNdasLock(DeviceObject);
		if(!NT_SUCCESS(RC)){
			RC =  STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		RC = XixFsReadLockState(
					DeviceObject, 
					LotNumber, 
					VCB->LotSize,
					VCB->SectorSize, 
					VCB->HostId, 
					LockOwnerId, 
					LockOwnerMac, 
					&LockState,
					&LockOwnerAcqTime
					);

		XixFsNdasUnLock(DeviceObject);

		*State = LockState;
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsGetLockState \n"));

	return RC;
}




NTSTATUS
XixFsCheckAndLock(
	IN BOOLEAN			Wait,
	IN PXIFS_VCB 		VCB,
	IN PDEVICE_OBJECT 	DeviceObject,
	IN uint64 				LotNumber,
	OUT uint32			*pIsAcuired
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	uint8			LockOwnerId[16];
	// Change by ILGU HONG
	//	chesung suggest
	//uint8 			LockOwnerMac[6];
	uint8 			LockOwnerMac[32];
	int32			LockState;
	uint64			LockOwnerAcqTime = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsCheckAndLock \n"));


	try{
		RC = XixFsNdasLock(DeviceObject);
		if(!NT_SUCCESS(RC)){
			RC =  STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		RC = XixFsReadLockState(
					DeviceObject, 
					LotNumber, 
					VCB->LotSize, 
					VCB->SectorSize, 
					VCB->HostId, 
					LockOwnerId, 
					LockOwnerMac, 
					&LockState,
					&LockOwnerAcqTime
					);

		if(!NT_SUCCESS(RC)){
			XixFsNdasUnLock(DeviceObject);
			RC =  STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		if(LockState ==1) {
			XixFsNdasUnLock(DeviceObject);
			*pIsAcuired = 0;
			RC =  STATUS_SUCCESS;
		}else {
			RC = XixFsLotLockRoutine(Wait,VCB, DeviceObject, LotNumber, FALSE);
			XixFsNdasUnLock(DeviceObject);
			
			if(!NT_SUCCESS(RC)){
				*pIsAcuired= 1;
				RC = STATUS_SUCCESS;	
			}
			
		}


	}finally{

	}

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsCheckAndLock \n"));

	return RC;	
}
