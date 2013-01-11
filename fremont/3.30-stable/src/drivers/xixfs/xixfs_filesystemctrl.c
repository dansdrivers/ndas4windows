#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"
#include "xixcore/callback.h"
#include "xixcore/ondisk.h"
#include "xcsystem/errinfo.h"


VOID
xixfs_ScanForDismount(
	IN PXIXFS_IRPCONTEXT IrpContext
);

BOOLEAN
xixfs_ScanForPreMountedObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PDEVICE_OBJECT DeviceObject
);

BOOLEAN
xixfs_ScanForPreMountedVCB(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN uint8			*VolumeId
);


NTSTATUS
xixfs_MountVolume(
	IN PXIXFS_IRPCONTEXT IrpContext
	);

NTSTATUS
xixfs_VerifyVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
xixfs_OplockRequest(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);





NTSTATUS
xixfs_LockVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
xixfs_UnlockVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
xixfs_DismountVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);


NTSTATUS
xixfs_IsThisVolumeMounted(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
xixfs_IsPathnameValid(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
xixfs_AllowExtendedDasdIo(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
xixfs_InvalidateVolumes(
	IN PXIXFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
	);


NTSTATUS
xixfs_UserFsctl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_DismountVCB)
#pragma alloc_text(PAGE, xixfs_ScanForDismount)
#pragma alloc_text(PAGE, xixfs_ScanForPreMountedObject)
#pragma alloc_text(PAGE, xixfs_MountVolume)
#pragma alloc_text(PAGE, xixfs_VerifyVolume)
#pragma alloc_text(PAGE, xixfs_OplockRequest)
#pragma alloc_text(PAGE, xixfs_DismountVolume)
#pragma alloc_text(PAGE, xixfs_IsThisVolumeMounted)
#pragma alloc_text(PAGE, xixfs_IsPathnameValid)
#pragma alloc_text(PAGE, xixfs_AllowExtendedDasdIo)
#pragma alloc_text(PAGE, xixfs_InvalidateVolumes)
#pragma alloc_text(PAGE, xixfs_UserFsctl)
#pragma alloc_text(PAGE, xixfs_CheckForDismount)
#pragma alloc_text(PAGE, xixfs_CommonFileSystemControl)
#pragma alloc_text(PAGE, xixfs_CommonShutdown) 
#pragma alloc_text(PAGE, xixfs_PurgeVolume)
#pragma alloc_text(PAGE, xixfs_CleanupFlushVCB)
#endif
//#pragma alloc_text(PAGE, xixfs_LockVolume)
//#pragma alloc_text(PAGE, xixfs_UnlockVolume)







BOOLEAN
xixfs_DismountVCB(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PXIXFS_VCB		pVCB
)
{
	KIRQL			SavedIrql;
	uint32			i = 0;
	BOOLEAN			bVCBPresent = TRUE;
	BOOLEAN			bFinalReference = FALSE;
	PVPB			OldVpb= NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_DismountVCB \n"));


	ASSERT_VCB(pVCB);
	ASSERT_EXCLUSIVE_XIFS_GDATA;
    ASSERT_EXCLUSIVE_VCB( pVCB );



	XifsdLockVcb( pIrpContext, pVCB );

	ASSERT(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS);

	pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS;
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("xixfs_DismountVCB Status(%ld).\n", pVCB->VCBState));

	XifsdUnlockVcb(pIrpContext, pVCB );

	OldVpb = pVCB->PtrVPB;

	XifsdLockVcb(pIrpContext, pVCB );
	
	pVCB->VCBReference -= 1;

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("Current VCB %d/%d  Current VCB->PtrVPB->ReferenceCount %d \n", 
						 pVCB->VCBReference, pVCB->VCBUserReference, pVCB->PtrVPB->ReferenceCount ));



	IoAcquireVpbSpinLock( &SavedIrql );
	bFinalReference= (BOOLEAN) ((pVCB->VCBReference == 0) &&
							(OldVpb->ReferenceCount == 1));
	


	if(OldVpb->RealDevice->Vpb == OldVpb){
		if(!bFinalReference){

			ASSERT(pVCB->SwapVpb != NULL);
			pVCB->SwapVpb->Type = IO_TYPE_VPB;
			pVCB->SwapVpb->Size = sizeof(VPB);
			pVCB->SwapVpb->RealDevice = OldVpb->RealDevice;
			pVCB->SwapVpb->RealDevice->Vpb = pVCB->SwapVpb;

			pVCB->SwapVpb->Flags = XIXCORE_MASK_FLAGS(OldVpb->Flags, VPB_REMOVE_PENDING);
			
			IoReleaseVpbSpinLock( SavedIrql );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("pVCB->SwapVpb->RealDevice %p \n", pVCB->SwapVpb->RealDevice));

			pVCB->SwapVpb = NULL;
			XifsdUnlockVcb( pIrpContext, pVCB );



		}else{
			
			OldVpb->ReferenceCount -= 1;
			OldVpb->DeviceObject = NULL;
			XIXCORE_CLEAR_FLAGS( pVCB->PtrVPB->Flags, VPB_MOUNTED );
			XIXCORE_CLEAR_FLAGS( pVCB->PtrVPB->Flags, VPB_LOCKED);

		
			pVCB->PtrVPB = NULL;
			IoReleaseVpbSpinLock( SavedIrql );
			XifsdUnlockVcb( pIrpContext, pVCB );
					
			xixfs_DeleteVCB( pIrpContext, pVCB );
			bVCBPresent = FALSE;
		}

	}else if(bFinalReference){
		OldVpb->ReferenceCount -=1;
		IoReleaseVpbSpinLock( SavedIrql );
		XifsdUnlockVcb( pIrpContext, pVCB );
		
		
		
		xixfs_DeleteVCB( pIrpContext, pVCB );
		bVCBPresent = FALSE;
	}else{
		IoReleaseVpbSpinLock( SavedIrql );
		XifsdUnlockVcb( pIrpContext, pVCB );
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_DismountVCB \n"));
	return bVCBPresent;
}


BOOLEAN
xixfs_CheckForDismount (
    IN PXIXFS_IRPCONTEXT pIrpContext,
    IN PXIXFS_VCB pVCB,
    IN BOOLEAN Force
    )
{
	BOOLEAN UnlockVcb = TRUE;
	BOOLEAN VcbPresent = TRUE;
	BOOLEAN	CanWait = FALSE;
	KIRQL SavedIrql;

	//PAGED_CODE();

	DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
						 ("Enter xixfs_CheckForDismount pVCB(%p)\n", pVCB));


	ASSERT_IRPCONTEXT( pIrpContext );
	ASSERT_VCB( pVCB );

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	ASSERT_EXCLUSIVE_VCB(pVCB);

	ASSERT(XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

	//
	//  Acquire and lock this Vcb to check the dismount state.
	//

	

	//XifsdRealCloseFCB(pVCB);

	XifsdLockVcb( pIrpContext, pVCB );

	//
	//  If the dismount is not already underway then check if the
	//  user reference count has gone to zero or we are being forced
	//  to disconnect.  If so start the teardown on the Vcb.
	//

	DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB| DEBUG_TARGET_REFCOUNT), 
						 ("xixfs_CheckForDismount VCB %d/%d \n", pVCB->VCBReference, pVCB->VCBUserReference ));


	if (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS) {

		if (pVCB->VCBUserReference <= pVCB->VCBResidualUserReference || Force) {

			XifsdUnlockVcb( pIrpContext, pVCB );
			UnlockVcb = FALSE;
			
			DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB),
						 ("call xixfs_CheckForDismount\n"));

			VcbPresent = xixfs_DismountVCB( pIrpContext, pVCB );
		}

	//
	//  If the teardown is underway and there are absolutely no references
	//  remaining then delete the Vcb.  References here include the
	//  references in the Vcb and Vpb.
	//

	} else if (pVCB->VCBReference == 0) {

		IoAcquireVpbSpinLock( &SavedIrql );

		//
		//  If there are no file objects and no reference counts in the
		//  Vpb we can delete the Vcb.  Don't forget that we have the
		//  last reference in the Vpb.
		//

		if (pVCB->PtrVPB->ReferenceCount == 1) {
			IoReleaseVpbSpinLock( SavedIrql );
			XifsdUnlockVcb( pIrpContext, pVCB );
			UnlockVcb = FALSE;
			DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB| DEBUG_TARGET_REFCOUNT), 
						 ("XifsdCheckForDismount call DeleteVCB \n"));		
			xixfs_DeleteVCB( pIrpContext, pVCB );
			VcbPresent = FALSE;

		} else {

			IoReleaseVpbSpinLock( SavedIrql );
		}
	}

	//
	//  Unlock the Vcb if still held.
	//
	if (UnlockVcb) {

		XifsdUnlockVcb( pIrpContext, pVCB );
	}

	DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
						 ("Exit xixfs_CheckForDismount\n"));
	return VcbPresent;
}


VOID
xixfs_ScanForDismount(
	IN PXIXFS_IRPCONTEXT IrpContext)
{
	PXIXFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_ScanForDismount\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIXFS_VCB)CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS) ||
			(pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_INVALID))
		{

			XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			if(xixfs_CheckForDismount(IrpContext, pVCB, FALSE)){
				XifsdReleaseVcb(TRUE,pVCB);
			}
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_ScanForDismount\n"));
	
}


BOOLEAN
xixfs_IsThereMountedVolume()
{
	ASSERT_EXCLUSIVE_XIFS_GDATA;
	if(IsListEmpty(&XiGlobalData.XifsVDOList)){
		return TRUE;
	}else{
		return FALSE;
	}
}


BOOLEAN
xixfs_ScanForPreMountedObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PDEVICE_OBJECT DeviceObject
)
{
	PXIXFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_ScanForPreMountedObject\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIXFS_VCB)CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) 
			&& (pVCB->TargetDeviceObject == DeviceObject))
		{
			return FALSE;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_ScanForPreMountedObject\n"));	
	return TRUE;
}



BOOLEAN
xixfs_ScanForPreMountedVCB(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN uint8			*VolumeId
)
{
	PXIXFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_ScanForPreMountedVCB\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIXFS_VCB)CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;


		// Changed by ILGU HONG

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) 
			&& (RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, VolumeId, 16) == 16)
		)
		{
			return FALSE;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_ScanForPreMountedVCB\n"));	
	return TRUE;
}




