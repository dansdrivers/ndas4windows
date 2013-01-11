#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDiskForm.h"
#include "XixFsDrv.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"
#include "XixFsGlobalData.h"
#include "XixFsdInternalApi.h"

WCHAR XifsdUnicodeSelfArray[] = { L'.' };
WCHAR XifsdUnicodeParentArray[] = { L'.', L'.' };

UNICODE_STRING XifsdUnicodeDirectoryNames[] = {
    { sizeof(XifsdUnicodeSelfArray), sizeof(XifsdUnicodeSelfArray), XifsdUnicodeSelfArray},
    { sizeof(XifsdUnicodeParentArray), sizeof(XifsdUnicodeParentArray), XifsdUnicodeParentArray}
};


NTSTATUS
XixFsRawReadDirEntry(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB,
	IN uint32	Index,
	IN uint8	*buffer,
	IN uint32	bufferSize,
	IN uint8	*AddrBuffer,
	IN uint32	AddrBufferSize,
	IN uint32	*AddrStartSecIndex,
	IN BOOLEAN	Waitable
);


NTSTATUS
XixFsRawWriteDirEntry(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB,
	IN uint32	Index,
	IN uint8	*buffer,
	IN uint32	bufferSize,
	IN uint8	*AddrBuffer,
	IN uint32	AddrBufferSize,
	IN uint32	*AddrStartSecIndex,
	IN BOOLEAN	Waitable
);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsRawReadDirEntry)
#pragma alloc_text(PAGE, XixFsRawWriteDirEntry)
#pragma alloc_text(PAGE, XixFsInitializeDirContext)
#pragma alloc_text(PAGE, XixFsCleanupDirContext)
#pragma alloc_text(PAGE, XixFsLookupInitialDirEntry)
#pragma alloc_text(PAGE, XixFsUpdateDirNames)
#pragma alloc_text(PAGE, XixFsFindDirEntryByLotNumber)
#pragma alloc_text(PAGE, XixFsLookupInitialFileIndex)
#pragma alloc_text(PAGE, XixFsFindDirEntry) 
#pragma alloc_text(PAGE, XixFsAddChildToDir)
#pragma alloc_text(PAGE, XixFsDeleteChildFromDir)
#pragma alloc_text(PAGE, XixFsUpdateChildFromDir)
#endif




/*
 *	Function must be done within waitable thread context
 */



NTSTATUS
XixFsRawReadDirEntry(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB,
	IN uint32	Index,
	IN uint8	*buffer,
	IN uint32	bufferSize,
	IN uint8	*AddrBuffer,
	IN uint32	AddrBufferSize,
	IN uint32	*AddrStartSecIndex,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	int64				RequestOffset = 0;
	int64				LotAddressOffset = 0;
	int64				Margin = 0;
	uint64				DataOffset = 0;
	LARGE_INTEGER		Offset;
	XIFS_IO_LOT_INFO	AddressInfo;
	uint32				LotSize = 0;
	uint32				Reason = 0;
	uint32				BlockSize = 0;


	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Enter XixFsRawReadDirEntry .\n"));

	ASSERT(pVCB);
	ASSERT(pFCB);
	ASSERT(Waitable == TRUE);
	LotSize = pVCB->LotSize;

	
	

	ASSERT(bufferSize >=XIDISK_CHILD_RECORD_SIZE);
	
	ASSERT(AddrBufferSize >= pVCB->SectorSize);

	
	DataOffset = GetChildOffset(Index);
	BlockSize = XIDISK_CHILD_RECORD_SIZE;


	if(DataOffset + BlockSize > (uint64)pFCB->RealAllocationSize){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail Too Big Allocation Size : DataOffset (%I64d) BlockSize(%ld) Realallocationsize(%I64d)\n",
					DataOffset, BlockSize, pFCB->RealAllocationSize));

		return STATUS_UNSUCCESSFUL;
	}

	RtlZeroMemory(&AddressInfo, sizeof(XIFS_IO_LOT_INFO));


	try{
		RC = XixFsGetAddressInfoForOffset(Waitable, pFCB, DataOffset, LotSize, AddrBuffer,AddrBufferSize, AddrStartSecIndex, &Reason, &AddressInfo);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsGetAddressInfoForOffset\n"));
			if(Reason != 0){
				//	print reason
			}
			//Raise exception
			try_return(RC);
		}

		ASSERT(!(AddressInfo.LogicalStartOffset % SECTORSIZE_512));
		ASSERT(!(AddressInfo.LotTotalDataSize % SECTORSIZE_512));
		ASSERT(DataOffset >= AddressInfo.LogicalStartOffset);

		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		// Check Request and forward
		if((DataOffset  + BlockSize) <=(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){
			
			Margin = DataOffset - AddressInfo.LogicalStartOffset;
			Offset.QuadPart = 
				GetAddressOfFileData(AddressInfo.Flags, pVCB->LotSize, AddressInfo.LotIndex) + Margin;
			//Make sigle request and forward request to lower driver
			RC = XixFsRawReadBlockDevice(
				pVCB->TargetDeviceObject,
				&Offset,
				BlockSize, 
				buffer);

			if(!NT_SUCCESS(RC))
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsRawReadBlockDevice\n"));
				try_return(RC);
			}
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XixFsRawReadDirEntry .\n"));
			RC =  STATUS_SUCCESS ;
			try_return(RC);
		}

		if((DataOffset + BlockSize) > (AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){	
			int64				RemainingDataSize = 0;
			int32				TransferDataSize = 0;
			char *				TransferBuffer = 0;
			
			
			Margin = DataOffset - AddressInfo.LogicalStartOffset;
			RemainingDataSize = BlockSize;
			TransferBuffer = buffer;
			
			while(RemainingDataSize){
				TransferDataSize = (uint32)(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize - DataOffset);
				Offset.QuadPart = 
					GetAddressOfFileData(AddressInfo.Flags,pVCB->LotSize, AddressInfo.LotIndex) + Margin;
				Margin = 0;

				RC = XixFsRawReadBlockDevice(
					pVCB->TargetDeviceObject,
					&Offset,
					TransferDataSize, 
					TransferBuffer);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsRawReadBlockDevice2\n"));
					return STATUS_UNSUCCESSFUL;
				}

				DataOffset += TransferDataSize;
				TransferBuffer += TransferDataSize;
				RemainingDataSize -= TransferDataSize;

				RC = XixFsGetAddressInfoForOffset(Waitable, pFCB, DataOffset, LotSize, AddrBuffer, AddrBufferSize,AddrStartSecIndex, &Reason, &AddressInfo);


				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail XixFsGetAddressInfoForOffset\n"));
					if(Reason != 0){
						//	print reason
					}
					//Raise exception
					RC =  STATUS_UNSUCCESSFUL;
					try_return(RC);
				}

				ASSERT(DataOffset == AddressInfo.LogicalStartOffset);
			}
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XixFsRawReadDirEntry .\n"));
			RC =  STATUS_SUCCESS;
			try_return(RC);
		}


	}finally{

		if(NT_SUCCESS(RC)){
			XixFsGetDirEntryInfo((PXIDISK_CHILD_INFORMATION)buffer);
		}		
	}
	
	return RC;
}


