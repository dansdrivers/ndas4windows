#ifndef __NDFS_PROTOCOL_HEADER_H__
#define __NDFS_PROTOCOL_HEADER_H__


#define _U8		UCHAR
#define _U16	USHORT
#define _U32	ULONG
#define _U64	ULONGLONG

#define _S8		CHAR
#define _S16	SHORT
#define _S32	LONG
#define _S64	LONGLONG

// Little Endian


#include <pshpack1.h>

#define	DEFAULT_PRIMARY_PORT	((USHORT)0x0001)
#define NDFS_MAX_PATH			(1024 * sizeof(WCHAR))
#define DEFAULT_MAX_DATA_SIZE	(128*1024) // header is not included
#define DEFAULT_MIN_DATA_SIZE	(4*1024) // header is not included

//
//	Traffic control byte unit
//

#define DEFAULT_DATA_SIZE_UINT	(4*1024)

#define MAX_REQUEST_PER_SESSION	16

#define NDFS_PROTOCOL	"NDFS"	

#define NDFS_PROTOCOL_MAJOR_1	0x0001
#define NDFS_PROTOCOL_MINOR_0	0x0000
#define NDFS_PROTOCOL_MINOR_1	0x0001

// OS types

#define OS_TYPE_WINDOWS	0x0001
#define		OS_TYPE_WIN98SE		0x0001
#define		OS_TYPE_WIN2K		0x0002
#define		OS_TYPE_WINXP		0x0003
#define		OS_TYPE_WIN2003SERV	0x0004

#define	OS_TYPE_LINUX	0x0002
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


typedef UNALIGNED struct _NDFS_REQUEST_HEADER
{
	_U8		Protocol[4];
	_U8		Command;	
	
	union 
	{
	_U8		Flags;
		struct 
		{
			_U8 MessageSecurity :1;
			_U8 RwDataSecurity  :1;
			_U8	Splitted		:1;
			_U8 Reserved		:5;
		};
	};

	_U16	Uid;		// Set by Primary When returning NDFS_REPLY_SETUP
	_U16	Tid;
	_U16	Mid;
	_U32	MessageSize;// Total Message Size (including header and data body)

} NDFS_REQUEST_HEADER, *PNDFS_REQUEST_HEADER;


typedef UNALIGNED struct _NDFS_REPLY_HEADER
{
	_U8		Protocol[4];

#define NDFS_SUCCESS			0x00
#define NDFS_NOPS				0x01
#define NDFS_UNSUCCESSFUL		0x80

	_U8		Status;
	union 
	{
	_U8		Flags;
		struct 
		{
			_U8 MessageSecurity	:1;
			_U8 RwDataSecurity	:1;
			_U8	Splitted		:1; // MessageSize is MaxBufferSize
			_U8	Reserved		:5;
		};
	};
	_U16	Uid;
	_U16	Tid;
	_U16	Mid;
	_U32	MessageSize; // Total Message Size (including Header Size)

} NDFS_REPLY_HEADER, *PNDFS_REPLY_HEADER;


typedef UNALIGNED struct _NDFS_REQUEST_NEGOTIATE
{
	_U16	NdfsMajorVersion;
	_U16	NdfsMinorVersion;
	_U16	OsMajorType;
	_U16	OsMinorType;
	union 
	{
		_U8		Flags;

		struct 
		{
			_U8 DataSecurity:1;
			_U8 MinorVersionPlusOne:1;
			_U8 Reserved:6;
		};
	};
	
} NDFS_REQUEST_NEGOTIATE, *PNDFS_REQUEST_NEGOTIATE;


#define MAX_CHALLENGE_LEN 256

typedef UNALIGNED struct _NDFS_REPLY_NEGOTIATE
{
#define NDFS_NEGOTIATE_SUCCESS			0x00
#define NDFS_NEGOTIATE_UNSUCCESSFUL		0x80
	_U8		Status;
	_U16	NdfsMajorVersion;
	_U16	NdfsMinorVersion;
	_U16	OsMajorType;
	_U16	OsMinorType;
	_U32	SessionKey;
	_U32	MaxBufferSize;
	_U16	ChallengeLength;
	_U8		ChallengeBuffer[MAX_CHALLENGE_LEN];

} NDFS_REPLY_NEGOTIATE, *PNDFS_REPLY_NEGOTIATE;