NTSTATUS
xixfs_CheckBootSector(
	IN PDEVICE_OBJECT	TargetDevice,
	IN uint32			SectorSize,
	IN uint32			SectorSizeBit,
	OUT uint64			*VolumeLotIndex,
	OUT	uint32			*LotSize,
	OUT uint8			VolumeId[16]
)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	PXIXCORE_BUFFER				BootInfo	= NULL;
	PXIXCORE_BUFFER				LotHeader	= NULL;
	PXIXCORE_BUFFER				VolumeInfo	= NULL;
	LARGE_INTEGER				offset;
	PXIDISK_VOLUME_INFO 		pVolumeHeader = NULL;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	PPACKED_BOOT_SECTOR			pBootSector = NULL;
	
	uint64						vIndex = 0;
	uint32						reason = 0;

	PAGED_CODE();

	ASSERT(TargetDevice);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_CheckBootSector .\n"));

	VolumeInfo = xixcore_AllocateBuffer(XIDISK_DUP_VOLUME_INFO_SIZE);

	if(!VolumeInfo){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	BootInfo = xixcore_AllocateBuffer(XIDISK_DUP_VOLUME_INFO_SIZE);

	if(!BootInfo){
		xixcore_FreeBuffer(VolumeInfo);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	

	LotHeader = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(!LotHeader){
		xixcore_FreeBuffer(VolumeInfo);
		xixcore_FreeBuffer(BootInfo);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	RtlZeroMemory(xixcore_GetDataBuffer(VolumeInfo), XIDISK_DUP_VOLUME_INFO_SIZE);
	RtlZeroMemory(xixcore_GetDataBuffer(BootInfo), XIDISK_DUP_VOLUME_INFO_SIZE);
	RtlZeroMemory(xixcore_GetDataBuffer(LotHeader), XIDISK_DUP_COMMON_LOT_HEADER_SIZE);



	try{
		RC = xixcore_RawReadBootSector(
						(PXIXCORE_BLOCK_DEVICE)TargetDevice,
						SectorSize,
						SectorSizeBit,
						0,
						BootInfo,
						&reason
						);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  xixfs_CheckBootSector:XixFsRawReadBootSector .\n"));
			if( RC != XCCODE_CRCERROR ){
				try_return(RC);
			}

			
		}

		
		pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(BootInfo);
		
		if( (pBootSector->Oem[0] != 'X' ) 
			|| (pBootSector->Oem[1] != 'I' ) 
			|| (pBootSector->Oem[2] != 'F' ) 
			|| (pBootSector->Oem[3] != 'S' )
			|| (pBootSector->Oem[4] != 0 )
			|| (pBootSector->Oem[5] != 0 )
			|| (pBootSector->Oem[6] != 0 )
			|| (pBootSector->Oem[7] != 0 )
			)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fianl Fail  xixfs_CheckBootSector:XixFsRawReadBootSector .\n"));

			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);

		}else{
			
			if( RC == XCCODE_CRCERROR){
				xixcore_ZeroBufferOffset(BootInfo);
				RtlZeroMemory(xixcore_GetDataBuffer(BootInfo), XIDISK_DUP_VOLUME_INFO_SIZE);
				// try sector*128
				RC = xixcore_RawReadBootSector(
								(PXIXCORE_BLOCK_DEVICE)TargetDevice,
								SectorSize,
								SectorSizeBit,
								127,
								BootInfo,
								&reason
								);
				
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail  xixfs_CheckBootSector:XixFsRawReadBootSector .\n"));
					RC = STATUS_UNRECOGNIZED_VOLUME;
					try_return(RC);
				}
			}

			pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(BootInfo);

			if( (pBootSector->Oem[0] != 'X' ) 
				|| (pBootSector->Oem[1] != 'I' ) 
				|| (pBootSector->Oem[2] != 'F' ) 
				|| (pBootSector->Oem[3] != 'S' )
				|| (pBootSector->Oem[4] != 0 )
				|| (pBootSector->Oem[5] != 0 )
				|| (pBootSector->Oem[6] != 0 )
				|| (pBootSector->Oem[7] != 0 )
			)
			{
				RC = STATUS_UNRECOGNIZED_VOLUME;
				try_return(RC);
			}

		}		

		RC = xixcore_checkVolume(
				(PXIXCORE_BLOCK_DEVICE)TargetDevice,
				SectorSize,
				SectorSizeBit,
				pBootSector->LotSize,
				(xc_sector_t)pBootSector->FirstVolumeIndex,
				VolumeInfo,
				LotHeader,
				VolumeId
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  xixfs_CheckBootSector:xixcore_checkVolume .\n"));
			
			xixcore_ZeroBufferOffset(VolumeInfo);
			RtlZeroMemory(xixcore_GetDataBuffer(VolumeInfo), XIDISK_DUP_VOLUME_INFO_SIZE);
			xixcore_ZeroBufferOffset(LotHeader);
			RtlZeroMemory(xixcore_GetDataBuffer(LotHeader), XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
		
			RC = xixcore_checkVolume(
				(PXIXCORE_BLOCK_DEVICE)TargetDevice,
				SectorSize,
				SectorSizeBit,
				pBootSector->LotSize,
				(xc_sector_t)pBootSector->SecondVolumeIndex,
				VolumeInfo,
				LotHeader,
				VolumeId
				);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail  xixfs_CheckBootSector:xixcore_checkVolume .\n"));
			
				try_return(RC);
			}else{
				vIndex = pBootSector->SecondVolumeIndex;
			}
		}else{
			vIndex = pBootSector->FirstVolumeIndex;
		}

		pVolumeHeader = (PXIDISK_VOLUME_INFO)xixcore_GetDataBufferWithOffset(VolumeInfo);

		if((pBootSector->VolumeSignature != pVolumeHeader->VolumeSignature)
			|| (pBootSector->NumLots != pVolumeHeader->NumLots)
			|| (pBootSector->LotSignature != pVolumeHeader->LotSignature)
			|| (pBootSector->XifsVesion != pVolumeHeader->XifsVesion)
			|| (pBootSector->LotSize != pVolumeHeader->LotSize)
		)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x)  Is XIFS_CURRENT_ Volsignature, Lotnumber, Lotsignature, XifsVersion .\n",
				pBootSector->VolumeSignature ));
			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);
		}

		RC = STATUS_SUCCESS;
		*VolumeLotIndex = vIndex; 
		*LotSize = pVolumeHeader->LotSize;

		;
	}finally{
		xixcore_FreeBuffer(VolumeInfo);
		xixcore_FreeBuffer(BootInfo);
		xixcore_FreeBuffer(LotHeader);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(0x%x) xixfs_CheckBootSector .\n", RC));
	return RC;
}


/*
NTSTATUS
xixfs_GetSuperBlockInformation(
	IN PXIXFS_VCB VCB
	)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIXCORE_BUFFER		buffer = NULL;
	LARGE_INTEGER		offset;
	PDEVICE_OBJECT 		TargetDevice = NULL;
	PXIDISK_VOLUME_LOT 	VolumeLot = NULL;
	PXDISK_VOLUME_INFO	VolInfo = NULL;
	uint32									reason = 0;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_GetSuperBlockInformation .\n"));

	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);

	



	
	buffer = xixcore_AllocateBuffer(XIDISK_VOLUME_LOT_SIZE);
	
	if(!buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(buffer), XIDISK_VOLUME_LOT_SIZE);


	try{
		RC = xixcore_RawReadVolumeHeader(
						(PXIXCORE_BLOCK_DEVICE)TargetDevice,
						VCB->XixcoreVcb.SectorSize,
						VCB->XixcoreVcb.SectorSizeBit,
						buffer,
						&reason
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  xixfs_GetSuperBlockInformation .\n"));
			try_return(RC);
		}

		VolumeLot = (PXIDISK_VOLUME_LOT)xixcore_GetDataBuffer(buffer);

		VolInfo = &(VolumeLot->VolInfo);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VolInfo HostRegLotMap %I64d:RootLotMap %I64d: LotSize: %ld : TotalLotNumber %I64d .\n",
				VolInfo->HostRegLotMapIndex, VolInfo->RootDirectoryLotIndex, VolInfo->LotSize, VolInfo->NumLots));

		VCB->XixcoreVcb.MetaContext.HostRegLotMapIndex = VolInfo->HostRegLotMapIndex;
		VCB->XixcoreVcb.RootDirectoryLotIndex = VolInfo->RootDirectoryLotIndex;
		VCB->XixcoreVcb.NumLots = VolInfo->NumLots;
		VCB->XixcoreVcb.LotSize = VolInfo->LotSize;
	
		VCB->XixcoreVcb.VolumeLotSignature = VolInfo->LotSignature;

		// Changed by ILGU HONG
		VCB->XixcoreVcb.VolCreateTime = VolInfo->VolCreationTime;
		VCB->XixcoreVcb.VolSerialNumber = VolInfo->VolSerialNumber;
		VCB->XixcoreVcb.VolumeNameLength = (uint16)VolInfo->VolLabelLength;
		
		if(VCB->XixcoreVcb.VolumeNameLength != 0){
			VCB->XixcoreVcb.VolumeName =  ExAllocatePoolWithTag(NonPagedPool, SECTORALIGNSIZE_512(VolInfo->VolLabelLength), XCTAG_VOLNAME);
			if(!VCB->XixcoreVcb.VolumeName){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}
			RtlCopyMemory(VCB->XixcoreVcb.VolumeName, VolInfo->VolLabel, VCB->XixcoreVcb.VolumeNameLength);
		}


			
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VCB HostRegLotMap %I64d:RootLotMap %I64d: LotSize: %ld : TotalLotNumber %I64d .\n",
			VCB->XixcoreVcb.MetaContext.HostRegLotMapIndex, VCB->XixcoreVcb.RootDirectoryLotIndex, VCB->XixcoreVcb.LotSize, VCB->XixcoreVcb.NumLots));

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VCB Volume Signature (0x%x).\n",
			VCB->XixcoreVcb.VolumeLotSignature));


		;
	}finally{
		xixcore_FreeBuffer(buffer);
		if(!NT_SUCCESS(RC)){
			if(VCB->XixcoreVcb.VolumeName) {
				ExFreePoolWithTag(VCB->XixcoreVcb.VolumeName, XCTAG_VOLNAME);
				VCB->XixcoreVcb.VolumeNameLength = 0;
			}
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statue(0x%x) xixfs_GetSuperBlockInformation .\n", RC));
	return RC;
}
*/


int 
xixfs_VCBSectorSizeBits(unsigned int size)
{
	unsigned int bits = 0;
	do {
		bits++;
		size >>= 1;
	} while (size > 1);
	return bits;
}



