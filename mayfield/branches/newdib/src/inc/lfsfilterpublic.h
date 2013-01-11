#ifndef __LFS_FILTER_PUBLIC_H__
#define __LFS_FILTER_PUBLIC_H__

#define BUFFER_SIZE     4096



#define FILESPY_Reset				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x00, METHOD_BUFFERED, FILE_WRITE_ACCESS )
#define FILESPY_StartLoggingDevice	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x01, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_StopLoggingDevice	(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x02, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetLog				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x03, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetVer				(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x04, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_ListDevices			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x05, METHOD_BUFFERED, FILE_READ_ACCESS )
#define FILESPY_GetStats			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x06, METHOD_BUFFERED, FILE_READ_ACCESS )

#define LFS_FILTER_SHUTDOWN			(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x10, METHOD_BUFFERED, FILE_READ_ACCESS )
#define LFS_FILTER_GETVERSION		(ULONG) CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x201, METHOD_BUFFERED, FILE_READ_ACCESS )

#define FILESPY_DRIVER_NAME      TEXT("LfsFilt.SYS")
#define FILESPY_DEVICE_NAME      TEXT("LfsFilt")
#define FILESPY_W32_DEVICE_NAME  TEXT("\\\\.\\LfsFilt")
#define FILESPY_DOSDEVICE_NAME   TEXT("\\DosDevices\\LfsFilt")
#define FILESPY_FULLDEVICE_NAME1 TEXT("\\FileSystem\\Filters\\LfsFilt")
#define FILESPY_FULLDEVICE_NAME2 TEXT("\\FileSystem\\LfsFiltCDO")

#define FILESPY_MAJ_VERSION 1
#define FILESPY_MIN_VERSION 0

#ifndef ROUND_TO_SIZE
#define ROUND_TO_SIZE(_length, _alignment)    \
            (((_length) + ((_alignment)-1)) & ~((_alignment) - 1))
#endif 

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
//
#define NDFS_COMPAT_VERSION	((USHORT)0x0002)
//
//	Increase the minor version number whenever NDFS protocol changes.
//
#define NDFS_VERSION		((USHORT)0x0001)


#endif	//	 __LFS_FILTER_PUBLIC_H__
