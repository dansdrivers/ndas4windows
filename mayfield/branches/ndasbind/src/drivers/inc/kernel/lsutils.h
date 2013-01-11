#ifndef KERNEL_IDE_UTILS_H
#define KERNEL_IDE_UTILS_H

#include "LSProto.h"
#include <hdreg.h>


NTSTATUS
LsuConnectLogin(
	OUT PLANSCSI_SESSION		LanScsiSession,
	IN  PTA_LSTRANS_ADDRESS		TargetAddress,
	IN  PTA_LSTRANS_ADDRESS		BindingAddress,
	IN  PLSSLOGIN_INFO			LoginInfo,
	OUT PLSTRANS_TYPE			LstransType
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
	PNDAS_DIB				DiskInfomationBlock,
	UINT64					LogicalBlockAddress,
	ULONG					PduFlags
);


NTSTATUS
LsuGetDiskInfoBlockV2(
	PLANSCSI_SESSION			LSS,
	PNDAS_DIB_V2				DiskInfomationBlock,
	UINT64						LogicalBlockAddress,
	ULONG						PduFlags
);

#endif