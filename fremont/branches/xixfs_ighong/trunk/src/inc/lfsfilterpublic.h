#ifndef __LFS_FILTER_PUBLIC_H__
#define __LFS_FILTER_PUBLIC_H__

#include "lfseventq.h"

//#define BUFFER_SIZE     4096

#define FILESPY_Reset				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x00, METHOD_BUFFERED, FILE_WRITE_ACCESS )
#define FILESPY_StartLoggingDevice	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x01, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_StopLoggingDevice	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x02, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetLog				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x03, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetVer				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x04, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_ListDevices			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x05, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetStats			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x06, METHOD_BUFFERED, FILE_READ_ACCESS )

#define LFS_FILTER_SHUTDOWN			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x10, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_CREATE_EVTQ		(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x11, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_CLOSE_EVTQ		(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x12, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_GET_EVTHEADER	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x13, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_DEQUEUE_EVT		(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x14, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_QUERY_NDASUSAGE	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x15, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_STOP_SECVOLUME	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x16, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_READY			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x17, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_GETVERSION		(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x201, METHOD_BUFFERED, FILE_READ_ACCESS )

#define FILESPY_DRIVER_NAME      TEXT("lfsfilt.sys")
#define FILESPY_DEVICE_NAME      TEXT("lfsfilt")
#define FILESPY_W32_DEVICE_NAME  TEXT("\\\\.\\LfsFilt")
#define FILESPY_DOSDEVICE_NAME   TEXT("\\DosDevices\\LfsFilt")
#define FILESPY_FULLDEVICE_NAME1 TEXT("\\FileSystem\\Filters\\LfsFilt")
#define FILESPY_FULLDEVICE_NAME2 TEXT("\\FileSystem\\LfsFiltCDO")

#define LFS_FILTER_DRIVER_NAME      FILESPY_DRIVER_NAME
#define LFS_FILTER_SERVICE_NAME     FILESPY_DEVICE_NAME
#define LFS_FILTER_DEVICE_NAME      FILESPY_DEVICE_NAME
#define LFS_FILTER_W32_DEVICE_NAME  FILESPY_W32_DEVICE_NAME
#define LFS_FILTER_DOSDEVICE_NAME   FILESPY_DOSDEVICE_NAME
#define LFS_FILTER_FULLDEVICE_NAME1 FILESPY_FULLDEVICE_NAME1
#define LFS_FILTER_FULLDEVICE_NAME2 FILESPY_FULLDEVICE_NAME2

#define FILESPY_MAJ_VERSION 1
#define FILESPY_MIN_VERSION 0

#ifndef ROUND_TO_SIZE
#define ROUND_TO_SIZE(_length, _alignment)    \
            (((_length) + ((_alignment)-1)) & ~((_alignment) - 1))
#endif 


//
//	LFS event queue handle
//	Wait for event queue handle to get signaled when an event is ready.
//

//
//	LFS_FILTER_CREATE_EVTQ
//

typedef struct _LFS_CREATEEVTQ_RETURN {
	LFS_EVTQUEUE_HANDLE	EventQueueHandle;
	HANDLE				EventWaitHandle;
} LFS_CREATEEVTQ_RETURN, *PLFS_CREATEEVTQ_RETURN;

typedef struct _LFS_CREATEEVTQ_RETURN_32 {
	LFS_EVTQUEUE_HANDLE_32	EventQueueHandle;
	VOID*POINTER_32 		EventWaitHandle;
} LFS_CREATEEVTQ_RETURN_32, *PLFS_CREATEEVTQ_RETURN_32;

//
//	LFS_FILTER_CLOSE_EVTQ
//

typedef struct _LFS_CLOSEVTQ_PARAM {
	LFS_EVTQUEUE_HANDLE	EventQueueHandle;
} LFS_CLOSEVTQ_PARAM, *PLFS_CLOSEVTQ_PARAM;

typedef struct _LFS_CLOSEVTQ_PARAM_32 {
	LFS_EVTQUEUE_HANDLE_32	EventQueueHandle;
} LFS_CLOSEVTQ_PARAM_32, *PLFS_CLOSEVTQ_PARAM_32;


//
//	LFS_FILTER_GET_EVTHEADER
//

typedef struct _LFS_GET_EVTHEADER_PARAM {
	LFS_EVTQUEUE_HANDLE	EventQueueHandle;
} LFS_GET_EVTHEADER_PARAM, *PLFS_GET_EVTHEADER_PARAM;

typedef struct _LFS_GET_EVTHEADER_PARAM_32 {
	LFS_EVTQUEUE_HANDLE_32	EventQueueHandle;
} LFS_GET_EVTHEADER_PARAM_32, *PLFS_GET_EVTHEADER_PARAM_32;


typedef struct _LFS_GET_EVTHEADER_RETURN {
	UINT32	EventLength;
	UINT32	EventClass;
} LFS_GET_EVTHEADER_RETURN, *PLFS_GET_EVTHEADER_RETURN;

