#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"





VOID
XixFsdScanForDismount(
	IN PXIFS_IRPCONTEXT IrpContext
);

BOOLEAN
XixFsdScanForPreMounted(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PDEVICE_OBJECT DeviceObject
);

BOOLEAN
XixFsdScanForPreMountedVCB(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN uint32			PartitionID,
	IN uint8			*DiskID
);


NTSTATUS
XixFsdMountVolume(
	IN PXIFS_IRPCONTEXT IrpContext
	);

NTSTATUS
XixFsdVerifyVolume(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
XixFsdOplockRequest(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);





NTSTATUS
XixFsdLockVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
XixFsdUnlockVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
XixFsdDismountVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);


NTSTATUS
XixFsdIsVolumeMounted(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
XixFsdIsPathnameValid(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
XixFsdAllowExtendedDasdIo(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
);

NTSTATUS
XixFsdInvalidateVolumes(
	IN PXIFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
	);


NTSTATUS
XixFsdUserFsctl(
	IN PXIFS_IRPCONTEXT pIrpContext
	);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdDismountVCB)
#pragma alloc_text(PAGE, XixFsdScanForDismount)
#pragma alloc_text(PAGE, XixFsdScanForPreMounted)
#pragma alloc_text(PAGE, XixFsdMountVolume)
#pragma alloc_text(PAGE, XixFsdVerifyVolume)
#pragma alloc_text(PAGE, XixFsdOplockRequest)
#pragma alloc_text(PAGE, XixFsdDismountVolume)
#pragma alloc_text(PAGE, XixFsdIsVolumeMounted)
#pragma alloc_text(PAGE, XixFsdIsPathnameValid)
#pragma alloc_text(PAGE, XixFsdAllowExtendedDasdIo)
#pragma alloc_text(PAGE, XixFsdInvalidateVolumes)
#pragma alloc_text(PAGE, XixFsdUserFsctl)
#pragma alloc_text(PAGE, XixFsdCheckForDismount)
#pragma alloc_text(PAGE, XixFsdCommonFileSystemControl)
#pragma alloc_text(PAGE, XixFsdCommonShutdown) 
#pragma alloc_text(PAGE, XixFsdPurgeVolume)
#pragma alloc_text(PAGE, XixFsdCleanupFlushVCB)
#endif
//#pragma alloc_text(PAGE, XixFsdLockVolume)
//#pragma alloc_text(PAGE, XixFsdUnlockVolume)

VOID
XixFsdMetaUpdateFunction(
		PVOID	lpParameter
)
{
	PXIFS_VCB pVCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	PKEVENT				Evts[3];
	LARGE_INTEGER		TimeOut;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdMetaUpdateFunction .\n"));


	pVCB = (PXIFS_VCB)lpParameter;
	ASSERT_VCB(pVCB);
	
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
				("Request Stop VCB XixFsdMetaUpdateFunction .\n"));
			KeSetEvent(&pVCB->VCBStopOkEvent, 0, FALSE);
			break;
		}else if ( RC == 1) {
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call XixFsdGetMoreCheckOutLotMap .\n"));
			
			KeClearEvent(&pVCB->VCBGetMoreLotEvent);
			
			RC = XixFsdGetMoreCheckOutLotMap(pVCB);
			
			if(!NT_SUCCESS(RC)){
				XifsdLockVcb(TRUE, pVCB);
				XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_RECHECK_RESOURCES);
				XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES);
				XifsdUnlockVcb(TRUE, pVCB);
			}else{
		
				XifsdLockVcb(TRUE, pVCB);
				XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_RECHECK_RESOURCES);
				XifsdUnlockVcb(TRUE, pVCB);
			}

			KeSetEvent(&pVCB->ResourceEvent, 0, FALSE);


		}else if ((RC == 2) || (RC == STATUS_TIMEOUT)){

			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call XixFsdMetaUpdateFunction .\n"));

			KeClearEvent(&pVCB->VCBUpdateEvent);
			
			RC = XixFsdUpdateMetaData(pVCB);
			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail XixFsdMetaUpdateFunction  Update MetaData"));			
			}
		
		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Error XixFsdMetaUpdateFunction Unsupported State"));			
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdMetaUpdateFunction .\n"));

	return;
}





BOOLEAN
XixFsdDismountVCB(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB		pVCB
)
{
	KIRQL			SavedIrql;
	uint32			i = 0;
	BOOLEAN			bVCBPresent = TRUE;
	BOOLEAN			bFinalReference = FALSE;
	PVPB			OldVpb= NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDismountVCB \n"));


	ASSERT_VCB(pVCB);
	ASSERT_EXCLUSIVE_XIFS_GDATA;
    ASSERT_EXCLUSIVE_VCB( pVCB );



	XifsdLockVcb( pIrpContext, pVCB );

	ASSERT(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS);

	pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS;
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("XixFsdDismountVCB Status(%ld).\n", pVCB->VCBState));

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

			pVCB->SwapVpb->Flags = XifsdCheckFlag(OldVpb->Flags, VPB_REMOVE_PENDING);
			
			IoReleaseVpbSpinLock( SavedIrql );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("pVCB->SwapVpb->RealDevice %p \n", pVCB->SwapVpb->RealDevice));

			pVCB->SwapVpb = NULL;
			XifsdUnlockVcb( pIrpContext, pVCB );



		}else{
			
			OldVpb->ReferenceCount -= 1;
			OldVpb->DeviceObject = NULL;
			XifsdClearFlag( pVCB->PtrVPB->Flags, VPB_MOUNTED );
			XifsdClearFlag( pVCB->PtrVPB->Flags, VPB_LOCKED);

		
			pVCB->PtrVPB = NULL;
			IoReleaseVpbSpinLock( SavedIrql );
			XifsdUnlockVcb( pIrpContext, pVCB );
					
			XixFsdDeleteVCB( pIrpContext, pVCB );
			bVCBPresent = FALSE;
		}

	}else if(bFinalReference){
		OldVpb->ReferenceCount -=1;
		IoReleaseVpbSpinLock( SavedIrql );
		XifsdUnlockVcb( pIrpContext, pVCB );
		
		
		
		XixFsdDeleteVCB( pIrpContext, pVCB );
		bVCBPresent = FALSE;
	}else{
		IoReleaseVpbSpinLock( SavedIrql );
		XifsdUnlockVcb( pIrpContext, pVCB );
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdDismountVCB \n"));
	return bVCBPresent;
}


