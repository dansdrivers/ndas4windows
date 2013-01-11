#ifndef __XIXFS_DRV_H__
#define __XIXFS_DRV_H__

#include "XixFsType.h"
#include "XixFsDebug.h"
#include "xixfscontrol.h"

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions
#include "xixfs.ver"
#include <ndasverp.h>


/*
//
//	Desired Access
//

#define DELETE                           			(0x00010000L)
#define READ_CONTROL					(0x00020000L)
#define WRITE_DAC                        			(0x00040000L)
#define WRITE_OWNER                      		(0x00080000L)
#define SYNCHRONIZE						(0x00100000L)

#define STANDARD_RIGHTS_REQUIRED         	(0x000F0000L)
#define STANDARD_RIGHTS_READ             	(READ_CONTROL)
#define STANDARD_RIGHTS_WRITE            	(READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE          	(READ_CONTROL)
#define STANDARD_RIGHTS_ALL              		(0x001F0000L)
#define SPECIFIC_RIGHTS_ALL              		(0x0000FFFFL)

#define FILE_READ_DATA            			( 0x0001 )    // file & pipe
#define FILE_LIST_DIRECTORY       			( 0x0001 )    // directory
#define FILE_WRITE_DATA           			( 0x0002 )    // file & pipe
#define FILE_ADD_FILE             				( 0x0002 )    // directory
#define FILE_APPEND_DATA          			( 0x0004 )    // file
#define FILE_ADD_SUBDIRECTORY     		( 0x0004 )    // directory
#define FILE_CREATE_PIPE_INSTANCE 		( 0x0004 )    // named pipe


#define FILE_READ_EA              				( 0x0008 )    // file & directory
#define FILE_WRITE_EA             				( 0x0010 )    // file & directory
#define FILE_EXECUTE              				( 0x0020 )    // file
#define FILE_TRAVERSE             				( 0x0020 )    // directory
#define FILE_DELETE_CHILD         			( 0x0040 )    // directory
#define FILE_READ_ATTRIBUTES      			( 0x0080 )    // all
#define FILE_WRITE_ATTRIBUTES     			( 0x0100 )    // all

#define FILE_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1FF)
#define FILE_GENERIC_READ         (STANDARD_RIGHTS_READ     |\
                                   FILE_READ_DATA           |\
                                   FILE_READ_ATTRIBUTES     |\
                                   FILE_READ_EA             |\
                                   SYNCHRONIZE)


#define FILE_GENERIC_WRITE        (STANDARD_RIGHTS_WRITE    |\
                                   FILE_WRITE_DATA          |\
                                   FILE_WRITE_ATTRIBUTES    |\
                                   FILE_WRITE_EA            |\
                                   FILE_APPEND_DATA         |\
                                   SYNCHRONIZE)


#define FILE_GENERIC_EXECUTE      (STANDARD_RIGHTS_EXECUTE  |\
                                   FILE_READ_ATTRIBUTES     |\
                                   FILE_EXECUTE             |\
                                   SYNCHRONIZE)

//
// Define share access rights to files and directories
//

#define FILE_SHARE_READ                 0x00000001  
#define FILE_SHARE_WRITE                0x00000002  
#define FILE_SHARE_DELETE               0x00000004  
#define FILE_SHARE_VALID_FLAGS          0x00000007

*/


/*
//
// Define the create disposition values
//

#define FILE_SUPERSEDE                  0x00000000
#define FILE_OPEN                       0x00000001
#define FILE_CREATE                     0x00000002
#define FILE_OPEN_IF                    0x00000003
#define FILE_OVERWRITE                  0x00000004
#define FILE_OVERWRITE_IF               0x00000005
#define FILE_MAXIMUM_DISPOSITION        0x00000005

//
// Define the create/open option flags
//

#define FILE_DIRECTORY_FILE                     0x00000001
#define FILE_WRITE_THROUGH                      0x00000002
#define FILE_SEQUENTIAL_ONLY                    0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING          0x00000008

#define FILE_SYNCHRONOUS_IO_ALERT               0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT            0x00000020
#define FILE_NON_DIRECTORY_FILE                 0x00000040
#define FILE_CREATE_TREE_CONNECTION             0x00000080

#define FILE_COMPLETE_IF_OPLOCKED               0x00000100
#define FILE_NO_EA_KNOWLEDGE                    0x00000200
#define FILE_OPEN_FOR_RECOVERY                  0x00000400
#define FILE_RANDOM_ACCESS                      0x00000800

#define FILE_DELETE_ON_CLOSE                    0x00001000
#define FILE_OPEN_BY_FILE_ID                    0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT             0x00004000
#define FILE_NO_COMPRESSION                     0x00008000

#define FILE_RESERVE_OPFILTER                   0x00100000
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#define FILE_OPEN_NO_RECALL                     0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY          0x00800000

#define FILE_COPY_STRUCTURED_STORAGE            0x00000041
#define FILE_STRUCTURED_STORAGE                 0x00000441

#define FILE_VALID_OPTION_FLAGS                 0x00ffffff
#define FILE_VALID_PIPE_OPTION_FLAGS            0x00000032
#define FILE_VALID_MAILSLOT_OPTION_FLAGS        0x00000032
#define FILE_VALID_SET_FLAGS                    0x00000036
*/

