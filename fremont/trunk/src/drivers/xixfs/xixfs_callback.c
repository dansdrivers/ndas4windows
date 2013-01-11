#include "xixfs_types.h"
#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xcsystem/winnt/xcsysdep.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/evtpkt.h"

#include "xixfs_drv.h"
#include "xixfs_event.h"
#include "xixfs_global.h"

#include "ndasscsi.h"

#define NTSTRSAFE_LIB
#include <ntddscsi.h>

#include <Ntstrsafe.h>


#define HTONS(Data)		(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))
#define NTOHS(Data)		(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))

#define HTONL(Data)		( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))
#define NTOHL(Data)		( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))

#define HTONLL(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))

#define NTOHLL(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))




#ifndef RtlInitEmptyUnicodeString

#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
		((_ucStr)->Buffer = (_buf), \
		(_ucStr)->Length = 0, \
		(_ucStr)->MaximumLength = (USHORT)(_bufSize))

#endif

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSCALLBACK"


extern NPAGED_LOOKASIDE_LIST	XifsCoreBuffHeadList;

xc_le16 *
xixcore_call
xixcore_AllocateUpcaseTable()
{
	return  (xc_le16 *)ExAllocatePoolWithTag(NonPagedPool,(XIXCORE_DEFAULT_UPCASE_NAME_LEN*sizeof(xc_le16)), TAG_UPCASETLB);
}

/*
 * Free a memory block for default upcase table.
 */
void
xixcore_call
xixcore_FreeUpcaseTable(
	xc_le16 *UpcaseTable
)
{
	ExFreePoolWithTag((PVOID)UpcaseTable, TAG_UPCASETLB);
	return;
}

/*
 * Allocate an Xixcore buffer.
 */

PXIXCORE_BUFFER
xixcore_call
xixcore_AllocateBuffer(xc_uint32 Size)
{
	PXIXCORE_BUFFER xixbuff = NULL;
	uint8			*buff = NULL;
	xixbuff = ExAllocateFromNPagedLookasideList(&(XifsCoreBuffHeadList));
	if(!xixbuff) {
		xixbuff = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIXCORE_BUFFER), TAG_COREBUFF);
		if(!xixbuff) {
			return NULL;
		}
		RtlZeroMemory(xixbuff, sizeof(XIXCORE_BUFFER));
		XIXCORE_SET_FLAGS(xixbuff->xcb_flags, XIXCORE_BUFF_ALLOC_F_MEMORY);
	}else {
		RtlZeroMemory(xixbuff, sizeof(XIXCORE_BUFFER));
		XIXCORE_SET_FLAGS(xixbuff->xcb_flags, XIXCORE_BUFF_ALLOC_F_POOL);
	}

	buff = ExAllocatePoolWithTag(NonPagedPool, Size, TAG_BUFFER);

	if(!buff) {
		xixcore_FreeBuffer(xixbuff);
		return NULL;
	}

	xixcore_InitializeBuffer(
		xixbuff,
		buff,
		Size,
		0
		);

	return xixbuff;

}

/*
 * Free the Xixcore buffer.
 */

void
xixcore_call
xixcore_FreeBuffer(PXIXCORE_BUFFER XixcoreBuffer)
{

	if(XixcoreBuffer->xcb_data){
		ExFreePool(XixcoreBuffer->xcb_data);
	}

	if(XIXCORE_TEST_FLAGS(XixcoreBuffer->xcb_flags,XIXCORE_BUFF_ALLOC_F_MEMORY)){
		ExFreePool(XixcoreBuffer);
	}else{
		ExFreeToNPagedLookasideList(&(XifsCoreBuffHeadList),XixcoreBuffer);	
	}

}

/*
 * Acquire the shared device lock
 */