BOOLEAN
XixFsdCheckForDismount (
    IN PXIFS_IRPCONTEXT pIrpContext,
    IN PXIFS_VCB pVCB,
    IN BOOLEAN Force
    )
{
	BOOLEAN UnlockVcb = TRUE;
	BOOLEAN VcbPresent = TRUE;
	BOOLEAN	CanWait = FALSE;
	KIRQL SavedIrql;

	//PAGED_CODE();

	DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
						 ("Enter XixFsdCheckForDismount pVCB(%p)\n", pVCB));


	ASSERT_IRPCONTEXT( pIrpContext );
	ASSERT_VCB( pVCB );

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	ASSERT_EXCLUSIVE_VCB(pVCB);

	ASSERT(XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

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
						 ("XixFsdCheckForDismount VCB %d/%d \n", pVCB->VCBReference, pVCB->VCBUserReference ));


	if (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS) {

		if (pVCB->VCBUserReference <= pVCB->VCBResidualUserReference || Force) {

			XifsdUnlockVcb( pIrpContext, pVCB );
			UnlockVcb = FALSE;
			
			DebugTrace( DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB),
						 ("call XixFsdCheckForDismount\n"));

			VcbPresent = XixFsdDismountVCB( pIrpContext, pVCB );
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
			XixFsdDeleteVCB( pIrpContext, pVCB );
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
						 ("Exit XixFsdCheckForDismount\n"));
	return VcbPresent;
}


VOID
XixFsdScanForDismount(
	IN PXIFS_IRPCONTEXT IrpContext)
{
	PXIFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdScanForDismount\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIFS_VCB)CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS) ||
			(pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_INVALID))
		{

			XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			if(XixFsdCheckForDismount(IrpContext, pVCB, FALSE)){
				XifsdReleaseVcb(TRUE,pVCB);
			}
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdScanForDismount\n"));
	
}


BOOLEAN
XixFsdScanForPreMounted(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PDEVICE_OBJECT DeviceObject
)
{
	PXIFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdScanForPreMounted\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIFS_VCB)CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) 
			&& (pVCB->TargetDeviceObject == DeviceObject))
		{
			return FALSE;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdScanForPreMounted\n"));	
	return TRUE;
}



BOOLEAN
XixFsdScanForPreMountedVCB(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN uint32			PartitionID,
	IN uint8			*DiskID
)
{
	PXIFS_VCB		pVCB = NULL;
	PLIST_ENTRY		pListEntry = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdScanForPreMountedVCB\n"));

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	
	while(pListEntry != &XiGlobalData.XifsVDOList){
		pVCB = (PXIFS_VCB)CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);
		pListEntry = pListEntry->Flink;


		// Changed by ILGU HONG
		/*
		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) 
			&& (RtlCompareMemory(pVCB->DiskId, DiskID, 6) == 6)
			&& (pVCB->PartitionId == PartitionID))
		{
			return FALSE;
		}
		*/

		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) 
			&& (RtlCompareMemory(pVCB->DiskId, DiskID, 16) == 16)
			&& (pVCB->PartitionId == PartitionID))
		{
			return FALSE;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdScanForPreMountedVCB\n"));	
	return TRUE;
}





NTSTATUS
XixFsdMountVolume(
	IN PXIFS_IRPCONTEXT IrpContext
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
	PXIFS_VCB				VCB = NULL;
	uint32					i = 0;
	CC_FILE_SIZES			FileSizes;
	
	uint32					PrevLevel;
	uint32					PrevTarget;

	DISK_GEOMETRY 			DiskGeometry;
	PARTITION_INFORMATION	PartitionInfo;

	//	Added by ILGU HONG for 08312006
	BLOCKACE_ID				BlockAceId = 0;
	//	Added by ILGU HONG end

	// Changed by ILGU HONG
	//uint8					DiskID[6];
	uint8					DiskID[16];	
	
	//	Added by ILGU HONG for readonly 09052006
	BOOLEAN					IsWriteProtect = FALSE;
	//	Added by ILGU HONG end


//	PrevLevel = XifsdDebugLevel;
//	PrevTarget = XifsdDebugTarget;



	PAGED_CODE();

	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);


