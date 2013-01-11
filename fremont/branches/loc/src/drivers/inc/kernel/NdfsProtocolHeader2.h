#ifndef __NDAS_FS_PROTOCOL_HEADER_H__
#define __NDAS_FS_PROTOCOL_HEADER_H__


// Little Endian


#include <pshpack1.h>

#define NTFS_MAX_FILE_NAME_LENGTH       (255)

#define	DEFAULT_PRIMARY_PORT			((USHORT)0x0001)
#define NDFS_MAX_PATH					((NTFS_MAX_FILE_NAME_LENGTH+1)*sizeof(WCHAR))		
#define DEFAULT_MAX_DATA_SIZE			(128*1024) // header is not included
#define DEFAULT_MIN_DATA_SIZE			(4*1024) // header is not included
#define DEFAULT_NDAS_MAX_DATA_SIZE		(1024*1024) // header is not included

#define MAX_SLOT_PER_SESSION	16

#define NDFS_PROTOCOL	"NDFS"	

#define NDFS_PROTOCOL_MAJOR_3	0x0003
#define NDFS_PROTOCOL_MINOR_0	0x0000

// OS types

#define OS_TYPE_WINDOWS			0x0001

#define		OS_TYPE_WIN98SE		0x0001
#define		OS_TYPE_WIN2K		0x0002
#define		OS_TYPE_WINXP		0x0003
#define		OS_TYPE_WIN2003SERV	0x0004

#define	OS_TYPE_LINUX			0x0002

#define		OS_TYPE_LINUX22		0x0001
#define		OS_TYPE_LINUX24		0x0002

#define	OS_TYPE_MAC				0x0003

#define		OS_TYPE_MACOS9		0x0001
#define		OS_TYPE_MACOSX		0x0002


#define NDFS_COMMAND_NEGOTIATE			0x01
#define NDFS_COMMAND_SETUP				0x02
#define NDFS_COMMAND_LOGOFF				0x03
#define NDFS_COMMAND_TREE_CONNECT		0x04
#define NDFS_COMMAND_TREE_DISCONNECT	0x05
#define NDFS_COMMAND_EXECUTE			0x06
#define NDFS_COMMAND_DATA_READ			0x07
#define NDFS_COMMAND_DATA_WRITE			0x08
#define NDFS_COMMAND_NOPS				0x09

#define NDSC_ID_LENGTH	16

typedef UNALIGNED struct _NDFS_REQUEST_HEADER {

	UINT8		Protocol[4];

	UINT8		Command;	
	
	union {

		UINT8		Flags;

		struct {

			UINT8 MessageSecurity :1;
			UINT8 RwDataSecurity  :1;
			UINT8 Splitted		  :1;
			UINT8 Reserved		  :5;
		};
	};

	UINT16	Uid2;		// Set by Primary When returning NDFS_REPLY_SETUP
	UINT16	Tid2;
	UINT16	Mid2;

	UINT32	MessageSize4; // Total Message Size (including Header Size)

} NDFS_REQUEST_HEADER, *PNDFS_REQUEST_HEADER;

C_ASSERT( sizeof( NDFS_REQUEST_HEADER) == 16 );

typedef UNALIGNED struct _NDFS_REPLY_HEADER {

	UINT8	Protocol[4];

#define NDFS_SUCCESS			0x00
#define NDFS_NOPS				0x01
#define NDFS_UNSUCCESSFUL		0x80

	UINT8	Status;
	
	union {

		UINT8	Flags;
		
		struct {

			UINT8 MessageSecurity	:1;
			UINT8 RwDataSecurity	:1;
			UINT8 Splitted			:1; // MessageSize is MaxBufferSize
			UINT8 Reserved			:5;
		};
	};

	UINT16	Uid2;
	UINT16	Tid2;
	UINT16	Mid2;

	UINT32	MessageSize4; // Total Message Size (including Header Size)

} NDFS_REPLY_HEADER, *PNDFS_REPLY_HEADER;

C_ASSERT( sizeof( NDFS_REQUEST_HEADER) == 16 );

