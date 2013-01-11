#pragma once
//
// Windows kernel library headers
//

#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <ntintsafe.h>
#include <windef.h>

//
//	NDAS common headers
//

#include "public/ndas/ndasiomsg.h"
#include "ndas/ndasdib.h"

//
//	kernel library headers
//
#include "ndascommonheader.h"
#include "ndasscsi.h"
#include "ndasbusioctl.h"

#include "kdebug.h"
#include "ndasklib.h"

//
//	Pool tags
//
#define	NDAS_DLU_PTAG_IOCTL			'oiUN'
#define NDAS_DLU_PTAG_WORKITEM_XTX	'cwUN'
#define NDAS_DLU_PTAG_SRB			'rsUN'
#define NDAS_DLU_PTAG_SRB_IOCTL		'isUN'
#define NDAS_DLU_PTAG_SRB_CMPDATA	'dsUN'
#define NDAS_DLU_PTAG_CMPDATA		'mcUN'
#define NDAS_DLU_PTAG_ENUMINFO		'neUN'
#define NDAS_DLU_PTAG_DEVTEXT		'dtUN'
#define NDAS_DLU_PTAG_DEVTEXT		'dtUN'

#ifdef __cplusplus
extern "C" {
#endif

//
// Logical Unit Interface Query
//

NTSTATUS
NdasDluGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

//
// Logical Unit Interface Implementation
//

NTSTATUS
NdasDluInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

VOID
NdasDluCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
NdasDluLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters);

NTSTATUS
NdasDluQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId);

NTSTATUS
NdasDluQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR* DeviceText);

NTSTATUS
NdasDluQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities);

NTSTATUS
NdasDluQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
NdasDluQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

BOOLEAN
NdasDluBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

BOOLEAN
NdasDluStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

//
// Implementation
//

//
//	Windows 2000 major and minor version numbers
//

#define NT_MAJOR_VERSION	5
#define W2K_MINOR_VERSION	0

//
//	NDAS DLU driver-wide variables
//

typedef struct _NDAS_DLU_GLOBALS {

	//
	// Current Windows versions
	//

	ULONG			MajorVersion;
	ULONG			MinorVersion;
	ULONG			BuildNumber;
	BOOLEAN			CheckedVersion;


	//
	// Driver object
	//

	PDRIVER_OBJECT	DriverObject;


	//
	//	Handle for TDI PnP event
	//

	HANDLE			TdiPnP;

#if __NDAS_SCSI_OLD_VERSION__
	//
	// DRaid context
	//

	DRAID_GLOBALS	DraidGlobal;
#endif

} NDAS_DLU_GLOBALS, *PNDAS_DLU_GLOBALS;


extern NDAS_DLU_GLOBALS _NdasDluGlobals;


//////////////////////////////////////////////////////////////////////////
//
//	Event log modules
//	Event log unique IDs
//
#define	EVTLOG_MASK(LOGVALUE, MASK) ((LOGVALUE) & (MASK))
#define	EVTLOG_UNIQUEID(EVTMODULE, EVTID, USERDATA) ((EVTLOG_MASK(EVTMODULE, 0xff) << 24) |	\
	(EVTLOG_MASK(EVTID, 0xff)<<16) |		\
	EVTLOG_MASK(USERDATA, 0xffff))


//	FindEnumInfo module
#define EVTLOG_MODULE_FINDADAPTER			0x02
#define	EVTLOG_START_FIND					0x01
#define	EVTLOG_DETECT_LURDESC				0x02
#define	EVTLOG_DETECT_ADDTARGET				0x03
#define	EVTLOG_SUCCEED_FIND					0x04
#define	EVTLOG_FAIL_TO_GET_ENUM_INFO		0x10
#define	EVTLOG_FAIL_TO_ADD_LUR				0x11
#define	EVTLOG_INVALID_TARGETTYPE			0x12
#define	EVTLOG_FAIL_TO_ALLOC_LURDESC		0x13
#define	EVTLOG_FAIL_TO_TRANSLATE			0x14
#define	EVTLOG_FAIL_TO_CREATE_LUR			0x15
#define	EVTLOG_FIRST_TIME_INSTALL			0x16