NTSTATUS
xixfs_MountVolume(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
/*
function description
	1. Check Xi filesystem volume
	2. Create VDO(Volume device object) and Root FCB (File control block)
*/
	NTSTATUS 				RC;
	PIRP					Irp = NULL;
	PIO_STACK_LOCATION 		IrpSp = NULL;
	PVPB					Vpb = NULL;
	PDEVICE_OBJECT			TargetDevice = NULL;
	PXI_VOLUME_DEVICE_OBJECT VDO = NULL;
	PREVENT_MEDIA_REMOVAL 	Prevent;
	//BOOLEAN					IsVolumeWriteProctected = FALSE;
	BOOLEAN					AcqVcb = FALSE;
	PXIXFS_VCB				VCB = NULL;
	uint32					i = 0;
	CC_FILE_SIZES			FileSizes;
	

	DISK_GEOMETRY 			DiskGeometry;
	PARTITION_INFORMATION	PartitionInfo;

	//	Added by ILGU HONG for 08312006
	BLOCKACE_ID				BlockAceId = 0;
	//	Added by ILGU HONG end

	// Changed by ILGU HONG
	uint8					VolumeId[16];	
	
	//	Added by ILGU HONG for readonly 09052006
	BOOLEAN					IsWriteProtect = FALSE;
	uint32					SectorSizeBit = 0;
	uint32					SectorSize = 0;
	//	Added by ILGU HONG end
	uint64					VolumeIndex = 0;
	uint32					LotSize = 0;




	PAGED_CODE();

	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_MountVolume .\n"));


	Irp = IrpContext->Irp;
	IrpSp = IoGetCurrentIrpStackLocation( Irp );
	TargetDevice = IrpSp->Parameters.MountVolume.DeviceObject;
	

	if(!XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Xifs mount Can't wait\n"));

		RC = xixfs_PostRequest(IrpContext, IrpContext->Irp);
		return RC;
	}


	if(XIXCORE_TEST_FLAGS(IrpSp->Flags, SL_ALLOW_RAW_MOUNT)){
		//DbgPrint("Try raw mount !!!\n");
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Xifs raw mount diabled\n"));

		xixfs_CompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		return STATUS_UNRECOGNIZED_VOLUME;		
	}

	// Irp must be waitable
	ASSERT(IrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_WAIT);


	// set read device from Vpb->RealDevice
	IrpContext->TargetDeviceObject = IrpSp->Parameters.MountVolume.Vpb->RealDevice;

	
	if(XiDataDisable){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Xifs mount diabled\n"));

		xixfs_CompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		return STATUS_UNRECOGNIZED_VOLUME;
	}

	XifsdAcquireGData(IrpContext);

	try{
		if(!XiGlobalData.IsXifsComInit){
			SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
			PTDI_ADDRESS_LPX		pLpxAddress = NULL;
			OBJECT_ATTRIBUTES		objectAttributes;
			
			RC = LpxTdiGetAddressList(
					&socketLpxAddressList);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail(0x%x) LpxTdiGetAddressList .\n", RC));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}	

			if(0 == socketLpxAddressList.iAddressCount){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail socketLpxAddressList.iAddressCount == 0 .\n"));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}

			pLpxAddress = &(socketLpxAddressList.SocketLpx[0].LpxAddress);
			// Changed by ILGU HONG
			//	chesung suggest
			//RtlCopyMemory(XiGlobalData.HostMac, pLpxAddress->Node, 6);
			RtlZeroMemory(XiGlobalData.HostMac, 32);
			RtlCopyMemory((uint8 *)&XiGlobalData.HostMac[26], pLpxAddress->Node, 6);

			RtlZeroMemory(XiGlobalData.XifsComCtx.HostMac, 32);
			RtlCopyMemory(XiGlobalData.XifsComCtx.HostMac, XiGlobalData.HostMac, 32);

			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				("SerAddr[0x%02x:%02x:%02x:%02x:%02x:%02x]\n",
				pLpxAddress->Node[0],pLpxAddress->Node[1],pLpxAddress->Node[2],
				pLpxAddress->Node[3],pLpxAddress->Node[4],pLpxAddress->Node[5]));

			/*
				Initialize Communication entities
			*/

			xixcore_IntializeGlobalData(
				XiGlobalData.HostId,
				(PXIXCORE_SPINLOCK)&(XiGlobalData.XifsAuxLotLockListMutex)
				);

			RtlZeroMemory(xixcore_global.HostMac, 32);
			RtlCopyMemory((uint8 *)&xixcore_global.HostMac[26], pLpxAddress->Node, 6);

			//Server side event
			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.ServShutdownEvent,
				NotificationEvent,
				FALSE
			);	
			
			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.ServNetworkEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.ServDatagramRecvEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.ServStopEvent,
				NotificationEvent,
				FALSE
			);


			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.CliShutdownEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.CliNetworkEvent,
				NotificationEvent,
				FALSE
			);
			

			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.CliSendDataEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&XiGlobalData.XifsComCtx.CliStopEvent,
				NotificationEvent,
				FALSE
			);

			InitializeListHead(&XiGlobalData.XifsComCtx.RecvPktList);
			ExInitializeFastMutex(&XiGlobalData.XifsComCtx.RecvPktListMutex);
			
			InitializeListHead(&XiGlobalData.XifsComCtx.SendPktList);
			ExInitializeFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
			XiGlobalData.XifsComCtx.PacketNumber = 0;

			
			
			
			InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
			RC = PsCreateSystemThread(
					&XiGlobalData.XifsComCtx.hServHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					xixfs_EventComSvrThreadProc,
					&XiGlobalData.XifsComCtx
					);
			

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail(0x%x) XifsCom Server Thread .\n", RC));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}

			InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
			RC = PsCreateSystemThread(
					&XiGlobalData.XifsComCtx.hCliHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					xixfs_EventComCliThreadProc,
					&XiGlobalData.XifsComCtx
					);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail(0x%x) XifsCom Client Thread .\n", RC));
				
				KeSetEvent(&XiGlobalData.XifsComCtx.ServShutdownEvent,0,FALSE);
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}


			XiGlobalData.XifsNetEventCtx.Callbacks[0] = xixfs_SevEventCallBack;
			XiGlobalData.XifsNetEventCtx.CallbackContext[0] = (PVOID)&XiGlobalData.XifsComCtx;
			XiGlobalData.XifsNetEventCtx.Callbacks[1] = xixfs_CliEventCallBack;
			XiGlobalData.XifsNetEventCtx.CallbackContext[1] = (PVOID)&XiGlobalData.XifsComCtx;
			XiGlobalData.XifsNetEventCtx.CallbackCnt = 2;

			xixfs_NetEvtInit(&XiGlobalData.XifsNetEventCtx);


			
			XiGlobalData.IsXifsComInit = 1;
			
												
		}

		/*
		 *		XifsdScanForDismount
		 */
		xixfs_ScanForDismount(IrpContext);


		if(!xixfs_ScanForPreMountedObject(IrpContext, TargetDevice))
		{
			RC = STATUS_UNSUCCESSFUL;
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail(0x%x) XifsdScanForPreMounted .\n", RC));
			try_return(RC);
		}


		RtlZeroMemory(&DiskGeometry, sizeof(DISK_GEOMETRY));

		RC = xixfs_RawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_GET_DRIVE_GEOMETRY,
				NULL,
				0,
				(uint8 *)&(DiskGeometry),
				sizeof(DISK_GEOMETRY),
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
			try_return(RC);
		}


		if(DiskGeometry.BytesPerSector == 0) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Sectorsize is not set!! .\n", RC));
			try_return(RC);
		}

		SectorSize = DiskGeometry.BytesPerSector;
		SectorSizeBit = xixfs_VCBSectorSizeBits(SectorSize);


		RtlZeroMemory(&PartitionInfo, sizeof(PARTITION_INFORMATION));

		RC = xixfs_RawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_GET_PARTITION_INFO,
				NULL,
				0,
				(uint8 *)&(PartitionInfo),
				sizeof(PARTITION_INFORMATION),
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
			try_return(RC);
		}

		// Changed by ILGU HONG
		RtlZeroMemory(VolumeId, 16);

		/*
			Volume check
		*/

		RC = xixfs_CheckBootSector(
				TargetDevice, 
				SectorSize,
				SectorSizeBit,
				&VolumeIndex,
				&LotSize,
				VolumeId
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) xixfs_CheckBootSector .\n", RC));
			try_return(RC);
		}

	//	Added by ILGU HONG for 08312006
	//	Changed by ILGU HONG for readonly 09052006
		RC = xixfs_CheckXifsd(TargetDevice, &PartitionInfo, &BlockAceId, &IsWriteProtect);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixfs_CheckXifsd .\n", RC));
			try_return(RC);
		}
	//	Added by ILGU HONG end


	//	Added by ILGU HONG for readonly 09052006
		RC = xixfs_RawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_IS_WRITABLE,
				NULL,
				0,
				NULL,
				0,
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			if (RC == STATUS_MEDIA_WRITE_PROTECTED) {
				IsWriteProtect = TRUE;
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Volume is write protected .\n"));
			}else{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
				try_return(RC);
			}
		}	
	//	Added by ILGU HONG end




		if(!xixfs_ScanForPreMountedVCB(IrpContext,VolumeId)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) xixfs_ScanForPreMountedVCB .\n", RC));
			
			//DbgPrint("xixfs_ScanForPreMountedVCB\n");

			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);
		}


		
		Vpb = IrpSp->Parameters.MountVolume.Vpb;

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),  
						 ("IN Mount  Vpb(%p) Realdevice->vpb(%p) 2\n", 
						 Vpb,
						 Vpb->RealDevice->Vpb));



	
		//check raw device --> must be Disk device
		if(Vpb->RealDevice->DeviceType != FILE_DEVICE_DISK){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Xifs mount diabled\n"));

			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);
		}



		/*
			Create VDO , Initialize it, and register VDO
		*/
		 if (!NT_SUCCESS(RC = IoCreateDevice(
									XiGlobalData.XifsDriverObject,
									sizeof(XI_VOLUME_DEVICE_OBJECT)-sizeof(DEVICE_OBJECT),
									NULL,
									FILE_DEVICE_DISK_FILE_SYSTEM,
									0,
									FALSE,
									&(PDEVICE_OBJECT)VDO))) 
		 {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Create IoCreateDevice .\n", RC));
			try_return(RC);
		 }

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
				("DriverObject(%p)  DriverObject->Name(%wZ) DriverObject->DeviceObject(%p)\n", 
					XiGlobalData.XifsDriverObject, 
					XiGlobalData.XifsDriverObject->DriverName, 
					XiGlobalData.XifsDriverObject->DeviceObject));



		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
				("XifsControlDeviceObject(%p) DriverObject(%p) NextDevice(%p) AttachedDevice(%p)\n", 
				XiGlobalData.XifsControlDeviceObject,
				XiGlobalData.XifsControlDeviceObject->DriverObject,
				XiGlobalData.XifsControlDeviceObject->NextDevice,
				XiGlobalData.XifsControlDeviceObject->AttachedDevice));	


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
				("VDODeviceObject(%p) DriverObject(%p) NextDevice(%p) AttachedDevice(%p)\n", 
				&VDO->DeviceObject,
				VDO->DeviceObject.DriverObject,
				VDO->DeviceObject.NextDevice,
				VDO->DeviceObject.AttachedDevice));	


		if(TargetDevice->AlignmentRequirement > VDO->DeviceObject.AlignmentRequirement){
			VDO->DeviceObject.AlignmentRequirement = TargetDevice->AlignmentRequirement;
		}

		XIXCORE_SET_FLAGS(VDO->DeviceObject.Flags, DO_DIRECT_IO);
		
		XIXCORE_CLEAR_FLAGS(VDO->DeviceObject.Flags, DO_DEVICE_INITIALIZING);

		//  Initialize VDO
		VDO->OverflowQueueCount = 0;
		VDO->PostedRequestCount = 0;
		InitializeListHead(&VDO->OverflowQueue);
		KeInitializeSpinLock( &VDO->OverflowQueueSpinLock );



		// Initialize VCB
		VCB = (PXIXFS_VCB)&VDO->VCB;
		RtlZeroMemory(VCB, sizeof(XIXFS_VCB));
		
		VCB->PtrVDO = VDO;
		VCB->NodeTypeCode = XIFS_NODE_VCB;
		VCB->NodeByteSize = sizeof(XIXFS_VCB);

		// Init Syncronization primitive
		ExInitializeResourceLite(&(VCB->VCBResource));
		ExInitializeFastMutex(&(VCB->VCBMutex));
		
		VCB->VCBLockThread = NULL;
		ExInitializeResourceLite(&(VCB->FileResource));

		// Init Nofity
		FsRtlNotifyInitializeSync(&VCB->NotifyIRPSync);
		InitializeListHead(&VCB->NextNotifyIRP);
		InitializeListHead(&VCB->VCBLink);
		

		KeInitializeEvent(
				&VCB->ResourceEvent,
				NotificationEvent,
				FALSE
		);

	
		ObReferenceObject(TargetDevice);
		VCB->TargetDeviceObject = TargetDevice;
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),  
						 ("ILGU1 TargetDeviceObject(%p)->ReferenceCount (%d) \n", 
						 TargetDevice,
						 TargetDevice->ReferenceCount));


		
		VCB->SwapVpb = ExAllocatePoolWithTag(NonPagedPool,sizeof(VPB), TAG_BUFFER);
		
		if(!VCB->SwapVpb){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail Allocation SwapVpb .\n"));
				try_return(RC);
			
		}
		
		RtlZeroMemory(VCB->SwapVpb, sizeof(VPB));

		VCB->PtrVPB = Vpb;
		
		

	
		
		VCB->PtrVPB->DeviceObject = (PDEVICE_OBJECT)&VDO->DeviceObject;
		VCB->PtrVPB->Flags |= VPB_MOUNTED;

		//VDO->DeviceObject.Vpb = VCB->PtrVPB;
		VDO->DeviceObject.StackSize = VCB->TargetDeviceObject->StackSize + 1;
		VDO->DeviceObject.Flags &= (~DO_DEVICE_INITIALIZING);
		
		Vpb = NULL;
		VDO = NULL;

		ExInitializeWorkItem( &VCB->WorkQueueItem,(PWORKER_THREAD_ROUTINE) xixfs_CallCloseFCB,VCB);
		InitializeListHead(&(VCB->DelayedCloseList));
		VCB->DelayedCloseCount = 0;


		RtlInitializeGenericTable(&VCB->FCBTable,
			(PRTL_GENERIC_COMPARE_ROUTINE)xixfs_FCBTLBCompareEntry,
			(PRTL_GENERIC_ALLOCATE_ROUTINE)xixfs_FCBTLBAllocateEntry,
			(PRTL_GENERIC_FREE_ROUTINE)xixfs_FCBTLBDeallocateEntry,
			NULL);
		

		VCB->VCBState = XIFSD_VCB_STATE_VOLUME_MOUNTING_PROGRESS;

		// changed by ILGU HONG for readonly end


		VCB->IsVolumeRemovable = FALSE;
		if(DiskGeometry.MediaType != FixedMedia)
		{
			Prevent.PreventMediaRemoval = TRUE;
			RC = xixfs_RawDevIoCtrl(
					TargetDevice,
					IOCTL_DISK_MEDIA_REMOVAL,
					(uint8 *)&Prevent,
					sizeof(PREVENT_MEDIA_REMOVAL),
					NULL,
					0,
					FALSE,
					NULL
					);
			VCB->IsVolumeRemovable = TRUE;
		}else{
			VCB->IsVolumeRemovable = FALSE;
		}




	


		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, 
				("bytesPerSector (%ld)  .\n", DiskGeometry.BytesPerSector));

		//DbgPrint("SectorSize of Disk %ld\n", VCB->DiskGeometry.BytesPerSector );
		
		if(SectorSize != SECTORSIZE_512){
			if(SectorSize % 1024){
				RC = STATUS_UNSUCCESSFUL;
				try_return(RC);
			}
		}
	

		SectorSize;
		SectorSizeBit;
	
	
		
		RC = xixfs_RawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_GET_PARTITION_INFO,
				NULL,
				0,
				(uint8 *)&(VCB->PartitionInformation),
				sizeof(PARTITION_INFORMATION),
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
			try_return(RC);
		}
		
		/*
			Get Supper block information
				Read Volume info
				Check volume integrity
				Get Lots Resource
				Register Host
				Create Root Directory
		*/


		xixcore_InitializeVolume(
					&(VCB->XixcoreVcb),
					(PXIXCORE_BLOCK_DEVICE)	TargetDevice,
					(PXIXCORE_SPINLOCK)&(VCB->ChildCacheLock),
					((IsWriteProtect)?1:0),
					(xc_uint16)SectorSize,
					(xc_uint16)SectorSizeBit,
					SECTORSIZE_4096,
					VolumeId
					);		


		xixcore_InitializeMetaContext(
					&(VCB->XixcoreVcb.MetaContext),
					&(VCB->XixcoreVcb),
					(PXIXCORE_SPINLOCK)&(VCB->ResourceMetaLock)
					);


		XifsdAcquireVcbExclusive(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT), 
								VCB, 
								FALSE);

		

		AcqVcb = TRUE;

		RC=xixcore_GetSuperBlockInformation(&VCB->XixcoreVcb,LotSize,(xc_sector_t)VolumeIndex);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdGetSuperBlockInformation .\n", RC));
			try_return(RC);
		}


	//	Added by ILGU HONG for 08312006
		VCB->NdasVolBacl_Id = (uint64)BlockAceId;
		BlockAceId = 0;
	//	Added by ILGU HONG for 08312006

		// Changed by ILGU HONG for readonly 09052006
		if(!VCB->XixcoreVcb.IsVolumeWriteProtected){
			RC = xixcore_RegisterHost(&VCB->XixcoreVcb);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdRegisterHost .\n", RC));
				try_return(RC);
			}
			
			xixcore_CleanUpAuxLotLockInfo(&VCB->XixcoreVcb);
		}
		// Changed by ILGU HONG for readonly end


		InsertTailList(&(XiGlobalData.XifsVDOList), &(VCB->VCBLink));
		


		VCB->PtrVPB->ReferenceCount +=1;
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("a2 VCB->PtrVPB->ReferenceCount(%ld) .\n", VCB->PtrVPB->ReferenceCount));
		
		VCB->TargetDeviceObject->Flags  &= (~DO_VERIFY_VOLUME);
	
		VCB->VCBState = XIFSD_VCB_STATE_VOLUME_MOUNTED;

		/*
		 *	Initialize Dasd file /MetaFcb / Root file
		 */
		
		XifsdLockVcb(IrpContext, VCB);
		VCB->VolumeDasdFCB = xixfs_CreateAndAllocFCB(VCB,0,FCB_TYPE_VOLUME,NULL);	
		XifsdIncRefCount(VCB->VolumeDasdFCB,1,1);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("VolumeDasdFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->VolumeDasdFCB->FCBReference,
						 VCB->VolumeDasdFCB->FCBUserReference ));		

		
		XifsdUnlockVcb(IrpContext, VCB);

		VCB->VolumeDasdFCB->AllocationSize.QuadPart =
		VCB->VolumeDasdFCB->FileSize.QuadPart = 
		VCB->VolumeDasdFCB->ValidDataLength.QuadPart = VCB->PartitionInformation.PartitionLength.QuadPart;

		VCB->VolumeDasdFCB->Resource = &VCB->FileResource;
		
		xixfs_CreateInternalStream(IrpContext, VCB, VCB->VolumeDasdFCB, FALSE);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_REFCOUNT),
						 ("VolumeDasdFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->VolumeDasdFCB->FCBReference,
						 VCB->VolumeDasdFCB->FCBUserReference ));


		XifsdLockVcb(IrpContext, VCB);
		VCB->MetaFCB = xixfs_CreateAndAllocFCB(VCB,0,FCB_TYPE_VOLUME,NULL);	
		XifsdIncRefCount(VCB->MetaFCB,1,1);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("MetaFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->MetaFCB->FCBReference,
						 VCB->MetaFCB->FCBUserReference ));		

		
		XifsdUnlockVcb(IrpContext, VCB);

		VCB->MetaFCB->AllocationSize.QuadPart =
		VCB->MetaFCB->FileSize.QuadPart = 
		VCB->MetaFCB->ValidDataLength.QuadPart = VCB->PartitionInformation.PartitionLength.QuadPart;
		VCB->MetaFCB->Resource = &VCB->FileResource;
		
		xixfs_CreateInternalStream(IrpContext, VCB, VCB->MetaFCB, FALSE);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("MetaFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->MetaFCB->FCBReference,
						 VCB->MetaFCB->FCBUserReference ));


		XifsdLockVcb(IrpContext, VCB);
		VCB->RootDirFCB = xixfs_CreateAndAllocFCB(VCB,VCB->XixcoreVcb.RootDirectoryLotIndex,FCB_TYPE_DIR,NULL);	
		XifsdIncRefCount(VCB->RootDirFCB,1,1);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("RootDirFCB, LotNumber(%I64d) VCB %d/%d FCB %d/%d\n",
						 VCB->RootDirFCB->XixcoreFcb.LotNumber,
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->RootDirFCB->FCBReference,
						 VCB->RootDirFCB->FCBUserReference ));

		
		
		RC = xixfs_InitializeFCB(
			VCB->RootDirFCB,
			XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)
		);
		XifsdUnlockVcb(IrpContext, VCB);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdInitializeFCBInfo .\n", RC));
			try_return(RC);
		}

		VCB->VCBResidualUserReference = XIFSD_RESIDUALUSERREF;
		VCB->VCBReference ++;
		RC = STATUS_SUCCESS;	

		VCB->VCBState = XIFSD_VCB_STATE_VOLUME_MOUNTED;

		ObDereferenceObject(TargetDevice);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
						 ("ILGU2 TargetDeviceObject(%p)->ReferenceCount (%d) \n", 
						 TargetDevice,TargetDevice->ReferenceCount));


		// Changed by ILGU HONG for readonly 09052006
		if(!VCB->XixcoreVcb.IsVolumeWriteProtected){

			OBJECT_ATTRIBUTES		objectAttributes;
			
			KeInitializeEvent(
				&VCB->VCBUmountEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&VCB->VCBUpdateEvent,
				NotificationEvent,
				FALSE
			);
			
			KeInitializeEvent(
				&VCB->VCBGetMoreLotEvent,
				NotificationEvent,
				FALSE
			);

			KeInitializeEvent(
				&VCB->VCBStopOkEvent,
				NotificationEvent,
				FALSE
			);		
			
			InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
			RC = PsCreateSystemThread(
					&VCB->hMetaUpdateThread,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					xixfs_MetaUpdateFunction,
					VCB
					);
			

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail(0x%x) XifsdMetaUpdate Function .\n", RC));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}
		}
		// Changed by ILGU HONG for readonly end

		XifsdReleaseVcb(IrpContext, VCB);
		AcqVcb = FALSE;
		VCB = NULL;
		RC = STATUS_SUCCESS;


		;
	}finally{
		DebugUnwind("XifsdMountVolume");
		
		if(Vpb != NULL) {
			Vpb->DeviceObject = NULL;
		}

		if(VCB != NULL){
			//VCB->VCBReference -= VCB->VCBResidualUserReference;
			if(xixfs_DismountVCB(IrpContext, VCB)){
				XifsdReleaseVcb(IrpContext, VCB);
			}
			AcqVcb = FALSE;

		}else if(VDO != NULL){
			IoDeleteDevice((PDEVICE_OBJECT)VDO);
			Vpb->DeviceObject = NULL;
		}
		
		if(AcqVcb) XifsdReleaseVcb(IrpContext, VCB);
		
		XifsdReleaseGData(IrpContext);

	
	//	Added by ILGU HONG for 08312006		
		if(BlockAceId != 0){

			xixfs_RemoveUserBacl(TargetDevice,BlockAceId);
		
		}
	//	Added by ILGU HONG end




		xixfs_CompleteRequest(IrpContext, RC, 0);
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Exit Status(0x%x) XifsdMountVolume .\n", RC));
		
	}
	return (RC);
}