int 
xixcore_call
xixcore_AcquireDeviceLock(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	)
{
	NTSTATUS			RC;
	PDEVICE_OBJECT		DeviceObject = NULL;
	PSRB_IO_CONTROL		pSrbIoCtl = NULL;
	xc_uint32			OutbuffSize = 0;
	PNDSCIOCTL_DEVICELOCK	QueryLock = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = 0;

	DeviceObject = &(XixcoreBlockDevice->impl);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsNdasLock \n"));

	result = xixfs_GetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsGetScsiportAdpater!!! \n"));
		return XCCODE_UNSUCCESS;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) + sizeof(NDSCIOCTL_DEVICELOCK);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return XCCODE_ENOMEM;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = NDASSCSI_IOCTL_DEVICE_LOCK;
		pSrbIoCtl->Length = sizeof(NDSCIOCTL_DEVICELOCK);

		QueryLock = (PNDSCIOCTL_DEVICELOCK)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));

		QueryLock->LockId = NDSCLOCK_ID_XIFS;
		QueryLock->LockOpCode = LURNLOCK_OPCODE_ACQUIRE;
		QueryLock->AdvancedLock = FALSE;
		QueryLock->AddressRangeValid = FALSE;
		QueryLock->RequireLockAcquisition = FALSE;
		QueryLock->StartingAddress = 0;
		QueryLock->EndingAddress = 0;
		QueryLock->ContentionTimeOut = 0;
	
		
		//Fist step Send Disk
		RC = xixfs_RawDevIoCtrl ( 
						DeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
					RC,
					QueryLock->LockData[0],	QueryLock->LockData[1],
					QueryLock->LockData[2],	QueryLock->LockData[3],
					QueryLock->LockData[4],	QueryLock->LockData[5],
					QueryLock->LockData[6],	QueryLock->LockData[7] ));
		}else{
			// Send port
			RC = xixfs_RawDevIoCtrl ( 
							scsiAdpaterDeviceObject,
							IOCTL_SCSI_MINIPORT,
							(uint8 *)pSrbIoCtl,
							OutbuffSize,
							(uint8 *)pSrbIoCtl,
							OutbuffSize,
							FALSE,
							NULL
							);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixfs_RawDevIoCtrl Status (0x%x)\n", RC));
			}else{
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
						RC,
						QueryLock->LockData[0],	QueryLock->LockData[1],
						QueryLock->LockData[2],	QueryLock->LockData[3],
						QueryLock->LockData[4],	QueryLock->LockData[5],
						QueryLock->LockData[6],	QueryLock->LockData[7] ));
			}
		}


	

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsNdasLock Status (0x%x)\n", RC));

	return (int)RC;
}

/*
 * Release the shared device lock
 */
int 
xixcore_call
xixcore_ReleaseDevice(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	)
{
	NTSTATUS		RC;
	PDEVICE_OBJECT DeviceObject = NULL;
	PSRB_IO_CONTROL	pSrbIoCtl = NULL;
	uint32			OutbuffSize = 0;
	PNDSCIOCTL_DEVICELOCK	QueryLock = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;
	
	DeviceObject = &(XixcoreBlockDevice->impl);
	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsNdasUnLock \n"));

	result = xixfs_GetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail GetScsiportAdpater!!! \n"));
		return XCCODE_UNSUCCESS;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) +  sizeof(NDSCIOCTL_DEVICELOCK);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return XCCODE_ENOMEM;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = NDASSCSI_IOCTL_DEVICE_LOCK;
		pSrbIoCtl->Length = sizeof(NDSCIOCTL_DEVICELOCK);

		QueryLock = (PNDSCIOCTL_DEVICELOCK)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		QueryLock->LockId = NDSCLOCK_ID_XIFS;
		QueryLock->LockOpCode = LURNLOCK_OPCODE_RELEASE;
		QueryLock->AdvancedLock = FALSE;
		QueryLock->AddressRangeValid = FALSE;
		QueryLock->RequireLockAcquisition = FALSE;
		QueryLock->StartingAddress = 0;
		QueryLock->EndingAddress = 0;
		QueryLock->ContentionTimeOut = 0;
		

		//Fist step Send Disk
		RC = xixfs_RawDevIoCtrl ( 
						DeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
					RC,
					QueryLock->LockData[0],	QueryLock->LockData[1],
					QueryLock->LockData[2],	QueryLock->LockData[3],
					QueryLock->LockData[4],	QueryLock->LockData[5],
					QueryLock->LockData[6],	QueryLock->LockData[7] ));
		}else{
			// Send port
			RC = xixfs_RawDevIoCtrl ( 
							scsiAdpaterDeviceObject,
							IOCTL_SCSI_MINIPORT,
							(uint8 *)pSrbIoCtl,
							OutbuffSize,
							(uint8 *)pSrbIoCtl,
							OutbuffSize,
							FALSE,
							NULL
							);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixfs_RawDevIoCtrl Status (0x%x)\n", RC));
			}else{
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
						RC,
						QueryLock->LockData[0],	QueryLock->LockData[1],
						QueryLock->LockData[2],	QueryLock->LockData[3],
						QueryLock->LockData[4],	QueryLock->LockData[5],
						QueryLock->LockData[6],	QueryLock->LockData[7] ));
			}
		}


	

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsNdasUnLock Status (0x%x)\n", RC));

	return RC;
}