/*

//
//		File Attribute
//

#define FILE_ATTRIBUTE_READONLY             0x00000001  
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  
//OLD DOS VOLID                             0x00000008

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  
#define FILE_ATTRIBUTE_DEVICE               0x00000040  
#define FILE_ATTRIBUTE_NORMAL               0x00000080  

#define FILE_ATTRIBUTE_TEMPORARY            0x00000100  
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200  
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400  
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800  

#define FILE_ATTRIBUTE_OFFLINE              0x00001000  
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000  
#define FILE_ATTRIBUTE_ENCRYPTED            0x00004000  

//
//  This definition is old and will disappear shortly
//

#define FILE_ATTRIBUTE_CONTENT_INDEXED      FILE_ATTRIBUTE_NOT_CONTENT_INDEXED

#define FILE_ATTRIBUTE_VALID_FLAGS      0x00007fb7
#define FILE_ATTRIBUTE_VALID_SET_FLAGS  0x000031a7

*/
//#include <ntifs.h>

#ifndef FILE_READ_ONLY_VOLUME
#define FILE_READ_ONLY_VOLUME           0x00080000  
#endif

#define XIXFS_READ_AHEAD_GRANULARITY			(0x10000)
#define XIFSD_MAX_BUFF_LIMIT					(0x10000)
#define XIFSD_MAX_IO_LIMIT						(0x10000)		


#define try_return(S)						{ (S); leave; }


#define Add2Ptr(P, I)			((PVOID)((PUCHAR)(P) + (I)))


#define XIFS_NODE_UNDEFINED					(0x0000)
#define XIFS_NODE_FS_GLOBAL					(0x1901)
#define XIFS_NODE_FCB						(0x1902)
#define XIFS_NODE_CCB						(0x1904)
#define XIFS_NODE_VCB						(0x1908)
#define XIFS_NODE_IRPCONTEXT				(0x1910)
#define XIFS_NODE_CLOSEFCBCTX				(0x1920)
#define XIFS_NODE_LCB						(0x1940)


#define XifsNodeType(P)			( (P) != NULL ? (*((uint16 *)(P))) : XIFS_NODE_UNDEFINED )
#define XifsSafeNodeType(P)		( *((uint16 *)(P)) )


#define TAG_FCB								'tbcf'
#define TAG_CCB								'tbcc'
#define TAG_IPCONTEXT						'tcip'
#define TAG_CLOSEFCBCTX						'tcfx'
#define TAG_CHILD							'tdch'
#define TAG_BUFFER							'tfub'
#define TAG_ADDRESS							'trda'
#define TAG_G_TABLE							'ttgf'
#define TAG_FILE_NAME						'tmnf'
#define TAG_LCB								'tbcl'
#define TAG_EXP								'tpxe'
#define TAG_IOCONTEX						'txoi'
#define TAG_REMOVE_LOCK						'tler'

#define	XifsdSetFlag(Flag, Value)				( (Flag) |= (Value) )
#define	XifsdClearFlag(Flag, Value)				( (Flag) &= ~(Value) )
#define XifsdCheckFlag(Flag, Value) 			( (Flag) & (Value) )
#define XifsdCheckFlagBoolean(Flag, Value)		( ( (Flag) & (Value) )?TRUE:FALSE )

//#define NDAS_XIXFS_UNLOAD				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 18, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//#define NDAS_XIXFS_VERSION				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x200, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
//#define XIFS_NAME	L"\\Xifs"
//#define XIXFS_CONTROL_DEVICE_NAME	L"\\Device\\XixfsControl"
//#define XIXFS_CONTROL_LINK_NAME		L"\\DosDevices\\XixfsControl"	



typedef struct _XIFS_NODE_ID {
	uint16  NodeTypeCode;
	uint16  NodeByteSize;
}XIFS_NODE_ID, *PXIFS_NODE_ID;



/*
	LOT INFORMATION
*/
#define LOT_INFO_TYPE_INVALID				(0x00000000)
#define LOT_INFO_TYPE_VOLUME				(0x00000001)
#define LOT_INFO_TYPE_FILE					(0x00000002)
#define LOT_INFO_TYPE_DIRECTORY				(0x00000004)
#define LOT_INFO_TYPE_HOSTREG				(0x00000008)
#define LOT_INFO_TYPE_BITMAP				(0x00000010)
#define LOT_INFO_TYPE_ADDRMAP				(0x00000020)
#define LOT_INFO_TYPE_LOG					(0x00000040)



#define LOT_FLAG_INVALID					(0x00000000)
#define LOT_FLAG_BEGIN						(0x00000001)
#define LOT_FLAG_END						(0x00000002)
#define LOT_FLAG_BODY						(0x00000004)



/*
	DISK FILE ENTRY INFORMATION
*/
#define XIFS_FD_STATE_DELETED				(0x00000000)
#define XIFS_FD_STATE_CREATE				(0x00000001)

#define XIFS_FD_ACCESS_U_R					(0x00000001)
#define XIFS_FD_ACCESS_U_W					(0x00000002)
#define XIFS_FD_ACCESS_U_X					(0x00000004)				
#define XIFS_FD_ACCESS_G_R					(0x00000010)
#define XIFS_FD_ACCESS_G_W					(0x00000020)
#define XIFS_FD_ACCESS_G_X					(0x00000040)				
#define XIFS_FD_ACCESS_O_R					(0x00000100)				
#define XIFS_FD_ACCESS_O_W					(0x00000200)				
#define XIFS_FD_ACCESS_O_X					(0x00000400)				
#define XIFS_FD_DEFAULT_ACCESS				(0x00000557)