//
//	LFS_FILTER_DEQUEUE_EVT
//
typedef struct _LFS_DEQUEUE_EVT_PARAM {
	LFS_EVTQUEUE_HANDLE	EventQueueHandle;
} LFS_DEQUEUE_EVT_PARAM, *PLFS_DEQUEUE_EVT_PARAM;

typedef struct _LFS_DEQUEUE_EVT_PARAM_32 {
	LFS_EVTQUEUE_HANDLE_32	EventQueueHandle;
} LFS_DEQUEUE_EVT_PARAM_32, *PLFS_DEQUEUE_EVT_PARAM_32;

//
//	LFS_FILTER_QUERY_NDASUSAGE
//

typedef struct _LFS_QUERY_NDASUSAGE_PARAM {

	UINT32				SlotNumber;
	UINT32				Reserved;
	LFS_SCSI_ADDRESS	ScsiAddress;	// Unused for now.

} LFS_QUERY_NDASUSAGE_PARAM, *PLFS_QUERY_NDASUSAGE_PARAM;

//
//	Disk volume does not exist.
//

#define LFS_NDASUSAGE_FLAG_NODISKVOL	0x00000001


//
//	LFS Filter is not attached to the disk volume
//	nor the file system volume
//

#define LFS_NDASUSAGE_FLAG_ATTACHED		0x00000002


//
//	Operation mode
//

//	Volumes attached to the disk volumes act as a primary volume
#define LFS_NDASUSAGE_FLAG_PRIMARY		0x00000100

//	Volumes attached to the disk volumes act as a secondary volume
#define LFS_NDASUSAGE_FLAG_SECONDARY	0x00000200

//	Volumes attached to the disk volumes act as a read-only volume
#define LFS_NDASUSAGE_FLAG_READONLY		0x00000400


//
//	More than one volume attached to the disk volumes
//	are locked to perform raw volume access
//

#define LFS_NDASUSAGE_FLAG_LOCKED		0x00010000

typedef struct _LFS_QUERY_NDASUSAGE_RETURN {
	UINT32	NdasUsageFlags;


	//
	//	NOTE: this counter may not match the number of disk volumes.
	//			One disk volume can have more than one file system volume
	//

	UINT32	MountedFSVolumeCount;
} LFS_QUERY_NDASUSAGE_RETURN, *PLFS_QUERY_NDASUSAGE_RETURN;


//
//	LFS_FILTER_STOP_SECVOLUME
//

typedef struct _LFS_STOPSECVOLUME_PARAM {
	UINT32	PhysicalDriveNumber;
	UINT32	VolumeHint;
} LFS_STOPSECVOLUME_PARAM, *PLFS_STOPSECVOLUME_PARAM;


//
//	LFS_FILTER_GETVERSION
//

typedef struct _LFSFILT_VER {
	USHORT						VersionMajor;
	USHORT						VersionMinor;
	USHORT						VersionBuild;
	USHORT						VersionPrivate;
	USHORT						VersionNDFSMajor;
	USHORT						VersionNDFSMinor;
	UCHAR						Reserved[12];
} LFSFILT_VER, *PLFSFILT_VER;

typedef struct _FILESPYVER {
    USHORT Major;
    USHORT Minor;
} FILESPYVER, *PFILESPYVER;

typedef ULONG_PTR FILE_ID;        //  To allow passing up PFILE_OBJECT as 
                                  //     unique file identifier in user-mode
typedef ULONG_PTR DEVICE_ID;      //  To allow passing up PDEVICE_OBJECT as
                                  //     unique device identifier in user-mode
typedef LONG NTSTATUS;            //  To allow status values to be passed up 
                                  //     to user-mode

//
//  This is set to the number of characters we want to allow the 
//  device extension to store for the various names used to identify
//  a device object.
//

#define DEVICE_NAMES_SZ  100

//
//  An array of these structures are returned when the attached device list is
//  returned.
//

typedef struct _ATTACHED_DEVICE {
    BOOLEAN LoggingOn;
    WCHAR DeviceNames[DEVICE_NAMES_SZ];
} ATTACHED_DEVICE, *PATTACHED_DEVICE;





//////////////////////////////////////////////////////////////////////////
//
//	NDFS Version Information
//

//
//	Increase the major version number when it does not provide different version compatibility.
//
//	3.02.1009 Beta		0x0001
//	3.02.1011 Release	0x0002
//	3.03.1013 Release	0x0002
//  3.******  Beta		0x0103

#define NDFS_COMPAT_VERSION	((USHORT)0x0103)
//
//	Increase the minor version number whenever NDFS protocol changes.
//
#define NDFS_VERSION		((USHORT)0x0001)


#endif	//	 __LFS_FILTER_PUBLIC_H__