/*
 * Ask a host if it has the lot lock.
 */

int
xixcore_call
xixcore_HaveLotLock(
	xc_uint8		* HostMac,
	xc_uint8		* LockOwnerMac,
	xc_uint64		LotNumber,
	xc_uint8		* VolumeId,
	xc_uint8		* LockOwnerId
	)
{
	NTSTATUS					RC = STATUS_UNSUCCESSFUL;
	PXIXFSDG_PKT				pPacket = NULL;
	PXIXFS_LOCK_REQUEST			pPacketData = NULL;
	XIFS_LOCK_CONTROL			LockControl;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_HaveLotLock \n"));

	
	if(FALSE == xixfs_AllocDGPkt(&pPacket, HostMac, LockOwnerMac, XIXFS_TYPE_LOCK_REQUEST))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_HaveLotLock:xixfs_AllocDGPkt  \n"));
		return XCCODE_ENOMEM;
	}

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Packet Dest Info  [0x%02x:%02x:%02x:%02x:%02x:%02x]\n",
		LockOwnerMac[26], LockOwnerMac[27], LockOwnerMac[28],
		LockOwnerMac[29], LockOwnerMac[30], LockOwnerMac[31]));

	pPacketData = &(pPacket->RawDataDG.LockReq);
	RtlCopyMemory(pPacketData->LotOwnerID, LockOwnerId, 16);
	RtlCopyMemory(pPacketData->VolumeId, VolumeId, 16);
	RtlCopyMemory(pPacketData->LotOwnerMac, LockOwnerMac, 32);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->PacketNumber = HTONL(XiGlobalData.XifsComCtx.PacketNumber);
	XiGlobalData.XifsComCtx.PacketNumber++;

	pPacket->TimeOut.QuadPart = xixcore_GetCurrentTime64() + DEFAULT_REQUEST_MAX_TIMEOUT;


	KeInitializeEvent(&LockControl.KEvent, SynchronizationEvent, FALSE);
	LockControl.Status = LOCK_INVALID;

	pPacket->pLockContext = &LockControl;


	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	RC = KeWaitForSingleObject(&LockControl.KEvent, Executive, KernelMode, FALSE, NULL);

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit xixcore_HaveLotLock \n"));
		return RC;
	}

	if(LockControl.Status == LOCK_OWNED_BY_OWNER){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit xixcore_HaveLotLock Lock is realy acquired by other \n"));
		return STATUS_SUCCESS;
	}


	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdAreYouHaveLotLock Lock is status(0x%x) \n", LockControl.Status));
	/*
		TRUE --> Lock is Owner by Me
		FALSE --> Lock is Not Mine
		FALSE--> TimeOut Lock Owner is not in Network
	*/
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdAreYouHaveLotLock \n"));

	return RC;
}

/*
 * perform raw IO to a block device
 */