#define	XIFS_FD_TYPE_INVALID				(0x00000000)
#define	XIFS_FD_TYPE_FILE					(0x00000001)
#define	XIFS_FD_TYPE_DIRECTORY				(0x00000002)
#define	XIFS_FD_TYPE_ROOT_DIRECTORY			(0x00000004)
#define	XIFS_FD_TYPE_PAGE_FILE				(0x00000008)
#define	XIFS_FD_TYPE_SYSTEM_FILE			(0x00000010)
#define	XIFS_FD_TYPE_PIPE_FILE				(0x00000020)
#define	XIFS_FD_TYPE_DEVICE_FILE			(0x00000040)
#define XIFS_FD_TYPE_VOLUME					(0x00000080)
#define XIFS_FD_TYPE_CHILD_SF_LINK			(0x00000100)
#define XIFS_FD_TYPE_CHILD_HD_LINK			(0x00000200)



/*
	NAME CHECK FLAGS
*/
#define XIFS_FILE_NAME_TYPE_INVALID			(0x00000000)
#define XIFS_FILE_NAME_TYPE_GENERAL			(0x00000001)
#define XIFS_FILE_NAME_TYPE_DOT				(0x00000002)
#define XIFS_FILE_NAME_TYPE_DOTDOT			(0x00000003)
#define XIFS_FILE_NAME_TYPE_STAR			(0x00000004)



/*
	ACCESS CHECK FLAGS
*/
#define XIFSD_DESIRED_ACCESS_FILE_READ			(FILE_GENERIC_READ | FILE_GENERIC_EXECUTE)		
#define	XIFSD_DESIRED_ACCESS_FILE_WRITE			(FILE_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)
#define	XIFSD_DERIRED_ACCESS_DIRECTORY_READ 	(FILE_LIST_DIRECTORY | FILE_TRAVERSE )
#define	XIFSD_DERIRED_ACCESS_DIRECOTRY_WRITE	(FILE_ADD_SUBDIRECTORY | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD ) 




typedef struct _XIFS_IO_LOT_INFO {
	uint64						LogicalStartOffset;
	uint64						PhysicalAddress;
	uint32						Type;						
	uint32						Flags;						
	uint64						LotIndex;					
	uint64						BeginningLotIndex;			
	uint64						PreviousLotIndex;			
	uint64						NextLotIndex;
	uint32						StartDataOffset;
	uint32						LotTotalDataSize;			
	uint32						LotUsedDataSize;			
}XIFS_IO_LOT_INFO, *PXIFS_IO_LOT_INFO;



#define XIFSD_LCB_STATE_DELETE_ON_CLOSE			(0x00000001)
#define XIFSD_LCB_STATE_LINK_IS_GONE			(0x00000002)

#define XIFSD_LCB_STATE_IGNORE_CASE_SET			(0x00000010)

#define XIFSD_LCB_NOT_FROM_POOL					(0x80000000)

typedef struct _XIFS_LCB{
	XIFS_NODE_ID			;
	LIST_ENTRY ParentFcbLinks;
	struct _XIFS_FCB *ParentFcb;

	LIST_ENTRY ChildFcbLinks;
	struct _XIFS_FCB *ChildFcb;


	ULONG Reference;

	ULONG LCBFlags;

	ULONG FileAttributes;


	RTL_SPLAY_LINKS Links;
	RTL_SPLAY_LINKS	IgnoreCaseLinks;


	UNICODE_STRING FileName;
	UNICODE_STRING IgnoreCaseFileName;
	
}XIFS_LCB, *PXIFS_LCB;




#define FCB_FILE_LOCK_INVALID		0x00000000
#define FCB_FILE_LOCK_HAS			0x00000001
#define FCB_FILE_LOCK_OTHER_HAS		0x00000002





/*
typedef struct _XIFS_ADDR_MAP_INFO {
	uint64		AddressCount;
	uint64		ReadLotCount;
	uint64		AllocationCount;
	uint64		*Addrs;
}XIFS_ADDR_MAP_INFO, *PXIFS_ADDR_MAP_INFO;	
*/





typedef struct _XIFS_LOT_MAP{
	uint32		MapType;				//allocated/free/checkout/system
	uint64		NumLots;
	uint32		BitMapBytes;
	uint64		StartIndex;
	uint8		Data[0];
}XIFS_LOT_MAP, *PXIFS_LOT_MAP;


#define	XIFSD_VCB_STATE_VOLUME_MOUNTED				(0x00000001)
#define	XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS	(0x00000002)
#define	XIFSD_VCB_STATE_VOLUME_DISMOUNTED			(0x00000004)
#define XIFSD_VCB_STATE_VOLUME_MOUNTING_PROGRESS	(0x00000008)
#define XIFSD_VCB_STATE_VOLUME_SHOTDOWN				(0x00001000)
#define	XIFSD_VCB_STATE_VOLUME_INVALID				(0x00000020)

#define	XIFSD_VCB_FLAGS_VOLUME_LOCKED				(0x00000100)
#define	XIFSD_VCB_FLAGS_VOLUME_READ_ONLY			(0x00000200)
#define	XIFSD_VCB_FLAGS_VCB_INITIALIZED				(0x00000400)
#define	XIFSD_VCB_FLAGS_VCB_NEED_CHECK				(0x00000800)
#define XIFSD_VCB_FLAGS_PROCESSING_PNP				(0x00010000)
#define XIFSD_VCB_FLAGS_DEFERED_CLOSE				(0x00020000)


#define XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES		(0x00001000)
#define XIFSD_VCB_FLAGS_RECHECK_RESOURCES			(0x00002000)

#define XIFSD_VCB_RESOURCE_UPDATE_FALAG				(0x00000001)


#define XIFSD_RESIDENT_REFERENCE					2
#define XIFSD_RESIDUALUSERREF						3
#define XIFSD_RESIDUALREF							5

