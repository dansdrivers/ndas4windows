#ifndef _NDAS_KLIB_PROC_H_
#define _NDAS_KLIB_PROC_H_


#define __NDAS_KLIB__						1
#define __NDAS_KLIB_W2K_SUPPORT__			1
#define __NDAS_KLIB_ENABLE_READ_LOCK__		1
#define __NDAS_SCSI_REMAIN_QUERY_IDE_BUG__	1	// it's bug but it has no side effect, next time fix it.

#define __NDAS_ODD__						1

#define __NDAS_CHIP_BUG_AVOID__				1
#define __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__	1
#define __NDAS_CHIP_PIO_READ_AND_WRITE__	1

#define __NDAS_BAD_SECTOR_TEST__				1
#define __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__	0
#define __TEST_BLOCK_SIZE__	1		


// ATA/ATAPI device options

#define __USE_PRESET_PIO_MODE__			0


// ATA disk options

#define __DETECT_CABLE80__				0
#define __ENABLE_WRITECACHE_CONTROL__	0
#define __ENABLE_CHECK_POWER_MODE__		0


// ATA optical devices ( CD/DVD ) options


#define __BSRECORDER_HACK__				0
#define __DVD_ENCRYPT_CONTENT__			0


// Debugging options


#define __SPINLOCK_DEBUG__				0
#define __ENABLE_PERFORMACE_CHECK__		0


// Internal module test

#define __ENABLE_BACL_TEST__				0
#define __ENABLE_CONTENTENCRYPT_AES_TEST__	0


#if __NDAS_KLIB_W2K_SUPPORT__

#define STATUS_CLUSTER_NODE_UNREACHABLE  ((NTSTATUS)0xC013000DL)
#define STATUS_CLUSTER_NETWORK_NOT_FOUND ((NTSTATUS)0xC0130007L)

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#endif

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

#include <ntddk.h>
#include <ntdddisk.h>
#include <TdiKrnl.h>
#include <windef.h>
#include <regstr.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <stdarg.h>


#include "ndascommonheader.h"

extern BOOLEAN NdasTestBug;

#if DBG

#define NDAS_ASSERT(exp)	ASSERT(exp)

#else

#define NDAS_ASSERT(exp)				\
	((NdasTestBug && (exp) == FALSE) ?	\
	NdasDbgBreakPoint() :				\
	FALSE)

#endif

#include "scrc32.h"
#include "rijndael-api-fst.h"
#include "hash.h"

#include "binparams.h"
#include "cipher.h"

#include "hdreg.h"
#include "devreg.h"
#include "ver.h"

#include "lsprotospec.h"
#include "lsprotoidespec.h"
#include "ndas/ndasdib.h"

#include "ndasfs.h"

#include "ndasscsi.h"

#include "ndasrprotocol.h"

#include "lpxtdiv2.h"

#include "ndasflowcontrol.h"
#include "lsproto.h"

#include "ndasklib.h"
#define	KDPrint( DEBUGLEVEL, FORMAT )
#define	KDPrintM( DEBUGMASK, FORMAT )

#include "ndastransport.h"

#include "lurn.h"
#include "lurnata.h"

#include "ndasrclient.h"
#include "ndasrarbitrator.h"
#include "lurnndasraid.h"

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
	IN  BYTE				LockOpCode,
	IN  UINT32				LockId,
	IN	BOOLEAN				Wait
	);

NTSTATUS
NdasRaidLurnExecuteSynchrously (
	IN PLURELATION_NODE	ChildLurn,
	IN UCHAR			CdbOperationCode,
	IN BOOLEAN			ForceUnitAccess,
	IN PCHAR			DataBuffer,
	IN UINT64			BlockAddress,		// Child block addr
	IN UINT32			BlockTransfer,		// Child block count
	IN BOOLEAN			RelativeAddress
	);

NTSTATUS
NdasRaidLurnExecuteSynchrouslyCompletionRoutine (
	IN PCCB		Ccb,
	IN PKEVENT	Event
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
NdasRaidClientUpdateLocalNodeFlags (
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

NTSTATUS
LurnIdeDiskDeviceLockControl(
	IN PLURELATION_NODE		Lurn,
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PCCB					Ccb
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
	IN PTA_NDAS_ADDRESS		SrcAddr,
	IN PTA_NDAS_ADDRESS		DestAddr,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN PLSTRANS_OVERLAPPED	Overlapped,
	IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspConnectMultiBindAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_NDAS_ADDRESS	BoundAddr,
	IN	PTA_NDAS_ADDRESS	BindAddr1,
	IN  PTA_NDAS_ADDRESS	BindAddr2,
	IN  PTA_NDAS_ADDRESS	DestAddr,
	IN  BOOLEAN				BindAnyAddress,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspConnectMultiBindTaAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_NDAS_ADDRESS	BoundAddr,
	IN	PTA_NDAS_ADDRESS	BindAddr1,
	IN  PTA_NDAS_ADDRESS	BindAddr2,
	IN  PTA_NDAS_ADDRESS	DestAddr,
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
LspMove (
	PLANSCSI_SESSION	ToLSS,
	PLANSCSI_SESSION	FromLSS,
	BOOLEAN				MoveBufferPointers
	);

VOID
LspSetDefaultTimeOut(
		IN PLANSCSI_SESSION	LSS,
		IN PLARGE_INTEGER	TimeOut
	);

VOID
LspGetAddresses (
	PLANSCSI_SESSION	LSS,
	PTA_NDAS_ADDRESS	BindingAddress,
	PTA_NDAS_ADDRESS	TargetAddress
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


#endif // _NDAS_KLIB_PROC_H_
