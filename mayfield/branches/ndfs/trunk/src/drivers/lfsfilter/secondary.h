#ifndef __SECONDARY_H__
#define __SECONDARY_H__


typedef enum _SECONDARY_REQ_TYPE
{
	SECONDARY_REQ_CONNECT = 1,
	SECONDARY_REQ_SEND_MESSAGE,
	SECONDARY_REQ_DISCONNECT,
	SECONDARY_REQ_DOWN

} SECONDARY_REQ_TYPE, *PSECONDARY_REQ_TYPE;


typedef struct _SESSION_CONTEXT
{
	_U32	SessionKey;
	union 
	{
	_U8		Flags;
		struct 
		{
			_U8 MessageSecurity:1;
			_U8 RwDataSecurity:1;
			_U8 Reserved:6;
		};
	};
	_U16	ChallengeLength;
	_U8		ChallengeBuffer[MAX_CHALLENGE_LEN];

	//
	//	Traffic control stats
	//

	_U32			PrimaryMaxDataSize;
	_U32			SecondaryMaxDataSize;
	LFS_TRANS_CTX	TransportCtx;


	_U16	Uid;
	_U16	Tid;
	_U16	NdfsMajorVersion;
	_U16	NdfsMinorVersion;

	_U8		SessionSlotCount;
	
	_U32	BytesPerFileRecordSegment;
	_U32	BytesPerSector;
	ULONG	SectorShift;
	ULONG	SectorMask;
	_U32	BytesPerCluster;
	ULONG	ClusterShift;
	ULONG	ClusterMask;

} SESSION_CONTEXT, *PSESSION_CONTEXT;

typedef struct _SECONDARY
{
	//KSPIN_LOCK						SpinLock;	//	protects Flags, Thread.Flags
	FAST_MUTEX						FastMutex ;	//  make critical section.
	
	KSEMAPHORE						Semaphore;
	_U16							SemaphoreConsumeRequiredCount;		
	_U16							SemaphoreReturnCount;
	
    LONG							ReferenceCount;

#define SECONDARY_INITIALIZING	0x00000001
#define SECONDARY_START			0x00000004
#define SECONDARY_RECONNECTING	0x00000008
//#define SECONDARY_STOP			0x00000010
#define SECONDARY_ERROR			0x80000000
		
	ULONG							Flags;

	struct _LFS_DEVICE_EXTENSION	*LfsDeviceExt;
	
	//HANDLE						VolumeRootFileHandle;
	//NETDISK_PARTITION_INFO		NetDiskPartitionInfo;

	LPX_ADDRESS						PrimaryAddress;
	
	LIST_ENTRY						FcbQueue;
	KSPIN_LOCK						FcbQSpinLock;	// protects FcbQueue

	LIST_ENTRY						FileExtQueue;
	FAST_MUTEX						FileExtQMutex;	// protects FileExtQueue
	ULONG							FileExtCount;
	LARGE_INTEGER					CleanUpTime;

    LIST_ENTRY						DirNotifyList;
    PNOTIFY_SYNC					NotifySync;

	HANDLE							ThreadHandle;
	PVOID							ThreadObject;
	ULONG							SessionId;

	struct 
	{
#define SECONDARY_THREAD_INITIALIZING			0x00000001
#define SECONDARY_THREAD_CONNECTED				0x00000002
#define SECONDARY_THREAD_DISCONNECTED			0x04000008
#define SECONDARY_THREAD_ERROR					0x80000000
#define SECONDARY_THREAD_CLOSED					0x08000000
#define SECONDARY_THREAD_TERMINATED				0x01000000
#define SECONDARY_THREAD_UNCONNECTED			0x02000000
#define SECONDARY_THREAD_REMOTE_DISCONNECTED	0x04000000
		
		ULONG						Flags;
		NTSTATUS					ConnectionStatus;

		HANDLE						AddressFileHandle;
		PFILE_OBJECT				AddressFileObject;

		HANDLE						ConnectionFileHandle;
		PFILE_OBJECT				ConnectionFileObject;

		struct _SECONDARY_REQUEST	*ProcessingSecondaryRequest[MAX_SLOT_PER_SESSION];
		LONG						WaitReceive;
		TDI_RECEIVE_CONTEXT			TdiReceiveContext;
		NDFS_REPLY_HEADER			NdfsReplyHeader;
		
		KEVENT						ReadyEvent;

		LIST_ENTRY					RequestQueue;
		KSPIN_LOCK					RequestQSpinLock;	// protects RequestQueue
		KEVENT						RequestEvent;

		SESSION_CONTEXT				SessionContext;
		NDFS_REPLY_HEADER			SplitNdfsReplyHeader;
		BOOLEAN						DiskGroup;

		LONG						VolRefreshTick;

	}Thread;
	
} SECONDARY, *PSECONDARY;