#define	DEFAULT_XIFS_UPDATEWAIT				( 180 * TIMERUNIT_SEC)	// 60 seconds.
#define DEFAULT_XIFS_UMOUNTWAIT				( 30 * TIMERUNIT_SEC)
#define DEFAULT_XIFS_RECHECKRESOURCE		( 180 * TIMERUNIT_SEC)

typedef struct _XIFS_VCB{
	XIFS_NODE_ID;

	uint32 VCBCleanup;
    uint32 VCBReference;
    uint32 VCBUserReference;

	uint32 VCBResidualReference;
	uint32 VCBResidualUserReference;	


	LIST_ENTRY							VCBLink;
	struct _XI_VOLUME_DEVICE_OBJECT		*PtrVDO;
	SHARE_ACCESS  						ShareAccess;	
	PFILE_OBJECT 						PtrStreamFileObject;
	PFILE_OBJECT						LockVolumeFileObject;
	
	// Syncronization object
	ERESOURCE							VCBResource;
	ERESOURCE							FileResource;
	

	FAST_MUTEX							VCBMutex;
	PVOID								VCBLockThread;
	PFILE_OBJECT						VCBLockFileObject;
	
	
	PDEVICE_OBJECT						TargetDeviceObject;
	PDEVICE_OBJECT						RealDeviceObject;
	PVPB								PtrVPB;

	PNOTIFY_SYNC						NotifyIRPSync;
	LIST_ENTRY							NextNotifyIRP;

	uint32								VCBFlags;
	uint32								VCBState;

	

	BOOLEAN								IsVolumeRemovable;
	BOOLEAN								IsVolumeWriteProctected;
	BOOLEAN								IsNasProduct;


	
	uint8								HostId[16];
	// Changed by ILGU HONG
	//	chesung suggest
	//uint8								HostMac[6];
	uint8								HostMac[32];
	// Changed by ILGU HONG
	//uint8								DiskId[6];
	uint8								DiskId[16];
	uint32								PartitionId;

	DISK_GEOMETRY 						DiskGeometry;
	PARTITION_INFORMATION				PartitionInformation;
	LARGE_INTEGER 						LengthInfo;
	uint64								NumLots;
	uint32								LotSize;
	uint32								VolumeLotSignature;
	uint64								VolCreationTime;
	uint32								VolSerialNumber;
	UNICODE_STRING						VolLabel;

	uint64								AllocatedLotMapIndex;
	uint64								FreeLotMapIndex;
	uint64								CheckOutLotMapIndex;
	uint64								HostRegLotMapIndex;
	uint32								HostRegLotMapLockStatus;
	uint64								RootDirectoryLotIndex;
	uint64								HostCheckOutLotMapIndex;
	uint64								HostUsedLotMapIndex;
	uint64								HostUnUsedLotMapIndex;
		//	Added by ILGU HONG for 08312006
	uint64								NdasVolBacl_Id;
		//	Added by ILGU HONG end
	uint32								SectorSize;

	uint32								HostRecordIndex;
	PXIFS_LOT_MAP						HostFreeLotMap;
	PXIFS_LOT_MAP						HostDirtyLotMap;
	PXIFS_LOT_MAP						VolumeFreeMap;
	uint32								ResourceFlag;

	RTL_GENERIC_TABLE					FCBTable;

	struct _XIFS_FCB					*VolumeDasdFCB;					
	struct _XIFS_FCB					*RootDirFCB;
	struct _XIFS_FCB					*MetaFCB;


	PVPB								SwapVpb;

	/* Delayed close queue */
	WORK_QUEUE_ITEM		WorkQueueItem;
	LIST_ENTRY			DelayedCloseList;
	uint32				DelayedCloseCount;

	/* Resource Check Event */
	KEVENT				ResourceEvent;

	/* Meta Delayed Write Thread */
	KEVENT				VCBUmountEvent;
	KEVENT				VCBUpdateEvent;
	KEVENT				VCBGetMoreLotEvent;
	KEVENT				VCBStopOkEvent;
	HANDLE				hMetaUpdateThread;

}XIFS_VCB, *PXIFS_VCB;



typedef struct _XI_VOLUME_DEVICE_OBJECT {
	DEVICE_OBJECT	DeviceObject;
    uint32 PostedRequestCount;
    uint32 OverflowQueueCount;
    LIST_ENTRY OverflowQueue;
    KSPIN_LOCK OverflowQueueSpinLock;
	XIFS_VCB		VCB;
}XI_VOLUME_DEVICE_OBJECT, *PXI_VOLUME_DEVICE_OBJECT;

/*
typedef struct _XIFSD_REMOTE_LOCK {
	int32			RefCount;
	LIST_ENTRY		NextRemoteLock;
	uint64			StartOffset;
	uint64			Size;
}XIFSD_REMOTE_LOCK, *PXIFSD_REMOTE_LOCK;


typedef struct _XIFSD_REMOTE_LOCK_INFO{
	FAST_MUTEX		RemoteFileLockMutex;
	LIST_ENTRY		RemoteFileLockList;
} XIFSD_REMOTE_LOCK_INFO, *PXIFSD_REMOTE_LOCK_INFO;
*/




/*
	FCB FLAGS
*/
#define	XIFSD_FCB_INIT								(0x00000001)
#define	XIFSD_FCB_TEARDOWN							(0x00000002)
#define XIFSD_FCB_IN_TABLE							(0x00000004)


//		Open State
#define	XIFSD_FCB_OPEN_REF							(0x00000010)
#define	XIFSD_FCB_OPEN_READ_ONLY					(0x00000020)
#define XIFSD_FCB_OPEN_WRITE						(0x00000040)
#define	XIFSD_FCB_OPEN_TEMPORARY					(0x00000080)