// Adapter control
#define EVTLOG_MODULE_ADAPTERCONTROL		0x03
#define	EVTLOG_BUSRESET_OCCUR				0x01
#define	EVTLOG_ADAPTER_STOP					0x02
#define	EVTLOG_STOP_DURING_POWERSAVING		0x03
#define	EVTLOG_STOP_DURING_STOPPING			0x04
#define EVTLOG_INQUIRY_DURING_POWERSAVING	0x05
#define EVTLOG_INQUIRY_DURING_STOPPING		0x06
#define EVTLOG_INQUIRY_LUR_NOT_FOUND		0x07
#define EVTLOG_DOUBLE_POWERSAVING			0x08
#define EVTLOG_RESTART_NOT_INPOWERSAVING	0x09
#define EVTLOG_SCSIOP_INPOWERSAVING			0x0a
#define EVTLOG_DISCARD_SRB					0x0b
#define EVTLOG_STOP_WITH_INVALIDLUR			0x0c

// IRP-method completion
#define EVTLOG_MODULE_COMPIRP				0x04
#define EVTLOG_NO_SHIPPED_SRB				0x01
#define EVTLOG_ABORT_SRB_ENTERED			0x02
#define EVTLOG_CCB_ALLOCATION_FAIL			0x03

// Completion
#define EVTLOG_MODULE_COMPLETION			0x05
#define EVTLOG_START_RECONNECTION			0x01
#define EVTLOG_END_RECONNECTION				0x02
#define EVTLOG_SUCCEED_UPGRADE				0x03
#define EVTLOG_FAIL_UPGRADE					0x04
#define EVTLOG_MEMBER_IN_ERROR				0x05
#define EVTLOG_FAIL_COMPLIRP				0x06
#define EVTLOG_MEMBER_RECOVERED				0x07
#define EVTLOG_START_RECOVERING				0x08
#define EVTLOG_END_RECOVERING				0x09
#define EVTLOG_DISK_POWERRECYCLED			0x0a
#define EVTLOG_RAID_FAILURE					0x0b				

// Ioctl
#define EVTLOG_MODULE_IOCTL					0x06
#define EVTLOG_FAIL_UPGRADEIOCTL			0x01
#define EVTLOG_FAIL_WHILESTOPPING			0x02
#define EVTLOG_FAIL_LURNULL					0x03
#define EVTLOG_FAIL_SENDSTOPCCB				0x04
#define EVTLOG_FAIL_DELAYEDOP				0x05

//////////////////////////////////////////////////////////////////////////
//
//	Structures
//	Keep the same values as in ndasscsiioctl.h, ndastypes.h
//

#define DLUINTERNAL_STATUS_MASK						0x000000ff
#define	DLUINTERNAL_STATUS_INIT						0x00000000
#define	DLUINTERNAL_STATUS_RUNNING					0x00000001
#define DLUINTERNAL_STATUS_STOPPING					0x00000002
//#define DLUINTERNAL_STATUS_IN_ERROR					0x00000003
#define DLUINTERNAL_STATUS_STOPPED					0x00000004	// only used for NDASBUS notification

#define DLUINTERNAL_STATUSFLAG_MASK					0xffffff00
#define DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING	0x00000100
#define DLUINTERNAL_STATUSFLAG_RESTARTING			0x00000200
#define DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING	0x00000400
#define DLUINTERNAL_STATUSFLAG_MEMBER_FAULT			0x00000800
#define DLUINTERNAL_STATUSFLAG_RECOVERING			0x00001000
#define DLUINTERNAL_STATUSFLAG_ABNORMAL_TERMINAT	0x00002000
//#define DLUINTERNAL_STATUSFLAG_DEFECTIVE			0x00004000
#define DLUINTERNAL_STATUSFLAG_POWERRECYCLED		0x00008000
#define DLUINTERNAL_STATUSFLAG_RAID_FAILURE			0x00010000
#define DLUINTERNAL_STATUSFLAG_RAID_NORMAL			0x00020000
#define DLUINTERNAL_STATUSFLAG_RESETSTATUS			0x01000000	// only used for NDASBUS notification
#define DLUINTERNAL_STATUSFLAG_NEXT_EVENT_EXIST		0x02000000	// only used for NDASSVC notification

