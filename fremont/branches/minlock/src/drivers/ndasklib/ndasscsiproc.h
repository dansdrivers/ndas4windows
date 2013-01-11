#ifndef _NDAS_SCSI_PROC_H_
#define _NDAS_SCSI_PROC_H_


#define __NDAS_SCSI__					1
#define __NDAS_SCSI_W2K_SUPPORT__		1

#define __NDAS_CHIP_BUG_AVOID__				1
#define __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__	1
#define __NDAS_CHIP_PIO_READ_AND_WRITE__	1

#define __NDAS_SCSI_RAID_DISABLE_READ_LOCK__		1
#define	__NDAS_SCSI_DISABLE_LOCK_RELEASE_DELAY__	1


#define __NDAS_SCSI_UNRECHABLE_NODE_TEST__		0

#define __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__	0
#define __TEST_BLOCK_SIZE__	1		


#define __NDAS_SCSI_NEW_ODD__			1
#define __NDAS_SCSI_B_RECORDER__		0

// ATA/ATAPI device options

#define __USE_PRESET_PIO_MODE__			0


// ATA disk options

#define __DETECT_CABLE80__				0
#define __ENABLE_LOCKED_READ__			1
#define __ENABLE_WRITECACHE_CONTROL__	0


// ATA optical devices ( CD/DVD ) options


#define __BSRECORDER_HACK__				0
#define __DVD_ENCRYPT_CONTENT__			0


// Debugging options


#define __SPINLOCK_DEBUG__				0
#define __ENABLE_PERFORMACE_CHECK__		0


// Internal module test

#define __ENABLE_BACL_TEST__				0
#define __ENABLE_CONTENTENCRYPT_AES_TEST__	0




#if __NDAS_SCSI_W2K_SUPPORT__

#define STATUS_CLUSTER_NODE_UNREACHABLE  ((NTSTATUS)0xC013000DL)
#define STATUS_CLUSTER_NETWORK_NOT_FOUND ((NTSTATUS)0xC0130007L)

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#endif

#include <ntddk.h>
#include <ntdddisk.h>
#include <TdiKrnl.h>
#include <windef.h>
#include <regstr.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <stdarg.h>
#include <scrc32.h>

C_ASSERT( sizeof(CDB) == 16 );

#include "ndascommonheader.h"

#include <ndas/ndasdib.h>

#include "basetsdex.h"
#include "rijndael-api-fst.h"
#include "hash.h"
#include "hdreg.h"
#include "binparams.h"

#include "socketlpx.h"
#include "cipher.h"
#include "devreg.h"

#include "lpxtdiV2.h"

#include "ver.h"

#include "ndasflowcontrol.h"

#include "LSKlib.h"
#include "KDebug.h"
#include "cipher.h"
#include "LSProto.h"
#include "LSTransport.h"
#include "lurdesc.h"
#include "LsCcb.h"
#include "ndasscsiioctl.h"
#include "lsutils.h"
#include "LSLur.h"
#include "LSLurn.h"
#include "lurnata.h"
#include "ndasprotoata.h"
#include "lsutils.h"


#include "ndasrprotocol.h"

#include "ndasrclient.h"
#include "ndasrarbitrator.h"

#include "lurnndasraid.h"

#include "Scrc32.h"

#include "ndaslurn.h"

#include "ndastransport.h"

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif


// ndasraid.c

NTSTATUS 
NdasRaidStart (
	IN PNDASR_GLOBALS NdasrGlobals
	); 

NTSTATUS 
NdasRaidClose (
	IN PNDASR_GLOBALS NdasrGlobals
	);

INT32 
NdasRaidReallocEventArray (
	PKEVENT**		Events,
	PKWAIT_BLOCK*	WaitBlocks,
	INT32			CurrentCount
	); 

VOID
NdasRaidFreeEventArray (
	PKEVENT*		Events,
	PKWAIT_BLOCK	WaitBlocks
	); 

NTSTATUS
NdasRaidRegisterIdeDisk (
	IN PNDASR_GLOBALS		NdasrGlobals,
	IN PLURNEXT_IDE_DEVICE  IdeDisk
	);

NTSTATUS
NdasRaidUnregisterIdeDisk (
	IN PNDASR_GLOBALS		NdasrGlobals,
	IN PLURNEXT_IDE_DEVICE  IdeDisk
	); 

NTSTATUS 
NdasRaidRegisterArbitrator (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_ARBITRATOR NdasrArbitrator
	); 

NTSTATUS
NdasRaidUnregisterArbitrator (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_ARBITRATOR NdasrArbitrator
	); 