//		Operation State
#define	XIFSD_FCB_OP_WRITE_THROUGH					(0x00000100)
#define	XIFSD_FCB_OP_FAST_IO_READ_IN_PROGESS		(0x00000200)
#define	XIFSD_FCB_OP_FAST_IO_WRITE_IN_PROGESS		(0x00000400)



//		Additional File State
#define	XIFSD_FCB_CACHED_FILE						(0x00001000)
#define	XIFSD_FCB_DELETE_ON_CLOSE					(0x00002000)

#define XIFSD_FCB_OUTOF_LINK						(0x00004000)



//		File State Change Flag
//		File Header Change
#define	XIFSD_FCB_MODIFIED_ALLOC_SIZE				(0x00010000)
#define	XIFSD_FCB_MODIFIED_FILE_SIZE				(0x00020000)
#define	XIFSD_FCB_MODIFIED_FILE_NAME				(0x00040000)
#define	XIFSD_FCB_MODIFIED_FILE_TIME				(0x00080000)
#define	XIFSD_FCB_MODIFIED_FILE_ATTR				(0x00100000)


#define	XIFSD_FCB_MODIFIED_PATH						(0x00200000)
#define	XIFSD_FCB_MODIFIED_LINKCOUNT				(0x00400000)

#define XIFSD_FCB_MODIFIED_FILE						(0x005F2000)


#define	XIFSD_FCB_MODIFIED_CHILDCOUNT				(0x00800000)
#define	XIFSD_FCB_CHANGE_HEADER						(0x00FF0000)



//		File State Change
#define XIFSD_FCB_CHANGE_DELETED					(0x01000000)
#define XIFSD_FCB_CHANGE_RELOAD						(0x02000000)
#define	XIFSD_FCB_CHANGE_RENAME						(0x04000000)
#define	XIFSD_FCB_CHANGE_LINK						(0x08000000)
#define XIFSD_FCB_LOCKED							(0x10000000)
#define XIFSD_FCB_EOF_IO							(0x20000000)
#define	XIFSD_FCB_NOT_FROM_POOL						(0x80000000)



//		FCB state
#define XIFS_STATE_FCB_NORM							(0x00000000)
#define XIFS_STATE_FCB_LOCKED						(0x00000001)				
#define XIFS_STATE_FCB_INVALID						(0x00000002)
#define XIFS_STATE_FCB_NEED_RELOAD					(0x00000004)

/*
typedef struct _FSRTL_COMMON_FCB_HEADER {
  CSHORT  NodeTypeCode;
  CSHORT  NodeByteSize;
  UCHAR   Flags;
  UCHAR   IsFastIoPossible;
  UCHAR   Flags2;
  UCHAR   Reserved;
  PERESOURCE  Resource;
  PERESOURCE  PagingIoResource;
  LARGE_INTEGER  AllocationSize;
  LARGE_INTEGER  FileSize;
  LARGE_INTEGER  ValidDataLength;
} FSRTL_COMMON_FCB_HEADER;

*/


#define FCB_TYPE_FILE		(0x000000001)
#define FCB_TYPE_DIR		(0x000000002)
#define FCB_TYPE_VOLUME		(0x000000003)


#define FCB_TYPE_DIR_INDICATOR	(0x8000000000000000)
#define FCB_TYPE_FILE_INDICATOR	(0x0000000000000000)
#define FCB_ADDRESS_MASK		(0x7FFFFFFFFFFFFFFF)

#define XIFSD_TYPE_FILE(FileId)	(!((FileId) & FCB_TYPE_DIR_INDICATOR))
#define XIFSD_TYPE_DIR(FileId)	((FileId) & FCB_TYPE_DIR_INDICATOR) 


#define XIFSD_FCB_UPDATE_TIMEOUT		(10 * TIMERUNIT_SEC)	

typedef struct _XIFS_FCB {
	union {
        FSRTL_COMMON_FCB_HEADER Header;
        FSRTL_COMMON_FCB_HEADER;
    };

	SECTION_OBJECT_POINTERS				SectionObject;
	PXIFS_VCB							PtrVCB;


	ERESOURCE							MainResource;
	ERESOURCE							RealPagingIoResource;
	PERESOURCE							FileResource;
	SHARE_ACCESS						FCBShareAccess;
	uint32								FileAttribute;
	uint32								LinkCount;
	uint64								CreationTime;
	uint64								LastAccessTime;
	uint64								LastWriteTime;
	uint32								DesiredAccess;
	UNICODE_STRING						FCBName;
	UNICODE_STRING						FCBFullPath;
	uint32								FCBTargetOffset;
	ULONG FCBCleanup;
    ULONG FCBReference;
    ULONG FCBUserReference;

	int32 FCBCloseCount;

	uint64	RealAllocationSize;
	uint64	RealFileSize;

	// Syncronization object
	ERESOURCE				FCBResource;
	ERESOURCE				AddrResource;
	FAST_MUTEX				FCBMutex;
	PVOID					FCBLockThread;
	uint32					FCBLockCount;
	
	PVOID					LazyWriteThread;


	uint32					FCBType;



	uint32								FCBFlags;
	uint32								Type;
	uint32								HasLock;

	uint64								FileId;
	uint64								LotNumber;
	uint64								ParentLotNumber;
	uint64								AddrLotNumber;	

    LIST_ENTRY ParentLcbQueue;
    LIST_ENTRY ChildLcbQueue;
	//LIST_ENTRY DelayedCloseLink;
	LIST_ENTRY EndOfFileLink;
	LIST_ENTRY	CCBListQueue;


	PFILE_OBJECT						FileObject;

	/*
	May be used only File
	*/
	PFILE_LOCK							FCBFileLock;
	OPLOCK								FCBOplock;

	/*
	May be used only Dir
	*/
	uint32								ChildCount;
	PRTL_SPLAY_LINKS					Root;
	PRTL_SPLAY_LINKS					IgnoreCaseRoot;


	/* Write Control */
	int64		WriteStartOffset;


	/*Address info Managing*/
	uint32 		AddrStartSecIndex;
	uint64 *	AddrLot;
	uint32		AddrLotSize;
	uint32		FcbNonCachedOpenCount;

}XIFS_FCB, *PXIFS_FCB;


