#ifndef __LANSCSI_MINIPORT_H__
#define __LANSCSI_MINIPORT_H__

#define __LANSCSI_BUS__

#include <scsi.h>
#include "oem.h"
#include "LSKLib.h"
#include "LSLurn.h"
#include "LSCcb.h"

//////////////////////////////////////////////////////////////////////////
//
//	Defines
//
//
//	Adapter information for HwInitialize()
//
#define BLOCK_SIZE_BITS				9						// 2 ^ BLOCK_SIZE_BITS
#define BLOCK_SIZE					(1<<BLOCK_SIZE_BITS)	// 2 ^ 9 = 512 Bytes
#define MAX_SG_DESCRIPTORS			17

//
//	define new SCSI operation for the completion notification
//
#define SCSIOP_COMPLETE		0xFE

//
//	Pool tags
//
#define	LSMP_PTAG_IOCTL		'oiML'
#define LSMP_PTAG_WORKCTX	'cwML'
#define LSMP_PTAG_SRB		'rsML'
#define LSMP_PTAG_CMPDATA	'mcML'

//
//	Defines for reconnecting
//
#define MAX_RECONNECTION_INTERVAL	(NANO100_PER_SEC*2)
#define RECONNECTION_MAX_TRY		20


//////////////////////////////////////////////////////////////////////////
//
//	Structures
//	Keep the same values as in LSMPIoctl.h
//

#define ADAPTER_STATUS_MASK						0x000000ff
#define	ADAPTER_STATUS_INIT						0x00000000
#define	ADAPTER_STATUS_RUNNING					0x00000001
#define ADAPTER_STATUS_STOPPING					0x00000002
#define ADAPTER_STATUS_IN_ERROR					0x00000003
#define ADAPTER_STATUS_STOPPED					0x00000004

#define ADAPTER_STATUSFLAG_MASK					0xffffff00
#define ADAPTER_STATUSFLAG_RECONNECTPENDING		0x00000100
#define ADAPTER_STATUSFLAG_POWERSAVING_PENDING	0x00000200
#define ADAPTER_STATUSFLAG_BUSRESET_PENDING		0x00000400
#define ADAPTER_STATUSFLAG_MEMBER_FAULT			0x00000800

#define ADAPTER_ISSTATUS(PHWDEV, STATUS)		\
		(((PHWDEV)->AdapterStatus & ADAPTER_STATUS_MASK) == (STATUS))
#define ADAPTER_SETSTATUS(PHWDEV, STATUS)		\
		( (PHWDEV)->AdapterStatus = ((PHWDEV)->AdapterStatus & (ADAPTER_STATUSFLAG_MASK)) | (STATUS))

#define ADAPTER_ISSTATUSFLAG(PHWDEV, STATUSFLAG)		\
		(((PHWDEV)->AdapterStatus & (STATUSFLAG)) != 0)
#define ADAPTER_SETSTATUSFLAG(PHWDEV, STATUSFLAG)		\
		( (PHWDEV)->AdapterStatus |= (STATUSFLAG))
#define ADAPTER_RESETSTATUSFLAG(PHWDEV, STATUSFLAG)		\
		( (PHWDEV)->AdapterStatus &= ~(STATUSFLAG))



#define NR_MAX_LANSCSIMINIPORT_LURS				1


typedef struct _MINIPORT_DEVICE_EXTENSION {
	PVOID			ScsiportFdoObject;

	//
	//	Lanscsi adapter status
	//
	KSPIN_LOCK		LanscsiAdapterSpinLock;	// protect AdapterStatus, LastBusResetTime.
	ULONG			AdapterStatus;
	PKEVENT			DisconEventToService;
	PKEVENT			AlarmEventToService;
	BOOLEAN			TimerOn;					// protected by Scsiport synch.

	LONG			RequestExecuting;			// protected by Scsiport synch.
	KSPIN_LOCK		CcbCompletionListSpinLock;
	LIST_ENTRY		CcbCompletionList;

	//
	//	Lanscsi adapter spec information.
	//
	ULONG			SlotNumber;
	UCHAR			InitiatorId;
    UCHAR			NumberOfBuses;
    UCHAR			MaximumNumberOfTargets;
    UCHAR			MaximumNumberOfLogicalUnits;
	ULONG			MaxBlocksPerRequest;
	LARGE_INTEGER	EnabledTime;
	LARGE_INTEGER	LastBusResetTime;

	//
	//	Ccb allocation pool
	//
	NPAGED_LOOKASIDE_LIST	CcbLookaside;

	//
	//	LUR
	//
	LONG			LURCount;
	PLURELATION		LURs[NR_MAX_LANSCSIMINIPORT_LURS];

} MINIPORT_DEVICE_EXTENSION, *PMINIPORT_DEVICE_EXTENSION;


