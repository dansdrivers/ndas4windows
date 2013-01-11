#ifndef KERNEL_IDE_UTILS_H
#define KERNEL_IDE_UTILS_H

#include "ndas/ndasdib.h"
#include "LSProto.h"

#define LSU_POOLTAG_ERRORLOGWORKER	'cwUL'
#define LSU_POOLTAG_BLOCKBUFFER		'fbUL'
#define LSU_POOLTAG_BLOCKACE		'eaUL'

#define TDIPNPCLIENT_MAX_HOLDING_LOOP	120

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


ULONG
AtaGetBytesPerBlock(
	IN struct hd_driveid *IdentifyData
);


NTSTATUS
LsuConfigureIdeDisk(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			UdmaRestrict,
	OUT PULONG			PduFlags,
	OUT PBOOLEAN		Dma,
	OUT PULONG			BlockBytes
);


NTSTATUS
LsuGetIdentify(
	IN PLANSCSI_SESSION		LSS,
	OUT struct hd_driveid	*info
);

NTSTATUS
LsuSetTransferMode(
	PLANSCSI_SESSION	LSS,
	UCHAR				TransferMode
);

NTSTATUS
LsuSetPioMode(
	PLANSCSI_SESSION	LSS,
	UCHAR				PioMode
);

NTSTATUS
LsuSetDmaMode(
	PLANSCSI_SESSION	LSS,
	UCHAR				DmaMode,
	BOOLEAN				UltraDma
);

NTSTATUS
LsuReadBlocks(
	IN PLANSCSI_SESSION	LSS,
	OUT PBYTE			Buffer,
	IN UINT64			LogicalBlockAddress,
	IN ULONG			TransferBlocks,
	IN ULONG			BlockBytes,
	IN ULONG			PduFlags
);


NTSTATUS
LsuWriteBlocks(
	IN PLANSCSI_SESSION	LSS,
	IN PBYTE			Buffer,
	IN UINT64			LogicalBlockAddress,
	IN ULONG			TransferBlocks,
	IN ULONG			BlockBytes,
	IN ULONG			PduFlags
);


NTSTATUS
LsuGetDiskInfoBlockV1(
	IN PLANSCSI_SESSION		LSS,
	OUT PNDAS_DIB			DiskInformationBlock,
	IN UINT64				LogicalBlockAddress,
	IN ULONG				BlockBytes,
	IN ULONG				PduFlags
);


NTSTATUS
LsuGetDiskInfoBlockV2(
	IN PLANSCSI_SESSION		LSS,
	OUT PNDAS_DIB_V2		DiskInformationBlock,
	IN UINT64				LogicalBlockAddress,
	IN ULONG				BlockBytes,
	IN ULONG				PduFlags
);

NTSTATUS
LsuGetBlockACL(
	IN PLANSCSI_SESSION				LSS,
	OUT PBLOCK_ACCESS_CONTROL_LIST	BlockACL,
	IN ULONG						BlockACLLen,
	IN UINT64						LogicalBlockAddress,
	IN ULONG						BlockBytes,
	IN ULONG						PduFlags
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
LsuRegisterTdiPnPHandler(
	IN PUNICODE_STRING			TdiClientName,
	IN TDI_BINDING_HANDLER		TdiBindHandler,
	TDI_ADD_ADDRESS_HANDLER_V2 TdiAddAddrHandler,
	TDI_DEL_ADDRESS_HANDLER_V2 TdiDelAddrHandler,
	IN TDI_PNP_POWER_HANDLER	TdiPnPPowerHandler,
	OUT PHANDLE					TdiClient
);

VOID
LsuDeregisterTdiPnPHandlers(
	IN HANDLE			TdiClient
);

VOID
LsuIncrementTdiClientInProgress();

VOID
LsuDecrementTdiClientInProgress();


VOID
LsuIncrementTdiClientDevice();

VOID
LsuDecrementTdiClientDevice();

VOID
LsuClientPnPBindingChange(
	IN TDI_PNP_OPCODE  PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR  MultiSZBindList
);

NTSTATUS
LsuClientPnPPowerChange(
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