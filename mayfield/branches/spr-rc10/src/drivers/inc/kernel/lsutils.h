#ifndef KERNEL_IDE_UTILS_H
#define KERNEL_IDE_UTILS_H

#include "LSProto.h"

#define LSU_POOLTAG_ERRORLOGWORKER	'cwUL'

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


VOID
LsuWriteLogErrorEntry(
	IN PDEVICE_OBJECT		DeviceObject,
    IN PLSU_ERROR_LOG_ENTRY ErrorLogEntry
);

#endif