int
xixcore_call
xixcore_DeviceIoSync(
	PXIXCORE_BLOCK_DEVICE XixcoreBlkDev,
	xc_sector_t startsector,
	xc_int32 size, 
	xc_uint32 sectorsize,
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER XixcoreBuffer,
	xc_int32 xixfsCoreRw,
	xc_int32 * Reason
	)
{
	PDEVICE_OBJECT DeviceObject = NULL;
	KEVENT			event;
	PIRP			irp;
	IO_STATUS_BLOCK io_status;
	NTSTATUS		status;

	LARGE_INTEGER	NewOffset = {0,0};
	uint8			*NewBuffer = NULL;
	uint32			NewLength = 0;
	BOOLEAN			bNewAligned = FALSE;
	uint64			tmpSectorSize = sectorsize;
	DeviceObject = &(XixcoreBlkDev->impl);



	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_DeviceIoSync \n"));

	XIXCORE_ASSERT(DeviceObject != NULL);
	XIXCORE_ASSERT(XixcoreBuffer != NULL);
	XIXCORE_ASSERT(size <= (xc_int32)(XixcoreBuffer->xcb_size - XixcoreBuffer->xcb_offset));
	XIXCORE_ASSERT(XixcoreBuffer->xcb_data != NULL);


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("xixcore_DeviceIoSync startsector (0x%I64x)  Length(%d)\n", 
		startsector,  size));	


	NewOffset.QuadPart = tmpSectorSize * startsector;

	NewLength = (uint32) SECTOR_ALIGNED_SIZE(sectorsizeBit, size);

	if(NewLength > (XixcoreBuffer->xcb_size - XixcoreBuffer->xcb_offset))
	{
		bNewAligned = TRUE;
		NewBuffer = ExAllocatePoolWithTag(NonPagedPool, NewLength, TAG_BUFFER);
		if(!NewBuffer){
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(NewBuffer, NewLength);
		if(xixfsCoreRw == XIXCORE_WRITE) {
			RtlCopyMemory(NewBuffer,(XixcoreBuffer->xcb_data + XixcoreBuffer->xcb_offset),size);
		}

	}else{
		NewBuffer = XixcoreBuffer->xcb_data + XixcoreBuffer->xcb_offset;
	}




	KeInitializeEvent(&event, NotificationEvent, FALSE);


	if(xixfsCoreRw == XIXCORE_READ) {
	
		irp = IoBuildSynchronousFsdRequest(
			IRP_MJ_READ,
			DeviceObject,
			NewBuffer,
			NewLength,
			&NewOffset,
			&event,
			&io_status
			);

		if (!irp)
		{
			if(bNewAligned){
				ExFreePool(NewBuffer);
			}

			return STATUS_INSUFFICIENT_RESOURCES;
		}

	}else{
		irp = IoBuildSynchronousFsdRequest(
			IRP_MJ_WRITE,
			DeviceObject,
			NewBuffer,
			NewLength,
			&NewOffset,
			&event,
			&io_status
			);

		if (!irp)
		{
			if(bNewAligned){
				ExFreePool(NewBuffer);
			}

			return STATUS_INSUFFICIENT_RESOURCES;
		}	

	}

	status = IoCallDriver(DeviceObject, irp);

	if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = io_status.Status;
	}

	if(xixfsCoreRw == XIXCORE_READ) {
		if(bNewAligned){
			RtlCopyMemory((XixcoreBuffer->xcb_data + XixcoreBuffer->xcb_offset),NewBuffer,size);
			ExFreePool(NewBuffer);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixfs_RawReadBlockDevice \n"));
	return status;


}

/*
 * Notify file changes
 */

void
xixcore_call
xixcore_NotifyChange(
	PXIXCORE_VCB XixcoreVcb,
	xc_uint32 VCBMetaFlags
	)
{
	PXIXFS_VCB	VCB = NULL;
	VCB = CONTAINING_RECORD( XixcoreVcb, XIXFS_VCB, XixcoreVcb );
	KeSetEvent(&VCB->VCBUpdateEvent, 0, FALSE);
}

/*
 * Wait for meta resource event
 */

int
xixcore_call
xixcore_WaitForMetaResource(
	PXIXCORE_VCB	XixcoreVcb
	)
{
	PXIXFS_VCB	VCB = NULL;
	LARGE_INTEGER	TimeOut;
	NTSTATUS		RC = STATUS_SUCCESS;

	VCB = CONTAINING_RECORD( XixcoreVcb, XIXFS_VCB, XixcoreVcb );

	KeClearEvent(&VCB->ResourceEvent);
	KeSetEvent(&VCB->VCBGetMoreLotEvent, 0, FALSE);

	TimeOut.QuadPart = - DEFAULT_XIFS_RECHECKRESOURCE;
	RC = KeWaitForSingleObject(&VCB->ResourceEvent,
						Executive,
						KernelMode,
						FALSE,
						&TimeOut);

	if(!NT_SUCCESS(RC)){
		return -1;
	}else {
		return 1;
	}
}