NTSTATUS
xixfs_VerifyVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PULONG					VolumeState = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_VerifyVolume .\n"));
	
	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);


	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);


	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Post Process Request (0x%x) .\n", pIrpContext));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(TypeOfOpen != UserVolumeOpen)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Exit Invalid Type (%x)  .\n", TypeOfOpen));
		
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	pVCB = (PXIXFS_VCB)pFCB->PtrVCB;
	ASSERT(pVCB);

	if(!ExAcquireResourceSharedLite(&pVCB->VCBResource, TRUE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT ),
					("PostRequest IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}
	
	try{
		
		
			
		
		if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Volume not mounted  .\n"));

			RC = STATUS_UNSUCCESSFUL;

		}else if(XIXCORE_TEST_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Volume is Locked  .\n"));
			RC = STATUS_ACCESS_DENIED;	
		}else{
			RC = STATUS_SUCCESS;
		}

	}finally{
		DebugUnwind(XifsdVerifyVolume);
		ExReleaseResourceLite(&pVCB->VCBResource);
		xixfs_CompleteRequest(pIrpContext, RC, 0);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statsu(0x%x) xixfs_VerifyVolume .\n", RC));
	return RC;
}


NTSTATUS
xixfs_OplockRequest(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS RC;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	ULONG 					FsControlCode = 0;
	PFILE_OBJECT			pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;
	ULONG 					OplockCount = 0;
	BOOLEAN					bAcquireFCB = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_OplockRequest .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);
	FsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;


	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserFileOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);

	
	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!xixfs_OplockRequest  pCCB(%p)\n", pCCB));



	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Not support Volume .\n"));
	
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}



	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type , 
		(XIFS_FD_TYPE_DIRECTORY | XIFS_FD_TYPE_ROOT_DIRECTORY)))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Not support Directory .\n"));

		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;		
	}
	
	if (pIrpSp->Parameters.FileSystemControl.OutputBufferLength > 0) 
	{	
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Buffer size is too small .\n"));

		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;		
	}
	
	switch(FsControlCode){
	case FSCTL_REQUEST_OPLOCK_LEVEL_1:
	case FSCTL_REQUEST_BATCH_OPLOCK:
	case FSCTL_REQUEST_FILTER_OPLOCK:
	case FSCTL_REQUEST_OPLOCK_LEVEL_2:
	{
		if(!ExAcquireResourceExclusiveLite(pFCB->Resource,TRUE)){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
		}
		bAcquireFCB = TRUE;
	}
	break;
	case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
	case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
	case FSCTL_OPLOCK_BREAK_NOTIFY:
	case FSCTL_OPLOCK_BREAK_ACK_NO_2:
		{
		
			if(!ExAcquireResourceExclusiveLite(pFCB->Resource,TRUE)){
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
				RC = xixfs_PostRequest(pIrpContext, pIrp);
				return RC;
			}
			bAcquireFCB = TRUE;
		}
	break;
	default:
		break;
	}



	switch(FsControlCode){
	case FSCTL_REQUEST_OPLOCK_LEVEL_1:
	case FSCTL_REQUEST_BATCH_OPLOCK:
	case FSCTL_REQUEST_FILTER_OPLOCK:
	case FSCTL_REQUEST_OPLOCK_LEVEL_2:
	{
		if(pIrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2){
			if(pFCB->FCBFileLock) {
				OplockCount = FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock);
			}
			
		}else{
			OplockCount = pFCB->FCBCleanup;
		}
		
	}
	break;
	case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
	case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
	case FSCTL_OPLOCK_BREAK_NOTIFY:
	case FSCTL_OPLOCK_BREAK_ACK_NO_2:
	break;
	default:
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;			

		break;
	}

	try{
		RC = FsRtlOplockFsctrl(&pFCB->FCBOplock, pIrp, OplockCount);
		XifsdLockFcb(TRUE,pFCB);
		pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);
		XifsdUnlockFcb(TRUE,pFCB);
				
	}finally{
		DebugUnwind(XifsdOplockRequest);
		if(bAcquireFCB)
			ExReleaseResourceLite(pFCB->Resource);
	}

	//
	//	oplock page will complete the irp
	//
	
	xixfs_ReleaseIrpContext(pIrpContext);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(0x%x) xixfs_OplockRequest .\n", RC));
	return RC;	
}