/*
typedef struct _XIFS_CCB_FILE_INFO_HINT{
	uint32			FileIndex;
	uint32			FileLength;
	PWCHAR			FileNameBuffer;
}XIFS_CCB_FILE_INFO_HINT, *PXIFS_CCB_FILE_INFO_HINT;
*/

/*
	CCB FLAGS
*/
#define	XIFSD_CCB_OPENED_BY_FILEID					(0x00000001)
#define	XIFSD_CCB_OPENED_FOR_SYNC_ACCESS			(0x00000002)
#define	XIFSD_CCB_OPENED_FOR_SEQ_ACCESS				(0x00000004)

#define	XIFSD_CCB_CLEANED							(0x00000008)
#define	XIFSD_CCB_ACCESSED							(0x00000010)
#define	XIFSD_CCB_MODIFIED							(0x00000020)

#define	XIFSD_CCB_ACCESS_TIME_SET					(0x00000040)
#define	XIFSD_CCB_MODIFY_TIME_SET					(0x00000080)
#define	XIFSD_CCB_CREATE_TIME_SET					(0x00000100)

#define	XIFSD_CCB_ALLOW_XTENDED_DASD_IO				(0x00000200)
#define XIFSD_CCB_DISMOUNT_ON_CLOSE					(0x00001000)
#define XIFSD_CCB_OUTOF_LINK						(0x00002000)
#define XIFSD_CCB_OPENED_RELATIVE_BY_ID				(0x00004000)
#define XIFSD_CCB_OPENED_AS_FILE					(0x00008000)

#define XIFSD_CCB_FLAG_ENUM_NAME_EXP_HAS_WILD		(0x00010000)
#define XIFSD_CCB_FLAG_ENUM_VERSION_EXP_HAS_WILD	(0x00020000)
#define XIFSD_CCB_FLAG_ENUM_MATCH_ALL				(0x00040000)
#define XIFSD_CCB_FLAG_ENUM_VERSION_MATCH_ALL		(0x00080000)
#define XIFSD_CCB_FLAG_ENUM_RETURN_NEXT				(0x00100000)
#define XIFSD_CCB_FLAG_ENUM_INITIALIZED				(0x00200000)
#define XIFSD_CCB_FLAG_ENUM_NOMATCH_CONSTANT_ENTRY	(0x00400000)
#define XIFSD_CCB_FLAG_NOFITY_SET					(0x00800000)


#define XIFSD_CCB_FLAGS_DELETE_ON_CLOSE				(0x01000000)
#define XIFSD_CCB_FLAGS_IGNORE_CASE					(0x02000000)
#define	XIFSD_CCB_NOT_FROM_POOL						(0x80000000)



#define XIFSD_CCB_TYPE_INVALID					XIFS_FD_TYPE_INVALID
#define XIFSD_CCB_TYPE_FILE						(XIFS_FD_TYPE_FILE |XIFS_FD_TYPE_SYSTEM_FILE |\
														XIFS_FD_TYPE_PIPE_FILE| XIFS_FD_TYPE_DEVICE_FILE)
#define XIFSD_CCB_TYPE_DIRECTORY				(XIFS_FD_TYPE_DIRECTORY | XIFS_FD_TYPE_ROOT_DIRECTORY)
#define XIFSD_CCB_TYPE_PAGEFILE					XIFS_FD_TYPE_PAGE_FILE
#define	XIFSD_CCB_TYPE_VOLUME					XIFS_FD_TYPE_VOLUME
#define XIFSD_CCB_TYPE_MASK						(0x000008FF)




typedef struct _XIFS_CCB {
	XIFS_NODE_ID				;
	PXIFS_FCB					PtrFCB;
	PXIFS_LCB					PtrLCB;
	uint64						currentFileIndex;
	uint64						highestFileIndex;
	UNICODE_STRING				SearchExpression;
	uint32						TypeOfOpen;
	uint32						CCBFlags;
	uint64						CurrentByteOffset;

	// Test
	LIST_ENTRY					LinkToFCB;
	PFILE_OBJECT				FileObject;
	/*
	UNICODE_STRING				FullPath;
	uint32						TargetNameOffset;
	*/
}XIFS_CCB, *PXIFS_CCB;




typedef struct _XIFS_AUXI_LOT_LOCK_INFO{
	LIST_ENTRY	AuxLink;
	PXIFS_VCB	pVCB;
	// Changed by ILGU HONG
	//	chesung suggest
	//uint8		DiskId[6];
	//uint8		HostMac[6];
	uint8		DiskId[16];
	uint8		HostMac[32];
	uint8		HostId[16];
	int32		RefCount;
	uint32		PartitionId;
	uint64		LotNumber;
	uint32		HasLock;
}XIFS_AUXI_LOT_LOCK_INFO, *PXIFS_AUXI_LOT_LOCK_INFO;



