#ifndef __LFS_MESSAGE_HEADER_H__
#define __LFS_MESSAGE_HEADER_H__


#define	DEFAULT_PRIMARY_PORT	((USHORT)0x0001)

#define _U8		UCHAR
#define _U16	USHORT
#define _U32	ULONG
#define _U64	ULONGLONG


// Little Endian


#include <pshpack1.h>

#define LFS_MAGIC			0x81422418
#define LFS_MESSAGE_MAJOR	1
#define LFS_MESSAGE_MINOR	0

#define LFS_MAX_PATH		255		



// Request Type

#define	LFS_REQUEST_OPEN	0x0001
#define LFS_REQUEST_EXECUTE	0x0002
#define LFS_REQUEST_CLOSE	0x0003


// Request Type

#define	LFS_REPLY_OPEN		0x0001
#define LFS_REPLY_EXECUTE	0x0002
#define LFS_REPLY_CLOSE		0x0003

#define LFS_REPLY_SUCCESS	0x0
#define LFS_REPLY_FAIL		0x1

#define LFS_RESULT_MASK		0x8000

#if 0

typedef UNALIGNED struct _LFS_REQUEST_MESSAGE
{
	_U32	LfsMagic;
	_U8		MajorVersion;
	_U8		MinorVersion;

	_U32	RequesterOs;

	_U16	RequestType;
	_U32	MessageSize;	// Total Message Size

} LFS_REQUEST_MESSAGE, *PLFS_REQUEST_MESSAGE;


typedef UNALIGNED struct _LFS_REPLY_MESSAGE
{
	_U32	LfsMagic;
	_U8		MajorVersion;
	_U8		MinorVersion;

	_U32	ReplyerOs;

	union
	{
		_U16	Reply;
		struct 
		{
			_U16	ReplyType:15;
			_U16	ReplyResult:1;
		};
	};
	
	_U32	MessageSize; // Total Data Size

} LFS_REPLY_MESSAGE, *PLFS_REPLY_MESSAGE;

#endif

typedef UNALIGNED struct _LFS_WINXP_OPEN_DATA
{
	_U16			NetDiskPort;
	_U8				NetDiskNode[6];
	_U16			UnitDiskNo;
	_U64			StartingOffset;

} LFS_WINXP_OPEN_DATA, *PLFS_WINXP_OPEN_DATA;


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

} WINXP_REQUEST_CREATE, *PWINXP_REQUEST_CREATE;


typedef UNALIGNED struct _WINXP_REQUEST_DEVICE_IOCONTROL
{
	_U32	OutputBufferLength;
    _U32	InputBufferLength;
    _U32	IoControlCode;

} WINXP_REQUEST_DEVICE_IOCONTROL, *PWINXP_REQUEST_DEVICE_IOCONTROL;


typedef UNALIGNED struct _WINXP_REQUEST_FILE_SYSTEM_CONTROL
{
	_U32	OutputBufferLength;
    _U32	InputBufferLength;
    _U32	FsControlCode;

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
    _U32	RootDirectoryHandle;
    _U32	FileNameLength;

} WINXP_FILE_LINK_INFORMATION, *PWINXP_FILE_LINK_INFORMATION;


typedef UNALIGNED struct _WINXP_FILE_RENAME_INFORMATION 
{
    _U8		ReplaceIfExists;
    _U32	RootDirectoryHandle;
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
    _U32	FileHandle;
    union 
	{
		struct 
		{
			_U8 ReplaceIfExists;
            _U8 AdvanceOnly;
		};
        _U32 ClusterCount;
        _U32 DeleteHandle;
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


typedef UNALIGNED ULONG SECURITY_INFORMATION, *PSECURITY_INFORMATION;

#define MAX_REQUEST_DATA_SIZE	(64*1024)
#define MAX_REPLY_DATA_SIZE		(64*1024)

typedef UNALIGNED struct _LFS_WINXP_REQUEST_MESSAGE
{
	_U32	IrpTag;			// Irp Address etc

	//_U16	RequestType;

	_U8		IrpMajorFunction;
	_U8		IrpMinorFunction;

	_U32	FileHandle;

	_U32	IrpFlags;
	_U8		IrpSpFlags;
	_U32	DataSize;		// Appended Data Size

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
	};

} LFS_WINXP_REQUEST_MESSAGE, *PLFS_WINXP_REQUEST_MESSAGE;


typedef UNALIGNED struct _WINXP_REPLY_CREATE
{
	_U32	FileHandle;
	_U8		SetSectionObjectPointer;
	
} WINXP_REPLY_CREATE, *PWINXP_REPLY_CREATE;


typedef UNALIGNED struct _LFS_WINXP_REPLY_MESSAGE
{
	_U32	IrpTag;		// Irp Address

	_U8		IrpMajorFunction;
	_U8		IrpMinorFunction;

#if 0
	union
	{
		_U16	Reply;
		struct 
		{
			_U16	ReplyType:15;
			_U16	ReplyResult:1;
		};
	};
#endif

	_U32	Status;
	_U32	Information;
	
	union
	{
		WINXP_REPLY_CREATE				Open;
	};

	_U32	DataSize;	// Appended Data Size

} LFS_WINXP_REPLY_MESSAGE, *PLFS_WINXP_REPLY_MESSAGE;


#include <poppack.h>

#endif