NTSTATUS
XixFsRawWriteDirEntry(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB,
	IN uint32	Index,
	IN uint8	*buffer,
	IN uint32	bufferSize,
	IN uint8	*AddrBuffer,
	IN uint32	AddrBufferSize,
	IN uint32	*AddrStartSecIndex,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	int64				RequestOffset = 0;
	int64				LotAddressOffset = 0;
	int64				Margin = 0;
	uint64				DataOffset = 0;
	LARGE_INTEGER		Offset;
	XIFS_IO_LOT_INFO	AddressInfo;
	uint32				LotSize = 0;
	uint32				Reason = 0;
	uint32				BlockSize = 0;


	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Enter XixFsRawWriteDirEntry .\n"));

	ASSERT(pVCB);
	ASSERT(pFCB);
	ASSERT(Waitable == TRUE);
	LotSize = pVCB->LotSize;


	

	ASSERT(bufferSize >=XIDISK_CHILD_RECORD_SIZE);
	
	ASSERT(AddrBufferSize >= pVCB->SectorSize);

	
	DataOffset = GetChildOffset(Index);
	BlockSize = XIDISK_CHILD_RECORD_SIZE;


	if(DataOffset + BlockSize > (uint64)pFCB->RealAllocationSize){
		uint64 LastOffset = 0;
		uint64 RequestStartOffset = 0;
		uint32 EndLotIndex = 0;
		uint32 CurentEndIndex = 0;
		uint32 RequestStatIndex = 0;
		uint32 LotCount = 0;
		uint32 AllocatedLotCount = 0;
		
		RequestStartOffset = DataOffset;
		LastOffset = DataOffset + BlockSize;
		

		CurentEndIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, (pFCB->RealAllocationSize-1));
		EndLotIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, LastOffset);

		if(EndLotIndex > CurentEndIndex){
			LotCount = EndLotIndex - CurentEndIndex;

			try{
				RC = XixFsAddNewLot(pFCB->PtrVCB, 
										pFCB, 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount, 
										AddrBuffer,
										AddrBufferSize,
										AddrStartSecIndex
										);
			}finally{

			}
	
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
					pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));
				
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail Too Big Allocation Size : DataOffset (%I64d) BlockSize(%ld) Realallocationsize(%I64d)\n",
							DataOffset, BlockSize, pFCB->RealAllocationSize));

				return RC;

			}

		}
	}

	RtlZeroMemory(&AddressInfo, sizeof(XIFS_IO_LOT_INFO));


	XixFsSetDirEntryInfo((PXIDISK_CHILD_INFORMATION)buffer);
	

	try{
		RC = XixFsGetAddressInfoForOffset(Waitable, pFCB, DataOffset, LotSize, AddrBuffer,AddrBufferSize, AddrStartSecIndex, &Reason, &AddressInfo);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsGetAddressInfoForOffset\n"));
			if(Reason != 0){
				//	print reason
			}
			//Raise exception
			try_return(RC);
		}

		ASSERT(!(AddressInfo.LogicalStartOffset % SECTORSIZE_512));
		ASSERT(!(AddressInfo.LotTotalDataSize % SECTORSIZE_512));
		ASSERT(DataOffset >= AddressInfo.LogicalStartOffset);

		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		// Check Request and forward
		if((DataOffset  + BlockSize) <=(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){
			
			Margin = DataOffset - AddressInfo.LogicalStartOffset;
			Offset.QuadPart = 
				GetAddressOfFileData(AddressInfo.Flags, pVCB->LotSize, AddressInfo.LotIndex) + Margin;
			//Make sigle request and forward request to lower driver
			RC = XixFsRawAlignSafeWriteBlockDevice(
				pVCB->TargetDeviceObject,
				pVCB->SectorSize,
				&Offset,
				BlockSize, 
				buffer);

			if(!NT_SUCCESS(RC))
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsRawReadBlockDevice\n"));
				try_return(RC);
			}
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XixFsRawWriteDirEntry .\n"));
			RC =  STATUS_SUCCESS ;
			try_return(RC);
		}

		if((DataOffset + BlockSize) > (AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){	
			int64				RemainingDataSize = 0;
			int32				TransferDataSize = 0;
			char *				TransferBuffer = 0;
			
			
			Margin = DataOffset - AddressInfo.LogicalStartOffset;
			RemainingDataSize = BlockSize;
			TransferBuffer = buffer;
			
			while(RemainingDataSize){
				TransferDataSize = (uint32)(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize - DataOffset);
				Offset.QuadPart = 
					GetAddressOfFileData(AddressInfo.Flags,pVCB->LotSize, AddressInfo.LotIndex) + Margin;
				Margin = 0;

				RC = XixFsRawAlignSafeWriteBlockDevice(
					pVCB->TargetDeviceObject,
					pVCB->SectorSize,
					&Offset,
					TransferDataSize, 
					TransferBuffer);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsRawReadBlockDevice2\n"));
					return STATUS_UNSUCCESSFUL;
				}

				DataOffset += TransferDataSize;
				TransferBuffer += TransferDataSize;
				RemainingDataSize -= TransferDataSize;

				RC = XixFsGetAddressInfoForOffset(Waitable, pFCB, DataOffset, LotSize, AddrBuffer,AddrBufferSize, AddrStartSecIndex, &Reason, &AddressInfo);


				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail XixFsGetAddressInfoForOffset\n"));
					if(Reason != 0){
						//	print reason
					}
					//Raise exception
					RC =  STATUS_UNSUCCESSFUL;
					try_return(RC);
				}

				ASSERT(DataOffset == AddressInfo.LogicalStartOffset);
			}
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XixFsRawWriteDirEntry .\n"));
			RC =  STATUS_SUCCESS;
			try_return(RC);
		}


	}finally{

		if(NT_SUCCESS(RC)){
			XixFsGetDirEntryInfo((PXIDISK_CHILD_INFORMATION)buffer);
		} 		
	}
	
	return RC;	
}