#define	XIFSD_IRP_CONTEXT_WAIT					(0x00000001)
#define	XIFSD_IRP_CONTEXT_WRITE_THROUGH			(0x00000002)
#define	XIFSD_IRP_CONTEXT_EXCEPTION				(0x00000004)
#define	XIFSD_IRP_CONTEXT_DEFERRED_WRITE		(0x00000008)
#define	XIFSD_IRP_CONTEXT_ASYNC_PROCESSING		(0x00000010)
#define	XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL			(0x00000020)
#define XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT		(0x00000040)
#define XIFSD_IRP_CONTEXT_RECURSIVE_CALL		(0x00000100)
#define	XIFSD_IRP_CONTEXT_NOT_FROM_POOL			(0x80000000)






typedef struct _XIFS_IO_CONTEXT {

    //
    //  These two fields are used for multiple run Io
    //
	struct _XIFS_IRPCONTEXT * pIrpContext;
	PIRP pIrp;
    LONG IrpCount;
    PIRP MasterIrp;
    UCHAR IrpSpFlags;
    NTSTATUS Status;
    BOOLEAN PagingIo;

    union {

        //
        //  This element handles the asynchronous non-cached Io
        //

        struct {

            PERESOURCE Resource;
            ERESOURCE_THREAD ResourceThreadId;
            ULONG RequestedByteCount;

        } Async;

        //
        //  and this element handles the synchronous non-cached Io.
        //

        KEVENT SyncEvent;

    };

} XIFS_IO_CONTEXT, *PXIFS_IO_CONTEXT;



#define LOCK_INVALID				(0x00000000)
#define LOCK_OWNED_BY_OWNER			(0x00000001)
#define LOCK_NOT_OWNED				(0x00000002)
#define LOCK_TIMEOUT				(0x00000004)



typedef struct _XIFS_LOCK_CONTROL{
	KEVENT KEvent;
	int32	Status;
}XIFS_LOCK_CONTROL, *PXIFS_LOCK_CONTROL;


typedef struct _XIFS_IRPCONTEXT{
	XIFS_NODE_ID					;
	uint32							IrpContextFlags;
	uint8							MajorFunction;
	uint8							MinorFunction;
	WORK_QUEUE_ITEM					WorkQueueItem;
	PIRP							Irp;
	PDEVICE_OBJECT					TargetDeviceObject;
	NTSTATUS						SavedExceptionCode;
	PXIFS_VCB						VCB;
	PXIFS_IO_CONTEXT				IoContext;
	PXIFS_FCB						*TeardownFcb;
}XIFS_IRPCONTEXT, *PXIFS_IRPCONTEXT;




#define	XIFSD_CTX_FLAGS_NOT_FROM_POOL				(0x80000000)

typedef struct _XIFS_CLOSE_FCB_CTX{
	XIFS_NODE_ID					;
	uint32							CtxFlags;
	LIST_ENTRY						DelayedCloseLink;
	PXIFS_FCB						TargetFCB;
}XIFS_CLOSE_FCB_CTX, *PXIFS_CLOSE_FCB_CTX;



typedef
NTSTATUS
(*XIFS_DELAYEDPROCESSING)(
	IN PVOID			Context
    );



typedef struct _XIFS_COMPLETE_CONTEXT{
	PXIFS_IRPCONTEXT	pIrpContext;
	KEVENT				Event;
}XIFS_COMPLETE_CONTEXT, *PXIFS_COMPLETE_CONTEXT;



#define SELF_ENTRY		0
#define PARENT_ENTRY	1
extern UNICODE_STRING UdfUnicodeDirectoryNames[];


typedef struct _XIFS_DIR_EMUL_CONTEXT{
	uint8	*AddrMap;
	uint32	AddrMapSize;
	uint32	AddrSecNumber;

	uint8	*ChildBitMap;
	uint8	*ChildEntry;
	//		Added by ILGU HONG 12082006
	uint8	*ChildHashTable;
	//		Added by ILGU HONG 12082006 END

	uint64			LotNumber; // Dir Fcb lot number
	uint32			FileType;
	UNICODE_STRING	ChildName;

	PXIFS_FCB		pFCB;
	PXIFS_VCB		pVCB;
	uint64			VirtualDirIndex;
	int64			RealDirIndex;
	
	uint64			SearchedVirtualDirIndex;
	uint64			SearchedRealDirIndex;
	uint64			NextSearchedIndex;

}XIFS_DIR_EMUL_CONTEXT, *PXIFS_DIR_EMUL_CONTEXT;



typedef struct _XIFS_FILE_EMUL_CONTEXT{
	PXIFS_VCB	pVCB;
	PXIFS_FCB	pSearchedFCB;
	uint64		LotNumber;
	uint32		FileType;
	uint8 *		Buffer;
}XIFS_FILE_EMUL_CONTEXT, *PXIFS_FILE_EMUL_CONTEXT;



typedef struct _XIFS_RECORD_EMUL_CONTEXT{
	PXIFS_VCB	VCB;
	uint32	RecordIndex;
	int32	RecordSearchIndex;
	uint8	HostSignature[16];	
	uint8 *	RecordInfo;
	uint8 * RecordEntry;
}XIFS_RECORD_EMUL_CONTEXT, *PXIFS_RECORD_EMUL_CONTEXT;


typedef struct _XIFS_BITMAP_EMUL_CONTEXT{
	PXIFS_VCB VCB;
	uint32  BitMapBytes;
	uint64	UsedBitmapIndex;
	uint64	UnusedBitmapIndex;
	uint64	CheckOutBitmapIndex;
	uint8	*BitmapLotHeader;
	uint8	*UsedBitmap;
	uint8	*UnusedBitmap;
	uint8	*CheckOutBitmap;
}XIFS_BITMAP_EMUL_CONTEXT, *PXIFS_BITMAP_EMUL_CONTEXT;

#ifdef XIXFS_DEBUG

