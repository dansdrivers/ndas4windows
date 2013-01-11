#include "xixfs_types.h"

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xcsystem/winnt/xcsysdep.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/bitmap.h"

#include "xixfs_drv.h"
#include "xixfs_internal.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_GetMoreCheckOutLotMap)
#pragma alloc_text(PAGE, xixfs_UpdateMetaData)
#pragma alloc_text(PAGE, xixfs_MetaUpdateFunction)
#endif




NTSTATUS
xixfs_GetMoreCheckOutLotMap(
	IN PXIXFS_VCB VCB
)
{
	NTSTATUS						RC = STATUS_SUCCESS;
	PXIXCORE_BITMAP_EMUL_CONTEXT	pDiskBitmapEmulCtx= NULL;
	PXIXCORE_LOT_MAP 				pTempFreeLotMap = NULL;
	uint32							size = 0;
	uint32							trycount = 0;
	LARGE_INTEGER					timeout;
	PXIXCORE_META_CTX				xixcoreCtx = NULL;

	
	
	ASSERT(VCB);
	xixcoreCtx = &VCB->XixcoreVcb.MetaContext;



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XifsdGetMoreCheckOutLotMap.\n"));

	pDiskBitmapEmulCtx = (PXIXCORE_BITMAP_EMUL_CONTEXT) xixcore_AllocateMem(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), 0, XCTAG_BITMAPEMUL);
	
	if(NULL == pDiskBitmapEmulCtx){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pTempFreeLotMap = (PXIXCORE_LOT_MAP)xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP), 0,  TAG_BUFFER);
	if(NULL == pTempFreeLotMap){
		xixcore_FreeMem(pDiskBitmapEmulCtx, XCTAG_BITMAPEMUL);
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	
retry:
	//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
	RC = xixcore_LotLock(
		&VCB->XixcoreVcb,
		xixcoreCtx->HostRegLotMapIndex,
		&xixcoreCtx->HostRegLotMapLockStatus,
		1,
		1
		);
		


	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLotLock .\n", RC));
		
		if(trycount > 3){
			xixcore_FreeMem(pDiskBitmapEmulCtx, XCTAG_BITMAPEMUL);
			xixcore_FreeMem(pTempFreeLotMap, TAG_BUFFER);
			return RC;
		}

		trycount++;
		goto retry;
		
	}


	try{

		// Zero Bit map context;
		RtlZeroMemory(pDiskBitmapEmulCtx, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));


		// Read Disk Bitmap information
		RC = xixcore_InitializeBitmapContext(pDiskBitmapEmulCtx,
										&VCB->XixcoreVcb,
										xixcoreCtx->HostCheckOutLotMapIndex,
										xixcoreCtx->FreeLotMapIndex,
										xixcoreCtx->CheckOutLotMapIndex
										);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}
		

		size = SECTOR_ALIGNED_SIZE(VCB->XixcoreVcb.SectorSizeBit, (uint32) ((VCB->XixcoreVcb.NumLots + 7)/8));


		pTempFreeLotMap->Data = xixcore_AllocateBuffer(size);

		if(!pTempFreeLotMap->Data){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Allocate TempFreeLotMap .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		RtlZeroMemory(xixcore_GetDataBuffer(pTempFreeLotMap->Data), size);

		// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
		
		RC = xixcore_ReadDirmapFromBitmapContext(pDiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
	
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UsedBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Host CheckOut BitMap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		RC = xixcore_ReadFreemapFromBitmapContext(pDiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UnusedBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		RC = xixcore_ReadCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk CheckOut Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}	
		

		// Get Real FreeMap without CheckOut
		xixcore_XORMap(&(pDiskBitmapEmulCtx->UnusedBitmap), &(pDiskBitmapEmulCtx->CheckOutBitmap));
			
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UnusedBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Real free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		


		RC = xixcore_SetCheckOutLotMap((PXIXCORE_LOT_MAP)&pDiskBitmapEmulCtx->UnusedBitmap, pTempFreeLotMap, xixcoreCtx->HostRecordIndex + 1);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  SetCheckOutLotMap Status (0x%x) .\n", RC));
			try_return(RC);
		}
		
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pTempFreeLotMap->Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Allocated Bit Map\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		





		//Update Host CheckOutLotMap
		xixcore_ORMap(&(pDiskBitmapEmulCtx->UsedBitmap), pTempFreeLotMap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UsedBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Host Checkout Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(xixcoreCtx->HostFreeLotMap->Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Before Allocate Host free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		//Update Host FreeLotMap
		xixcore_ORMap(xixcoreCtx->HostFreeLotMap, pTempFreeLotMap);


		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(xixcoreCtx->HostFreeLotMap->Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Host free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		
		

		//Update Disk CheckOut BitMap
		xixcore_ORMap(&(pDiskBitmapEmulCtx->CheckOutBitmap), pTempFreeLotMap);	
		
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Disk CheckOut Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		
		

		//	Added by ILGU HONG	2006 06 12
		xixcoreCtx->VolumeFreeMap->BitMapBytes = pDiskBitmapEmulCtx->UnusedBitmap.BitMapBytes;
		xixcoreCtx->VolumeFreeMap->MapType = pDiskBitmapEmulCtx->UnusedBitmap.MapType;
		xixcoreCtx->VolumeFreeMap->NumLots = pDiskBitmapEmulCtx->UnusedBitmap.NumLots;

		RtlCopyMemory(xixcore_GetDataBuffer(xixcoreCtx->VolumeFreeMap->Data),
						xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UnusedBitmap.Data), 
						xixcoreCtx->VolumeFreeMap->BitMapBytes
					);
		//	Added by ILGU HONG	2006 06 12 End




		// Update Disk Information

		RC = xixcore_WriteCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}



		// Update Host Record Information
		RC = xixcore_WriteDirmapFromBitmapContext(pDiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = xixcore_WriteBitMapWithBuffer(
				&VCB->XixcoreVcb,
				xixcoreCtx->HostUsedLotMapIndex, 
				xixcoreCtx->HostDirtyLotMap, 
				pDiskBitmapEmulCtx->BitmapLotHeader, 
				0
				);

		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XifsdWriteBitMap Host Dirty map .\n", RC));
			try_return(RC);
		}


		RC = xixcore_WriteBitMapWithBuffer(
				&VCB->XixcoreVcb,
				xixcoreCtx->HostUnUsedLotMapIndex, 
				xixcoreCtx->HostFreeLotMap, 
				pDiskBitmapEmulCtx->BitmapLotHeader, 
				0
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XifsdWriteBitMap  Host Free map.\n", RC));
			try_return(RC);
		}

	}finally{
		xixcore_LotUnLock(
			&VCB->XixcoreVcb,
			xixcoreCtx->HostRegLotMapIndex,
			&(xixcoreCtx->HostRegLotMapLockStatus)
			);	

		xixcore_CleanupBitmapContext(pDiskBitmapEmulCtx);
		xixcore_FreeMem(pDiskBitmapEmulCtx, XCTAG_BITMAPEMUL);

		if(pTempFreeLotMap->Data){
			xixcore_FreeBuffer(pTempFreeLotMap->Data);
		}
		xixcore_FreeMem(pTempFreeLotMap, TAG_BUFFER);

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdGetMoreCheckOutLotMap Status (0x%x).\n", RC));

	return RC;
}



NTSTATUS
xixfs_UpdateMetaData(
	IN PXIXFS_VCB VCB
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIXCORE_META_CTX		xixcoreCtx = NULL;
	PXIXCORE_BUFFER			tmpBuf = NULL;


	ASSERT(VCB);
	xixcoreCtx = &VCB->XixcoreVcb.MetaContext;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdUpdateMetaData .\n"));

	tmpBuf = xixcore_AllocateBuffer(XIDISK_MAP_LOT_SIZE);
	if(!tmpBuf){
		RC = STATUS_INSUFFICIENT_RESOURCES;
		return RC;	
	}
	

	RtlZeroMemory(xixcore_GetDataBuffer(tmpBuf), XIDISK_MAP_LOT_SIZE);
	
	try{

		RC = xixcore_WriteBitMapWithBuffer(&VCB->XixcoreVcb, xixcoreCtx->HostUsedLotMapIndex, xixcoreCtx->HostDirtyLotMap, tmpBuf, 0);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XixFsWriteBitMapWithBuffer Host Dirty map .\n", RC));
			try_return(RC);
		}


		RtlZeroMemory(xixcore_GetDataBuffer(tmpBuf), XIDISK_MAP_LOT_SIZE);

		RC = xixcore_WriteBitMapWithBuffer(&VCB->XixcoreVcb, xixcoreCtx->HostUnUsedLotMapIndex, xixcoreCtx->HostFreeLotMap, tmpBuf, 0);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XixFsWriteBitMapWithBuffer  Host Free map.\n", RC));
			try_return(RC);
		}

	}finally{

		xixcore_FreeBuffer(tmpBuf);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit Status(0x%x) XixFsdUpdateMetaData .\n", RC));

	return RC;

}