typedef UNALIGNED struct _NDFS_REQUEST_SETUP
{
	_U32	SessionKey;
	_U32	MaxBufferSize;
	_U8		NetDiskNode[6];
	_U16	NetDiskPort;
	_U16	UnitDiskNo;
	_U64	StartingOffset;
	_U16	ResponseLength;
	_U8		ResponseBuffer[MAX_CHALLENGE_LEN]; 
	
} NDFS_REQUEST_SETUP, *PNDFS_REQUEST_SETUP;


typedef UNALIGNED struct _NDFS_REPLY_SETUP
{
#define NDFS_SETUP_SUCCESS			0x00
#define NDFS_SETUP_UNSUCCESSFUL		0x80
	_U8		Status;
	
} NDFS_REPLY_SETUP, *PNDFS_REPLY_SETUP;


typedef UNALIGNED struct _NDFS_REQUEST_LOGOFF
{
	_U32	SessionKey;
	
} NDFS_REQUEST_LOGOFF, *PNDFS_REQUEST_LOGOFF;


typedef UNALIGNED struct _NDFS_REPLY_LOGOFF
{
#define NDFS_LOGOFF_SUCCESS			0x00
#define NDFS_LOGOFF_UNSUCCESSFUL	0x80
	_U8		Status;

} NDFS_REPLY_LOGOFF, *PNDFS_REPLY_LOGOFF;


typedef UNALIGNED struct _NDFS_REQUEST_TREE_CONNECT
{
	_U64	StartingOffset;
	_U16	ResponseLength;
	_U8		ResponseBuffer[MAX_CHALLENGE_LEN]; 
	
} NDFS_REQUEST_TREE_CONNECT, *PNDFS_REQUEST_TREE_CONNECT;


typedef UNALIGNED struct _NDFS_REPLY_TREE_CONNECT
{
#define NDFS_TREE_CONNECT_SUCCESS			0x00
#define NDFS_TREE_CORRUPTED					0x01
#define NDFS_TREE_CONNECT_NO_PARTITION		0x02
#define NDFS_TREE_CONNECT_UNSUCCESSFUL		0x80

	_U8		Status;
	_U8		RequestsPerSession;

	_U32	BytesPerFileRecordSegment;
	_U32	BytesPerSector;
	_U32	BytesPerCluster;
	
} NDFS_REPLY_TREE_CONNECT, *PNDFS_REPLY_TREE_CONNECT;


#if 0

typedef UNALIGNED struct _NDFS_REQUEST_TREE_DISCONNECT
{
	
} NDFS_REQUEST_TREE_DISCONNECT, *PNDFS_REQUEST_TREE_DISCONNECT;


typedef UNALIGNED struct _NDFS_REQUEST_NOPS
{
	
} NDFS_REQUEST_NOPS, *PNDFS_REQUEST_NOPS;


typedef UNALIGNED struct _NDFS_REPLY_NOPS
{
	
} NDFS_REPLY_NOPS, *PNDFS_REPLY_NOPS;

#endif

typedef UNALIGNED struct _NDFS_REPLY_TREE_DISCONNECT
{
	_U8		Status;

} NDFS_REPLY_TREE_DISCONNECT, *PNDFS_REPLY_TREE_DISCONNECT;


typedef UNALIGNED struct _NDFS_REQUEST_DATA_READ
{
	_U32	Offset;
	_U8		DataSize;

} NDFS_REQUEST_DATA_READ, *PNDFS_REQUEST_DATA_READ;

#if 0

typedef UNALIGNED struct _NDFS_REPLY_DATA_READ
{
	
} NDFS_REPLY_DATA_READ, *PNDFS_REPLY_DATA_READ;

#endif


typedef UNALIGNED struct _NDFS_REQUEST_DATA_WRITE
{
	_U32	Offset;
	_U8		DataSize;

} NDFS_REQUEST_DATA_WRITE, *PNDFS_REQUEST_DATA_WRITE;

#if 0

typedef UNALIGNED struct _NDFS_REPLY_DATA_WRITE
{
	
} NDFS_REPLY_DATA_WRITE, *PNDFS_REPLY_DATA_WRITE;

#endif