NTSTATUS
NdasRaidRegisterClient (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidUnregisterClient (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidLurnReadRmd (
	IN  PLURELATION_NODE		Lurn,
	OUT PNDAS_RAID_META_DATA	Rmd,
	OUT PBOOLEAN				NodeIsUptoDate,
	OUT PUCHAR					UpToDateNode
	);

NTSTATUS
NdasRaidLurnSendStopCcbToChildrenSyncronously (
	IN PLURELATION_NODE		Lurn,
	IN PCCB					Ccb
	);

NTSTATUS
NdasRaidLurnUpdateSynchrously (
	IN  PLURELATION_NODE	ChildLurn,
	IN  UINT16				UpdateClass,
	OUT PUCHAR				CcbStatus
	);

NTSTATUS
NdasRaidLurnLockSynchrously (
	IN  PLURELATION_NODE	ChildLurn,
	IN  UINT32				LockId,
	IN  BYTE				LockOpCode,
	OUT PUCHAR				CcbStatus
	);

NTSTATUS
NdasRaidLurnExecuteSynchrously (
	IN PLURELATION_NODE	ChildLurn,
	IN UCHAR			CdbOperationCode,
	IN BOOLEAN			AcquireBufferLock,
	IN BOOLEAN			ForceUnitAccess,
	IN PCHAR			DataBuffer,
	IN UINT64			BlockAddress,		// Child block addr
	IN UINT32			BlockTransfer,		// Child block count
	IN BOOLEAN			RelativeAddress
	);

UINT32 
NdasRaidGetOverlappedRange (
	UINT64	Start1, 
	UINT32	Length1,
	UINT64	Start2, 
	UINT32	Length2,
	UINT64	*OverlapStart, 
	UINT32	*OverlapLength
	); 

UCHAR 
NdasRaidLurnStatusToNodeFlag (
	UINT32 LurnStatus
	);

NTSTATUS
NdasRaidLurnCreate (
	IN PLURELATION_NODE			Lurn,
	IN PLURELATION_NODE_DESC	LurnDesc
	);

VOID
NdasRaidLurnClose (
	IN PLURELATION_NODE Lurn
	);

VOID
NdasRaidLurnStop (
	PLURELATION_NODE Lurn
	);

NTSTATUS
NdasRaidLurnRequest (
	IN PLURELATION_NODE	Lurn,
	IN PCCB				Ccb
	);


// ndasraidclient.c

NTSTATUS 
NdasRaidClientStart (
	PLURELATION_NODE	Lurn
	);


NTSTATUS
NdasRaidClientStop (
	PLURELATION_NODE	Lurn
	);

NTSTATUS
NdasRaidClientShutdown (
	PLURELATION_NODE	Lurn
	);

NTSTATUS
NdasRaidClientReleaseBlockIoPermissionToClient (
	PNDASR_CLIENT	NdasClient,
	PCCB			Ccb
	);

VOID
NdasRaidClientUpdateNodeFlags (
	PNDASR_CLIENT		NdasrClient,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,		// Temp parameter.. set though lurn node info.
	UCHAR 				NewDefectCodes  // Temp parameter.. set though lurn node info.
	); 

// ndasraidarbiter.c

NTSTATUS 
NdasRaidArbitratorStart (
	PLURELATION_NODE	Lurn
	);

NTSTATUS
NdasRaidArbitratorStop (
	PLURELATION_NODE	Lurn
	);

NTSTATUS
NdasRaidArbitratorShutdown (
	PLURELATION_NODE	Lurn
	);

// lockmgmt.c

NTSTATUS
LMInitialize(
	IN PLURELATION_NODE		Lurn,
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN BOOLEAN				InitialState
);

VOID
LMEnable(
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN BOOLEAN				Activate,
	IN BOOLEAN				ParentControl
);

NTSTATUS
LMDestroy(
	IN PBUFFLOCK_CONTROL BuffLockCtl
);

NTSTATUS
StartIoIdleTimer(
	PBUFFLOCK_CONTROL BuffLockCtl
);

NTSTATUS
StopIoIdleTimer(
	PBUFFLOCK_CONTROL BuffLockCtl
);

NTSTATUS
LurnIdeDiskDeviceLockControl(
	IN PLURELATION_NODE		Lurn,
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PCCB					Ccb
);

static
INLINE
BOOLEAN
LockCacheIsAcquired (
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	return lockStatus->Acquired;
}

BOOLEAN
LockCacheAcquiredLocksExistsExceptForBufferLock(
	IN PLU_HWDATA	LuHwData
	);

VOID
LockCacheAllLocksLost(
	IN PLU_HWDATA	LuHwData
);

BOOLEAN
LockCacheCheckLostLockIORange(
	IN PLU_HWDATA	LuHwData,
	IN UINT64		StartingAddress,
	IN UINT64		EndingAddress
);

NTSTATUS
EnterBufferLockIoIdle(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData
);

NTSTATUS
NdasAcquireBufferLock(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
NdasReleaseBufferLock(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut,
	IN BOOLEAN			Force,
	IN ULONG			TransferredIoLength
);

// lsproto.c

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi protocol APIs
//

NTSTATUS
LspLookupProtocol(
		IN	ULONG			NdasHw,
		IN	ULONG			NdasHwVersion,
		OUT PLSPROTO_TYPE	Protocol
	);

NTSTATUS
LspUpgradeUserIDWithWriteAccess(
		IN PLANSCSI_SESSION	LSS
	);

NTSTATUS
LspConnect (
	IN PLANSCSI_SESSION		LSS,
	IN PTA_ADDRESS			SrcAddr,
	IN PTA_ADDRESS			DestAddr,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN PLSTRANS_OVERLAPPED	Overlapped,
	IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspConnectMultiBindAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_LSTRANS_ADDRESS	BoundAddr,
	IN	PTA_LSTRANS_ADDRESS	BindAddr1,
	IN  PTA_LSTRANS_ADDRESS	BindAddr2,
	IN  PTA_LSTRANS_ADDRESS	DestAddr,
	IN  BOOLEAN				BindAnyAddress,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspConnectMultiBindTaAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_ADDRESS			BoundAddr,
	IN	PTA_ADDRESS			BindAddr1,
	IN  PTA_ADDRESS			BindAddr2,
	IN  PTA_ADDRESS			DestAddr,
	IN  BOOLEAN				BindAnyAddress,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	); 

NTSTATUS
LspReconnect (
	IN PLANSCSI_SESSION LSS,
	IN PVOID			InDisconnectHandler,
	IN PVOID			InDisconnectEventContext,
	IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LspRelogin(
		IN OUT PLANSCSI_SESSION LSS,
		IN BOOLEAN				IsEncryptBuffer,
		IN PLARGE_INTEGER		TimeOut
	);


NTSTATUS
LspBuildLoginInfo(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo
	);

VOID
LspCopy(
		PLANSCSI_SESSION	ToLSS,
		PLANSCSI_SESSION	FromLSS,
		BOOLEAN				CopyBufferPointers
	);

VOID
LspSetDefaultTimeOut(
		IN PLANSCSI_SESSION	LSS,
		IN PLARGE_INTEGER	TimeOut
	);

VOID
LspGetAddresses(
		PLANSCSI_SESSION	LSS,
		PTA_ADDRESS			BindingAddress,
		PTA_ADDRESS			TargetAddress
	);

NTSTATUS
LspNoOperation(
		IN PLANSCSI_SESSION	LSS,
		IN UINT32			TargetId,
		OUT PBYTE			PduResponse,
		IN PLARGE_INTEGER	TimeOut
	);

//////////////////////////////////////////////////////////////////////////
//
//	Stub function for Lanscsi protocol interface
//


NTSTATUS
LspSendRequest(
			PLANSCSI_SESSION			LSS,
			PLANSCSI_PDU_POINTERS		Pdu,
			PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
			PLARGE_INTEGER				TimeOut
	);

NTSTATUS
LspReadReply(
		IN PLANSCSI_SESSION			LSS,
		OUT PCHAR					Buffer,
		IN PLANSCSI_PDU_POINTERS	Pdu,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
	);

NTSTATUS
LspLogin(
		IN PLANSCSI_SESSION LSS,
		IN PLSSLOGIN_INFO	LoginInfo,
		IN LSPROTO_TYPE		LSProto,
		IN PLARGE_INTEGER	TimeOut,
		IN BOOLEAN			LockCleanup
	);


NTSTATUS
LspTextTargetList(
		IN PLANSCSI_SESSION		LSS,
		OUT PTARGETINFO_LIST	TargetList,
		IN ULONG				TargetListLength,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspTextTartgetData(
		IN PLANSCSI_SESSION LSS,
		IN BOOLEAN			GetorSet, // Set == TRUE, Get == FALSE
		IN UINT32			TargetID,
		IN OUT PTARGET_DATA	TargetData,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LspRequest(
	IN  PLANSCSI_SESSION	LSS,
	IN  PLANSCSI_PDUDESC	PduDesc,
	OUT PBYTE				PduResponse,
	IN  PLANSCSI_SESSION	LSS2,
	IN  PLANSCSI_PDUDESC	PduDesc2,
	OUT PBYTE				PduResponse2
	);

NTSTATUS
LspPacketRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduResponse,
		OUT PUCHAR				PduRegister
		);

NTSTATUS
LspVendorRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduResponse
);

NTSTATUS
LspAcquireLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspReleaseLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	IN PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspGetLockOwner(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	OUT PUCHAR			LockOwner,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspGetLockData(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	OUT PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspWorkaroundCleanupLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	IN PLARGE_INTEGER	TimeOut
);

// lurnIdeDisk.c

VOID
IdeDiskAddAddressHandler (
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PTA_ADDRESS			Taddress
	); 


#endif // _NDAS_SCSI_PROC_H_