//	XifsdDebugLevel = DEBUG_LEVEL_ALL;
//	XifsdDebugTarget = DEBUG_TARGET_ALL;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdMountVolume .\n"));


	Irp = IrpContext->Irp;
	IrpSp = IoGetCurrentIrpStackLocation( Irp );
	TargetDevice = IrpSp->Parameters.MountVolume.DeviceObject;
	

	if(!XifsdCheckFlagBoolean(IrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Xifs mount Can't wait\n"));

		RC = XixFsdPostRequest(IrpContext, IrpContext->Irp);
		return RC;
	}


	if(XifsdCheckFlagBoolean(IrpSp->Flags, SL_ALLOW_RAW_MOUNT)){
		DbgPrint("Try raw mount !!!\n");
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Xifs raw mount diabled\n"));

		XixFsdCompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		return STATUS_UNRECOGNIZED_VOLUME;		
	}

	// Irp must be waitable
	ASSERT(IrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_WAIT);


	// set read device from Vpb->RealDevice
	IrpContext->TargetDeviceObject = IrpSp->Parameters.MountVolume.Vpb->RealDevice;

	
	if(XiDataDisable){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Xifs mount diabled\n"));

		XixFsdCompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		return STATUS_UNRECOGNIZED_VOLUME;
	}

	XifsdAcquireGData(IrpContext);

	try{
		if(XiGlobalData.IsXifsComInit == FALSE){
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
					XixFsdComSvrThreadProc,
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
					XixFsdComCliThreadProc,
					&XiGlobalData.XifsComCtx
					);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail(0x%x) XifsCom Client Thread .\n", RC));
				
				KeSetEvent(&XiGlobalData.XifsComCtx.ServShutdownEvent,0,FALSE);
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);		
			}


			XiGlobalData.XifsNetEventCtx.Callbacks[0] = XixFsdSevEventCallBack;
			XiGlobalData.XifsNetEventCtx.CallbackContext[0] = (PVOID)&XiGlobalData.XifsComCtx;
			XiGlobalData.XifsNetEventCtx.Callbacks[1] = XixFsdCliEventCallBack;
			XiGlobalData.XifsNetEventCtx.CallbackContext[1] = (PVOID)&XiGlobalData.XifsComCtx;
			XiGlobalData.XifsNetEventCtx.CallbackCnt = 2;

			NetEvtInit(&XiGlobalData.XifsNetEventCtx);
			XiGlobalData.IsXifsComInit = TRUE;
												
		}

		/*
		 *		XifsdScanForDismount
		 */
		XixFsdScanForDismount(IrpContext);


		if(!XixFsdScanForPreMounted(IrpContext, TargetDevice))
		{
			RC = STATUS_UNSUCCESSFUL;
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail(0x%x) XifsdScanForPreMounted .\n", RC));
			try_return(RC);
		}


		RtlZeroMemory(&DiskGeometry, sizeof(DISK_GEOMETRY));

		RC = XixFsRawDevIoCtrl(
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



		RtlZeroMemory(&PartitionInfo, sizeof(PARTITION_INFORMATION));

		RC = XixFsRawDevIoCtrl(
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
		//RtlZeroMemory(DiskID, 6);
		RtlZeroMemory(DiskID, 16);

		/*
			Volume check
		*/
	
		RC = XixFsdCheckVolume(TargetDevice, DiskGeometry.BytesPerSector, DiskID);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdCheckVolume .\n", RC));
			try_return(RC);	
		}
	

	//	Added by ILGU HONG for 08312006
	//	Changed by ILGU HONG for readonly 09052006
		RC = XixFsCheckXifsd(TargetDevice, &PartitionInfo, &BlockAceId, &IsWriteProtect);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XixFsCheckXifsd .\n", RC));
			try_return(RC);	
		}
	//	Added by ILGU HONG end


	//	Added by ILGU HONG for readonly 09052006
		RC = XixFsRawDevIoCtrl(
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




		if(!XixFsdScanForPreMountedVCB(IrpContext,PartitionInfo.PartitionNumber ,DiskID)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XixFsdScanForPreMountedVCB .\n", RC));
			
			DbgPrint("XixFsdScanForPreMountedVCB\n");

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

		XifsdSetFlag(VDO->DeviceObject.Flags, DO_DIRECT_IO);
		
		XifsdClearFlag(VDO->DeviceObject.Flags, DO_DEVICE_INITIALIZING);

		//  Initialize VDO
		VDO->OverflowQueueCount = 0;
		VDO->PostedRequestCount = 0;
		InitializeListHead(&VDO->OverflowQueue);
		KeInitializeSpinLock( &VDO->OverflowQueueSpinLock );



		// Initialize VCB
		VCB = (PXIFS_VCB)&VDO->VCB;
		RtlZeroMemory(VCB, sizeof(XIFS_VCB));
		
		VCB->PtrVDO = VDO;
		VCB->NodeTypeCode = XIFS_NODE_VCB;
		VCB->NodeByteSize = sizeof(XIFS_VCB);

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
		/*
		VCB->RealDeviceObject = VCB->PtrVPB->RealDevice;
		VCB->PtrVPB->RealDevice = VCB->TargetDeviceObject;
		*/
		VCB->PtrVPB->Flags |= VPB_MOUNTED;

		//VDO->DeviceObject.Vpb = VCB->PtrVPB;
		VDO->DeviceObject.StackSize = VCB->TargetDeviceObject->StackSize + 1;
		VDO->DeviceObject.Flags &= (~DO_DEVICE_INITIALIZING);
		
		Vpb = NULL;
		VDO = NULL;

		ExInitializeWorkItem( &VCB->WorkQueueItem,(PWORKER_THREAD_ROUTINE) XixFsdCallCloseFCB,VCB);
		InitializeListHead(&(VCB->DelayedCloseList));
		VCB->DelayedCloseCount = 0;


		RtlInitializeGenericTable(&VCB->FCBTable,
			(PRTL_GENERIC_COMPARE_ROUTINE)XixFsdFCBTableCompare,
			(PRTL_GENERIC_ALLOCATE_ROUTINE)XixFsdFCBTableAllocate,
			(PRTL_GENERIC_FREE_ROUTINE)XixFsdFCBTableDeallocate,
			NULL);
		

		VCB->VCBState = XIFSD_VCB_STATE_VOLUME_MOUNTING_PROGRESS;

		/*
			Add VCB to XiGlobal
		*/
		
		// changed by ILGU HONG for readonly 09052006
		/*
		VCB->IsVolumeWriteProctected = FALSE;
		RC = XixFsRawDevIoCtrl(
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
				VCB->IsVolumeWriteProctected = TRUE;
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Volume is write protected .\n"));
			}else{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
				try_return(RC);	
			}
		}	
		*/

		VCB->IsVolumeWriteProctected = IsWriteProtect;

		// changed by ILGU HONG for readonly end


		VCB->IsVolumeRemovable = FALSE;
		if(VCB->DiskGeometry.MediaType != FixedMedia)
		{
			Prevent.PreventMediaRemoval = TRUE;
			RC = XixFsRawDevIoCtrl(
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


		RC = XixFsRawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_GET_DRIVE_GEOMETRY,
				NULL,
				0,
				(uint8 *)&(VCB->DiskGeometry),
				sizeof(DISK_GEOMETRY),
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdRawDevIoCtrl .\n", RC));
			try_return(RC);	
		}


		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, 
				("bytesPerSector (%ld)  .\n", VCB->DiskGeometry.BytesPerSector));

		DbgPrint("SectorSize of Disk %ld\n", VCB->DiskGeometry.BytesPerSector );
		
		if(VCB->DiskGeometry.BytesPerSector != SECTORSIZE_512){
			if(VCB->DiskGeometry.BytesPerSector % 1024){
				RC = STATUS_UNSUCCESSFUL;
				try_return(RC);
			}
		}

		VCB->SectorSize = VCB->DiskGeometry.BytesPerSector;


		if(VCB->DiskGeometry.BytesPerSector != SECTORSIZE_512){
			if(VCB->DiskGeometry.BytesPerSector % 1024){
				RC = STATUS_UNSUCCESSFUL;
				try_return(RC);
			}

		}

		
		RC = XixFsRawDevIoCtrl(
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
		RC = XifsdRawDevIoCtrl(
				TargetDevice,
				IOCTL_DISK_GET_LENGTH_INFO,
				NULL,
				0,
				&(VCB->LengthInfo),
				sizeof(GET_LENGTH_INFORMATION),
				FALSE,
				NULL
				);

		if(!NT_SUCCESS(RC)){
			goto error_out;
		}
		*/

		/*
			Get Supper block information
				Read Volume info
				Check volume integrity
				Get Lots Resource
				Register Host
				Create Root Directory
		*/

		XifsdAcquireVcbExclusive(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT), 
								VCB, 
								FALSE);

		

		AcqVcb = TRUE;

		RC=XixFsdGetSuperBlockInformation(VCB);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdGetSuperBlockInformation .\n", RC));
			try_return(RC);	
		}

		VCB->PartitionId = VCB->PartitionInformation.PartitionNumber;

	//	Added by ILGU HONG for 08312006
		VCB->NdasVolBacl_Id = (uint64)BlockAceId;
		BlockAceId = 0;
	//	Added by ILGU HONG for 08312006

		RtlCopyMemory(VCB->HostId, XiGlobalData.HostId, 16);
		//	Changed by ILGU HONG
		//		chesung suggest
		//RtlCopyMemory(VCB->HostMac, XiGlobalData.HostMac, 6);
		RtlZeroMemory(VCB->HostMac, 32);
		RtlCopyMemory(VCB->HostMac, XiGlobalData.HostMac, 32);

		/*
		RC = XixFsCheckXifsd(IrpContext, VCB);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) XifsdSetXifsd .\n", RC));
			try_return(RC);	
		}
		*/


		// Changed by ILGU HONG for readonly 09052006
		if(!VCB->IsVolumeWriteProctected){
			RC = XixFsdRegisterHost(IrpContext,VCB);
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdRegisterHost .\n", RC));
				try_return(RC);	
			}
			
			XixFsCleanUpAuxLockLockInfo(VCB);
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
		VCB->VolumeDasdFCB = XixFsdCreateAndAllocFCB(VCB,0,FCB_TYPE_VOLUME,NULL);	
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
		
		XixFsdCreateInternalStream(IrpContext, VCB, VCB->VolumeDasdFCB, FALSE);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_REFCOUNT),
						 ("VolumeDasdFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->VolumeDasdFCB->FCBReference,
						 VCB->VolumeDasdFCB->FCBUserReference ));


		XifsdLockVcb(IrpContext, VCB);
		VCB->MetaFCB = XixFsdCreateAndAllocFCB(VCB,0,FCB_TYPE_VOLUME,NULL);	
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
		
		XixFsdCreateInternalStream(IrpContext, VCB, VCB->MetaFCB, FALSE);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("MetaFCB, VCB %d/%d FCB %d/%d\n",
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->MetaFCB->FCBReference,
						 VCB->MetaFCB->FCBUserReference ));


		XifsdLockVcb(IrpContext, VCB);
		VCB->RootDirFCB = XixFsdCreateAndAllocFCB(VCB,VCB->RootDirectoryLotIndex,FCB_TYPE_DIR,NULL);	
		XifsdIncRefCount(VCB->RootDirFCB,1,1);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_REFCOUNT ),
						 ("RootDirFCB, LotNumber(%I64d) VCB %d/%d FCB %d/%d\n",
						 VCB->RootDirFCB->LotNumber,
						 VCB->VCBReference,
						 VCB->VCBUserReference,
						 VCB->RootDirFCB->FCBReference,
						 VCB->RootDirFCB->FCBUserReference ));

		
		
		RC = XixFsdInitializeFCBInfo(
			VCB->RootDirFCB,
			XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)
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
		if(!VCB->IsVolumeWriteProctected){

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
					XixFsdMetaUpdateFunction,
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
	}finally{
		DebugUnwind("XifsdMountVolume");
		
		if(Vpb != NULL) {
			Vpb->DeviceObject = NULL;
		}

		if(VCB != NULL){
			//VCB->VCBReference -= VCB->VCBResidualUserReference;
			if(XixFsdDismountVCB(IrpContext, VCB)){
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

			XixFsRemoveUserBacl(TargetDevice,BlockAceId);
		
		}
	//	Added by ILGU HONG end


//		XifsdDebugLevel = PrevLevel;
//		XifsdDebugTarget = PrevTarget;

		XixFsdCompleteRequest(IrpContext, RC, 0);
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Exit Status(0x%x) XifsdMountVolume .\n", RC));
		
	}
	return (RC);
}