NTSTATUS
xixfs_LockVolumeInternal(
	IN PXIXFS_IRPCONTEXT 	pIrpContext,
	IN PXIXFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	NTSTATUS	FRC = (pFileObject? STATUS_ACCESS_DENIED: STATUS_DEVICE_BUSY);
	KIRQL		SavedIrql;
	uint32		RefCount  = (pFileObject? 1: 0);
	BOOLEAN		CanWait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	PVPB		pVPB = NULL;
	//PAGED_CODE();
	

	ASSERT_EXCLUSIVE_VCB(pVCB);



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_LockVolumeInternal .\n"));


	XifsdLockVcb( pIrpContext, pVCB );
	XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
	XifsdUnlockVcb(pIrpContext, pVCB);



	RC = xixfs_PurgeVolume( pIrpContext, pVCB, FALSE );

	XifsdReleaseVcb(TRUE, pVCB);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				 ("VCB %d/%d \n", pVCB->VCBReference, pVCB->VCBUserReference));		


	RC = CcWaitForCurrentLazyWriterActivity();

	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	
	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) CcWaitForCurrentLazyWriterActivity .\n", RC));
		XifsdLockVcb( pIrpContext, pVCB );
		XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
		XifsdUnlockVcb(pIrpContext, pVCB);
		return RC;
	}
	

	xixfs_RealCloseFCB((PVOID)pVCB);


	XifsdLockVcb( pIrpContext, pVCB );
	XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
	XifsdUnlockVcb(pIrpContext, pVCB);

	if((pVCB->VCBCleanup == RefCount) 
		&& (pVCB->VCBUserReference == pVCB->VCBResidualUserReference + RefCount)) 
	{

		pVPB = pVCB->PtrVPB;

		ASSERT(pVPB);
		XIXCORE_TEST_FLAGS(pVPB->Flags, VPB_LOCKED);

		IoAcquireVpbSpinLock( &SavedIrql );
		if( !XIXCORE_TEST_FLAGS(pVPB->Flags, VPB_LOCKED)){
			XIXCORE_SET_FLAGS(pVPB->Flags, VPB_LOCKED);
			IoReleaseVpbSpinLock(SavedIrql);

			XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED);
			pVCB->VCBLockFileObject = pFileObject;
			FRC = STATUS_SUCCESS;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("!!!VPB_LOCKED \n"));

		}else{
			IoReleaseVpbSpinLock(SavedIrql);
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixfs_LockVolumeInternal (%x).\n",FRC));
	return FRC;
}

NTSTATUS	
xixfs_UnlockVolumeInternal(
	IN PXIXFS_IRPCONTEXT 	pIrpContext,
	IN PXIXFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
)
{
	NTSTATUS Status = STATUS_NOT_LOCKED;
	KIRQL SavedIrql;
	//PAGED_CODE();
	ASSERT_EXCLUSIVE_VCB(pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_UnlockVolumeInternal .\n"));



	IoAcquireVpbSpinLock( &SavedIrql );
	try{
		if(XIXCORE_TEST_FLAGS(pVCB->PtrVPB->Flags, VPB_LOCKED) &&
			pVCB->VCBLockFileObject == pFileObject)
		{
				XIXCORE_CLEAR_FLAGS(pVCB->PtrVPB->Flags, VPB_LOCKED);
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
					("Enter XifsdUnlockVolumeInternal Unlock is done.\n"));
				XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED);
				
				pVCB->VCBLockFileObject = NULL;
				Status = STATUS_SUCCESS;
		}
	}finally{
		IoReleaseVpbSpinLock(SavedIrql);
	}


	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_UnlockVolumeInternal .\n"));
	return Status;
}


NTSTATUS
xixfs_LockVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;


	


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_LockVolume .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);



	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(0x%x).\n", pIrpContext));

		
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		
		return RC;
	}
	

	

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	
	FsRtlNotifyVolumeEvent(pFileObject, FSRTL_VOLUME_LOCK);

	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{
		RC = xixcore_IsSingleHost(&(pVCB->XixcoreVcb));

		if(!NT_SUCCESS(RC)){
			DbgPrint("FAIL xixcore_IsSingleHost\n");
			try_return(RC);
		}

		RC = xixfs_LockVolumeInternal(pIrpContext, pVCB, pFileObject);
	}finally{
		
		XifsdReleaseVcb(TRUE,pVCB);
	}

	if(!NT_SUCCESS(RC)){
		FsRtlNotifyVolumeEvent(pFileObject, FSRTL_VOLUME_LOCK_FAILED);
	}


	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdLockVolume .\n"));
	
	return RC;	
}

