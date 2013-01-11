#ifndef _NDAS_SCSI_PROC_H_
#define _NDAS_SCSI_PROC_H_


#define __NDAS_SCSI__					1
#define __NDAS_SCSI_W2K_SUPPORT__		1


// ATA disk options


#define __DETECT_CABLE80__				0
#define __ENABLE_LOCKED_READ__			0
#define __ENABLE_WRITECACHE_CONTROL__	0


// ATA optical devices ( CD/DVD ) options


#define __BSRECORDER_HACK__				1
#define __DVD_ENCRYPT_CONTENT__			0


// Debugging options


#define __SPINLOCK_DEBUG__				0
#define __ENABLE_PERFORMACE_CHECK__		0


// Internal module test

#define __ENABLE_BACL_TEST__				0
#define __ENABLE_CONTENTENCRYPT_AES_TEST__	0

#define __ALLOW_WRITE_WHEN_NO_ARBITER_STATUS__ 1 

#if __NDAS_SCSI_W2K_SUPPORT__

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#endif

#if WINVER >= 0x0501
#define NdasScsiDbgBreakPoint()	(KD_DEBUGGER_ENABLED ? DbgBreakPoint() : TRUE)
#else
#define NdasScsiDbgBreakPoint()	((*KdDebuggerEnabled) ? DbgBreakPoint() : TRUE) 
#endif

#if DBG

#define NDASSCSI_ASSERT( exp )	ASSERT( exp )

#else

#define NDASSCSI_ASSERT( exp )	\
	((!(exp)) ?					\
	NdasScsiDbgBreakPoint() :	\
	TRUE)

#endif


#include <ntddk.h>
#include <ntdddisk.h>
#include <TdiKrnl.h>
#include <windef.h>
#include <regstr.h>
#include <scsi.h>
#include "winscsiext.h"
#include <stdio.h>
#include <stdarg.h>
#include <scrc32.h>

#include <ndas/ndasdib.h>

#include "basetsdex.h"
#include "rijndael-api-fst.h"
#include "hash.h"
#include "hdreg.h"
#include "binparams.h"

#include "socketlpx.h"
#include "cipher.h"
#include "devreg.h"
#include "lpxtdi.h"

#include "ver.h"

#include "LSKlib.h"
#include "KDebug.h"
#include "cipher.h"
#include "LSProto.h"
#include "LSTransport.h"
#include "LSCcb.h"
#include "LSLur.h"
#include "LSLurn.h"
#include "LSLurnIDE.h"
#include "LSProtoIde.h"
#include "lsutils.h"

#include "draid.h"
#include "draidclient.h"
#include "draidarbiter.h"

#include "lslurnassoc.h"
#include "Scrc32.h"
#include "draidexp.h"

#include "KDebug.h"
#include "LSKLib.h"
#include "basetsdex.h"
#include "cipher.h"
#include "LSProto.h"
#include "LSLur.h"
#include "LSLurn.h"
#include "LSLurnIde.h"

#include "ndasscsidebug.h"

#include "ndaslurn.h"


// draid.c


NTSTATUS 
DraidRegisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
);

NTSTATUS
DraidUnregisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
);

NTSTATUS
DraidRegisterClient(
	PDRAID_CLIENT_INFO Client
);

NTSTATUS
DraidUnregisterClient(
	PDRAID_CLIENT_INFO Client
);


// Returns DRAID_RANGE_*
UINT32 DraidGetOverlappedRange(
	UINT64 Start1, UINT64 Length1,
	UINT64 Start2, UINT64 Length2,
	UINT64* OverlapStart, UINT64* OverlapEnd
);

// Returns new event count
INT32 
DraidReallocEventArray(
	PKEVENT** Events,
	PKWAIT_BLOCK* WaitBlocks,
	INT32 CurrentCount
);

VOID
DraidFreeEventArray(
	PKEVENT* Events,
	PKWAIT_BLOCK WaitBlocks
);


// lslurn.c

VOID
LurDetectSupportedNdasFeatures (
	PNDAS_FEATURES	NdasFeatures,
	UINT32			LowestHwVersion
	);

NTSTATUS
LurEnableNdasFeauresByDeviceMode (
	OUT PNDAS_FEATURES		EnabledFeatures,
	IN NDAS_FEATURES		SupportedFeatures,
	IN BOOLEAN				W2kReadOnlyPatch,
	IN NDAS_DEV_ACCESSMODE	DevMode
	);

NTSTATUS
LurEnableNdasFeaturesByUserRequest (
	OUT PNDAS_FEATURES	EnabledFeatures,
	IN NDAS_FEATURES	SupportedFeatures,
	IN ULONG			LurOptions
	); 

// lslurnassoc.c 

ULONG
LurnAsscGetChildBlockBytes (
	IN PLURELATION_NODE ParentLurn
	);