NTSTATUS
XixFsInitializeDirContext(
	IN PXIFS_VCB		pVCB,
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext
)
{
	uint32		Size = 0;
	NTSTATUS	RC = STATUS_SUCCESS;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Enter XixFsInitializeDirContext .\n"));
	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}
	//
	//  Provide defaults for fields, nothing too special.
	//

	RtlZeroMemory( DirContext, sizeof(XIFS_DIR_EMUL_CONTEXT) );	
	

	try{
		DirContext->ChildEntry = ExAllocatePoolWithTag(NonPagedPool, XIDISK_CHILD_RECORD_SIZE, TAG_CHILD);
		if(!DirContext->ChildEntry){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsInitializeDirContext Fail Alloc ChildEntry.\n"));
		
			try_return(RC =  STATUS_INSUFFICIENT_RESOURCES);
		}

		Size = pVCB->SectorSize;

		
		DirContext->AddrMapSize = Size;
		DirContext->AddrSecNumber = 0;
		DirContext->AddrMap = ExAllocatePoolWithTag(NonPagedPool, Size, TAG_CHILD);
		if(!DirContext->AddrMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsInitializeDirContext Fail Alloc AddrMap.\n"));
			try_return(RC =  STATUS_INSUFFICIENT_RESOURCES);
		}
		


		DirContext->ChildBitMap = ExAllocatePoolWithTag(NonPagedPool, 128, TAG_CHILD);
		if(!DirContext->ChildBitMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsInitializeDirContext Fail Alloc ChildBitMap.\n"));
			try_return(RC =  STATUS_INSUFFICIENT_RESOURCES);
		}

	}finally{
		if(!NT_SUCCESS(RC)){
			if(DirContext->ChildEntry){
				ExFreePool(DirContext->ChildEntry);
				DirContext->ChildEntry = NULL;
			}

			if(DirContext->AddrMap){
				ExFreePool(DirContext->AddrMap);
				DirContext->AddrMap = NULL;
			}

			if(DirContext->ChildBitMap){
				ExFreePool(DirContext->ChildBitMap);
				DirContext->ChildBitMap = NULL;
			}
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("Exit XifsdInitializeDirContext(0x%x) .\n, RC"));
	return RC;

}