NTSTATUS
xixfs_UnlockVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;

	

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_UnlockVolume .\n"));

	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);


	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

		
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}



	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	

	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{
			RC = xixfs_UnlockVolumeInternal(pIrpContext, pVCB, pFileObject);
	}finally{
		XifsdReleaseVcb(TRUE, pVCB);
	}



	if (NT_SUCCESS( RC )) {

		FsRtlNotifyVolumeEvent( pFileObject, FSRTL_VOLUME_UNLOCK );
	}
	
	xixfs_CompleteRequest(pIrpContext, RC, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixfs_UnlockVolume .\n"));
	
	return RC;
}


NTSTATUS
xixfs_DismountVolume(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Enter xixfs_DismountVolume .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	

	
	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}

	FsRtlNotifyVolumeEvent( pFileObject, FSRTL_VOLUME_DISMOUNT );

	
	if(ExAcquireResourceExclusiveLite(&(XiGlobalData.DataResource), TRUE)){
		if(!ExAcquireResourceExclusiveLite(&(pVCB->VCBResource), TRUE)){
			ExReleaseResourceLite(&(XiGlobalData.DataResource));
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
		}
	}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));
			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
	}
	

	try{


		XifsdLockVcb(pIrpContext, pVCB);
		if(pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNTED){
			XifsdUnlockVcb(pIrpContext, pVCB);
			RC = STATUS_VOLUME_DISMOUNTED;
			try_return(RC);
		}else if(pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
			pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
		}
		XifsdUnlockVcb(pIrpContext, pVCB);

		//
		//	Release System Resource
		//

		xixfs_CleanupFlushVCB(pIrpContext, pVCB, TRUE);			

		XifsdLockVcb(pIrpContext, pVCB);
		pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
		XIXCORE_SET_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_DISMOUNT_ON_CLOSE);
		XifsdUnlockVcb(pIrpContext, pVCB);
		
		RC = STATUS_SUCCESS;
		

		;
	}finally{
		DebugUnwind(XifsdDismountVolume);
		ExReleaseResourceLite(&(pVCB->VCBResource));
		ExReleaseResourceLite(&(XiGlobalData.DataResource));
		xixfs_CompleteRequest(pIrpContext, RC, 0);
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Exit Statue(0x%x) xixfs_DismountVolume .\n", RC));
	return RC;
	
}



NTSTATUS
xixfs_IsThisVolumeMounted(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PULONG				VolumeState = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_IsThisVolumeMounted .\n"));


	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}

	
	if(ExAcquireResourceExclusiveLite(&(XiGlobalData.DataResource), TRUE)){
		if(!ExAcquireResourceExclusiveLite(&(pVCB->VCBResource), TRUE)){
			ExReleaseResourceLite(&(XiGlobalData.DataResource));
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
		}
	}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
	}
	

	if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED){
		RC = STATUS_VOLUME_DISMOUNTED;

	}else {
		RC = STATUS_SUCCESS;
	}

	xixfs_CompleteRequest(pIrpContext, RC, 0);
	ExReleaseResourceLite(&(pVCB->VCBResource));
	ExReleaseResourceLite(&(XiGlobalData.DataResource));
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statue(0x%x) xixfs_IsThisVolumeMounted .\n", RC));
	return RC;
}

NTSTATUS
xixfs_IsPathnameValid(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XifsdIsPathnameValid .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_SUCCESS;
        xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!xixfs_IsPathnameValid  pCCB(%p)\n",  pCCB));
	

	xixfs_CompleteRequest(pIrpContext, RC, 0);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdIsPathnameValid .\n"));
	return RC;
}


NTSTATUS
xixfs_AllowExtendedDasdIo(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PULONG					VolumeState = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_VCB				pVCB = NULL;	
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_AllowExtendedDasdIo .\n"));



	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (xixfs_DecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	


	XIXCORE_SET_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_ALLOW_XTENDED_DASD_IO);

	RC = STATUS_SUCCESS;
	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixfs_AllowExtendedDasdIo .\n"));
	return RC;
}

NTSTATUS
xixfs_InvalidateVolumes(
	IN PXIXFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
	)
{
	NTSTATUS	Status;
	PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( pIrp );
    KIRQL SavedIrql;
	LUID TcbPrivilege = {SE_TCB_PRIVILEGE, 0};
	HANDLE Handle = NULL;

	PXIXFS_VCB pVCB;

	PLIST_ENTRY Links;

	PFILE_OBJECT FileToMarkBad;
	PDEVICE_OBJECT DeviceToMarkBad;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_InvalidateVolumes .\n"));

	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

		xixfs_PostRequest(pIrpContext, pIrp);
		return STATUS_PENDING;
	}



	//Check Top level privilege
	if (!SeSinglePrivilegeCheck( TcbPrivilege, pIrp->RequestorMode )) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail is not top level privilege .\n"));

		xixfs_CompleteRequest( pIrpContext, STATUS_PRIVILEGE_NOT_HELD, 0);

		return STATUS_PRIVILEGE_NOT_HELD;
	}

	#if defined(_WIN64)
	if (IoIs32bitProcess( pIrp )) {
    
		if (pIrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof( UINT32 )) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail Invalid length .\n"));

			xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
			return STATUS_INVALID_PARAMETER;
		}

		Handle = (HANDLE) LongToHandle( *((PUINT32) pIrp->AssociatedIrp.SystemBuffer) );

	} else {
	#endif
		if (pIrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof( HANDLE )) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail Invalid length .\n"));
			
			xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER,0 );
			return STATUS_INVALID_PARAMETER;
		}
		Handle = *((PHANDLE) pIrp->AssociatedIrp.SystemBuffer);
	#if defined(_WIN64)
	}
	#endif

	Status = ObReferenceObjectByHandle( Handle,
										0,
										*IoFileObjectType,
										KernelMode,
										&FileToMarkBad,
										NULL );
	
	if (!NT_SUCCESS(Status)) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Handle ref .\n"));

		xixfs_CompleteRequest( pIrpContext, Status, 0 );
		return Status;
	}
	
	DeviceToMarkBad = FileToMarkBad->DeviceObject;
	ObDereferenceObject( FileToMarkBad );	


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Search Target DeviceObject 0x%x .\n", DeviceToMarkBad));


	XifsdAcquireGData(pIrpContext);

	Links = XiGlobalData.XifsVDOList.Flink;

	while (Links != &XiGlobalData.XifsVDOList) {

		pVCB = CONTAINING_RECORD( Links, XIXFS_VCB, VCBLink );

		Links = Links->Flink;

		//
		//  If we get a match, mark the volume Bad, and also check to
		//  see if the volume should go away.
		//
		
		XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);

		if (pVCB->PtrVPB->RealDevice == DeviceToMarkBad) {

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("find Target DeviceObject 0x%x .\n", DeviceToMarkBad));

			//
			//  Take the VPB spinlock,  and look to see if this volume is the 
			//  one currently mounted on the actual device.  If it is,  pull it 
			//  off immediately.
			//

			IoAcquireVpbSpinLock( &SavedIrql );

			if (DeviceToMarkBad->Vpb == pVCB->PtrVPB) {
				PVPB NewVpb = pVCB->SwapVpb;
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
					("VPB is same(%p) DeviceToMarkBad(%p).\n", pVCB->PtrVPB, DeviceToMarkBad));

				ASSERT( XIXCORE_TEST_FLAGS( pVCB->PtrVPB->Flags, VPB_MOUNTED));
				ASSERT(NULL != NewVpb);

				RtlZeroMemory(NewVpb, sizeof(VPB));
				NewVpb->Type = IO_TYPE_VPB;
				NewVpb->Size = sizeof(VPB);
				NewVpb->RealDevice = DeviceToMarkBad;
				NewVpb->Flags = XIXCORE_MASK_FLAGS( DeviceToMarkBad->Vpb->Flags, VPB_REMOVE_PENDING);
				DeviceToMarkBad->Vpb = NewVpb;
				pVCB->SwapVpb = NULL;
				
			}

			IoReleaseVpbSpinLock( SavedIrql );

			XifsdLockVcb(pIrpContext, pVCB);
			if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
				pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
			}
			XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);

			XifsdUnlockVcb(pIrpContext, pVCB);

			xixfs_PurgeVolume( pIrpContext, pVCB, FALSE );

			XifsdReleaseVcb(TRUE, pVCB);

			Status = CcWaitForCurrentLazyWriterActivity();

			XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);			

			xixfs_RealCloseFCB((PVOID)pVCB);

			XifsdLockVcb( pIrpContext, pVCB );
			XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
			XifsdUnlockVcb(pIrpContext, pVCB);

			xixfs_CheckForDismount( pIrpContext, pVCB, FALSE );
			
		} 
		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData( pIrpContext );

	xixfs_CompleteRequest( pIrpContext, STATUS_SUCCESS, 0 );
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdInvalidateVolumes .\n"));
	return STATUS_SUCCESS;
}


/*
 
   ||XIFS_OFFSET_VOLUME_LOT_LOC|XIFS_RESERVED_LOT_SIZE(24)|Lot||
		Reserved				   Reserved		
   
 */