VOID
xixfs_MetaUpdateFunction(
		PVOID	lpParameter
)
{
	PXIXFS_VCB pVCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	PKEVENT				Evts[3];
	LARGE_INTEGER		TimeOut;
	XIXCORE_IRQL	oldIrql;
	PXIXCORE_META_CTX				pXixcoreCtx = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_MetaUpdateFunction .\n"));


	pVCB = (PXIXFS_VCB)lpParameter;
	ASSERT_VCB(pVCB);
	pXixcoreCtx = &(pVCB->XixcoreVcb.MetaContext);

	Evts[0] = &pVCB->VCBUmountEvent;
	Evts[1] = &pVCB->VCBGetMoreLotEvent;
	Evts[2] = &pVCB->VCBUpdateEvent;
	

	while(1){
		TimeOut.QuadPart = - DEFAULT_XIFS_UPDATEWAIT;
		RC = KeWaitForMultipleObjects(
				3,
				Evts,
				WaitAny,
				Executive,
				KernelMode,
				TRUE,
				&TimeOut,
				NULL
			);


		if(RC == 0){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Stop VCB xixfs_MetaUpdateFunction .\n"));
			KeSetEvent(&pVCB->VCBStopOkEvent, 0, FALSE);
			break;
		}else if ( RC == 1) {
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call XixFsdGetMoreCheckOutLotMap .\n"));
			
			KeClearEvent(&pVCB->VCBGetMoreLotEvent);
			
			RC = xixfs_GetMoreCheckOutLotMap(pVCB);
			
			if(!NT_SUCCESS(RC)){
				xixcore_AcquireSpinLock(pXixcoreCtx->MetaLock, &oldIrql);	
				XIXCORE_CLEAR_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES);
				XIXCORE_SET_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_INSUFFICIENT_RESOURCES);
				xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
			}else{
		
				xixcore_AcquireSpinLock(pXixcoreCtx->MetaLock, &oldIrql);	
				XIXCORE_CLEAR_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES);
				xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
			}

			KeSetEvent(&pVCB->ResourceEvent, 0, FALSE);


		}else if ((RC == 2) || (RC == STATUS_TIMEOUT)){

			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call xixfs_MetaUpdateFunction .\n"));

			KeClearEvent(&pVCB->VCBUpdateEvent);
			
			xixcore_AcquireSpinLock(pXixcoreCtx->MetaLock, &oldIrql);	
			if(XIXCORE_TEST_FLAGS(pXixcoreCtx->ResourceFlag,XIXCORE_META_RESOURCE_NEED_UPDATE)){
				XIXCORE_CLEAR_FLAGS(pXixcoreCtx->ResourceFlag,XIXCORE_META_RESOURCE_NEED_UPDATE);
				xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
				RC = xixfs_UpdateMetaData(pVCB);
				
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail xixfs_MetaUpdateFunction  Update MetaData"));			
				}
			}else {
				xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
			}
		
		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Error xixfs_MetaUpdateFunction Unsupported State"));			
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_MetaUpdateFunction .\n"));

	return;
}