typedef UNALIGNED struct _WINXP_REQUEST_CREATE
{
	_U32	DesiredAccess;
	_U32	Options;
    _U16	FileAttributes;
    _U16	ShareAccess;
    _U32	EaLength;

	_U32	RelatedFileHandle;
	_U16	FileNameLength;

	_U32	AllocationSizeLowPart;
	_U32	AllocationSizeHighPart;

	_U32	FullCreateOptions;

} WINXP_REQUEST_CREATE, *PWINXP_REQUEST_CREATE;


typedef UNALIGNED struct _WINXP_REQUEST_DEVICE_IOCONTROL
{
	_U32	OutputBufferLength;
    _U32	InputBufferLength;
    _U32	IoControlCode;

} WINXP_REQUEST_DEVICE_IOCONTROL, *PWINXP_REQUEST_DEVICE_IOCONTROL;


typedef UNALIGNED struct _WINXP_FSC_MOVE_FILE_DATA
{
    _U32	FileHandle;					// [64bit issue] should 64 bit long
    _U64	StartingVcn;
    _U64	StartingLcn;
    _U32	ClusterCount;

} WINXP_FSC_MOVE_FILE_DATA, *PWINXP_FSC_MOVE_FILE_DATA;


typedef UNALIGNED struct _WINXP_FSC_MOVE_FILE_DATA32
{
    _U32	FileHandle;					// [64bit issue] should 64 bit long
    _U64	StartingVcn;
    _U64	StartingLcn;
    _U32	ClusterCount;

} WINXP_FSC_MOVE_FILE_DATA32, *PWINXP_FSC_MOVE_FILE_DATA32;


typedef UNALIGNED struct _WINXP_FSC_MARK_HANDLE_INFO
{
    _U32	UsnSourceInfo;
    _U32	VolumeHandle;				// [64bit issue] should 64 bit long
    _U32	HandleInfo;

} WINXP_FSC_MARK_HANDLE_INFO, *PWINXP_FSC_MARK_HANDLE_INFO;


typedef UNALIGNED struct _WINXP_FSC_MARK_HANDLE_INFO32
{
    _U32	UsnSourceInfo;
    _U32	VolumeHandle;				// [64bit issue] should 64 bit long
    _U32	HandleInfo;

} WINXP_FSC_MARK_HANDLE_INFO32, *PWINXP_FSC_MARK_HANDLE_INFO32;


typedef UNALIGNED struct _WINXP_REQUEST_FILE_SYSTEM_CONTROL
{
	_U32	OutputBufferLength;
    _U32	InputBufferLength;
    _U32	FsControlCode;

	union
	{
		WINXP_FSC_MOVE_FILE_DATA		FscMoveFileData;		// FSCTL_MOVE_FILE
		WINXP_FSC_MOVE_FILE_DATA32		FscMoveFileData32;
		WINXP_FSC_MARK_HANDLE_INFO		FscMarkHandleInfo;		// FSCTL_MARK_HANDLE
		WINXP_FSC_MARK_HANDLE_INFO32	FscMarkHandleInfo32;
	};

} WINXP_REQUEST_FILE_SYSTEM_CONTROL, *PWINXP_REQUEST_FILE_SYSTEM_CONTROL;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_DIRECTORY
{
	_U32	Length;
    _U32	FileInformationClass;
    _U32	FileIndex;

} WINXP_REQUEST_QUERY_DIRECTORY, *PWINXP_REQUEST_QUERY_DIRECTORY;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_FILE
{
	_U32	Length;
    _U32	FileInformationClass;

} WINXP_REQUEST_QUERY_FILE, *PWINXP_REQUEST_QUERY_FILE;


typedef UNALIGNED struct _WINXP_FILE_LINK_INFORMATION 
{
    _U8		ReplaceIfExists;
    _U32	RootDirectoryHandle;		// [64bit issue] should 64 bit long
    _U32	FileNameLength;

} WINXP_FILE_LINK_INFORMATION, *PWINXP_FILE_LINK_INFORMATION;


typedef UNALIGNED struct _WINXP_FILE_RENAME_INFORMATION 
{
    _U8		ReplaceIfExists;
    _U32	RootDirectoryHandle;		// [64bit issue] should 64 bit long
    _U32	FileNameLength;

} WINXP_FILE_RENAME_INFORMATION, *PWINXP_FILE_RENAME_INFORMATION;