NTSTATUS
xixfs_TransLotToCluster(
	IN PXIXFS_VCB	pVCB,
	IN uint32	StartingCluster,
	IN uint32	BytesToCopy,
	IN OUT uint8 *Buffer
)
{
	uint32 i = 0;
	uint32	ClusterPerLot = 0;
	uint32	tLcn = 0;


	ASSERT_VCB(pVCB);
	ASSERT(!(StartingCluster % 8));
	ASSERT(StartingCluster >= (uint32)(xixfs_GetLcnFromLot(pVCB->XixcoreVcb.LotSize, XIFS_RESERVED_LOT_SIZE)));

	ClusterPerLot = (uint32)(pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE);
	
	if(BytesToCopy ==0){
		return STATUS_SUCCESS;
	}
	
	for(i = XIFS_RESERVED_LOT_SIZE; i<pVCB->XixcoreVcb.NumLots; i++){
		tLcn = (uint32)xixfs_GetLcnFromLot(pVCB->XixcoreVcb.LotSize, i);
		
		if(tLcn >= (uint32)(StartingCluster + BytesToCopy*8)){
			
			return STATUS_SUCCESS;
			
		}else{
		
			if( xixcore_TestBit(i, xixcore_GetDataBufferOfBitMap(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap->Data) ) ){
				uint32	NumSetBit = 0;
				uint32	Offset = 0;
				uint32	Margin = 0;

				if((tLcn + ClusterPerLot) > (StartingCluster + BytesToCopy*8)){
					
					NumSetBit = StartingCluster + BytesToCopy * 8 - tLcn;		
					Offset = tLcn - StartingCluster;
					
					if(Offset % 8){
						Margin = (Offset %8);
						for(i = Offset; i<Margin; i++){
							xixcore_SetBit(Offset, Buffer);
							Offset++;
							NumSetBit --;
						}
					}

					Margin = (uint32)(NumSetBit/8);

					for(i = 0; i< Margin; i++){
						Buffer[Offset>>3] = 0xFF;
						Offset += 8;
						NumSetBit -= 8;
					}


					if(Offset %8){
						Margin = (Offset %8);
						for(i = Offset; i<Margin; i++){
							xixcore_SetBit(Offset, Buffer);
							Offset++;
							NumSetBit --;
						}
					}

					return STATUS_SUCCESS;
				}else{

					NumSetBit = ClusterPerLot;		
					Offset = tLcn - StartingCluster;
					
					if(Offset % 8){
						Margin = (Offset %8);
						for(i = Offset; i<Margin; i++){
							xixcore_SetBit(Offset, Buffer);
							Offset++;
							NumSetBit --;
						}
					}

					Margin = (uint32)(NumSetBit/8);

					for(i = 0; i< Margin; i++){
						Buffer[Offset>>3] = 0xFF;
						Offset += 8;
						NumSetBit -= 8;
					}


					if(Offset %8){
						Margin = (Offset %8);
						for(i = Offset; i<Margin; i++){
							xixcore_SetBit(Offset, Buffer);
							Offset++;
							NumSetBit --;
						}
					}

				}
				
			}	//	if(xixcore_TestBit(i,pVCB->HostFreeLotMap->Data)){
			

		}
	} //for(i = XIFS_RESERVED_LOT_SIZE; i<pVCB->NumLots; i++){



	return STATUS_SUCCESS;
	
}




NTSTATUS
xixfs_TransLotBitmapToClusterBitmap
(
	IN PXIXFS_VCB	pVCB,
	IN uint32	StartingCluster,
	IN uint32	BytesToCopy,
	IN OUT uint8 *Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint32	ReservedClusterAreadMax = 0;
	
	
	uint32	i = 0;
	uint32	IndexOfBuffer = 0;
	uint32	LastLcn = 0;
	uint32	RemainingBytes = 0;


	ASSERT_VCB(pVCB);
	ASSERT(!(StartingCluster % 8));

	if(BytesToCopy==0){
		return STATUS_SUCCESS;
	}

	RtlZeroMemory(Buffer, BytesToCopy);

	ReservedClusterAreadMax = (uint32)((XIFS_OFFSET_VOLUME_LOT_LOC + 
		pVCB->XixcoreVcb.LotSize * XIFS_RESERVED_LOT_SIZE) / CLUSTER_SIZE) ;


	LastLcn = (uint32)StartingCluster;
	RemainingBytes = (uint32)BytesToCopy;


	if(StartingCluster < ReservedClusterAreadMax){
		uint32	BitCount = 0;
		uint32	Margin = 0;
		
		/*	
		DbgPrint(" In Reserved Aread ReservedArea(%ld):LastLcn(%ld):RemainingBytes(%ld)\n",
			ReservedClusterAreadMax, LastLcn, RemainingBytes);
		*/

		BitCount = ReservedClusterAreadMax - StartingCluster;
		
		if((uint32)(BitCount/8) > BytesToCopy){
			return STATUS_SUCCESS;
		}else{
			LastLcn += BitCount;
			IndexOfBuffer += (uint32)(BitCount/8);
			RemainingBytes -= (uint32)(BitCount/8);
			ASSERT(RemainingBytes > 0);

			RC = xixfs_TransLotToCluster(pVCB,
							(uint32)((LastLcn/8)*8),
							RemainingBytes,
							&Buffer[IndexOfBuffer]
							);
		}

	}else{
		RC = xixfs_TransLotToCluster(pVCB,
						StartingCluster,
						BytesToCopy,
						Buffer
						);
	}

	return RC;
}




NTSTATUS
xixfs_GetVolumeBitMap(	
	IN PXIXFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp = NULL;

	PXIXFS_VCB			pVCB = NULL;
	PXIXFS_FCB			pFCB = NULL;
	PXIXFS_CCB			pCCB = NULL;

	ULONG BytesToCopy = 0;
	ULONG DesiredClusters = 0;
	ULONG TotalClusters = 0;
	ULONG StartingCluster = 0;
	ULONG InputBufferLength = 0;
	ULONG OutputBufferLength = 0;
	LARGE_INTEGER StartingLcn = {0,0};
	PVOLUME_BITMAP_BUFFER OutputBuffer = NULL;
	BOOLEAN		CanWait = FALSE;

	pIrpSp = IoGetCurrentIrpStackLocation( pIrp );



	//
	//  Extract and decode the file object and check for type of open.
	//

	if (xixfs_DecodeFileObject( pIrpSp->FileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}

	CanWait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	if(!CanWait){
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}


	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	InputBufferLength = pIrpSp->Parameters.FileSystemControl.InputBufferLength;
	OutputBufferLength = pIrpSp->Parameters.FileSystemControl.OutputBufferLength;

	OutputBuffer = (PVOLUME_BITMAP_BUFFER)xixfs_GetCallersBuffer( pIrp );

	//
	//  Check for a minimum length on the input and output buffers.
	//

	if ((InputBufferLength < sizeof(STARTING_LCN_INPUT_BUFFER)) ||
		(OutputBufferLength < sizeof(VOLUME_BITMAP_BUFFER))) {

		xixfs_CompleteRequest( pIrpContext, STATUS_BUFFER_TOO_SMALL, 0 );
		return STATUS_BUFFER_TOO_SMALL;
	}

	TotalClusters = (uint32)((pVCB->XixcoreVcb.NumLots * (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE)) + (XIFS_OFFSET_VOLUME_LOT_LOC/CLUSTER_SIZE));


	try{
		if(pIrp->RequestorMode != KernelMode){
            ProbeForRead( pIrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                          InputBufferLength,
                          sizeof(UCHAR) );

            ProbeForWrite( OutputBuffer, OutputBufferLength, sizeof(UCHAR) );
		}
		
		StartingLcn = ((PSTARTING_LCN_INPUT_BUFFER)pIrpSp->Parameters.FileSystemControl.Type3InputBuffer)->StartingLcn;

	}except(pIrp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH ){
		RC = GetExceptionCode();
		
		XifsdRaiseStatus(pIrpContext, 
			FsRtlIsNtstatusExpected(RC) ?
			RC : STATUS_INVALID_USER_BUFFER );
	}
	
	
	if(StartingLcn.HighPart || (StartingLcn.LowPart >= TotalClusters) ){
		xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER , 0 );
		return STATUS_INVALID_PARAMETER ;		
	}else{
		StartingCluster = (StartingLcn.LowPart & ~7);
	}
	
	
    
	OutputBufferLength -= FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer);
	DesiredClusters = TotalClusters - StartingCluster;

	if (OutputBufferLength < (DesiredClusters + 7) / 8) {
		
		/*
		DbgPrint("Overflow OutputBufferLegngth (%ld) : DesiredClusters(%ld): BytesToCopy(%ld)\n",
			OutputBufferLength, DesiredClusters, BytesToCopy);
		*/
		BytesToCopy = OutputBufferLength;
		RC = STATUS_BUFFER_OVERFLOW;

	} else {
		/*
		DbgPrint("OutputBufferLegngth (%ld) : DesiredClusters(%ld): BytesToCopy(%ld)\n",
			OutputBufferLength, DesiredClusters, BytesToCopy);
		*/
		BytesToCopy = (DesiredClusters + 7) / 8;
		RC = STATUS_SUCCESS;
	}
	
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{

		try{
			xixfs_TransLotBitmapToClusterBitmap(pVCB, 
											StartingCluster, 
											BytesToCopy, 
											&OutputBuffer->Buffer[0]
											);
			
		}except(pIrp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH ){
			RC = GetExceptionCode();
		
			XifsdRaiseStatus(pIrpContext, 
				FsRtlIsNtstatusExpected(RC) ?
				RC : STATUS_INVALID_USER_BUFFER );
		}


	}finally{
		XifsdReleaseVcb(TRUE, pVCB);
		pIrp->IoStatus.Information = FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer) + BytesToCopy;
		xixfs_CompleteRequest( pIrpContext, RC, (uint32)pIrp->IoStatus.Information);
	}

	return RC;


}


