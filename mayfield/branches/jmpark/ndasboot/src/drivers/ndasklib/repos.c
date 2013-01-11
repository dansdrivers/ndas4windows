#ifdef __NDASBOOT__
#ifndef __ENABLE_LOADER__

#include <ntddk.h>
#include <stdio.h>

#include "ver.h"
#include "LSKLib.h"
#include "basetsdex.h"
#include "cipher.h"
#include "hdreg.h"
#include "binparams.h"
#include "hash.h"

#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnIde.h"


LIST_ENTRY RepositCcbQueue;

NTSTATUS InsertToReposite(PCCB Ccb)
{
	PCCB TmpCcb;
	NTSTATUS status;

	if(Ccb->CcbStatus == CCB_STATUS_BUSY) {
		KDPrintM(DBG_LURN_INFO, ("don't need to insert, Ccb = %08X\n", Ccb));
		return STATUS_SUCCESS;
	}

	status = LSCcbAllocate(&TmpCcb);
	if(!NT_SUCCESS(status)) {										
		KDPrint(1,("LSCcbAllocate() failed.\n" ));
		return status;
	}

	if(Ccb) RtlCopyMemory(TmpCcb, Ccb, sizeof(CCB));
	TmpCcb->DataBuffer = ExAllocatePoolWithTag(NonPagedPool, TmpCcb->DataBufferLength, LURNEXT_POOL_TAG);
	if(TmpCcb->DataBuffer) RtlCopyMemory(TmpCcb->DataBuffer, Ccb->DataBuffer, TmpCcb->DataBufferLength);
		
	TmpCcb->SenseBuffer = ExAllocatePoolWithTag(NonPagedPool, TmpCcb->SenseDataLength, LURNEXT_POOL_TAG);
	if(TmpCcb->SenseBuffer)	RtlCopyMemory(TmpCcb->SenseBuffer, Ccb->SenseBuffer, TmpCcb->SenseDataLength);

	KDPrintM(DBG_LURN_INFO, ("Insert to RepositList, Ccb = %08X\n", TmpCcb));
	InsertTailList(&RepositCcbQueue, &TmpCcb->ListEntry);

	return STATUS_SUCCESS;
}

extern NTSTATUS
LurnIdeDiskExecute(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeDisk,
		IN	PCCB				Ccb
	);

NTSTATUS ExecuteReposite(PLURELATION_NODE Lurn)
{
	PLIST_ENTRY ListEntry;
	PCCB Ccb;
	NTSTATUS status;

	while( 1 ) {	

		ListEntry = RemoveHeadList(&RepositCcbQueue);

		if(ListEntry == (&RepositCcbQueue)) {
			KDPrintM(DBG_LURN_TRACE, ("Empty.\n"));
			break;
		}
		Ccb = CONTAINING_RECORD(ListEntry, CCB, ListEntry);

		KDPrintM(DBG_LURN_INFO, ("RECONN: Remove Entry in RepositList, Ccb = %08X\n", Ccb));

		status = LurnIdeDiskExecute(Lurn, Lurn->LurnExtension, Ccb);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("LurnIdeDiskExecute RepositCcbQeue Error = %08X\n", status));
		}
	
		if(Ccb->DataBuffer) ExFreePool(Ccb->DataBuffer);
		if(Ccb->SecondaryBuffer) ExFreePool(Ccb->SecondaryBuffer);
		if(Ccb) ExFreePool(Ccb);
	}

	return STATUS_SUCCESS;
}

#endif
#endif