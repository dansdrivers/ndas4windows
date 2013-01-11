#ifndef KERNEL_IDE_UTILS_H
#define KERNEL_IDE_UTILS_H

#include "ndas/ndasdib.h"
#include "LSProto.h"

#define LSU_POOLTAG_ERRORLOGWORKER	'cwUL'
#define LSU_POOLTAG_BLOCKBUFFER		'fbUL'
#define LSU_POOLTAG_BLOCKACE		'eaUL'

#define TDIPNPCLIENT_MAX_HOLDING_LOOP	60

#define LSU_MAX_ERRLOG_DATA_ENTRIES	4

typedef struct _LSU_ERROR_LOG_ENTRY {

	UCHAR MajorFunctionCode;
	UCHAR DumpDataEntry;
	ULONG ErrorCode;
	ULONG IoctlCode;
	ULONG UniqueId;
	ULONG ErrorLogRetryCount;
	ULONG SequenceNumber;
	ULONG Parameter2;
	ULONG DumpData[LSU_MAX_ERRLOG_DATA_ENTRIES];

} LSU_ERROR_LOG_ENTRY, *PLSU_ERROR_LOG_ENTRY;


NTSTATUS
LsuConnectLogin(
	OUT PLANSCSI_SESSION		LanScsiSession,
	IN  PTA_LSTRANS_ADDRESS		TargetAddress,
	IN  PTA_LSTRANS_ADDRESS		BindingAddress,
	IN  PLSSLOGIN_INFO			LoginInfo,
	OUT PLSTRANS_TYPE			LstransType
);


NTSTATUS
LsuQueryBindingAddress(
	OUT PTA_LSTRANS_ADDRESS		BoundAddr,
	IN  PTA_LSTRANS_ADDRESS		TargetAddr,
	IN  PTA_LSTRANS_ADDRESS		BindingAddr,
	IN  PTA_LSTRANS_ADDRESS		BindingAddr2,
	IN	BOOLEAN					BindAnyAddr
);


NTSTATUS
LsuConfigureIdeDisk(
	PLANSCSI_SESSION	LSS,
	PULONG				PduFlags,
	PBOOLEAN			Dma
);


NTSTATUS
LsuGetIdentify(
	PLANSCSI_SESSION	LSS,
	struct hd_driveid	*info
);


NTSTATUS
LsuReadBlocks(
	PLANSCSI_SESSION	LSS,
	PBYTE				Buffer,
	UINT64				LogicalBlockAddress,
	ULONG				TransferBlocks,
	ULONG				PduFlags
);


NTSTATUS
LsuWriteBlocks(
	PLANSCSI_SESSION	LSS,
	PBYTE				Buffer,
	UINT64				LogicalBlockAddress,
	ULONG				TransferBlocks,
	ULONG				PduFlags
);


NTSTATUS
LsuGetDiskInfoBlockV1(
	PLANSCSI_SESSION		LSS,
	PNDAS_DIB				DiskInformationBlock,
	UINT64					LogicalBlockAddress,
	ULONG					PduFlags
);


NTSTATUS
LsuGetDiskInfoBlockV2(
	PLANSCSI_SESSION			LSS,
	PNDAS_DIB_V2				DiskInformationBlock,
	UINT64						LogicalBlockAddress,
	ULONG						PduFlags
);

NTSTATUS
LsuGetBlockACL(
	PLANSCSI_SESSION			LSS,
	PBLOCK_ACCESS_CONTROL_LIST	BlockACL,
	ULONG						BlockACLLen,
	UINT64						LogicalBlockAddress,
	ULONG						PduFlags
);

VOID
LsuWriteLogErrorEntry(
	IN PDEVICE_OBJECT		DeviceObject,
    IN PLSU_ERROR_LOG_ENTRY ErrorLogEntry
);



//
//	TDI PnP handle
//

NTSTATUS
LsuRegisterTdiPnPPowerHandler(
	IN PUNICODE_STRING			TdiClientName,
	IN TDI_PNP_POWER_HANDLER	TdiPnPPowerHandler,
	OUT PHANDLE					TdiClient
);

VOID
LsuDeregisterTdiPnPHandlers(
	HANDLE			TdiClient
);

VOID
LsuIncrementTdiClientInProgress();

VOID
LsuDecrementTdiClientInProgress();

NTSTATUS
ClientPnPPowerChange_Delay(
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
);

//////////////////////////////////////////////////////////////////////////
//
//	Bock access control entry ( BACE )
//

#define LSUBACE_ACCESS_READ		0x0001
#define LSUBACE_ACCESS_WRITE	0x0002

typedef struct _LSU_BLOCK_ACE {
	UINT64		BlockAceId;
	UCHAR		AccessMode;
	UCHAR		Reserved[7];
	UINT64		BlockStartAddr;
	UINT64		BlockEndAddr;

	LIST_ENTRY	BlockAclList;
} LSU_BLOCK_ACE, *PLSU_BLOCK_ACE;


//////////////////////////////////////////////////////////////////////////
//
//	Bock access control list ( BACL )
//

typedef struct _LSU_BLOCK_ACL {
	LONG			BlockACECnt;
	UINT32			Reserved;
	KSPIN_LOCK		BlcokAclSpinlock;
	LIST_ENTRY		BlockAclHead;
} LSU_BLOCK_ACL, *PLSU_BLOCK_ACL;

PLSU_BLOCK_ACE
LsuCreateBlockAce(
	IN UCHAR		AccessMode,
	IN UINT64		BlockStartAddr,
	IN UINT64		BlockEndAddr
);

VOID
LsuFreeBlockAce(
	IN PLSU_BLOCK_ACE LsuBlockAce
);

VOID
LsuFreeAllBlockAce(
	IN PLIST_ENTRY	AceHead
);

NTSTATUS
LsuConvertNdasBaclToLsuBacl(
	OUT PLSU_BLOCK_ACL	LsuBacl,
	IN PNDAS_BLOCK_ACL	NdasBacl
);

VOID
LsuInitializeBlockAcl(
	OUT PLSU_BLOCK_ACL	LsuBacl
);

NTSTATUS
LsuConvertLsuBaclToNdasBacl(
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN ULONG			NdasBaclLen,
	OUT	PULONG			RequiredBufLen,
	IN PLSU_BLOCK_ACL	LsuBacl
);

PLSU_BLOCK_ACE
LsuGetAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN UINT64			StartAddr,
	IN UINT64			EndAddr
);

PLSU_BLOCK_ACE
LsuGetAceById(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN BLOCKACE_ID		BlockAceId
);

VOID
LsuInsertAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN PLSU_BLOCK_ACE	BlockAce
);

VOID
LsuRemoveAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN PLSU_BLOCK_ACE	BlockAce
);

PLSU_BLOCK_ACE
LsuRemoveAceById(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN BLOCKACE_ID		BlockAceId
);


#endif