NTSTATUS
xixfs_UserFsctl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIRP pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;

	PAGED_CODE();

	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_UserFsctl  pIrpSp->Parameters.FileSystemControl.FsControlCode 0x%x.\n",
		pIrpSp->Parameters.FileSystemControl.FsControlCode));

	switch(pIrpSp->Parameters.FileSystemControl.FsControlCode) {
		case FSCTL_REQUEST_OPLOCK_LEVEL_1 :
		case FSCTL_REQUEST_OPLOCK_LEVEL_2 :
		case FSCTL_REQUEST_BATCH_OPLOCK :
		case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE :
		case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
		case FSCTL_OPLOCK_BREAK_NOTIFY :
		case FSCTL_OPLOCK_BREAK_ACK_NO_2 :
		case FSCTL_REQUEST_FILTER_OPLOCK :

	

	
		    RC= xixfs_OplockRequest( pIrpContext, pIrp);
		    break;

		case FSCTL_LOCK_VOLUME :
			

		    
			RC = xixfs_LockVolume( pIrpContext, pIrp );
		    break;

		case FSCTL_UNLOCK_VOLUME :
			

		    
		    RC= xixfs_UnlockVolume( pIrpContext, pIrp );
		    break;

		case FSCTL_DISMOUNT_VOLUME :
			

		    //DbgPrint("FSCTL_DISMOUNT_VOLUME");
		   RC = xixfs_DismountVolume( pIrpContext, pIrp );
		    break;


		case FSCTL_IS_VOLUME_MOUNTED :
			

		    
		    RC = xixfs_IsThisVolumeMounted( pIrpContext, pIrp );
		    break;

		case FSCTL_IS_PATHNAME_VALID :
			

		 
		    RC = xixfs_IsPathnameValid( pIrpContext, pIrp );
		    break;
		    
		case FSCTL_ALLOW_EXTENDED_DASD_IO:
			

		 
		    RC = xixfs_AllowExtendedDasdIo( pIrpContext, pIrp );
		    break;
		case FSCTL_INVALIDATE_VOLUMES:

			RC = xixfs_InvalidateVolumes(pIrpContext, pIrp);
			break;

		case FSCTL_QUERY_RETRIEVAL_POINTERS:
			//DbgPrint("!!! CALL FSCTL_QUERY_RETRIEVAL_POINTERS\n");
			RC = STATUS_NOT_IMPLEMENTED;
			xixfs_CompleteRequest( pIrpContext, RC, 0);
		    break;
		case FSCTL_GET_RETRIEVAL_POINTERS:
			//DbgPrint("!!! CALL FSCTL_GET_RETRIEVAL_POINTERS\n");
			RC = STATUS_NOT_IMPLEMENTED;
			xixfs_CompleteRequest( pIrpContext, RC, 0);
		    break;
		case FSCTL_GET_VOLUME_BITMAP:
			//DbgPrint("!!! FSCTL_GET_VOLUME_BITMAP\n");
			//RC = xixfs_GetVolumeBitMap(pIrpContext, pIrp);
			RC =  STATUS_NOT_IMPLEMENTED;
			xixfs_CompleteRequest( pIrpContext, RC, 0);
		    break;
		case FSCTL_MOVE_FILE:
			//DbgPrint("!!! FSCTL_MOVE_FILE\n");
			RC = STATUS_NOT_IMPLEMENTED;
			xixfs_CompleteRequest( pIrpContext, RC, 0);
		    break;
		//
		//  We don't support any of the known or unknown requests.
		//
		case NDAS_XIXFS_UNLOAD:
			{
				PDEVICE_OBJECT	DeviceObject = pIrpContext->TargetDeviceObject;
				BOOLEAN			bEmpty = FALSE;
				PIO_REMOVE_LOCK	pRemoveLock = NULL;

				ASSERT(DeviceObject);
				
				if(DeviceObject != XiGlobalData.XifsControlDeviceObject){
					RC = STATUS_INVALID_PARAMETER;
					xixfs_CompleteRequest( pIrpContext, RC, 0);
					break;
				}
				

				pRemoveLock = (PIO_REMOVE_LOCK)((PUCHAR)DeviceObject+sizeof(DEVICE_OBJECT));
				ASSERT(pRemoveLock);

				RC = IoAcquireRemoveLock(pRemoveLock, (PVOID)TAG_REMOVE_LOCK);
				
				if(!NT_SUCCESS(RC)){
					xixfs_CompleteRequest(pIrpContext, RC, 0);
					break;
				}



				XifsdAcquireGData(pIrpContext);
				if(!xixfs_IsThereMountedVolume()) {
					XifsdReleaseGData(pIrpContext);
					RC = STATUS_UNSUCCESSFUL;
					xixfs_CompleteRequest( pIrpContext, RC, 0);
					IoReleaseRemoveLock(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
					break;
				}

				if(XiGlobalData.IsRegistered){
					
					IoReleaseRemoveLockAndWait(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
					XiGlobalData.IsRegistered = 0;
					XifsdReleaseGData(pIrpContext);
					IoUnregisterFileSystem( DeviceObject );
					RC = STATUS_SUCCESS;
					xixfs_CompleteRequest( pIrpContext, RC, 0);
					break;
				}

				XifsdReleaseGData(pIrpContext);
				RC = STATUS_UNSUCCESSFUL;
				xixfs_CompleteRequest( pIrpContext, RC, 0);
				IoReleaseRemoveLock(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
				break;
				

			}
			break;
		default:
			RC = STATUS_INVALID_DEVICE_REQUEST;
			xixfs_CompleteRequest( pIrpContext, RC, 0);
		    break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(%lx) xixfs_UserFsctl .\n", RC));
	return RC;
}


NTSTATUS
xixfs_CommonFileSystemControl(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS 	RC;
	PIO_STACK_LOCATION	PtrIoStackLocation = NULL;
	PAGED_CODE();

	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);
	ASSERT(IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL);


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!xixfs_CommonFileSystemControl \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_CommonFileSystemControl .\n"));


	PtrIoStackLocation = IoGetCurrentIrpStackLocation(IrpContext->Irp);

	if(PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == NDAS_XIXFS_UNLOAD)
	{
		PDEVICE_OBJECT	DeviceObject = IrpContext->TargetDeviceObject;
		BOOLEAN			bEmpty = FALSE;
		PIO_REMOVE_LOCK	pRemoveLock = NULL;

		//DbgPrint("enter NDAS_XIXFS_UNLOAD\n");

		ASSERT(DeviceObject);
		
		if(DeviceObject != XiGlobalData.XifsControlDeviceObject){
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(IrpContext, RC, 0);
			//DbgPrint("error NDAS_XIXFS_UNLOAD  1\n");
			return RC;
		}


		pRemoveLock = (PIO_REMOVE_LOCK)((PUCHAR)DeviceObject+sizeof(DEVICE_OBJECT));
		ASSERT(pRemoveLock);

		RC = IoAcquireRemoveLock(pRemoveLock, (PVOID)TAG_REMOVE_LOCK);
		
		if(!NT_SUCCESS(RC)){
			xixfs_CompleteRequest(IrpContext, RC, 0);
			//DbgPrint("error NDAS_XIXFS_UNLOAD  1\n");
			return RC;
		}


		
		XifsdAcquireGData(IrpContext);
		if(!xixfs_IsThereMountedVolume()) {
			XifsdReleaseGData(IrpContext);
			RC = STATUS_UNSUCCESSFUL;
			xixfs_CompleteRequest( IrpContext, RC, 0);
			IoReleaseRemoveLock(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
			//DbgPrint("error NDAS_XIXFS_UNLOAD  2\n");
			return RC;
		}

		if(XiGlobalData.IsRegistered){
			UNICODE_STRING		DriverDeviceLinkName;
			IoReleaseRemoveLockAndWait(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
			XiGlobalData.IsRegistered = 0;
			XifsdReleaseGData(IrpContext);
			RtlInitUnicodeString( &DriverDeviceLinkName, XIXFS_CONTROL_LINK_NAME );
			IoUnregisterFileSystem( DeviceObject );
			IoDeleteSymbolicLink(&DriverDeviceLinkName);
			IoDeleteDevice(DeviceObject);
			RC = STATUS_SUCCESS;
			xixfs_CompleteRequest(IrpContext, RC, 0);
			//DbgPrint("OK NDAS_XIXFS_UNLOAD  3\n");
			return RC;
		}

		//DbgPrint("error NDAS_XIXFS_UNLOAD  4\n");	
		XifsdReleaseGData(IrpContext);
		RC = STATUS_UNSUCCESSFUL;
		xixfs_CompleteRequest( IrpContext, RC, 0);
		IoReleaseRemoveLock(pRemoveLock,(PVOID)TAG_REMOVE_LOCK);
		return RC;
	}




	if(PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == NDAS_XIXFS_VERSION)
	{
		PXIXFS_VER  pVerInfo = NULL;
		uint32		outputlength = 0;

		pVerInfo = (PXIXFS_VER)IrpContext->Irp->AssociatedIrp.SystemBuffer;
		if(!pVerInfo) {
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(IrpContext, RC, 0);
			return RC;
		}

		outputlength = (uint32)PtrIoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
		if(outputlength < sizeof(XIXFS_VER)){
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(IrpContext, RC, 0);
			return RC;
		}
		
		pVerInfo->VersionMajor	= VER_FILEMAJORVERSION;
		pVerInfo->VersionMinor	= VER_FILEMINORVERSION;
		pVerInfo->VersionBuild	= VER_FILEBUILD;
		pVerInfo->VersionPrivate = VER_FILEBUILD_QFE;
		pVerInfo->VersionNDFSMajor = XIFS_CURRENT_RELEASE_VERSION;
		pVerInfo->VersionNDFSMinor= XIFS_CURRENT_VERSION;

		RC = STATUS_SUCCESS;
		xixfs_CompleteRequest(IrpContext, RC, sizeof(XIXFS_VER));
		return RC;

	}




	switch(IrpContext->MinorFunction){
	case IRP_MN_MOUNT_VOLUME:
		RC = xixfs_MountVolume( IrpContext);
		break;

	case IRP_MN_VERIFY_VOLUME:
		RC = xixfs_VerifyVolume( IrpContext);
		break;

	case IRP_MN_KERNEL_CALL: 
	case IRP_MN_USER_FS_REQUEST:
		RC = xixfs_UserFsctl( IrpContext);
		break;

	default:
		xixfs_CompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		RC = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(%lx) xixfs_CommonFileSystemControl .\n", RC));
	return RC;

}





NTSTATUS
xixfs_CommonShutdown(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIRP					NewIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	IO_STATUS_BLOCK			IoStatus;
	PXIXFS_VCB				pVCB = NULL;
	PLIST_ENTRY 			pListEntry = NULL;
	KEVENT 					Event;
	BOOLEAN					IsVCBPresent = TRUE;

	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonShutdown \n"));

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Enter xixfs_CommonShutdown .\n"));

	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT))
	{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));
		
		xixfs_PostRequest(pIrpContext, pIrp);
		return STATUS_PENDING;
	}

	XifsdAcquireGData(pIrpContext);

	
	try{

		
 		KeInitializeEvent( &Event, NotificationEvent, FALSE );

		pListEntry = XiGlobalData.XifsVDOList.Flink;
		while(pListEntry != &(XiGlobalData.XifsVDOList)){
			pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);
			ASSERT(pVCB);

			if( (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS)
					&& (pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED)) 
			{

				IsVCBPresent = TRUE;
				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
					("Shutdown VCB (%lx) .\n", pVCB));

				XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);

				XifsdLockVcb(pIrpContext, pVCB);
				XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
				XifsdUnlockVcb(pIrpContext, pVCB);
				//
				//	Release System Resource
				//

				//
				//	Release System Resource
				//

				xixfs_CleanupFlushVCB(pIrpContext, pVCB, TRUE);			
				
				NewIrp = IoBuildSynchronousFsdRequest( IRP_MJ_SHUTDOWN,
													   pVCB->TargetDeviceObject,
													   NULL,
													   0,
													   NULL,
													   &Event,
													   NULL );

				if (NewIrp == NULL) {

					//Rasise INSUFFICIENT RESOUCE
				}

				if (NT_SUCCESS(IoCallDriver( pVCB->TargetDeviceObject, NewIrp ))) {

					(VOID) KeWaitForSingleObject( &Event,
												  Executive,
												  KernelMode,
												  FALSE,
												  NULL );

					KeClearEvent( &Event );
				}			
				
				pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS;
				IsVCBPresent = xixfs_CheckForDismount(pIrpContext, pVCB, TRUE);
				
				if(IsVCBPresent){
					XifsdLockVcb(pIrpContext, pVCB);
					XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
					XifsdUnlockVcb(pIrpContext, pVCB);

					XifsdReleaseVcb(TRUE, pVCB);
				}
			}
			pListEntry = pListEntry->Flink;
		}
		
	


	}finally{
		XifsdReleaseGData(TRUE)	;
		DebugUnwind(XifsdCommonShutdown);
		if(RC != STATUS_PENDING){
			xixfs_CompleteRequest(pIrpContext, RC, 0 );
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL),
		("Exit XifsdCommonShutdown .\n"));

	return RC;
}