NTSTATUS
LurnAssocModeSense (
	IN PLURELATION_NODE Lurn,
	IN PCCB				Ccb
	); 

NTSTATUS 
LurnAssocModeSelect (
	IN PLURELATION_NODE Lurn,
	IN PCCB				Ccb
	); 

NTSTATUS
LurnRAIDUpdateCcbCompletion (
	IN PCCB Ccb,
	IN PCCB OriginalCcb
	);

NTSTATUS
LurnAssocQuery (
	IN PLURELATION_NODE			Lurn,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion,
	IN OUT PCCB					Ccb
	);

NTSTATUS
LurnAggrCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	OriginalCcb
	); 

// ndasdraidclient.c

NTSTATUS 
DraidClientResetRemoteArbiterContext (
	PDRAID_CLIENT_INFO	pClientInfo
	);

NTSTATUS
DraidClientEstablishArbiter (
	PDRAID_CLIENT_INFO	pClientInfo
	); 

VOID
DraidClientRefreshRaidStatusWithoutArbiter (
	PDRAID_CLIENT_INFO Client
	);

NTSTATUS 
DraidClientHandleCcb (
	PDRAID_CLIENT_INFO	pClientInfo
	); 

NTSTATUS
DraidClientHandleReplyForRequest (
	PDRAID_CLIENT_INFO  Client, 
	PDRIX_HEADER		ReplyMsg
	); 

NTSTATUS
DraidClientRecvHeaderWithoutWait (
	PDRAID_CLIENT_INFO			Client,
	PDRAID_CLIENT_CONNECTION	Connection
	);

NTSTATUS
DraidClientRecvHeaderAsync (
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
	);

NTSTATUS
DraidClientHandleNotificationMsg (
	PDRAID_CLIENT_INFO	pClientInfo,
	PDRIX_HEADER		Message
	); 

NTSTATUS
DraidClientSendRequest (
	PDRAID_CLIENT_INFO	Client, 
	UCHAR				Command,
	UINT32				CmdParam1,	// Command dependent.
	UINT64				CmdParam2,
	UINT32				CmdParam3,
	PLARGE_INTEGER		Timeout,
	DRAID_MSG_CALLBACK	Callback,	// Called when replied
	PVOID				CallbackParam1
	);

VOID
DraidClientFreeLock (
	PDRAID_CLIENT_LOCK Lock
	);

BOOLEAN
DraidClientProcessTerminateRequest (
	PDRAID_CLIENT_INFO	pClientInfo
	); 

NTSTATUS
DraidClientDisconnectFromArbiter (
	PDRAID_CLIENT_INFO			Client,
	PDRAID_CLIENT_CONNECTION	Connection
	); 

UCHAR 
DraidClientLurnStatusToNodeFlag (
	IN UINT32 LurnStatus
	);

PDRAID_CLIENT_LOCK
DraidClientAllocLock (
	UINT64		LockId,
	UCHAR 		LockType,
	UCHAR 		LockMode,
	UINT64		Addr,
	UINT32		Length
	);

NTSTATUS
DraidClientSendCcbToNondefectiveChildren (
	IN PDRAID_CLIENT_INFO		ClientInfo, 
	IN PCCB						Ccb,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion,
	IN BOOLEAN					CascadeMode	// Execute another ccb after previous ccb is completed.
	);

NTSTATUS 
DraidClientDispatchCcb (
	PDRAID_CLIENT_INFO	ClientInfo, 
	PCCB				Ccb
	);

ULONG 
DraidClientRAID1RSelectChildToRead (
	IN PRAID_INFO	pRaidInfo, 
	IN PCCB			Ccb
	);

VOID 
DraidClientUpdateAllNodeFlags (
	PDRAID_CLIENT_INFO	pClientInfo
	);

NTSTATUS 
DraidClientStart (
	PLURELATION_NODE	Lurn
	);

VOID
DraidClientUpdateNodeFlags (
	PDRAID_CLIENT_INFO	ClientInfo,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,	// Temp parameter.. set though lurn node info.
	UCHAR 				DefectCode  // Temp parameter.. set though lurn node info.
	); 

NTSTATUS
DraidClientFlush (
	PDRAID_CLIENT_INFO		ClientInfo, 
	PCCB					Ccb, 
	CCB_COMPLETION_ROUTINE	CompRoutine
	);

NTSTATUS
DraidReleaseBlockIoPermissionToClient (
	PDRAID_CLIENT_INFO pClientInfo,
	PCCB				Ccb
	);

NTSTATUS
DraidClientStop (
	PLURELATION_NODE	Lurn
	);

// ndasdraidarbiter.c

NTSTATUS 
DradArbiterInitializeOosBitmap (
	PDRAID_ARBITER_INFO pArbiter,
	UINT32				UpToDateNode
	); 