typedef UNALIGNED struct _NDFS_REQUEST_NEGOTIATE {

	UINT16	NdfsMajorVersion2;
	UINT16	NdfsMinorVersion2;
	UINT16	OsMajorType2;
	UINT16	OsMinorType2;

	union {

		UINT8		Flags;

		struct {

			UINT8 DataSecurity:1;
			UINT8 MinorVersionPlusOne:1;
			UINT8 Reserved:6;
		};
	};
	
} NDFS_REQUEST_NEGOTIATE, *PNDFS_REQUEST_NEGOTIATE;


#define MAX_CHALLENGE_LEN 256

typedef UNALIGNED struct _NDFS_REPLY_NEGOTIATE {

#define NDFS_NEGOTIATE_SUCCESS			0x00
#define NDFS_NEGOTIATE_UNSUCCESSFUL		0x80

	UINT8	Status;

	UINT16	NdfsMajorVersion2;
	UINT16	NdfsMinorVersion2;
	UINT16	OsMajorType2;
	UINT16	OsMinorType2;

	UINT32	MaxBufferSize4;

	UINT32	SessionKey4;
	UINT16	ChallengeLength2;
	UINT8	ChallengeBuffer[MAX_CHALLENGE_LEN];

} NDFS_REPLY_NEGOTIATE, *PNDFS_REPLY_NEGOTIATE;


typedef UNALIGNED struct _NDFS_REQUEST_SETUP {

	UINT32	SessionKey4;

	UINT32	MaxBufferSize4;
	
	UINT8	NetdiskNode[6];
	UINT16	NetdiskPort2;
	UINT16	UnitDiskNo2;
	UINT8	NdscId[NDSC_ID_LENGTH];
	UINT64	StartingOffset8;
	
	UINT16	ResponseLength2;
	UINT8	ResponseBuffer[MAX_CHALLENGE_LEN]; 

} NDFS_REQUEST_SETUP, *PNDFS_REQUEST_SETUP;


typedef UNALIGNED struct _NDFS_REPLY_SETUP {

#define NDFS_SETUP_SUCCESS			0x00
#define NDFS_SETUP_UNSUCCESSFUL		0x80

	UINT8		Status;
	
} NDFS_REPLY_SETUP, *PNDFS_REPLY_SETUP;


typedef UNALIGNED struct _NDFS_REQUEST_LOGOFF {

	UINT32	SessionKey4;
	
} NDFS_REQUEST_LOGOFF, *PNDFS_REQUEST_LOGOFF;


typedef UNALIGNED struct _NDFS_REPLY_LOGOFF {

#define NDFS_LOGOFF_SUCCESS			0x00
#define NDFS_LOGOFF_UNSUCCESSFUL	0x80

	UINT8		Status;

} NDFS_REPLY_LOGOFF, *PNDFS_REPLY_LOGOFF;


typedef UNALIGNED struct _NDFS_REQUEST_TREE_CONNECT {

	UINT64	StartingOffset8;
	UINT16	ResponseLength2;
	UINT8	ResponseBuffer[MAX_CHALLENGE_LEN]; 
	
} NDFS_REQUEST_TREE_CONNECT, *PNDFS_REQUEST_TREE_CONNECT;


typedef UNALIGNED struct _NDFS_REPLY_TREE_CONNECT {

#define NDFS_TREE_CONNECT_SUCCESS			0x00
#define NDFS_TREE_CORRUPTED					0x01
#define NDFS_TREE_CONNECT_NO_PARTITION		0x02
#define NDFS_TREE_CONNECT_UNSUCCESSFUL		0x80

	UINT8	Status;

	UINT8	SessionSlotCount;

	UINT32	BytesPerFileRecordSegment4;
	UINT32	BytesPerSector4;
	UINT32	BytesPerCluster4;
	
} NDFS_REPLY_TREE_CONNECT, *PNDFS_REPLY_TREE_CONNECT;


typedef UNALIGNED struct _NDFS_REPLY_TREE_DISCONNECT {

	UINT8		Status;

} NDFS_REPLY_TREE_DISCONNECT, *PNDFS_REPLY_TREE_DISCONNECT;