NTSTATUS
XixFsdVerifyVolume(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PULONG					VolumeState = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdVerifyVolume .\n"));
	
	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);


	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);


	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Post Process Request (0x%x) .\n", pIrpContext));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(TypeOfOpen != UserVolumeOpen)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Exit Invalid Type (%x)  .\n", TypeOfOpen));
		
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	pVCB = (PXIFS_VCB)pFCB->PtrVCB;
	ASSERT(pVCB);

	if(!ExAcquireResourceSharedLite(&pVCB->VCBResource, TRUE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT ),
					("PostRequest IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}
	
	try{
		
		
			
		
		if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Volume not mounted  .\n"));

			RC = STATUS_UNSUCCESSFUL;

		}else if(XifsdCheckFlagBoolean(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Volume is Locked  .\n"));
			RC = STATUS_ACCESS_DENIED;	
		}else{
			RC = STATUS_SUCCESS;
		}
		
	}finally{
		DebugUnwind(XifsdVerifyVolume);
		ExReleaseResourceLite(&pVCB->VCBResource);
		XixFsdCompleteRequest(pIrpContext, RC, 0);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statsu(0x%x) XixFsdVerifyVolume .\n", RC));
	return RC;
}


NTSTATUS
XixFsdOplockRequest(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS RC;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	ULONG 					FsControlCode = 0;
	PFILE_OBJECT			pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;
	ULONG 					OplockCount = 0;
	BOOLEAN					bAcquireFCB = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdOplockRequest .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);
	FsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;


	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserFileOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);

	
	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!XixFsdOplockRequest (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));



	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(pFCB->FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Not support Volume .\n"));
	
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}



	if(XifsdCheckFlagBoolean(pFCB->Type , 
		(XIFS_FD_TYPE_DIRECTORY | XIFS_FD_TYPE_ROOT_DIRECTORY)))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Not support Directory .\n"));

		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;		
	}
	
	if (pIrpSp->Parameters.FileSystemControl.OutputBufferLength > 0) 
	{	
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Buffer size is too small .\n"));

		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
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
			RC = XixFsdPostRequest(pIrpContext, pIrp);
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
				RC = XixFsdPostRequest(pIrpContext, pIrp);
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
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;			

		break;
	}

	try{
		RC = FsRtlOplockFsctrl(&pFCB->FCBOplock, pIrp, OplockCount);
		XifsdLockFcb(TRUE,pFCB);
		pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);
		XifsdUnlockFcb(TRUE,pFCB);
				
	}finally{
		DebugUnwind(XifsdOplockRequest);
		if(bAcquireFCB)
			ExReleaseResourceLite(pFCB->Resource);
	}

	//
	//	oplock page will complete the irp
	//
	
	XixFsdReleaseIrpContext(pIrpContext);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(0x%x) XixFsdOplockRequest .\n", RC));
	return RC;	
}