VOID
XixFsCleanupDirContext(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext
)
{
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Enter XifsdInitializeDirContext .\n"));

	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}

	try{
		if(DirContext->AddrMap){
			ExFreePool(DirContext->AddrMap);
			DirContext->AddrMap = NULL;
		}


		if(DirContext->ChildBitMap){
			ExFreePool(DirContext->ChildBitMap);
			DirContext->ChildBitMap = NULL;
		}


		if(DirContext->ChildEntry){
			ExFreePool(DirContext->ChildEntry);
			DirContext->ChildEntry = NULL;
		}

		RtlZeroMemory(DirContext, sizeof(XIFS_DIR_EMUL_CONTEXT));
	}finally{
		if(AbnormalTermination()){
			ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XifsdInitializeDirContext .\n"));
}



NTSTATUS
XixFsLookupInitialDirEntry(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext,
    IN uint32 InitIndex
    )
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	Offset;
	uint32			BlockSize = 0;
	PXIFS_VCB		pVCB = NULL;
	uint32			i = 0;
	uint64			*addr = NULL;
	uint8 *			Buffer = NULL;
    
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsLookupInitialDirEntry .\n"));
    	

    //
    //  Check inputs.
    //
	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}
	
    ASSERT_FCB( Fcb );
    
	ASSERT(Fcb->FCBType == FCB_TYPE_DIR);
	pVCB = Fcb->PtrVCB;
	ASSERT_VCB(pVCB);

	DirContext->RealDirIndex = 0;
	DirContext->VirtualDirIndex = InitIndex;

	if(DirContext->VirtualDirIndex > 1){
		//DirContext->RealDirIndex = DirContext->VirtualDirIndex - 2;
		DirContext->RealDirIndex = -1;
	}

	DirContext->LotNumber = Fcb->LotNumber;
	DirContext->pVCB = pVCB; 
	DirContext->pFCB = Fcb;
	
	Buffer = ExAllocatePoolWithTag(NonPagedPool, XIDISK_DIR_INFO_SIZE, TAG_CHILD);
	if(!Buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR XixFsLookupInitialDirEntry alloc Buffer\n"));

		return STATUS_INSUFFICIENT_RESOURCES;
	}



	try{

		
		RC = XixFsRawReadAddressOfFile(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						DirContext->LotNumber,
						DirContext->pFCB->AddrLotNumber,
						DirContext->AddrMap,
						DirContext->AddrMapSize,
						&(DirContext->AddrSecNumber),
						DirContext->AddrSecNumber,
						pVCB->SectorSize);
						
		
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR XixFsLookupInitialDirEntry Read Address info\n"));
			
			try_return(RC);
		}




//		addr = (uint64 *)DirContext->AddrMap;

//		for(i = 0; i< 10; i++)
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_INFO), 
//						("Read AddreeBCB Address[%ld](%I64d).\n",i,addr[i]));

		

		RC = XixFsRawReadFileHeader(
					pVCB->TargetDeviceObject,
					pVCB->LotSize,
					DirContext->LotNumber,
					Buffer,
					XIDISK_DIR_INFO_SIZE,
					pVCB->SectorSize
					);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsLookupInitialDirEntry Read File header info\n"));
			try_return(RC);
		}
		


		RtlCopyMemory(DirContext->ChildBitMap, Buffer +  FIELD_OFFSET(XIDISK_DIR_INFO, ChildMap), 128);


//		for(i = 0; i< 10; i++)
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_INFO), 
//						("Read ChildBitMap bitmap[%ld](0x%02x).\n",i,DirContext->ChildBitMap[i]));

		
		
		RtlZeroMemory(DirContext->ChildEntry,XIDISK_CHILD_RECORD_SIZE );
	}finally{
		
		ExFreePool(Buffer);

		if (AbnormalTermination()) {
			if(DirContext->AddrMap){
				ExFreePool(DirContext->AddrMap);
				DirContext->AddrMap = NULL;
			}

			if(DirContext->ChildBitMap){
				ExFreePool(DirContext->ChildBitMap);
				DirContext->ChildBitMap = NULL;
			}

			if(DirContext->ChildEntry){
				ExFreePool(DirContext->ChildEntry);
				DirContext->ChildEntry = NULL;
			}

			RC = STATUS_UNSUCCESSFUL;
			
		}
	}

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit XixFsLookupInitialDirEntry Status(0x%x).\n, RC"));

	return RC;
 
}