typedef UNALIGNED struct _NDFS_REQUEST_DATA_READ {

	UINT32	Offset;
	UINT8	DataSize;

} NDFS_REQUEST_DATA_READ, *PNDFS_REQUEST_DATA_READ;

#if 0

typedef UNALIGNED struct _NDFS_REPLY_DATA_READ {

} NDFS_REPLY_DATA_READ, *PNDFS_REPLY_DATA_READ;

#endif


typedef UNALIGNED struct _NDFS_REQUEST_DATA_WRITE {

	UINT32	Offset;
	UINT8	DataSize;

} NDFS_REQUEST_DATA_WRITE, *PNDFS_REQUEST_DATA_WRITE;

#if 0

typedef UNALIGNED struct _NDFS_REPLY_DATA_WRITE {

} NDFS_REPLY_DATA_WRITE, *PNDFS_REPLY_DATA_WRITE;

#endif


typedef UNALIGNED struct _WINXP_REQUEST_CREATE {

	UINT32	Options;
    UINT16	FileAttributes;
    UINT16	ShareAccess;
    UINT32	EaLength;

	UINT64	RelatedFileHandle;
	UINT16	FileNameLength;

	UINT64	AllocationSize;

	struct {

	    UINT32	DesiredAccess;
		UINT32	FullCreateOptions;

		struct {

			UINT32	Flags;
			UINT32	RemainingDesiredAccess;
			UINT32	PreviouslyGrantedAccess;
			UINT32	OriginalDesiredAccess;

		} AccessState;
	
	} SecurityContext;

} WINXP_REQUEST_CREATE, *PWINXP_REQUEST_CREATE;


typedef UNALIGNED struct _WINXP_REQUEST_DEVICE_IOCONTROL {

	UINT32	OutputBufferLength;
    UINT32	InputBufferLength;
    UINT32	IoControlCode;

} WINXP_REQUEST_DEVICE_IOCONTROL, *PWINXP_REQUEST_DEVICE_IOCONTROL;


typedef UNALIGNED struct _WINXP_FSC_MOVE_FILE_DATA {

    UINT64	FileHandle;
    UINT64	StartingVcn;
    UINT64	StartingLcn;
    UINT32	ClusterCount;

} WINXP_FSC_MOVE_FILE_DATA, *PWINXP_FSC_MOVE_FILE_DATA;


typedef UNALIGNED struct _WINXP_FSC_MOVE_FILE_DATA32 {

    UINT64	FileHandle;
    UINT64	StartingVcn;
    UINT64	StartingLcn;
    UINT32	ClusterCount;

} WINXP_FSC_MOVE_FILE_DATA32, *PWINXP_FSC_MOVE_FILE_DATA32;


typedef UNALIGNED struct _WINXP_FSC_MARK_HANDLE_INFO {

    UINT32	UsnSourceInfo;
    UINT64	VolumeHandle;
    UINT32	HandleInfo;

} WINXP_FSC_MARK_HANDLE_INFO, *PWINXP_FSC_MARK_HANDLE_INFO;


typedef UNALIGNED struct _WINXP_FSC_MARK_HANDLE_INFO32 {

    UINT32	UsnSourceInfo;
    UINT64	VolumeHandle;
    UINT32	HandleInfo;

} WINXP_FSC_MARK_HANDLE_INFO32, *PWINXP_FSC_MARK_HANDLE_INFO32;