#define ASSERT_STRUCT(S,T)				ASSERT(XifsSafeNodeType(S) == (T))
#define ASSERT_OPTIONAL_STRUCT(S,T)		ASSERT( ((S) == NULL) || (XifsSafeNodeType(S) == (T)) )

#define ASSERT_VCB(F)					ASSERT_STRUCT((F), XIFS_NODE_VCB)
#define ASSERT_OPTIONAL_VCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_VCB)

#define ASSERT_FCB(F)					ASSERT_STRUCT((F), XIFS_NODE_FCB)
#define ASSERT_OPTIONAL_FCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_FCB)

#define ASSERT_CCB(F)					ASSERT_STRUCT((F), XIFS_NODE_CCB)
#define ASSERT_OPTIONAL_CCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_CCB)

#define ASSERT_IRPCONTEXT(F)			ASSERT_STRUCT((F), XIFS_NODE_IRPCONTEXT)
#define ASSERT_OPTIONAL_IRPCONTEXT(F)	ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_IRPCONTEXT)

#define ASSERT_LCB(F)					ASSERT_STRUCT((F), XIFS_NODE_LCB)
#define ASSERT_OPTIONAL_LCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_LCB)


#define ASSERT_EXCLUSIVE_RESOURCE(R)	ASSERT( ExIsResourceAcquiredExclusiveLite(R))	
#define ASSERT_SHARED_RESOURCE(R)		ASSERT( ExIsResourceAcquiredSharedLite(R))

#define ASSERT_EXCLUSIVE_XIFS_GDATA		ASSERT_EXCLUSIVE_RESOURCE( &XiGlobalData.DataResource)	
#define ASSERT_EXCLUSIVE_VCB(V)			ASSERT_EXCLUSIVE_RESOURCE( &(V)->VCBResource)
#define ASSERT_SHARED_VCB(V)			ASSERT_SHARED_RESOURCE ( &(V)->VCBResource)
#define ASSERT_EXCLUSIVE_FCB(F)			ASSERT_EXCLUSIVE_RESOURCE( &(F)->FCBResource)
#define ASSERT_SHARED_FCB(F)			ASSERT_SHARED_RESOURCE ( &(F)->FCBResource)
#define ASSERT_EXCLUSIVE_FILE(F)		ASSERT_EXCLUSIVE_RESOURCE( (F)->Resource)
#define ASSERT_SHARED_FILE(F)			ASSERT_SHARED_RESOURCE ( (F)->Resource)

#define ASSERT_EXCLUSIVE_FCB_OR_VCB(F)	ASSERT( ExIsResourceAcquiredExclusiveLite( &(F)->FCBResource ) || \
												ExIsResourceAcquiredExclusiveLite( &(F)->PtrVCB->VCBResource ))


#define ASSERT_LOCKED_VCB(V)			ASSERT( (V)->VCBLockThread == PsGetCurrentThread())
#define ASSERT_UNLOCKED_VCB(V)			ASSERT( (V)->VCBLockThread != PsGetCurrentThread())

#define ASSERT_LOCKED_FCB(V)			ASSERT( (V)->FCBLockThread == PsGetCurrentThread())
#define ASSERT_UNLOCKED_FCB(V)			ASSERT( (V)->FCBLockThread != PsGetCurrentThread())

#else

#define ASSERT_STRUCT(S,T)				{NOTHING;}
#define ASSERT_OPTIONAL_STRUCT(S,T)		{NOTHING;}

#define ASSERT_VCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_VCB(F)			{NOTHING;}

#define ASSERT_FCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_FCB(F)			{NOTHING;}

#define ASSERT_CCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_CCB(F)			{NOTHING;}

#define ASSERT_IRPCONTEXT(F)			{NOTHING;}
#define ASSERT_OPTIONAL_IRPCONTEXT(F)	{NOTHING;}

#define ASSERT_LCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_LCB(F)			{NOTHING;}


#define ASSERT_EXCLUSIVE_RESOURCE(R)	{NOTHING;}
#define ASSERT_SHARED_RESOURCE(R)		{NOTHING;}

#define ASSERT_EXCLUSIVE_XIFS_GDATA		{NOTHING;}
#define ASSERT_EXCLUSIVE_VCB(V)			{NOTHING;}
#define ASSERT_SHARED_VCB(V)			{NOTHING;}
#define ASSERT_EXCLUSIVE_FCB(F)			{NOTHING;}
#define ASSERT_SHARED_FCB(F)			{NOTHING;}

#define ASSERT_EXCLUSIVE_FCB_OR_VCB(F)	{NOTHING;}

#define ASSERT_LOCKED_VCB(V)			{NOTHING;}
#define ASSERT_UNLOCKED_VCB(V)			{NOTHING;}

#define ASSERT_LOCKED_FCB(V)			{NOTHING;}
#define ASSERT_UNLOCKED_FCB(V)			{NOTHING;}

#endif


#define GenericTruncate(B, U) (					\
			(B) & ~((U) - 1)					\
)

#define GenericAlign(B, U) (					\
	GenericTruncate((B) + (U) - 1, U)			\
)

#define XifsdQuadAlign(B) GenericAlign((B), 8)



#ifndef Min
#define Min(a, b)   ((a) < (b) ? (a) : (b))
#endif

#ifndef Max
#define Max(a, b)   ((a) > (b) ? (a) : (b))
#endif


typedef struct _FCB_TABLE_ENTRY{
	uint64 FileId;
	PXIFS_FCB pFCB;
}FCB_TABLE_ENTRY, *PFCB_TABLE_ENTRY;


#endif // #ifndef __XIXFS_DRV_H__