NTSTATUS
XixFsUpdateDirNames(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	Offset;
	uint16			Size = 0;
	uint32			BlockSize = 0;
	int64			NextRealIndex = 0;
	PXIDISK_CHILD_INFORMATION Child = NULL;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsUpdateDirNames .\n"));

	//
	//  Check input.
	//
	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}
	//
	//  Handle the case of the self directory entry.
	//

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir(%I64d) of FCB(%I64d)\n", 
					DirContext->VirtualDirIndex, DirContext->pFCB->LotNumber));

	if (DirContext->VirtualDirIndex == 0) {
		DirContext->ChildName.Length = XifsdUnicodeDirectoryNames[SELF_ENTRY].Length;
		DirContext->ChildName.MaximumLength = XifsdUnicodeDirectoryNames[SELF_ENTRY].MaximumLength;
		DirContext->ChildName.Buffer = XifsdUnicodeDirectoryNames[SELF_ENTRY].Buffer;
		
		DirContext->SearchedVirtualDirIndex = 0;
		DirContext->SearchedRealDirIndex = 0;
		DirContext->VirtualDirIndex = 1;
		DirContext->RealDirIndex = -1;
		DirContext->LotNumber = DirContext->pFCB->LotNumber;
		DirContext->FileType = DirContext->pFCB->FCBType;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir vitural Index(0) of FCB(%I64d)\n",DirContext->pFCB->LotNumber ));
		return STATUS_SUCCESS;
	}else if ( DirContext->VirtualDirIndex == 1){
		DirContext->ChildName.Length = XifsdUnicodeDirectoryNames[PARENT_ENTRY].Length;
		DirContext->ChildName.MaximumLength = XifsdUnicodeDirectoryNames[PARENT_ENTRY].MaximumLength;
		DirContext->ChildName.Buffer = XifsdUnicodeDirectoryNames[PARENT_ENTRY].Buffer;
		
		DirContext->SearchedVirtualDirIndex = 1;
		DirContext->SearchedRealDirIndex = 0;		
		DirContext->VirtualDirIndex = 2;
		DirContext->RealDirIndex = -1;
		DirContext->LotNumber = DirContext->pFCB->ParentLotNumber;
		DirContext->FileType = FCB_TYPE_DIR;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir vitural Index(1) of FCB(%I64d)\n",DirContext->pFCB->LotNumber));
		return STATUS_SUCCESS;
	}else {

		try{

			NextRealIndex = DirContext->RealDirIndex;
				
			RtlZeroMemory(DirContext->ChildEntry,XIDISK_CHILD_RECORD_SIZE );
			

			NextRealIndex = (uint64)XixFsfindSetBitFromMap(1024, NextRealIndex, DirContext->ChildBitMap);
			if(NextRealIndex == 1024){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Can't find Dir endtry\n"));
				RC =  STATUS_NO_MORE_FILES;
				try_return(RC);
			}
			
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
						("SearCh Dir RealIndex(%I64d) of FCB(%I64d)\n", 
						NextRealIndex, DirContext->pFCB->LotNumber));
			


			RC = XixFsRawReadDirEntry(
								DirContext->pVCB,
								DirContext->pFCB,
								(uint32)NextRealIndex,
								DirContext->ChildEntry,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail XixFsRawReadDirEntry\n"));
				try_return(RC);
			}


			Child = (PXIDISK_CHILD_INFORMATION)DirContext->ChildEntry;
			while(Child->State == XIFS_FD_STATE_DELETED){
				RtlZeroMemory(DirContext->ChildEntry,XIDISK_CHILD_RECORD_SIZE );
				
				NextRealIndex = (uint32)XixFsfindSetBitFromMap(1024, NextRealIndex, DirContext->ChildBitMap);
				if(NextRealIndex == 1024){					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Can't find Dir endtry\n"));
					RC =  STATUS_NO_MORE_FILES;
					try_return(RC);
				}
				
					
				RC = XixFsRawReadDirEntry(
									DirContext->pVCB,
									DirContext->pFCB,
									(uint32)NextRealIndex,
									DirContext->ChildEntry,
									XIDISK_CHILD_RECORD_SIZE,
									DirContext->AddrMap,
									DirContext->AddrMapSize,
									&(DirContext->AddrSecNumber),
									TRUE
									);


				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail XixFsRawReadDirEntry\n"));
					try_return(RC);
				}


				Child = (PXIDISK_CHILD_INFORMATION)DirContext->ChildEntry;
			}
			

			//Update Dir Context
			
			DirContext->SearchedVirtualDirIndex = NextRealIndex + 2;
			DirContext->SearchedRealDirIndex = NextRealIndex;
			DirContext->RealDirIndex = NextRealIndex;
			DirContext->VirtualDirIndex = NextRealIndex + 2;
			DirContext->LotNumber = Child->StartLotIndex;
			
			Size = (uint16)Child->NameSize;
			if(Size){
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}
			}
			DirContext->ChildName.Length = DirContext->ChildName.MaximumLength =  Size;
			DirContext->ChildName.Buffer = (PWSTR)Child->Name;

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
						("SearCh Dir RealIndex(%I64d) and Name(%wZ)  LotNumber(%I64d)\n", 
						DirContext->RealDirIndex, &DirContext->ChildName, DirContext->LotNumber));

			if(XifsdCheckFlagBoolean(Child->Type , XIFS_FD_TYPE_DIRECTORY)){
				DirContext->FileType = FCB_TYPE_DIR;
			}else{
				DirContext->FileType = FCB_TYPE_FILE;
			}

			RC = STATUS_SUCCESS;

		}finally{

		}

	}
		

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit XixFsUpdateDirNames Status (0x%x) .\n", RC));
	
    return RC;
}