typedef struct _MINIPORT_LU_EXTENSION {

	PLURELATION		LUR;

} MINIPORT_LU_EXTENSION, *PMINIPORT_LU_EXTENSION;


//
//	Workitem context
//
typedef struct _LSMP_WORKITEM_CTX LSMP_WORKITEM_CTX, *PLSMP_WORKITEM_CTX;

typedef
VOID
(*PLSMP_WORKITEM_ROUTINE) (
    IN PDEVICE_OBJECT DeviceObject,
    IN PLSMP_WORKITEM_CTX Context
    );

typedef struct _LSMP_WORKITEM_CTX {

	PLSMP_WORKITEM_ROUTINE		WorkitemRoutine;
	PCCB						Ccb;
	PVOID						Arg1;
	PVOID						Arg2;
	PVOID						Arg3;

	PIO_WORKITEM				WorkItem;

} LSMP_WORKITEM_CTX, *PLSMP_WORKITEM_CTX;


#define LSMP_INIT_WORKITEMCTX(WICTX_POINTER, WIROUTINE, CCB, ARG1, ARG2, ARG3) {		\
	(WICTX_POINTER)->WorkitemRoutine = (WIROUTINE);										\
	(WICTX_POINTER)->Ccb = (CCB);														\
	(WICTX_POINTER)->Arg1 = (ARG1);														\
	(WICTX_POINTER)->Arg2 = (ARG2);														\
	(WICTX_POINTER)->Arg3 = (ARG3); }



//////////////////////////////////////////////////////////////////////////
//
//	exported functions to LanscsiMinport
//

//
//	LanscsiMiniport.c
//
NTSTATUS
LSMP_QueueMiniportWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PLSMP_WORKITEM_CTX			TmpWorkitemCtx
	);

NTSTATUS
SendCcbToLURSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PLURELATION		LUR,
		UINT32			CcbOpCode
	);

VOID
MiniStopAdapter(
	    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN BOOLEAN						NotifyService
	);

//
//
//	LSMPIoctl.c
//
NTSTATUS
MiniSrbControl(
	   IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	   IN PMINIPORT_LU_EXTENSION		LuExtension,
	   IN PSCSI_REQUEST_BLOCK			Srb
	);

NTSTATUS
LSMPSendIoctlSrb(
		IN PDEVICE_OBJECT	DeviceObject,
		IN ULONG			IoctlCode,
		IN PVOID			InputBuffer,
		IN LONG				InputBufferLength,
		OUT PVOID			OutputBuffer,
		IN LONG				OutputBufferLength
	);

//
//	Communication.c
//
PDEVICE_OBJECT
FindScsiportFdo(
		IN OUT	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
	);

ULONG
GetScsiAdapterPdoEnumInfo(
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN ULONG						SystemIoBusNumber,
	OUT PLONG						AddTargetDataLength,
	OUT PLANSCSI_ADD_TARGET_DATA	*AddTargetData
);

NTSTATUS
IoctlToLanscsiBus(
	IN ULONG  IoControlCode,
    IN PVOID  InputBuffer  OPTIONAL,
    IN ULONG  InputBufferLength,
    OUT PVOID  OutputBuffer  OPTIONAL,
    IN ULONG  OutputBufferLength,
	OUT PLONG	BufferNeeded
);

VOID
UpdatePdoInfoInLSBus(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						AdapterStatus
	);

VOID
MiniTimer(
    IN PVOID HwDeviceExtension
    );


NTSTATUS
LanscsiMiniportCompletion(
		  IN PCCB							Ccb,
		  IN PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension
	);

VOID
LsmpLurnCallback(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
	);


#endif // __LANSCSI_MINIPORT_H__