typedef struct _NON_PAGED_FCB 
{
	SECTION_OBJECT_POINTERS	SectionObjectPointers;
//	ULONG					OutstandingAsyncWrites;
//	PKEVENT					OutstandingAsyncEvent;
	FAST_MUTEX				AdvancedFcbHeaderMutex;

} NON_PAGED_FCB, *PNON_PAGED_FCB;


typedef struct _LFS_FCB 
{ 
    FSRTL_ADVANCED_FCB_HEADER	Header;
    PNON_PAGED_FCB				NonPaged;

    FILE_LOCK					FileLock;

	LONG						ReferenceCount;			// reference count
	LIST_ENTRY					ListEntry;

	UNICODE_STRING				FullFileName;
    WCHAR						FullFileNameBuffer[NDFS_MAX_PATH];	

	UNICODE_STRING				CaseInSensitiveFullFileName;
    WCHAR						CaseInSensitiveFullFileNameBuffer[NDFS_MAX_PATH];	

	LONG						OpenCount;
	LONG						UncleanCount;
	BOOLEAN						FileRecordSegmentHeaderAvail;
	BOOLEAN						DeletePending;
	LARGE_INTEGER				OpenTime;
	CHAR						Buffer[1];
	
//	LONG						CleanCountByCorruption;

} LFS_FCB, *PLFS_FCB;

typedef enum _TYPE_OF_OPEN 
{
    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile

} TYPE_OF_OPEN;

typedef struct	_FILE_EXTENTION 
{
	UINT32					LfsMark;
	PSECONDARY				Secondary;
	PFILE_OBJECT			FileObject;
	BOOLEAN					RelatedFileObjectClosed;

	PLFS_FCB				Fcb;
	
	LIST_ENTRY				ListEntry;

	BOOLEAN					Corrupted;
	ULONG					SessionId;
	_U32					PrimaryFileHandle;

	TYPE_OF_OPEN			TypeOfOpen;
	
	_U32					IrpFlags;
	_U8						IrpSpFlags;
	WINXP_REQUEST_CREATE	CreateContext;

	VCN						CachedVcn;
	PCHAR					Cache;

	//
	// Valid only in secondary mode(or handles opened in secondary mode) 
	//			  and after IRP_MJ_DIRECTORY_CONTROL/IRP_MN_QUERY_DIRECTORY is called with DirectoryFile.
	// Used to restore current file query index after reconnecting to another primary. 
	// Set to (ULONG)-1 for unused.
	//
	ULONG					LastQueryFileIndex;

	// 
	//	Set when LastQueryFileIndex is set with current session's ID
	//  Need to use LastQueryFileIndex value for next query if session ID is changed.
	//
	ULONG					LastDirectoryQuerySessionId;

		
#ifdef __TEST_MODE__
	PVOID					OriginalFsContext;
	PVOID					OriginalFsContext2;
#endif

	ULONG					BufferLength;
	_U8						Buffer[1];

} FILE_EXTENTION, *PFILE_EXTENTION;

	
typedef struct _SECONDARY_REQUEST
{
	SECONDARY_REQ_TYPE	RequestType;
	LONG				ReferenceCount;			// reference count
	LIST_ENTRY			ListEntry;
	ULONG				SessionId;

	PFILE_EXTENTION		FileExt;

	BOOLEAN				Synchronous;
	BOOLEAN				PotentialDeadVolLock;
	KEVENT				CompleteEvent;
	NTSTATUS			ExecuteStatus;

 	ULONG				OutputBufferLength;
   	PUCHAR				OutputBuffer;

	//DES_CBC_CTX			DesCbcContext;
	//_U8					Iv[8];

	ULONG				NdfsMessageLength;
#include <pshpack1.h>
	union
	{
		_U8		NdfsMessage[1];

		UNALIGNED struct
		{
			NDFS_REQUEST_HEADER			NdfsRequestHeader;
			_U8							NdfsRequestData[1];
		};

		UNALIGNED struct  
		{
			NDFS_REPLY_HEADER			NdfsReplyHeader;
			_U8							NdfsReplyData[1];
		};
	};
#include <poppack.h>
	
} SECONDARY_REQUEST, *PSECONDARY_REQUEST;


#endif