NTSTATUS
XixFsFindDirEntryByLotNumber(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN uint64	LotNumber,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT uint64	*EntryIndex
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIDISK_CHILD_INFORMATION pChild = NULL;
	PUNICODE_STRING MatchName;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsFindDirEntryByLotNumber .\n"));

	//
	//  Check inputs.
	//

	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}
	ASSERT_FCB( Fcb );

	//
	//  Go get the first entry.
	//

	MatchName = &DirContext->ChildName;


	RC = XixFsLookupInitialDirEntry( 
								IrpContext,
								Fcb,
								DirContext,
								2 
								);



	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsLookupInitialDirEntry\n"));
		return RC;
	}


    //
    //  Now loop looking for a good match.
    //
    
    while(1) {

        RC = XixFsUpdateDirNames( IrpContext,
										DirContext);
            
        
		if(!NT_SUCCESS(RC)){
			break;
		}

    
		pChild = (PXIDISK_CHILD_INFORMATION)DirContext->ChildEntry;
        if (pChild->StartLotIndex == LotNumber){
			*EntryIndex = DirContext->SearchedRealDirIndex;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("Exit XixFsFindDirEntryByLotNumber .\n"));
			return STATUS_SUCCESS;
        }

    } 
    //
    //  No match was found.
    //

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Fail Exit XixFsFindDirEntryByLotNumber .\n"));

    return STATUS_UNSUCCESSFUL;
}



NTSTATUS
XixFsLookupInitialFileIndex(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_FCB pFCB,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN uint64 InitialIndex
)
{
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	uint64			SearchIndex = InitialIndex;
	LARGE_INTEGER	Offset;


	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsLookupInitialFileIndex .\n"));


	DirContext->VirtualDirIndex = InitialIndex;
	DirContext->RealDirIndex = -1;
	if(DirContext->VirtualDirIndex > 1)
	{
		DirContext->RealDirIndex = DirContext->VirtualDirIndex - 3;
	}

	
	RC = XixFsUpdateDirNames( pIrpContext,
										DirContext);


	if(!NT_SUCCESS(RC)){
		return RC;
	}

	if(DirContext->SearchedVirtualDirIndex != InitialIndex){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("SearCh Dir InitialIndex(%I64d) and SearchedDirIndex(%I64d)\n", 
					InitialIndex, &DirContext->SearchedVirtualDirIndex));
		return STATUS_UNSUCCESSFUL;;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsLookupInitialFileIndex .\n"));
	return STATUS_SUCCESS;
}



NTSTATUS
XixFsFindDirEntry (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN PUNICODE_STRING Name,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN				bIgnoreCase
    )
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PUNICODE_STRING MatchName;
	PAGED_CODE();

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsFindDirEntry .\n"));

	//
	//  Check inputs.
	//
	if(IrpContext){
		ASSERT_IRPCONTEXT( IrpContext );
		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));
	}
	
	ASSERT_FCB( Fcb );

	//
	//  Go get the first entry.
	//

	MatchName = &DirContext->ChildName;


	RC = XixFsLookupInitialDirEntry( 
							IrpContext,
							Fcb,
							DirContext,
							0 
							);


	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsLookupInitialDirEntry\n"));
		return RC;
	}



    //
    //  Now loop looking for a good match.
    //
    
    while(1) {

        RC = XixFsUpdateDirNames( IrpContext, DirContext);
            
        
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsUpdateDirNames RC(0x%x)\n", RC));
			break;
		}

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("CurrentFCB's Searched Name(%wZ): TargetName(%wZ).\n", MatchName, Name));
  
        //
        //  If this is a constant entry, just keep going.
        //

        
        if (DirContext->VirtualDirIndex < 2) {
            
            continue;
        }
		
		
		if(bIgnoreCase){
			
			if (RtlCompareUnicodeString(MatchName, Name, TRUE) == 0) {

				//
				//  Got a match, so give it up.
				// 
				DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("Exit XixFsFindDirEntry .\n"));
				
				return STATUS_SUCCESS;
			}

			

		}else{
		  if (RtlCompareUnicodeString( MatchName, Name, FALSE ) == 0) {

				//
				//  Got a match, so give it up.
				// 
				DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("Exit XixFsFindDirEntry .\n"));
				
				return STATUS_SUCCESS;
			}			
		}

    } 
    //
    //  No match was found.
    //

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Fail Exit XixFsFindDirEntry .\n"));



    return STATUS_UNSUCCESSFUL;
}