typedef UNALIGNED struct _WINXP_FILE_BASIC_INFORMATION
{ 
	_U64	CreationTime;                            
    _U64	LastAccessTime;                         
    _U64	LastWriteTime;                      
    _U64	ChangeTime;                          
    _U32	FileAttributes;
	
} WINXP_FILE_BASIC_INFORMATION, *PWINXP_FILE_BASIC_INFORMATION;
 
  
typedef UNALIGNED struct _WINXP_FILE_DISPOSITION_INFORMATION 
{                  
    _U8		DeleteFile;                                         

} WINXP_FILE_DISPOSITION_INFORMATION, *PWINXP_FILE_DISPOSITION_INFORMATION; 


typedef UNALIGNED struct _WINXP_FILE_END_OF_FILE_INFORMATION 
{                  
    _U64	EndOfFile;                                         

} WINXP_FILE_END_OF_FILE_INFORMATION, *PWINXP_FILE_END_OF_FILE_INFORMATION; 

typedef UNALIGNED struct _WINXP_FILE_ALLOCATION_INFORMATION 
{                  
    _U64	AllocationSize;                                         

} WINXP_FILE_ALLOCATION_INFORMATION, *PWINXP_FILE_ALLOCATION_INFORMATION; 

typedef struct _WINXP_FILE_POSITION_INFORMATION 
{                  
    _U64	CurrentByteOffset;                                         

} WINXP_FILE_POSITION_INFORMATION, *PWINXP_FILE_POSITION_INFORMATION; 


typedef UNALIGNED struct _WINXP_REQUEST_SET_FILE
{
	_U32	Length;
    _U32	FileInformationClass;
    _U32	FileHandle;			// [64bit issue] should 64 bit long

    union 
	{
		struct 
		{
			_U8 ReplaceIfExists;
            _U8 AdvanceOnly;
		};
        _U32 ClusterCount;
        _U32 DeleteHandle;		// [64bit issue] should 64 bit long
	};

	union
	{
		WINXP_FILE_LINK_INFORMATION			LinkInformation;
		WINXP_FILE_RENAME_INFORMATION		RenameInformation;
		WINXP_FILE_BASIC_INFORMATION		BasicInformation;
		WINXP_FILE_DISPOSITION_INFORMATION	DispositionInformation;
		WINXP_FILE_END_OF_FILE_INFORMATION	EndOfFileInformation;
		WINXP_FILE_ALLOCATION_INFORMATION	AllocationInformation;
		WINXP_FILE_POSITION_INFORMATION		PositionInformation;
	};

} WINXP_REQUEST_SET_FILE, *PWINXP_REQUEST_SET_FILE;


typedef UNALIGNED struct _WINXP_REQUEST_QUERY_VOLUME
{
	_U32	Length;
    _U32	FsInformationClass;

} WINXP_REQUEST_QUERY_VOLUME, *PWINXP_REQUEST_QUERY_VOLUME;


typedef struct _WINXP_REQUEST_SET_VOLUME
{
	_U32	Length;
    _U32	FsInformationClass;

} WINXP_REQUEST_SET_VOLUME, *PWINXP_REQUEST_SET_VOLUME;


typedef UNALIGNED struct _WINXP_REQUEST_READ
{
	_U32	Length;
	_U32	Key;
    _U64	ByteOffset;

} WINXP_REQUEST_READ, *PWINXP_REQUEST_READ;


typedef UNALIGNED struct _WINXP_REQUEST_WRITE
{
	_U32	Length;
	_U32	Key;
    _U64	ByteOffset;

} WINXP_REQUEST_WRITE, *PWINXP_REQUEST_WRITE;


typedef UNALIGNED struct _WINXP_REQUEST_LOCK_CONTROL
{
	_U64	Length;
	_U32	Key;
    _U64	ByteOffset;

} WINXP_REQUEST_LOCK_CONTROL, *PWINXP_REQUEST_LOCK_CONTROL;


typedef struct _WINXP_REQUEST_QUERY_SECURITY 
{
	_U32	SecurityInformation ;
	_U32	Length ;

} WINXP_REQUEST_QUERY_SECURITY, *PWINXP_REQUEST_QUERY_SECURITY ;