typedef UNALIGNED struct _WINXP_REQUEST_FILE_SYSTEM_CONTROL {

	UINT32	OutputBufferLength;
    UINT32	InputBufferLength;
    UINT32	FsControlCode;

	union {

		WINXP_FSC_MOVE_FILE_DATA		FscMoveFileData;		// FSCTL_MOVE_FILE
		WINXP_FSC_MOVE_FILE_DATA32		FscMoveFileData32;
		WINXP_FSC_MARK_HANDLE_INFO		FscMarkHandleInfo;		// FSCTL_MARK_HANDLE
		WINXP_FSC_MARK_HANDLE_INFO32	FscMarkHandleInfo32;
	};

} WINXP_REQUEST_FILE_SYSTEM_CONTROL, *PWINXP_REQUEST_FILE_SYSTEM_CONTROL;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_DIRECTORY {

	UINT32	Length;
    UINT32	FileInformationClass;
    UINT32	FileIndex;

} WINXP_REQUEST_QUERY_DIRECTORY, *PWINXP_REQUEST_QUERY_DIRECTORY;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_FILE {

	UINT32	Length;
    UINT32	FileInformationClass;

} WINXP_REQUEST_QUERY_FILE, *PWINXP_REQUEST_QUERY_FILE;


typedef UNALIGNED struct _WINXP_FILE_LINK_INFORMATION {

    UINT8	ReplaceIfExists;
    UINT64	RootDirectoryHandle;
    UINT32	FileNameLength;

} WINXP_FILE_LINK_INFORMATION, *PWINXP_FILE_LINK_INFORMATION;


typedef UNALIGNED struct _WINXP_FILE_RENAME_INFORMATION {

    UINT8	ReplaceIfExists;
    UINT64	RootDirectoryHandle;
    UINT32	FileNameLength;

} WINXP_FILE_RENAME_INFORMATION, *PWINXP_FILE_RENAME_INFORMATION;


typedef UNALIGNED struct _WINXP_FILE_BASIC_INFORMATION {

	UINT64	CreationTime;                            
    UINT64	LastAccessTime;                         
    UINT64	LastWriteTime;                      
    UINT64	ChangeTime;                          
    UINT32	FileAttributes;
	
} WINXP_FILE_BASIC_INFORMATION, *PWINXP_FILE_BASIC_INFORMATION;
 
  
typedef UNALIGNED struct _WINXP_FILE_DISPOSITION_INFORMATION {

    UINT8		DeleteFile;                                         

} WINXP_FILE_DISPOSITION_INFORMATION, *PWINXP_FILE_DISPOSITION_INFORMATION; 


typedef UNALIGNED struct _WINXP_FILE_END_OF_FILE_INFORMATION {

    UINT64	EndOfFile;                                         

} WINXP_FILE_END_OF_FILE_INFORMATION, *PWINXP_FILE_END_OF_FILE_INFORMATION; 

typedef UNALIGNED struct _WINXP_FILE_ALLOCATION_INFORMATION {

    UINT64	AllocationSize;
	INT64	Lcn;

} WINXP_FILE_ALLOCATION_INFORMATION, *PWINXP_FILE_ALLOCATION_INFORMATION; 

typedef struct _WINXP_FILE_POSITION_INFORMATION {

    UINT64	CurrentByteOffset;                                         

} WINXP_FILE_POSITION_INFORMATION, *PWINXP_FILE_POSITION_INFORMATION; 


typedef UNALIGNED struct _WINXP_REQUEST_SET_FILE {

	UINT32	Length;
    UINT32	FileInformationClass;
    UINT64	FileHandle;

    union {

		struct {

			UINT8 ReplaceIfExists;
            UINT8 AdvanceOnly;
		};

        UINT32 ClusterCount;
        UINT64 DeleteHandle;
	};

	union {

		WINXP_FILE_LINK_INFORMATION			LinkInformation;
		WINXP_FILE_RENAME_INFORMATION		RenameInformation;
		WINXP_FILE_BASIC_INFORMATION		BasicInformation;
		WINXP_FILE_DISPOSITION_INFORMATION	DispositionInformation;
		WINXP_FILE_END_OF_FILE_INFORMATION	EndOfFileInformation;
		WINXP_FILE_ALLOCATION_INFORMATION	AllocationInformation;
		WINXP_FILE_POSITION_INFORMATION		PositionInformation;
	};

} WINXP_REQUEST_SET_FILE, *PWINXP_REQUEST_SET_FILE;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_VOLUME {

	UINT32	Length;
    UINT32	FsInformationClass;

} WINXP_REQUEST_QUERY_VOLUME, *PWINXP_REQUEST_QUERY_VOLUME;