#define DLUINTERNAL_ISSTATUS(PHWDEV, STATUS)		\
	(((PHWDEV)->DluInternalStatus & DLUINTERNAL_STATUS_MASK) == (STATUS))
#define DLUINTERNAL_SETSTATUS(PHWDEV, STATUS)		\
	( (PHWDEV)->DluInternalStatus = ((PHWDEV)->DluInternalStatus & (DLUINTERNAL_STATUSFLAG_MASK)) | (STATUS))

#define DLUINTERNAL_ISSTATUSFLAG(PHWDEV, STATUSFLAG)		\
	(((PHWDEV)->DluInternalStatus & (STATUSFLAG)) != 0)
#define DLUINTERNAL_SETSTATUSFLAG(PHWDEV, STATUSFLAG)		\
	( (PHWDEV)->DluInternalStatus |= (STATUSFLAG))
#define DLUINTERNAL_RESETSTATUSFLAG(PHWDEV, STATUSFLAG)		\
	( (PHWDEV)->DluInternalStatus &= ~(STATUSFLAG))

#define NDASDLU_DEFAULT_MAXIMUM_TRANSFER_LENGTH	(1024 * 1024) // 1Mbytes

#define DLU_MAXIMUM_REQUESTS	1

typedef struct _NDAS_DLU_EXTENSION {

	union {
		struct {
			UCHAR PortNumber;
			UCHAR PathId;
			UCHAR TargetId;
			UCHAR Lun;
		};
		ULONG LogicalUnitAddress;
	};


	PSCSI_REQUEST_BLOCK CurrentSrb;

	//
	// Adapter properties
	//

	STORAGE_ADAPTER_DESCRIPTOR StorageAdapterDescriptor;

	//
	// Property Data
	//
	INQUIRYDATA InquiryData;
	BOOLEAN		ValidInquiryDataVpd;
	UCHAR		InquiryDataVpd[VPD_MAX_BUFFER_SIZE];

	//
	//	LU internal status
	//

	KSPIN_LOCK		DluSpinLock;	// protect DluInternalStatus
	ULONG			DluInternalStatus;

	//
	// LU enabled time to support LFS filter
	//

	LARGE_INTEGER	EnabledTime;

	//
	// Logical unit relation
	//

	PLURELATION		LUR;

	//
	// Pending request count
	//
	LONG			RequestExecuting;

	//
	// Maximum request count.
	//

	LONG			MaximumRequests;

	//
	// LU name
	//

	ULONG			LuNameLength;
	WCHAR			LuName[MAX_LENGTH_LUR_NAME];

} NDAS_DLU_EXTENSION, *PNDAS_DLU_EXTENSION;

typedef struct _NDASDLU_WORKITEM_CONTEXT NDASDLU_WORKITEM_CONTEXT, *PNDASDLU_WORKITEM_CONTEXT;

typedef
VOID
(*PNDAS_DLU_WORKITEM_ROUTINE) (
    IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
    IN PNDASDLU_WORKITEM_CONTEXT	NdasDluWorkItemContext
    );

//
// NDAS DLU work context init structure
//

typedef struct _NDASDLU_WORKITEM_INIT {

	PNDAS_DLU_WORKITEM_ROUTINE	WorkitemRoutine;
	PCCB						Ccb;
	PVOID						Arg1;
	PVOID						Arg2;
	PVOID						Arg3;

} NDASDLU_WORKITEM_INIT, *PNDASDLU_WORKITEM_INIT;


//
//	NDAS DLU work context
//

