#ifndef _LFSMESSAGE_H_
#define _LFSMESSAGE_H_

#define	DEFAULT_PRIMARY_PORT	((USHORT)0x0001)

//
//	LFS Magic number
//
//#define	LFS_MAGIC	(0x81422418)

//
//	LFS current version
//
#define	LFS_MAJVER	0
#define	LFS_MINVER	1
//
// OS's types
//
#define OSTYPE_WINDOWS	0x00010000
#define		OSTYPE_WIN98SE		0x0001
#define		OSTYPE_WIN2K		0x0002
#define		OSTYPE_WINXP		0x0003
#define		OSTYPE_WIN2003SERV	0x0004

#define	OSTYPE_LINUX	0x00020000
#define		OSTYPE_LINUX22		0x0001
#define		OSTYPE_LINUX24		0x0002

#define	OSTYPE_MAC		0x00030000
#define		OSTYPE_MACOS9		0x0001
#define		OSTYPE_MACOSX		0x0002

//
//	packet type masks
//
#define LFSPKTTYPE_PREFIX		0xF000
#define	LFS_MESSAGE_MAJTYPE		0x0FF0
#define	LFSPKTTYPE_MINTYPE		0x000F


//
//	packet type prefixes
//
#define	LFS_MESSAGE_REQUEST		0x1000	//	request packet
#define	LFSPKTTYPE_REPLY		0x2000	//	reply packet
#define	LFSPKTTYPE_DATAGRAM		0x4000	//	datagram packet
#define	LFSPKTTYPE_BROADCAST	0x8000	//	broadcasting packet

/*
//
// define the packet type using datagram-type LPX
//
#define	LFSDG_PKTTYPE_QUERYOWNER	0x0010
#define LFSDG_PKTTYPE_ACQUIREOWNER	0x0020
#define	LFSDG_PKTTYPE_LASTTYPE		0x0020
*/
//
// define the packet type using stream-type LPX
//
#define	LFS_MESSAGE_OPEN			0x0010

#define	LFS_MESSAGE_CLOSE			0x0020

#define	LFS_MESSAGE_READ			0x0030
#define		LFSPKTYYPE_READ_NORMAL		0x0	//-- #define IRP_MN_NORMAL                   0x00
#define		LFSPKTYYPE_READ_DPC			0x1	//-- #define IRP_MN_DPC                      0x01
#define		LFSPKTYYPE_READ_MDL			0x2	//-- #define IRP_MN_MDL                      0x02
#define		LFSPKTYYPE_READ_COMPLETE	0x4	//-- #define IRP_MN_COMPLETE                 0x04
#define		LFSPKTYYPE_READ_COMPRESSED	0x8	//-- #define IRP_MN_COMPRESSED               0x08

#define	LFS_MESSAGE_WRITE		0x0040

#define	LFS_MESSAGE_QUERY_INFORMATION	0x0050

#define	LFS_MESSAGE_SET_INFORMATION	0x0060

#define	LFSPKTTYPE_GETATTR		0x0070

#define	LFSPKTTYPE_SETATTR		0x0080

#define	LFS_MESSAGE_FLUSH_BUFFERS	0x0090

#define	LFS_MESSAGE_QUERY_VOLUME_INFORMATION	0x00A0

#define	LFSPKTTYPE_SETVOLINFO	0x00B0

#define LFS_MESSAGE_DIRCONTROL	0x00C0
#define		LFS_MESSAGE_DIRCONTROL_QUERYDIR	0x1
#define		LFSPKTTYPE_DIRCONTROL_CHANGEDIR	0x2

#define	LFSPKTTYPE_LOCKCONTROL		0x00D0
#define		LFSPKTTYPE_LOCKCONTROL_LOCK					0x1
#define		LFSPKTTYPE_LOCKCONTROL_UNLOCK_SINGLE		0x2
#define		LFSPKTTYPE_LOCKCONTROL_UNLOCK_ALL			0x3
#define		LFSPKTTYPE_LOCKCONTROL_UNLOCK_ALL_BY_KEY	0x4

#define	LFSPKTTYPE_GETSECURITY	0x00E0

#define	LFSPKTTYPE_SETSECURITY	0x00F0

#define	LFS_MESSAGE_FILE_SYSTEM_CONTROL		0x0100
#define		LFSPKTTYPE_FSCTL_USER			0x0	//-- #define IRP_MN_USER_FS_REQUEST          0x00
#define		LFSPKTTYPE_FSCTL_MOUNT			0x1 //-- #define IRP_MN_MOUNT_VOLUME             0x01
#define		LFSPKTTYPE_FSCTL_VERIFY			0x2	//-- #define IRP_MN_VERIFY_VOLUME            0x02
#define		LFSPKTTYPE_FSCTL_LOADFS			0x3	//-- #define IRP_MN_LOAD_FILE_SYSTEM         0x03
#define		LFSPKTTYPE_FSCTL_KERNEL			0x4	//-- #define IRP_MN_KERNEL_CALL              0x04

#define	LFS_MESSAGE_DEVICE_CONTROL		0x0110

#define	LFSPKTTYPE_GETQUOTA		0x0120

#define	LFSPKTTYPE_SETQUOTA		0x0130

#define	LFS_MESSAGE_INTERNAL_DEVICE_CONTROL		0x0140

#define LFSPKTTYPE_RECOVERY		0x0150

#define	LFSPKTTYPE_LASTTYPE		0x0150


//
//	LFS status
//	LFS status contains NTSTATUS for use of stream type at this time.
//
#define LFSSTATUS_SUCCESS		0x0000

#define LFSSTATUS_FAILBIT		0x8000