typedef struct _WINXP_REQUEST_SET_SECURITY 
{
	_U32	SecurityInformation ;
	_U32	Length ;

} WINXP_REQUEST_SET_SECURITY, *PWINXP_REQUEST_SET_SECURITY ;

typedef struct _WINXP_REQUEST_QUERY_EA 
{
	_U32	Length;
	_U32	EaListLength;
	_U32	EaIndex;

} WINXP_REQUEST_QUERY_EA, * PWINXP_REQUEST_QUERY_EA ;


typedef struct _WINXP_REQUEST_SET_EA {
	_U32	Length;
} WINXP_REQUEST_SET_EA, * PWINXP_REQUEST_SET_EA ;

typedef struct _WINXP_REQUEST_QUERY_QUOTA
{
	_U32	Length;
	_U32	InputLength;
	_U32	StartSidOffset;

} WINXP_REQUEST_QUERY_QUOTA, *PWINXP_REQUEST_QUERY_QUOTA ;


typedef struct _WINXP_REQUEST_SET_QUOTA
{
	_U32	Length ;

} WINXP_REQUEST_SET_QUOTA, *PWINXP_REQUEST_SET_QUOTA ;

#ifdef _X86_
typedef UNALIGNED ULONG SECURITY_INFORMATION, *PSECURITY_INFORMATION;
#endif

typedef UNALIGNED struct _NDFS_WINXP_REQUEST_HEADER
{
	_U32	IrpTag;			// Irp Address etc
							// [64bit issue] should 64 bit long

	//_U16	RequestType;

	_U8		IrpMajorFunction;
	_U8		IrpMinorFunction;

	_U32	FileHandle;		// [64bit issue] should 64 bit long

	_U32	IrpFlags;
	_U8		IrpSpFlags;
//	_U32	DataSize;		// Appended Data Size

	union
	{
		WINXP_REQUEST_CREATE				Create;
		WINXP_REQUEST_DEVICE_IOCONTROL		DeviceIoControl;
		WINXP_REQUEST_FILE_SYSTEM_CONTROL	FileSystemControl;
		WINXP_REQUEST_QUERY_DIRECTORY		QueryDirectory;
		WINXP_REQUEST_QUERY_FILE			QueryFile;
		WINXP_REQUEST_SET_FILE				SetFile;
		WINXP_REQUEST_QUERY_VOLUME			QueryVolume;
		WINXP_REQUEST_SET_VOLUME			SetVolume ;
		WINXP_REQUEST_READ					Read;
		WINXP_REQUEST_WRITE					Write;
		WINXP_REQUEST_LOCK_CONTROL			LockControl;
		WINXP_REQUEST_QUERY_SECURITY		QuerySecurity ;
		WINXP_REQUEST_SET_SECURITY			SetSecurity ;
		WINXP_REQUEST_QUERY_EA				QueryEa ;
		WINXP_REQUEST_SET_EA				SetEa ;
		WINXP_REQUEST_QUERY_QUOTA			QueryQuota ;
		WINXP_REQUEST_SET_QUOTA				SetQuota ;
	};

	_U8		Reserved[5];
	
} NDFS_WINXP_REQUEST_HEADER, *PNDFS_WINXP_REQUEST_HEADER;


typedef UNALIGNED struct _WINXP_REPLY_CREATE
{
	_U32	FileHandle;		// [64bit issue] should 64 bit long
	_U8		SetSectionObjectPointer;
	_U32	OpenTimeHigPart;
	_U8		OpenTimeLowPartMsb;

} WINXP_REPLY_CREATE, *PWINXP_REPLY_CREATE;


typedef UNALIGNED struct _NDFS_WINXP_REPLY_HEADER
{
	_U32	IrpTag;				// Irp Address etc [64bit issue] should 64 bit long

	_U8		IrpMajorFunction;
	_U8		IrpMinorFunction;

	_U32	Status;
	_U32	Information;		// [64bit issue] should 64 bit long
	
	union
	{
		WINXP_REPLY_CREATE	Open;
		_U64				CurrentByteOffset;
	};

} NDFS_WINXP_REPLY_HEADER, *PNDFS_WINXP_REPLY_HEADER;


#include <poppack.h>


#endif // __NDFS_PROTOCOL_HEADER_H__