NTSTATUS
XixFsAddChildToDir(
	IN BOOLEAN					Wait,
	IN PXIFS_VCB				pVCB,
	IN PXIFS_FCB				pDir,
	IN uint64					ChildLotNumber,
	IN uint32					Type,
	IN PUNICODE_STRING 			ChildName,
	IN PXIFS_DIR_EMUL_CONTEXT	DirContext,
	OUT uint64 *				ChildIndex
)
{
	NTSTATUS					RC = STATUS_UNSUCCESSFUL;
	XIFS_IO_LOT_INFO 			AddressInfo;
	PXIDISK_DIR_HEADER_LOT		DirLotHeader = NULL;
	PXIDISK_DIR_INFO			DirInfo = NULL;
	LARGE_INTEGER 				Offset;
	uint32						blockSize;
	uint8 						*buffer = NULL;
	uint64						LotNumber = pDir->LotNumber;
	uint32						NumChild = 0;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	uint64						Index = 0;
	uint32						Reason = 0;

	PVOID						BitMapBCB = NULL;
	PVOID						BitMap;
	BOOLEAN						bSetLock = FALSE;


	PAGED_CODE();

	ASSERT(Wait == TRUE);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Enter XifsdAddChildToDir .\n"));


	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end

	
	if(pDir->HasLock != FCB_FILE_LOCK_HAS){

		RC = XixFsLotLock(Wait, pVCB, pVCB->TargetDeviceObject, pDir->LotNumber, &pDir->HasLock, TRUE, TRUE);
		if(!NT_SUCCESS(RC)){
			return STATUS_ACCESS_DENIED;
		}
		
		bSetLock = TRUE;
	
	}

	try{


		blockSize = XIDISK_DIR_HEADER_LOT_SIZE;
		
		buffer = ExAllocatePool(PagedPool, blockSize);
		
		
		if(!buffer) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR allocate buffer.\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}	
		

		RC = XixFsRawReadLotAndFileHeader(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						LotNumber,
						buffer,
						blockSize,
						pVCB->SectorSize
						);
						
		
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		
		DirLotHeader = (PXIDISK_DIR_HEADER_LOT)buffer;
		DirInfo = &(DirLotHeader->DirInfo);

			
		// Make Child Record Endry
		pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;
		if(!pChildRecord){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		Index = XixFsfindFreeBitFromMap(1024, 0, DirInfo->ChildMap);
		if(Index > 1024){
			RC = STATUS_TOO_MANY_LINKS;
			try_return(RC);
		}

		RtlZeroMemory(pChildRecord, XIDISK_CHILD_RECORD_SIZE);
		pChildRecord->Childsignature = pChildRecord->Childsignature2 = (uint32)(ULONG_PTR)pChildRecord;
		RtlCopyMemory(pChildRecord->HostSignature, pVCB->HostId, 16);
		pChildRecord->NameSize = ChildName->Length;
		RtlCopyMemory(pChildRecord->Name, ChildName->Buffer, ChildName->Length);
		pChildRecord->ChildIndex = (uint32)Index;
		pChildRecord->State = XIFS_FD_STATE_CREATE;
		pChildRecord->Type = Type;
		pChildRecord->StartLotIndex = ChildLotNumber;
		
		
		setBitToMap(Index, DirInfo->ChildMap);
		DirInfo->childCount++;

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
					("ChildMap Index(%ld).\n", Index));	


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
					("ChildMap Index(%ld).\n", Index));	
		
		RC = XixFsRawWriteDirEntry(
								pVCB,
								pDir,
								(uint32)Index,
								(uint8 *)pChildRecord,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}


		RC = XixFsRawWriteLotAndFileHeader(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						LotNumber,
						buffer,
						blockSize,
						pVCB->SectorSize
						);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
		
		pDir->ChildCount ++;
		*ChildIndex = Index;
		RC = STATUS_SUCCESS;


	}finally{
		
		if(bSetLock == TRUE){
			XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pDir->LotNumber, &pDir->HasLock);
		}

		
		if(buffer)ExFreePool(buffer);
	}	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit XixFsAddChildToDir Status(0x%x) .\n, RC"));

	return RC;				
}