NTSTATUS
XixFsdLockVolumeInternal(
	IN PXIFS_IRPCONTEXT 	pIrpContext,
	IN PXIFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	NTSTATUS	FRC = (pFileObject? STATUS_ACCESS_DENIED: STATUS_DEVICE_BUSY);
	KIRQL		SavedIrql;
	uint32		RefCount  = (pFileObject? 1: 0);
	BOOLEAN		CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	PVPB		pVPB = NULL;
	//PAGED_CODE();
	

	ASSERT_EXCLUSIVE_VCB(pVCB);



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdLockVolumeInternal .\n"));

	XifsdLockVcb( pIrpContext, pVCB );
	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
	XifsdUnlockVcb(pIrpContext, pVCB);



	RC = XixFsdPurgeVolume( pIrpContext, pVCB, FALSE );

	XifsdReleaseVcb(TRUE, pVCB);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				 ("VCB %d/%d \n", pVCB->VCBReference, pVCB->VCBUserReference));		


	RC = CcWaitForCurrentLazyWriterActivity();

	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	
	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) CcWaitForCurrentLazyWriterActivity .\n", RC));
		XifsdLockVcb( pIrpContext, pVCB );
		XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
		XifsdUnlockVcb(pIrpContext, pVCB);
		return RC;
	}
	

	XixFsdRealCloseFCB((PVOID)pVCB);


	XifsdLockVcb( pIrpContext, pVCB );
	XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
	XifsdUnlockVcb(pIrpContext, pVCB);

	if((pVCB->VCBCleanup == RefCount) 
		&& (pVCB->VCBUserReference == pVCB->VCBResidualUserReference + RefCount)) 
	{

		pVPB = pVCB->PtrVPB;

		ASSERT(pVPB);
		XifsdCheckFlagBoolean(pVPB->Flags, VPB_LOCKED);

		IoAcquireVpbSpinLock( &SavedIrql );
		if( !XifsdCheckFlagBoolean(pVPB->Flags, VPB_LOCKED)){
			XifsdSetFlag(pVPB->Flags, VPB_LOCKED);
			IoReleaseVpbSpinLock(SavedIrql);

			XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED);
			pVCB->VCBLockFileObject = pFileObject;
			FRC = STATUS_SUCCESS;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("!!!VPB_LOCKED \n"));

		}else{
			IoReleaseVpbSpinLock(SavedIrql);
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XixFsdLockVolumeInternal (%x).\n",FRC));
	return FRC;
}

NTSTATUS	
XixFsdUnlockVolumeInternal(
	IN PXIFS_IRPCONTEXT 	pIrpContext,
	IN PXIFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
)
{
	NTSTATUS Status = STATUS_NOT_LOCKED;
	KIRQL SavedIrql;
	//PAGED_CODE();
	ASSERT_EXCLUSIVE_VCB(pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdUnlockVolumeInternal .\n"));



	IoAcquireVpbSpinLock( &SavedIrql );
	try{
		if(XifsdCheckFlagBoolean(pVCB->PtrVPB->Flags, VPB_LOCKED) &&
			pVCB->VCBLockFileObject == pFileObject)
		{
				XifsdClearFlag(pVCB->PtrVPB->Flags, VPB_LOCKED);
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
					("Enter XifsdUnlockVolumeInternal Unlock is done.\n"));
				XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED);
				
				pVCB->VCBLockFileObject = NULL;
				Status = STATUS_SUCCESS;
		}
	}finally{
		IoReleaseVpbSpinLock(SavedIrql);
	}


	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdUnlockVolumeInternal .\n"));
	return Status;
}


NTSTATUS
XixFsdLockVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;


	XifsdDebugTarget = DEBUG_TARGET_ALL;


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdLockVolume .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		XifsdDebugTarget = 0;
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);



	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(0x%x).\n", pIrpContext));

		XifsdDebugTarget = 0;
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		
		return RC;
	}
	

	

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{
		RC = XixFsdLockVolumeInternal(pIrpContext, pVCB, pFileObject);
	}finally{
		XifsdReleaseVcb(TRUE,pVCB);
	}

	if(NT_SUCCESS(RC)){
		FsRtlNotifyVolumeEvent(pFileObject, FSRTL_VOLUME_LOCK);
	}


	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdLockVolume .\n"));
	XifsdDebugTarget = 0;
	return RC;	
}

NTSTATUS
XixFsdUnlockVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;

	XifsdDebugTarget = DEBUG_TARGET_ALL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdUnlockVolume .\n"));

	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		XifsdDebugTarget = 0;
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);


	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

		XifsdDebugTarget = 0;
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}



	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	

	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{
			RC = XixFsdUnlockVolumeInternal(pIrpContext, pVCB, pFileObject);
	}finally{
		XifsdReleaseVcb(TRUE, pVCB);
	}



	if (NT_SUCCESS( RC )) {

		FsRtlNotifyVolumeEvent( pFileObject, FSRTL_VOLUME_UNLOCK );
	}
	
	XixFsdCompleteRequest(pIrpContext, RC, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XixFsdUnlockVolume .\n"));
	XifsdDebugTarget = 0;
	return RC;
}