typedef struct _WINXP_REQUEST_SET_VOLUME {

	UINT32	Length;
    UINT32	FsInformationClass;

} WINXP_REQUEST_SET_VOLUME, *PWINXP_REQUEST_SET_VOLUME;


typedef UNALIGNED struct _WINXP_REQUEST_READ {

	UINT32	Length;
	UINT32	Key;
    UINT64	ByteOffset;

} WINXP_REQUEST_READ, *PWINXP_REQUEST_READ;


typedef UNALIGNED struct _WINXP_REQUEST_WRITE {

	UINT32	Length;
	UINT32	Key;
    UINT64	ByteOffset;
	
	struct {

		UINT8 ForceWrite :1;
		UINT8 Fake		 :1;
		UINT8 Reserved	 :6;
	};

	UINT64	FileSize;
	UINT64	ValidDataLength;
	UINT64  VaildDataToDisk;
	
} WINXP_REQUEST_WRITE, *PWINXP_REQUEST_WRITE;


typedef UNALIGNED struct _WINXP_REQUEST_CLEANUP {

	UINT64	FileSize;
	UINT64	AllocationSize;
	UINT64	ValidDataLength;
	UINT64    VaildDataToDisk;

} WINXP_REQUEST_CLEANUP, *PWINXP_REQUEST_CLEANUP;


typedef UNALIGNED struct _WINXP_REQUEST_LOCK_CONTROL {

	UINT64	Length;
	UINT32	Key;
    UINT64	ByteOffset;

} WINXP_REQUEST_LOCK_CONTROL, *PWINXP_REQUEST_LOCK_CONTROL;


typedef struct _WINXP_REQUEST_QUERY_SECURITY {

	UINT32	SecurityInformation;
	UINT32	Length;

} WINXP_REQUEST_QUERY_SECURITY, *PWINXP_REQUEST_QUERY_SECURITY;


typedef struct _WINXP_REQUEST_SET_SECURITY {

	UINT32	SecurityInformation;
	UINT32	Length;

} WINXP_REQUEST_SET_SECURITY, *PWINXP_REQUEST_SET_SECURITY;

typedef struct _WINXP_REQUEST_QUERY_EA {

	UINT32	Length;
	UINT32	EaListLength;
	UINT32	EaIndex;

} WINXP_REQUEST_QUERY_EA, * PWINXP_REQUEST_QUERY_EA;


typedef struct _WINXP_REQUEST_SET_EA {

	UINT32	Length;

} WINXP_REQUEST_SET_EA, * PWINXP_REQUEST_SET_EA;

typedef struct _WINXP_REQUEST_QUERY_QUOTA {

	UINT32	Length;
	UINT32	InputLength;
	UINT64	StartSidOffset;

} WINXP_REQUEST_QUERY_QUOTA, *PWINXP_REQUEST_QUERY_QUOTA;


typedef struct _WINXP_REQUEST_SET_QUOTA {

	UINT32	Length;

} WINXP_REQUEST_SET_QUOTA, *PWINXP_REQUEST_SET_QUOTA;


typedef UNALIGNED struct _NDFS_WINXP_REQUEST_HEADER {

	UINT32	IrpTag4;			// Irp Address etc

	UINT8	IrpMajorFunction;
	UINT8	IrpMinorFunction;

	UINT64	FileHandle8;

	UINT32	IrpFlags4;
	UINT8	IrpSpFlags;

	union {

		WINXP_REQUEST_CREATE				Create;
		WINXP_REQUEST_DEVICE_IOCONTROL		DeviceIoControl;
		WINXP_REQUEST_FILE_SYSTEM_CONTROL	FileSystemControl;
		WINXP_REQUEST_QUERY_DIRECTORY		QueryDirectory;
		WINXP_REQUEST_QUERY_FILE			QueryFile;
		WINXP_REQUEST_SET_FILE				SetFile;
		WINXP_REQUEST_QUERY_VOLUME			QueryVolume;
		WINXP_REQUEST_SET_VOLUME			SetVolume;
		WINXP_REQUEST_READ					Read;
		WINXP_REQUEST_WRITE					Write;
		WINXP_REQUEST_CLEANUP				CleanUp;
		WINXP_REQUEST_LOCK_CONTROL			LockControl;
		WINXP_REQUEST_QUERY_SECURITY		QuerySecurity;
		WINXP_REQUEST_SET_SECURITY			SetSecurity;
		WINXP_REQUEST_QUERY_EA				QueryEa;
		WINXP_REQUEST_SET_EA				SetEa;
		WINXP_REQUEST_QUERY_QUOTA			QueryQuota;
		WINXP_REQUEST_SET_QUOTA				SetQuota;
	};

	UINT8		Reserved[1];
	
} NDFS_WINXP_REQUEST_HEADER, *PNDFS_WINXP_REQUEST_HEADER;