NTSTATUS
XixFsDeleteChildFromDir(
	IN BOOLEAN				Wait,
	IN PXIFS_VCB 			pVCB,
	IN PXIFS_FCB			pDir,
	IN uint64				ChildIndex,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
)
{
	NTSTATUS				RC = STATUS_UNSUCCESSFUL;

	
	XIFS_IO_LOT_INFO 		AddressInfo;
	PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
	PXIDISK_DIR_INFO		DirInfo = NULL;
	LARGE_INTEGER 			Offset;
	uint32					blockSize;
	PCHAR					buffer;
	uint64					LotNumber = pDir->LotNumber;
	uint32					NumChild = 0;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	uint64					Index = ChildIndex;
	uint32					Reason = 0;
	BOOLEAN					bSetLock = FALSE;


	PAGED_CODE();

	ASSERT(Wait == TRUE);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsDeleteChildFromDir .\n"));

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end



	if(pDir->HasLock != FCB_FILE_LOCK_HAS){

		RC = XixFsLotLock(Wait, pVCB, pVCB->TargetDeviceObject, pDir->LotNumber, &pDir->HasLock, TRUE, TRUE);
		if(!NT_SUCCESS(RC)){
			return STATUS_ACCESS_DENIED;
		}
		
		bSetLock = TRUE;
	
	}

	try{
		blockSize = XIDISK_DIR_HEADER_LOT_SIZE;

		buffer = ExAllocatePool(NonPagedPool, blockSize);
		
		
		if(!buffer) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR allocate buffer.\n"));
			try_return(RC = STATUS_INSUFFICIENT_RESOURCES);
		}
		
		
		RC = XixFsRawReadLotAndFileHeader(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						LotNumber,
						buffer,
						blockSize,
						pVCB->SectorSize
						);
						
		
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
		

		DirLotHeader = (PXIDISK_DIR_HEADER_LOT)buffer;
		DirInfo = &(DirLotHeader->DirInfo);


		pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;

		RC = XixFsRawReadDirEntry(
								pVCB,
								pDir,
								(uint32)Index,
								(uint8 *)pChildRecord,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}



		// Make Child Record Endry
		
		pChildRecord->State = XIFS_FD_STATE_DELETED;

		RC = XixFsRawWriteDirEntry(
								pVCB,
								pDir,
								(uint32)Index,
								(uint8 *)pChildRecord,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		
		clearBitToMap(Index, DirInfo->ChildMap);
		DirInfo->childCount--;
		pDir->ChildCount--;
		

		RC = XixFsRawWriteLotAndFileHeader(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						LotNumber,
						buffer,
						blockSize,
						pVCB->SectorSize
						);
						
		
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}


		RC = STATUS_SUCCESS;
	}finally{
		if(bSetLock == TRUE){
			XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pDir->LotNumber, &pDir->HasLock);
		}

		if(buffer)ExFreePool(buffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsDeleteChildFromDir Status(0x%x).\n", RC));
	return RC;	
				
}



NTSTATUS
XixFsUpdateChildFromDir(
	IN BOOLEAN			Wait,
	IN PXIFS_VCB		pVCB,
	IN PXIFS_FCB		pDir,
	IN uint64			ChildLotNumber,
	IN uint32			Type,
	IN uint32			State,
	IN PUNICODE_STRING	ChildName,
	IN uint64			ChildIndex,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
)
{
	NTSTATUS				RC = STATUS_UNSUCCESSFUL;
	uint8					*NameBuffer = NULL;
	LARGE_INTEGER 			Offset;
	uint32					blockSize;
	uint64					LotNumber = pDir->LotNumber;
	uint32					NumChild = 0;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	uint32					Reason = 0;
	BOOLEAN					bSetLock = FALSE;

	ASSERT(Wait);

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsUpdateChildFromDir .\n"));
	

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end

	if(pDir->HasLock != FCB_FILE_LOCK_HAS){

		RC = XixFsLotLock(Wait, pVCB, pVCB->TargetDeviceObject, pDir->LotNumber, &pDir->HasLock, TRUE, TRUE);
		if(!NT_SUCCESS(RC)){
			return STATUS_ACCESS_DENIED;
		}
		
		bSetLock = TRUE;
	
	}

	try{
		pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;

		RC = XixFsRawReadDirEntry(
								pVCB,
								pDir,
								(uint32)ChildIndex,
								(uint8 *)pChildRecord,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		if(pChildRecord->State = XIFS_FD_STATE_DELETED){
			RC = STATUS_FILE_DELETED;
			try_return(RC);
		}

		pChildRecord->Childsignature = pChildRecord->Childsignature2 = (uint32)(ULONG_PTR)pChildRecord;
		pChildRecord->State = State;
		pChildRecord->Type = Type;
		pChildRecord->StartLotIndex = ChildLotNumber;
		pChildRecord->ChildIndex = (uint32)ChildIndex;
		if(ChildName != NULL){
			pChildRecord->NameSize = ChildName->Length;
			RtlZeroMemory(pChildRecord->Name, 2000);
			RtlCopyMemory(pChildRecord->Name, ChildName->Buffer, pChildRecord->NameSize);
		}


		RC = XixFsRawWriteDirEntry(
								pVCB,
								pDir,
								(uint32)ChildIndex,
								(uint8 *)pChildRecord,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMap,
								DirContext->AddrMapSize,
								&(DirContext->AddrSecNumber),
								TRUE
								);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		RC = STATUS_SUCCESS;

	}finally{
		if(bSetLock == TRUE){
			XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pDir->LotNumber , &pDir->HasLock);
			
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit XixFsUpdateChildFromDir Status(0x%x) .\n, RC"));
	return RC;	
}
