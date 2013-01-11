#ifndef _NDFS_INTERFACE_H_
#define _NDFS_INTERFACE_H_

#define NDFATCONTROL  0x00000866 

#define IOCTL_REGISTER_NDFS_CALLBACK	CTL_CODE(NDFATCONTROL, 0, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_UNREGISTER_NDFS_CALLBACK	CTL_CODE(NDFATCONTROL, 1, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_INSERT_PRIMARY_SESSION	CTL_CODE(NDFATCONTROL, 2, METHOD_BUFFERED, FILE_ANY_ACCESS) 
#define IOCTL_SHUTDOWN					CTL_CODE(NDFATCONTROL, 3, METHOD_BUFFERED, FILE_ANY_ACCESS) 

#define FSCTL_NDAS_FS_FLUSH_OR_PURGE		CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x3FDA, METHOD_BUFFERED, FILE_READ_ACCESS)

#define DEVICE_NAMES_SZ  100

typedef enum _NETDISK_ENABLE_MODE {

	NETDISK_UNKNOWN_MODE = 0,
	NETDISK_READ_ONLY,
	NETDISK_PRIMARY,		 
	NETDISK_SECONDARY,
	NETDISK_SECONDARY2PRIMARY
	
} NETDISK_ENABLE_MODE, *PNETDISK_ENABLE_MODE;

#define NDSC_ID_LENGTH	16

typedef struct _NETDISK_INFORMATION {

	LPX_ADDRESS			NetDiskAddress;
	USHORT				UnitDiskNo;

	UINT8					NdscId[NDSC_ID_LENGTH];

	NDAS_DEV_ACCESSMODE	DeviceMode;
	NDAS_FEATURES		SupportedFeatures;
	NDAS_FEATURES		EnabledFeatures;

	UCHAR				UserId[4];
	UCHAR				Password[8];

	BOOLEAN				MessageSecurity;
	BOOLEAN				RwDataSecurity;

	ULONG				SlotNo;
	LARGE_INTEGER		EnabledTime;
	LPX_ADDRESS			BindAddress;

	BOOLEAN				DiskGroup;

} NETDISK_INFORMATION, *PNETDISK_INFORMATION;


typedef struct _NETDISK_PARTITION_INFORMATION {

	ULONG						Flags;
#define NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT	0x00000001

	NETDISK_INFORMATION			NetdiskInformation;
	
	union {

		PARTITION_INFORMATION		PartitionInformation;	// for windows 2000
#if WINVER >= 0x0501
		PARTITION_INFORMATION_EX	PartitionInformationEx; // for winxp and later
#endif
	};
	
	PDEVICE_OBJECT				DiskDeviceObject;

	UNICODE_STRING				VolumeName;
    WCHAR						VolumeNameBuffer[DEVICE_NAMES_SZ];

} NETDISK_PARTITION_INFORMATION, *PNETDISK_PARTITION_INFORMATION;


typedef struct _NDFS_CALLBACK {

	ULONG	Size;

	NTSTATUS (*QueryPartitionInformation) ( IN  PDEVICE_OBJECT					RealDevice,
										    OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation );	

	NTSTATUS (*QueryPrimaryAddress) ( IN  PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
									  OUT PLPX_ADDRESS						PrimaryAddress,	
									  IN  PBOOLEAN							IsLocalAddress );	

	NTSTATUS (*SecondaryToPrimary) ( IN PDEVICE_OBJECT RealDevice, IN BOOLEAN ModeChange );

	NTSTATUS (*AddWriteRange) ( IN PDEVICE_OBJECT RealDevice, OUT PBLOCKACE_ID	BlockAceId );

	VOID (*RemoveWriteRange) ( IN PDEVICE_OBJECT RealDevice, IN BLOCKACE_ID	BlockAceId );
	
	NTSTATUS (*GetNdasScsiBacl) ( IN  PDEVICE_OBJECT	DiskDeviceObject,
								  OUT PNDAS_BLOCK_ACL	NdasBacl,
								  IN  BOOLEAN			SystemOrUser ); 


} NDFS_CALLBACK, *PNDFS_CALLBACK;


typedef struct _SESSION_CONTEXT {

	UINT32	SessionKey;

	union {

		UINT8		Flags;

		struct {

			UINT8 MessageSecurity:1;
			UINT8 RwDataSecurity:1;
			UINT8 Reserved:6;
		};
	};

	UINT16		ChallengeLength;
	UINT8			ChallengeBuffer[MAX_CHALLENGE_LEN];
	UINT32		PrimaryMaxDataSize;
	UINT32		SecondaryMaxDataSize;
	UINT16		Uid;
	UINT16		Tid;
	UINT16		NdfsMajorVersion;
	UINT16		NdfsMinorVersion;

	UINT16		SessionSlotCount;

	UINT32		BytesPerFileRecordSegment;
	UINT32		BytesPerSector;
	UINT32		SectorShift;
	UINT32		SectorMask;
	UINT32		BytesPerCluster;
	UINT32		ClusterShift;
	UINT32		ClusterMask;

} SESSION_CONTEXT, *PSESSION_CONTEXT;


typedef struct _SESSION_INFORMATION {

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;	

	HANDLE							ConnectionFileHandle;
	PFILE_OBJECT					ConnectionFileObject;
	LPXTDI_OVERLAPPED_CONTEXT		OverlappedContext;

	LPX_ADDRESS						RemoteAddress;
	BOOLEAN							IsLocalAddress;

	SESSION_CONTEXT					SessionContext;

	KEVENT							CompletionEvent;


} SESSION_INFORMATION, *PSESSION_INFORMATION;


#endif