typedef struct _NDASDLU_WORKITEM_CONTEXT {
	PCCB						Ccb;
	PVOID						Arg1;
	PVOID						Arg2;
	PVOID						Arg3;

	//
	// System use
	//
	PNDAS_DLU_WORKITEM_ROUTINE	WorkitemRoutine;
	PNDAS_LOGICALUNIT_EXTENSION	TargetNdasLUExtension;
	PVOID						OriginalWorkItem;

} NDASDLU_WORKITEM_CONTEXT, *PNDASDLU_WORKITEM_CONTEXT;


#define NDASDLU_INIT_WORKITEM(WI_POINTER, WIROUTINE, CCB, ARG1, ARG2, ARG3) {	\
	(WI_POINTER)->WorkitemRoutine = (WIROUTINE);							\
	(WI_POINTER)->Ccb = (CCB);												\
	(WI_POINTER)->Arg1 = (ARG1);											\
	(WI_POINTER)->Arg2 = (ARG2);											\
	(WI_POINTER)->Arg3 = (ARG3);											\
	}

//
// ndasdlu.c
//

VOID
NdasDluLogError(
	IN PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	IN PSCSI_REQUEST_BLOCK Srb OPTIONAL,
	IN UCHAR PathId,
	IN UCHAR TargetId,
	IN UCHAR Lun,
	IN ULONG ErrorCode,
	IN ULONG UniqueId
);

VOID
NdasDluRetryWorker(
	IN PNDAS_LOGICALUNIT_EXTENSION	NdasLUExtension,
	IN PNDASDLU_WORKITEM_CONTEXT	DluWorkItemCtx
);

NTSTATUS
NdasDluQueueWorkItem(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PNDASDLU_WORKITEM_INIT	NdasDluWorkItemInit
);

//
// Inline routines
//

FORCEINLINE 
PNDAS_DLU_EXTENSION 
NdasDluGetExtension(
	PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	return (PNDAS_DLU_EXTENSION)NdasPortGetLogicalUnit(LogicalUnitExtension, 0, 0, 0);
}

//
// ndasdluioctl.c
//
NTSTATUS
NdasDluSrbControl(
	   IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	   IN PSCSI_REQUEST_BLOCK			Srb
);

NTSTATUS
NdasDluSendIoctlSrbAsynch(
	IN PDEVICE_OBJECT	DeviceObject,
	IN ULONG			IoctlCode,
	IN PVOID			InputBuffer,
	IN LONG				InputBufferLength,
	OUT PVOID			OutputBuffer,
	IN LONG				OutputBufferLength
);

NTSTATUS
NdasDluFireEvent(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN ULONG DluInternalStatus
);

//
// ndasdlucomp.c
//
NTSTATUS
NdasDluCcbCompletion(
	IN PCCB							Ccb,
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension
);

//
// inline functions
//

static
VOID
INLINE
DluPrintSrb(PCHAR Prefix, PSCSI_REQUEST_BLOCK Srb) {
	switch(Srb->Function) {
		case SRB_FUNCTION_EXECUTE_SCSI:
			DbgPrint("%s %p %s(%X,%d) St:%x StF:%x Tx:%d S:%d To:%d F:%x ",
				Prefix,
				Srb,
				CdbOperationString(Srb->Cdb[0]),
				(int)Srb->Cdb[0],
				(UCHAR)Srb->CdbLength,
				SRB_STATUS(Srb->SrbStatus),
				Srb->SrbStatus & 0xc0,
				Srb->DataTransferLength,
				Srb->SenseInfoBufferLength,
				Srb->TimeOutValue,
				Srb->SrbFlags);
			if(Srb->SenseInfoBufferLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) {
				PSENSE_DATA pSenseData	= (PSENSE_DATA)(Srb->SenseInfoBuffer);

				DbgPrint("EC:%02x Key:%02x ASC:0x%02x ASCQ:%02x\n",
					pSenseData->ErrorCode,
					pSenseData->SenseKey,
					pSenseData->AdditionalSenseCode,
					pSenseData->AdditionalSenseCodeQualifier);
			} else {
				DbgPrint("\n");
			}
			break;
		default:;
			KDPrint(NDASSCSI_DBG_MINIPORT_ERROR, ("%x\n", Srb->Function));
	}
}

#ifdef __cplusplus
}
#endif
