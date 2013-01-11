#ifndef _NDFS_INTERFACE_H_
#define _NDFS_INTERFACE_H_

#define NDFATCONTROL  0x00000866 
#define IOCTL_REGISTER_NDFS_CALLBACK	CTL_CODE(NDFATCONTROL, 0, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_UNREGISTER_NDFS_CALLBACK	CTL_CODE(NDFATCONTROL, 1, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_INSERT_PRIMARY_SESSION	CTL_CODE(NDFATCONTROL, 2, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_SHUTDOWN					CTL_CODE(NDFATCONTROL, 3, METHOD_BUFFERED, FILE_ANY_ACCESS) 

#define DEVICE_NAMES_SZ  100

typedef enum _NETDISK_ENABLE_MODE {

	NETDISK_UNKNOWN_MODE = 0,
	NETDISK_READ_ONLY,
	NETDISK_PRIMARY,		 
	NETDISK_SECONDARY,
	NETDISK_SECONDARY2PRIMARY
	
} NETDISK_ENABLE_MODE, *PNETDISK_ENABLE_MODE;


typedef struct _NETDISK_INFORMATION {

	LPX_ADDRESS		NetDiskAddress;
	USHORT			UnitDiskNo;

	ACCESS_MASK		DesiredAccess;
	ACCESS_MASK		GrantedAccess;

	UCHAR			UserId[4];
	UCHAR			Password[8];

	BOOLEAN			MessageSecurity;
	BOOLEAN			RwDataSecurity;

	ULONG			SlotNo;
	LARGE_INTEGER	EnabledTime;
	LPX_ADDRESS		BindAddress;

	USHORT			LowestHwVer;

	BOOLEAN			DiskGroup;

} NETDISK_INFORMATION, *PNETDISK_INFORMATION;


typedef struct _NETDISK_PARTITION_INFORMATION {

	NETDISK_INFORMATION		NetdiskInformation;
    PARTITION_INFORMATION	PartitionInformation;
	PDEVICE_OBJECT			DiskDeviceObject;
	UNICODE_STRING			VolumeName;
    WCHAR					VolumeNameBuffer[DEVICE_NAMES_SZ];
	
} NETDISK_PARTITION_INFORMATION, *PNETDISK_PARTITION_INFORMATION;


typedef struct _NDFS_CALLBACK {

	ULONG	Size;

	NTSTATUS (*QueryPartitionInformation) ( IN  PDEVICE_OBJECT			RealDevice,
										    OUT PNETDISK_INFORMATION	NetdiskInformation, 
											OUT PPARTITION_INFORMATION	PartitionInformation );	

	NTSTATUS (*QueryPrimaryAddress) ( IN  PNETDISK_INFORMATION	NetdiskInformation,
									  OUT PLPX_ADDRESS			PrimaryAddress );	

	BOOLEAN (*SecondaryToPrimary) ( IN  PDEVICE_OBJECT	RealDevice );

} NDFS_CALLBACK, *PNDFS_CALLBACK;


typedef struct _SESSION_INFORMATION {

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;	
	HANDLE							ConnectionFileHandle;
	PFILE_OBJECT					ConnectionFileObject;
	_U32							SessionKey;
	union {

		_U8		SessionFlags;
		struct 	{
			
			_U8 MessageSecurity:1;
			_U8 RwDataSecurity:1;
			_U8 Reserved:6;
		};
	};

	_U32						PrimaryMaxDataSize;
	_U32						SecondaryMaxDataSize;
	_U16						Uid;
	_U16						Tid;
	_U16						NdfsMajorVersion;
	_U16						NdfsMinorVersion;
	_U16						RequestPerSession;

	KEVENT						CompletionEvent;

} SESSION_INFORMATION, *PSESSION_INFORMATION;


#endif