NTSTATUS
XixFsdDismountVolume(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Enter XixFsdDismountVolume .\n"));


	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	

	
	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	
	if(ExAcquireResourceExclusiveLite(&(XiGlobalData.DataResource), TRUE)){
		if(!ExAcquireResourceExclusiveLite(&(pVCB->VCBResource), TRUE)){
			ExReleaseResourceLite(&(XiGlobalData.DataResource));
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

			RC = XixFsdPostRequest(pIrpContext, pIrp);
			return RC;
		}
	}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));
			RC = XixFsdPostRequest(pIrpContext, pIrp);
			return RC;
	}
	

	try{

		FsRtlNotifyVolumeEvent( pFileObject, FSRTL_VOLUME_DISMOUNT );
		
		
		


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

		XixFsdCleanupFlushVCB(pIrpContext, pVCB, TRUE);			

		XifsdLockVcb(pIrpContext, pVCB);
		pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
		XifsdSetFlag(pCCB->CCBFlags, XIFSD_CCB_DISMOUNT_ON_CLOSE);
		XifsdUnlockVcb(pIrpContext, pVCB);
		
		RC = STATUS_SUCCESS;
		

	}finally{
		DebugUnwind(XifsdDismountVolume);
		ExReleaseResourceLite(&(pVCB->VCBResource));
		ExReleaseResourceLite(&(XiGlobalData.DataResource));
		XixFsdCompleteRequest(pIrpContext, RC, 0);
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Exit Statue(0x%x) XixFsdDismountVolume .\n", RC));
	return RC;
	
}



NTSTATUS
XixFsdIsVolumeMounted(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PULONG				VolumeState = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdIsVolumeMounted .\n"));


	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));

		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	
	if(ExAcquireResourceExclusiveLite(&(XiGlobalData.DataResource), TRUE)){
		if(!ExAcquireResourceExclusiveLite(&(pVCB->VCBResource), TRUE)){
			ExReleaseResourceLite(&(XiGlobalData.DataResource));
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

			RC = XixFsdPostRequest(pIrpContext, pIrp);
			return RC;
		}
	}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
				("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

			RC = XixFsdPostRequest(pIrpContext, pIrp);
			return RC;
	}
	

	if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED){
		RC = STATUS_VOLUME_DISMOUNTED;

	}else {
		RC = STATUS_SUCCESS;
	}

	XixFsdCompleteRequest(pIrpContext, RC, 0);
	ExReleaseResourceLite(&(pVCB->VCBResource));
	ExReleaseResourceLite(&(XiGlobalData.DataResource));
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statue(0x%x) XixFsdIsVolumeMounted .\n", RC));
	return RC;
}

NTSTATUS
XixFsdIsPathnameValid(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
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

	TypeOfOpen = XixFsdDecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_SUCCESS;
        XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!XixFsdIsPathnameValid (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));
	

	XixFsdCompleteRequest(pIrpContext, RC, 0);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdIsPathnameValid .\n"));
	return RC;
}


NTSTATUS
XixFsdAllowExtendedDasdIo(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIRP				pIrp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PULONG					VolumeState = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_VCB				pVCB = NULL;	
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdAllowExtendedDasdIo .\n"));



	ASSERT(pIrpContext);
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	if (XixFsdDecodeFileObject( pFileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	


	XifsdSetFlag(pCCB->CCBFlags, XIFSD_CCB_ALLOW_XTENDED_DASD_IO);

	RC = STATUS_SUCCESS;
	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XixFsdAllowExtendedDasdIo .\n"));
	return RC;
}

NTSTATUS
XixFsdInvalidateVolumes(
	IN PXIFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
	)
{
	NTSTATUS	Status;
	PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( pIrp );
    KIRQL SavedIrql;
	LUID TcbPrivilege = {SE_TCB_PRIVILEGE, 0};
	HANDLE Handle = NULL;

	PXIFS_VCB pVCB;

	PLIST_ENTRY Links;

	PFILE_OBJECT FileToMarkBad;
	PDEVICE_OBJECT DeviceToMarkBad;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdInvalidateVolumes .\n"));

	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));

		XixFsdPostRequest(pIrpContext, pIrp);
		return STATUS_PENDING;
	}



	//Check Top level privilege
	if (!SeSinglePrivilegeCheck( TcbPrivilege, pIrp->RequestorMode )) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail is not top level privilege .\n"));

		XixFsdCompleteRequest( pIrpContext, STATUS_PRIVILEGE_NOT_HELD, 0);

		return STATUS_PRIVILEGE_NOT_HELD;
	}

	#if defined(_WIN64)
	if (IoIs32bitProcess( pIrp )) {
    
		if (pIrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof( UINT32 )) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail Invalid length .\n"));

			XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
			return STATUS_INVALID_PARAMETER;
		}

		Handle = (HANDLE) LongToHandle( *((PUINT32) pIrp->AssociatedIrp.SystemBuffer) );

	} else {
	#endif
		if (pIrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof( HANDLE )) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail Invalid length .\n"));
			
			XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER,0 );
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

		XixFsdCompleteRequest( pIrpContext, Status, 0 );
		return Status;
	}
	
	DeviceToMarkBad = FileToMarkBad->DeviceObject;
	ObDereferenceObject( FileToMarkBad );	


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Search Target DeviceObject 0x%x .\n", DeviceToMarkBad));


	XifsdAcquireGData(pIrpContext);

	Links = XiGlobalData.XifsVDOList.Flink;

	while (Links != &XiGlobalData.XifsVDOList) {

		pVCB = CONTAINING_RECORD( Links, XIFS_VCB, VCBLink );

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

				ASSERT( XifsdCheckFlagBoolean( pVCB->PtrVPB->Flags, VPB_MOUNTED));
				ASSERT(NULL != NewVpb);

				RtlZeroMemory(NewVpb, sizeof(VPB));
				NewVpb->Type = IO_TYPE_VPB;
				NewVpb->Size = sizeof(VPB);
				NewVpb->RealDevice = DeviceToMarkBad;
				NewVpb->Flags = XifsdCheckFlag( DeviceToMarkBad->Vpb->Flags, VPB_REMOVE_PENDING);
				DeviceToMarkBad->Vpb = NewVpb;
				pVCB->SwapVpb = NULL;
				
			}

			IoReleaseVpbSpinLock( SavedIrql );

			XifsdLockVcb(pIrpContext, pVCB);
			if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
				pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
			}
			XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);

			XifsdUnlockVcb(pIrpContext, pVCB);

			XixFsdPurgeVolume( pIrpContext, pVCB, FALSE );

			XifsdReleaseVcb(TRUE, pVCB);

			Status = CcWaitForCurrentLazyWriterActivity();

			XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);			

			XixFsdRealCloseFCB((PVOID)pVCB);

			XifsdLockVcb( pIrpContext, pVCB );
			XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
			XifsdUnlockVcb(pIrpContext, pVCB);

			XixFsdCheckForDismount( pIrpContext, pVCB, FALSE );
			
		} 
		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData( pIrpContext );

	XixFsdCompleteRequest( pIrpContext, STATUS_SUCCESS, 0 );
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdInvalidateVolumes .\n"));
	return STATUS_SUCCESS;
}