//
//	status for ownership resolving
//
#define LFSSTATUS_RETRY			0x8001
#define LFSSTATUS_LOWPRIORITY	0x8002
#define LFSSTATUS_OWNED			0x8003


#include <pshpack1.h>

//
//	head part of a LFS packet
//
typedef struct _RWFILTER_MESSAGE_HEADER
{
	UINT32	LFSMagic ;			// LFSMAGIC
	UCHAR	Major ;				//	LFS protocol Version
	UCHAR	Minor ;
	UINT32	SenderOS ;			// sender's OS type

	USHORT	Type ;				//	packet type
	UINT32	PktSz ;				//	packet total size

	UINT32	FileID ;			// File's unique ID ( usually, file handler )
	UINT32	IOID ;				// Sequence number ( IRP address on Windows )

	UINT32	Offset ;			// file low offset
	UINT32	OffsetHigh ;		// file high offset

	UINT32	IrpFlags ;				// IRP flag
	UINT32	IrpStackFlags ;			// IO_STACK_LOCATION flag
								// FileObject->Flag when return.

	UINT32	DataSz ;			// data size contained in a body part.
	UINT32	ReqDataSz ;			// requestor's buffer size.
	UINT32	DoneDataSz ;		// data written by the actual file system. 
	UINT32	Status ;			// return status value

	//--> 56 bytes. keep longlong( 8bytes ) alignment!!!

} LFS_MESSAGE_HEADER, *PLFS_MESSAGE_HEADER ;


typedef struct _LFS_OPEN
{
	//
	//	information for indentification of a file to be created
	//
	UINT32	RelatedFileID ;		// related directory ID ( file handle )
	UCHAR	TND[6] ;			// NetDisk Address
	UCHAR	UnitDisk ;			// Unitdisk number
	UINT32	StartOffset ;		// Partition's start offset
	UINT32	StartOffsetHigh  ;	// Partition's start offset
	USHORT	FileNameLength ;	// Filename length in bytes
								// offset from Data is zero.
	//--> 16 bytes.

	//
	//	create options
	//
	UINT32	AccessMode ;
	UINT32	ShareMode ;
	UINT32	Security ;
	UINT32	Options ;
	UINT32	CreateDispo ;
	UINT32	FileAttr ;
	UINT32	AllocSize ;
	UINT32	AllocSizeHigh ;
	//--> 48 bytes.

	//
	//	EA buffer
	//
	UINT32	EaLength ;
	UINT32	EaDataOffset ;		// offset from Data
	//--> 56 bytes.

	//
	//	Data part
	//
	UINT32	DataLength ;
	UINT32	Reserved ;	// to keep longlong alignment.
	//--> 64 bytes
	UCHAR	Data[1] ;
} LFS_OPEN, *PLFS_OPEN;


typedef struct	_LFS_QUERY_DIR 
{
	UINT32	Class ;

	UINT32	FileIndex ;

	UINT32	DataLength ;				// it may be bigger than DoneDataSz
										// because of alignment.
	UINT32	FileNameLength ;			// in bytes
	//--> 32 bytes.
	union {
		UCHAR	FileNameUnicode[1] ;	// Unicode
		UCHAR	Data[1] ;
	} ;
} LFS_QUERY_DIR, *PLFS_QUERY_DIR;


typedef struct  _LFS_DEVICE_CONTROL
{
	UINT32	CtlCode ;
	UINT32	DataLength ;
	UINT32	OutputLength ;
	UINT32	Reserved ;					// to keep longlong alignment.
	//--> 16 bytes.
	union {
		UCHAR	InputData[1] ;
		UCHAR	OutputData[1] ;
	} ;
} LFS_DEVICE_CONTROL, *PLFS_DEVICE_CONTROL;

typedef struct _LFS_QUERY_FILE
{
	UINT32	Class ;
	UINT32	DataLength ;
	UCHAR	Data[1] ;
} LFS_QUERY_FILE, *PLFS_QUERY_FILE;


typedef struct _LFS_SET_FILE
{
	UINT32	Class ;

	UINT32	TargetFileHandle ;
	UCHAR	AdvanceOnly ;
	UCHAR	ReplaceIfExists ;
	UINT32	ClusterCount  ;				//	reserved for system use
	UINT32	DeleteHandle  ;				//	reserved for system use

	UINT32	DataLength ;
	USHORT	Reserved ;					// to keep longlong alignment.
	//--> 48 bytes.
	UCHAR	Data[1] ;

} LFS_SET_FILE, *PLFS_SET_FILE;


typedef struct _LFS_QUERY_VOLUME
{
	UINT32	Class ;
	UINT32	DataLength ;
	UCHAR	Data[1] ;
} LFS_QUERY_VOLUME, *PLFS_QUERY_VOLUME;


typedef	union _LFS_MESSAGE_DATA 
{
		UCHAR				Data[1] ;
		LFS_OPEN			Open ;
		LFS_QUERY_DIR		QueryDirectory;
		LFS_DEVICE_CONTROL	DeviceControl;
		LFS_QUERY_FILE		QueryFile;
		LFS_SET_FILE		SetFile;
		LFS_QUERY_VOLUME	QueryVolume;
		
#if 0
		struct	_LFSRecovery	Recovery ;
		struct	_LFSQuery		Query ;
		struct  _LFSSet			Set ;
		struct	_LFSQueryDir	QueryDir ;
		struct  _LFSQueryEa		QueryEa ;
		struct	_LFSSetFileInfo SetFile ;
		struct  _LFSIOCtl		IOCtl ;
		struct  _LFSLockCtl		LockCtl ;
		struct  _LFSQueryQuota	QueryQuota ;
#endif
		
} LFS_MESSAGE_DATA, *PLFS_MESSAGE_DATA ;




#include <poppack.h>


#endif