typedef UNALIGNED struct _WINXP_DUPLICATED_INFORMATION {

	INT64	CreationTime;
	INT64	LastModificationTime;
	INT64	LastChangeTime;
    INT64	LastAccessTime;
	INT64	AllocatedLength;
	INT64	FileSize;
	UINT32	FileAttributes;
	
    union {

        struct {

            UINT16	PackedEaSize;
            UINT16	Reserved;
        };

        UINT32	ReparsePointTag;
	};

} WINXP_DUPLICATED_INFORMATION, *PWINXP_DUPLICATED_INFORMATION;


typedef UNALIGNED struct _NDFS_FAT_MCB_ENTRY {

	UINT32 Vcn;
	UINT32 Lcn;
	UINT32 ClusterCount;	

} NDFS_FAT_MCB_ENTRY, *PNDFS_FAT_MCB_ENTRY;


typedef UNALIGNED struct _NDFS_NTFS_MCB_ENTRY {

	INT64 Vcn;
	INT64 Lcn;
	INT64 ClusterCount;	

} NDFS_NTFS_MCB_ENTRY, *PNDFS_NTFS_MCB_ENTRY;


typedef UNALIGNED struct _WINXP_REPLY_CREATE {

	UINT64	FileHandle;

	union {
	
		INT64	CreationTime;

		struct {

			UINT32	SegmentNumberLowPart;       //  offset = 0x000
			UINT16	SegmentNumberHighPart;      //  offset = 0x004
			UINT16	SequenceNumber;             //  offset = 0x006

		} FileReference;						//  sizeof = 0x008

		struct {
		
			UINT32	OpenTimeHigPart;
			UINT8		OpenTimeLowPartMsb;
		};
	};

	union {

		UINT64	ScbHandle;
		UINT64	FcbHandle;
	};

	UINT32	AttributeTypeCode;
	UINT8		TypeOfOpen;
	
	UINT8		Reserved1:4;
	UINT8		TruncateOnClose:1;
	UINT8		SetSectionObjectPointer:1;
	UINT8		Reserved2:2;

	UINT32	CcbFlags;


	struct {

	    UINT32	LcbState;
	    UINT64	Scb;

	} Lcb;

	UINT8		Reserved[2];

} WINXP_REPLY_CREATE, *PWINXP_REPLY_CREATE;


typedef UNALIGNED struct _NDFS_WINXP_REPLY_HEADER {

	UINT32	IrpTag4;		// Irp Address etc

	UINT8	IrpMajorFunction;
	UINT8	IrpMinorFunction;

	UINT32	Status4;

	union {
		
		UINT64	Information64;
		UINT32	Information32;
	};

	UINT8	FileInformationSet;

	INT64	FileSize8;
	INT64	AllocationSize8;
	INT64	ValidDataLength8;
	UINT32	NumberOfMcbEntry4;

	UINT8	Reserved[1];

	union {
	
		WINXP_REPLY_CREATE	Open;
		UINT64				CurrentByteOffset8;
	};

	WINXP_DUPLICATED_INFORMATION	DuplicatedInformation;

} NDFS_WINXP_REPLY_HEADER, *PNDFS_WINXP_REPLY_HEADER;


#include <poppack.h>


#endif // __NDAS_FS_PROTOCOL_HEADER_H__