/*
 
   ||XIFS_OFFSET_VOLUME_LOT_LOC|XIFS_RESERVED_LOT_SIZE(24)|Lot||
		Reserved				   Reserved		
   
 */


NTSTATUS
XixFsdTransLotToCluster(
	IN PXIFS_VCB	pVCB,
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
	ASSERT(StartingCluster >= (uint32)(GetLcnFromLot(pVCB->LotSize, XIFS_RESERVED_LOT_SIZE)));

	ClusterPerLot = (uint32)(pVCB->LotSize/CLUSTER_SIZE);
	
	if(BytesToCopy ==0){
		return STATUS_SUCCESS;
	}
	
	for(i = XIFS_RESERVED_LOT_SIZE; i<pVCB->NumLots; i++){
		tLcn = (uint32)GetLcnFromLot(pVCB->LotSize, i);
		
		if(tLcn >= (uint32)(StartingCluster + BytesToCopy*8)){
			
			return STATUS_SUCCESS;
			
		}else{
		
			if(IsSetBit(i,pVCB->HostFreeLotMap->Data)){
				uint32	NumSetBit = 0;
				uint32	Offset = 0;
				uint32	Margin = 0;

				if((tLcn + ClusterPerLot) > (StartingCluster + BytesToCopy*8)){
					
					NumSetBit = StartingCluster + BytesToCopy * 8 - tLcn;		
					Offset = tLcn - StartingCluster;
					
					if(Offset % 8){
						Margin = (Offset %8);
						for(i = Offset; i<Margin; i++){
							set_bit(Offset, Buffer);
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
							set_bit(Offset, Buffer);
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
							set_bit(Offset, Buffer);
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
							set_bit(Offset, Buffer);
							Offset++;
							NumSetBit --;
						}
					}

				}
				
			}	//	if(IsSetBit(i,pVCB->HostFreeLotMap->Data)){
			

		}
	} //for(i = XIFS_RESERVED_LOT_SIZE; i<pVCB->NumLots; i++){



	return STATUS_SUCCESS;
	
}




NTSTATUS
XixFsdTransLotBitmapToClusterBitmap
(
	IN PXIFS_VCB	pVCB,
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
									pVCB->LotSize * XIFS_RESERVED_LOT_SIZE) / CLUSTER_SIZE) ;


	LastLcn = (uint32)StartingCluster;
	RemainingBytes = (uint32)BytesToCopy;


	if(StartingCluster < ReservedClusterAreadMax){
		uint32	BitCount = 0;
		uint32	Margin = 0;

		
		DbgPrint(" In Reserved Aread ReservedArea(%ld):LastLcn(%ld):RemainingBytes(%ld)\n",
			ReservedClusterAreadMax, LastLcn, RemainingBytes);

		BitCount = ReservedClusterAreadMax - StartingCluster;
		
		if((uint32)(BitCount/8) > BytesToCopy){
			return STATUS_SUCCESS;
		}else{
			LastLcn += BitCount;
			IndexOfBuffer += (uint32)(BitCount/8);
			RemainingBytes -= (uint32)(BitCount/8);
			ASSERT(RemainingBytes > 0);

			RC = XixFsdTransLotToCluster(pVCB,
							(uint32)((LastLcn/8)*8),
							RemainingBytes,
							&Buffer[IndexOfBuffer]
							);
		}

	}else{
		RC = XixFsdTransLotToCluster(pVCB,
						StartingCluster,
						BytesToCopy,
						Buffer
						);
	}

	return RC;
}