NTSTATUS
DraidClientIoWithLock (
	PDRAID_CLIENT_INFO	ClientInfo,
	UCHAR				LockMode, // DRIX_LOCK_MODE_
	PCCB				Ccb
); 

BOOLEAN
DraidArbiterRefreshRaidStatus (
	PDRAID_ARBITER_INFO	pArbiter	,
	BOOLEAN				ForceChange // Test RAID status change when node is not changed
	);

VOID
DraidArbiterFreeLock (
	PDRAID_ARBITER_INFO			pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT Lock
	); 

BOOLEAN
DraidArbiterUpdateInCoreRmd (
	IN PDRAID_ARBITER_INFO	pArbiter,
	IN BOOLEAN				ForceUpdate
	);

NTSTATUS
DraidArbiterWriteRmd (
	IN  PDRAID_ARBITER_INFO		pArbiter,
	OUT PNDAS_RAID_META_DATA	rmd
	);

NTSTATUS
DraidRebuildIoStart (
	PDRAID_ARBITER_INFO pArbiter
	);

VOID
DraidArbiterTerminateClient (
	PDRAID_ARBITER_INFO		Arbiter,
	PDRAID_CLIENT_CONTEXT	Client,
	BOOLEAN					CleanExit
	);

VOID DraidRebuildIoAcknowledge (
	PDRAID_ARBITER_INFO Arbiter
	);

NTSTATUS
DraidArbiterNotify (
	PDRAID_ARBITER_INFO		pArbiter, 
	PDRAID_CLIENT_CONTEXT	Client,
	UCHAR					Command,
	UINT64					CmdParam1,	// CmdParam* are dependent on Command.
	UINT64					CmdParam2,
	UINT32					CmdParam3
	); 

NTSTATUS
DraidArbiterCheckRequestMsg (
	PDRAID_ARBITER_INFO	Arbiter,
	PBOOLEAN			MsgHandled
	);

NTSTATUS
DraidArbiterUseSpareIfNeeded (
	PDRAID_ARBITER_INFO pArbiter
	); 

NTSTATUS
DraidArbiterGrantLockIfPossible (
	PDRAID_ARBITER_INFO Arbiter
	); 

NTSTATUS DraidRebuilldIoInitiate (
	PDRAID_ARBITER_INFO Arbiter
	);

NTSTATUS
DraidRebuildIoStop (
	PDRAID_ARBITER_INFO pArbiter
	);

NTSTATUS 
DraidRebuildIoLockRangeAggressive (
	PDRAID_ARBITER_INFO	pArbiter, 
	UINT64				StartAddr,
	UINT32				Length
	); 

NTSTATUS 
DraidDoRebuildIo (
	PDRAID_ARBITER_INFO pArbiter,
	UINT64				Addr,
	UINT32 				Length
	); 

VOID
DraidArbiterChangeOosBitmapBit	(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN				Set,	// TRUE for set, FALSE for clear
	UINT64				Addr,
	UINT64				Length
	);

NTSTATUS 
DraidArbiterUpdateOnDiskOosBitmap (
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN				UpdateAll
	);

VOID
DraidArbiterUpdateLwrBitmapBit (
	PDRAID_ARBITER_INFO			pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT HintAddedLock,
	PDRAID_ARBITER_LOCK_CONTEXT HintRemovedLock
	);

NTSTATUS
DraidArbiterHandleRequestMsg (
	PDRAID_ARBITER_INFO		pArbiter, 
	PDRAID_CLIENT_CONTEXT	pClient,
	PDRIX_HEADER			Message
	);

NTSTATUS
DraidRebuildIoCancel (
	PDRAID_ARBITER_INFO Arbiter
	);

PDRAID_ARBITER_LOCK_CONTEXT
DraidArbiterAllocLock (
	PDRAID_ARBITER_INFO pArbiter,
	UCHAR				LockType,
	UCHAR 				LockMode,
	UINT64				Addr,
	UINT32				Length
	);

NTSTATUS
DraidArbiterArrangeLockRange (
	PDRAID_ARBITER_INFO			pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT Lock,
	UINT32						Granularity,
	BOOLEAN						CheckOverlap
	); 

NTSTATUS
DraidArbiterFlushDirtyCacheNdas1_0 (
	IN PDRAID_ARBITER_INFO		pArbiter,
	IN UINT64					LockId,
	IN PDRAID_CLIENT_CONTEXT	pClient
	); 

NTSTATUS
DraidArbiterWriteMetaSync (
	IN PDRAID_ARBITER_INFO	Arbiter,
	IN PUCHAR				BlockBuffer,
	IN INT64				Addr,
	IN UINT32				Length 	// in sector
	); 


#endif // _NDAS_SCSI_PROC_H_