NTSTATUS
XixFsdGetVolumeBitMap(	
	IN PXIFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpSp = NULL;

	PXIFS_VCB			pVCB = NULL;
	PXIFS_FCB			pFCB = NULL;
	PXIFS_CCB			pCCB = NULL;

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

	if (XixFsdDecodeFileObject( pIrpSp->FileObject,&pFCB,&pCCB ) != UserVolumeOpen ) 
	{
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
		return STATUS_INVALID_PARAMETER;
	}

	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	if(!CanWait){
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}


	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	InputBufferLength = pIrpSp->Parameters.FileSystemControl.InputBufferLength;
	OutputBufferLength = pIrpSp->Parameters.FileSystemControl.OutputBufferLength;

	OutputBuffer = (PVOLUME_BITMAP_BUFFER)XixFsdGetCallersBuffer( pIrp );

	//
	//  Check for a minimum length on the input and output buffers.
	//

	if ((InputBufferLength < sizeof(STARTING_LCN_INPUT_BUFFER)) ||
		(OutputBufferLength < sizeof(VOLUME_BITMAP_BUFFER))) {

		XixFsdCompleteRequest( pIrpContext, STATUS_BUFFER_TOO_SMALL, 0 );
		return STATUS_BUFFER_TOO_SMALL;
	}

	TotalClusters = (uint32)((pVCB->NumLots * (pVCB->LotSize/CLUSTER_SIZE)) + (XIFS_OFFSET_VOLUME_LOT_LOC/CLUSTER_SIZE));


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
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER , 0 );
		return STATUS_INVALID_PARAMETER ;		
	}else{
		StartingCluster = (StartingLcn.LowPart & ~7);
	}
	
	
    
	OutputBufferLength -= FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer);
	DesiredClusters = TotalClusters - StartingCluster;

	if (OutputBufferLength < (DesiredClusters + 7) / 8) {

		DbgPrint("Overflow OutputBufferLegngth (%ld) : DesiredClusters(%ld): BytesToCopy(%ld)\n",
			OutputBufferLength, DesiredClusters, BytesToCopy);

		BytesToCopy = OutputBufferLength;
		RC = STATUS_BUFFER_OVERFLOW;

	} else {

		DbgPrint("OutputBufferLegngth (%ld) : DesiredClusters(%ld): BytesToCopy(%ld)\n",
			OutputBufferLength, DesiredClusters, BytesToCopy);

		BytesToCopy = (DesiredClusters + 7) / 8;
		RC = STATUS_SUCCESS;
	}
	
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	try{

		try{
			XixFsdTransLotBitmapToClusterBitmap(pVCB, 
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
		XixFsdCompleteRequest( pIrpContext, RC, (uint32)pIrp->IoStatus.Information);
	}

	return RC;


}


NTSTATUS
XixFsdUserFsctl(
	IN PXIFS_IRPCONTEXT pIrpContext
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
		("Enter XixFsdUserFsctl  pIrpSp->Parameters.FileSystemControl.FsControlCode 0x%x.\n",
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

	

	
		    RC= XixFsdOplockRequest( pIrpContext, pIrp);
		    break;

		case FSCTL_LOCK_VOLUME :
			

		    
			RC = XixFsdLockVolume( pIrpContext, pIrp );
		    break;

		case FSCTL_UNLOCK_VOLUME :
			

		    
		    RC= XixFsdUnlockVolume( pIrpContext, pIrp );
		    break;

		case FSCTL_DISMOUNT_VOLUME :
			

		    DbgPrint("FSCTL_DISMOUNT_VOLUME");
		   RC = XixFsdDismountVolume( pIrpContext, pIrp );
		    break;


		case FSCTL_IS_VOLUME_MOUNTED :
			

		    
		    RC = XixFsdIsVolumeMounted( pIrpContext, pIrp );
		    break;

		case FSCTL_IS_PATHNAME_VALID :
			

		 
		    RC = XixFsdIsPathnameValid( pIrpContext, pIrp );
		    break;
		    
		case FSCTL_ALLOW_EXTENDED_DASD_IO:
			

		 
		    RC = XixFsdAllowExtendedDasdIo( pIrpContext, pIrp );
		    break;
		case FSCTL_INVALIDATE_VOLUMES:

			RC = XixFsdInvalidateVolumes(pIrpContext, pIrp);
			break;

		case FSCTL_QUERY_RETRIEVAL_POINTERS:
			DbgPrint("!!! CALL FSCTL_QUERY_RETRIEVAL_POINTERS\n");
			RC = STATUS_NOT_IMPLEMENTED;
			XixFsdCompleteRequest( pIrpContext, RC, 0);
		    break;
		case FSCTL_GET_RETRIEVAL_POINTERS:
			DbgPrint("!!! CALL FSCTL_GET_RETRIEVAL_POINTERS\n");
			RC = STATUS_NOT_IMPLEMENTED;
			XixFsdCompleteRequest( pIrpContext, RC, 0);
		    break;
		case FSCTL_GET_VOLUME_BITMAP:
			DbgPrint("!!! FSCTL_GET_VOLUME_BITMAP\n");
			RC = XixFsdGetVolumeBitMap(pIrpContext, pIrp);
		    break;
		case FSCTL_MOVE_FILE:
			DbgPrint("!!! FSCTL_MOVE_FILE\n");
			RC = STATUS_NOT_IMPLEMENTED;
			XixFsdCompleteRequest( pIrpContext, RC, 0);
		    break;
		//
		//  We don't support any of the known or unknown requests.
		//

		default:
			RC = STATUS_INVALID_DEVICE_REQUEST;
			XixFsdCompleteRequest( pIrpContext, RC, 0);
		    break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(%lx) XixFsdUserFsctl .\n", RC));
	return RC;
}


NTSTATUS
XixFsdCommonFileSystemControl(
	IN PXIFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS 	RC;
	PAGED_CODE();

	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);
	ASSERT(IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL);


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!XixFsdCommonFileSystemControl \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdCommonFileSystemControl .\n"));


	switch(IrpContext->MinorFunction){
	case IRP_MN_MOUNT_VOLUME:
		RC = XixFsdMountVolume( IrpContext);
		break;

	case IRP_MN_VERIFY_VOLUME:
		RC = XixFsdVerifyVolume( IrpContext);
		break;

	case IRP_MN_KERNEL_CALL: 
	case IRP_MN_USER_FS_REQUEST:
		RC = XixFsdUserFsctl( IrpContext);
		break;

	default:
		XixFsdCompleteRequest(IrpContext, STATUS_UNRECOGNIZED_VOLUME, 0);
		RC = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(%lx) XixFsdCommonFileSystemControl .\n", RC));
	return RC;

}





NTSTATUS
XixFsdCommonShutdown(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIRP					NewIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	IO_STATUS_BLOCK			IoStatus;
	PXIFS_VCB				pVCB = NULL;
	PLIST_ENTRY 			pListEntry = NULL;
	KEVENT 					Event;
	BOOLEAN					IsVCBPresent = TRUE;

	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonShutdown \n"));

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
		("Enter XixFsdCommonShutdown .\n"));

	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT))
	{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("Post Process Request IrpContext(%p) Irp(%p).\n", pIrpContext,  pIrp));
		
		XixFsdPostRequest(pIrpContext, pIrp);
		return STATUS_PENDING;
	}

	XifsdAcquireGData(pIrpContext);

	
	try{

		
 		KeInitializeEvent( &Event, NotificationEvent, FALSE );

		pListEntry = XiGlobalData.XifsVDOList.Flink;
		while(pListEntry != &(XiGlobalData.XifsVDOList)){
			pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);
			ASSERT(pVCB);

			if( (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS)
					&& (pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED)) 
			{

				IsVCBPresent = TRUE;
				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL ),
					("Shutdown VCB (%lx) .\n", pVCB));

				XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);

				XifsdLockVcb(pIrpContext, pVCB);
				XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
				XifsdUnlockVcb(pIrpContext, pVCB);
				//
				//	Release System Resource
				//

				//
				//	Release System Resource
				//

				XixFsdCleanupFlushVCB(pIrpContext, pVCB, TRUE);			
				
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
				IsVCBPresent = XixFsdCheckForDismount(pIrpContext, pVCB, TRUE);
				
				if(IsVCBPresent){
					XifsdLockVcb(pIrpContext, pVCB);
					XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
					XifsdUnlockVcb(pIrpContext, pVCB);

					XifsdReleaseVcb(TRUE, pVCB);
				}
			}
			pListEntry = pListEntry->Flink;
		}
		ExReleaseResourceLite(&(XiGlobalData.DataResource));
	


	}finally{
		
		DebugUnwind(XifsdCommonShutdown);
		if(RC != STATUS_PENDING){
			XixFsdCompleteRequest(pIrpContext, RC, 0 );
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO|DEBUG_TARGET_ALL),
		("Exit XifsdCommonShutdown .\n"));

	return RC;
}