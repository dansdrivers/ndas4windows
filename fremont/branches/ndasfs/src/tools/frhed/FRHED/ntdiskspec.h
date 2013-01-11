#ifndef ntdiskspec_h
#define ntdiskspec_h

/*
code bits taken from WINTIOCTL.H (the cygwin version doesn't have dis)
This header is *NOT* copyrighted by me, but contains code copyrighted by Microsoft.
Download the latest version (of WINIOCTL.H) for free from
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/
*/

#ifndef IsContainerPartition

typedef struct _PARTITION_INFORMATION {
	LARGE_INTEGER StartingOffset;
	LARGE_INTEGER PartitionLength;
	DWORD HiddenSectors;
	DWORD PartitionNumber;
	BYTE PartitionType;
	BOOLEAN BootIndicator;
	BOOLEAN RecognizedPartition;
	BOOLEAN RewritePartition;
} PARTITION_INFORMATION, *PPARTITION_INFORMATION;

typedef struct _DRIVE_LAYOUT_INFORMATION {
	DWORD PartitionCount;
	DWORD Signature;
	PARTITION_INFORMATION PartitionEntry[1];
} DRIVE_LAYOUT_INFORMATION, *PDRIVE_LAYOUT_INFORMATION;

typedef enum _MEDIA_TYPE {
	Unknown,        // Format is unknown
	F5_1Pt2_512,    // 5.25", 1.2MB,  512 bytes/sector
	F3_1Pt44_512,   // 3.5",  1.44MB, 512 bytes/sector
	F3_2Pt88_512,   // 3.5",  2.88MB, 512 bytes/sector
	F3_20Pt8_512,   // 3.5",  20.8MB, 512 bytes/sector
	F3_720_512,     // 3.5",  720KB,  512 bytes/sector
	F5_360_512,     // 5.25", 360KB,  512 bytes/sector
	F5_320_512,     // 5.25", 320KB,  512 bytes/sector
	F5_320_1024,    // 5.25", 320KB,  1024 bytes/sector
	F5_180_512,     // 5.25", 180KB,  512 bytes/sector
	F5_160_512,     // 5.25", 160KB,  512 bytes/sector
	RemovableMedia, // Removable media other than floppy
	FixedMedia,     // Fixed hard disk media
	F3_120M_512,    // 3.5", 120M Floppy
	F3_640_512,     // 3.5" ,  640KB,  512 bytes/sector
	F5_640_512,     // 5.25",  640KB,  512 bytes/sector
	F5_720_512,     // 5.25",  720KB,  512 bytes/sector
	F3_1Pt2_512,    // 3.5" ,  1.2Mb,  512 bytes/sector
	F3_1Pt23_1024,  // 3.5" ,  1.23Mb, 1024 bytes/sector
	F5_1Pt23_1024,  // 5.25",  1.23MB, 1024 bytes/sector
	F3_128Mb_512,   // 3.5" MO 128Mb   512 bytes/sector
	F3_230Mb_512,   // 3.5" MO 230Mb   512 bytes/sector
	F8_256_128      // 8",     256KB,  128 bytes/sector
} MEDIA_TYPE, *PMEDIA_TYPE;


typedef struct _DISK_GEOMETRY {
	LARGE_INTEGER Cylinders;
	MEDIA_TYPE MediaType;
	DWORD TracksPerCylinder;
	DWORD SectorsPerTrack;
	DWORD BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

typedef enum
{
	AttributeStandardInformation = 0x10,
	AttributeAttributeList = 0x20,
	AttributeFileName = 0x30,
	AttributeObjectID = 0x40,
	AttributeSecurityDescriptor = 0x50,
	AttributeVolumeName = 0x60,
	AttributeVolumeInformation = 0x70,
	AttributeData = 0x80,
	AttributeIndexRoot = 0x90,
	AttributeIndexAllocation = 0xA0,
	AttributeBitmap = 0xB0,
	AttributeReparsePoint = 0xC0,
	AttributeEAInformation = 0xD0,
	AttributeEA = 0xE0,
	AttributePropertySet = 0xF0,
	AttributeLoggedUtilityStream = 0x100,
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

#define FILE_DEVICE_DISK 0x00000007
#define IOCTL_DISK_BASE FILE_DEVICE_DISK
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define FILE_READ_ACCESS ( 0x0001 )// file & pipe

#define CTL_CODE(DeviceType,Function,Method,Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) )
#define IOCTL_DISK_GET_DRIVE_GEOMETRY CTL_CODE(IOCTL_DISK_BASE, 0x0000, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_DRIVE_LAYOUT CTL_CODE(IOCTL_DISK_BASE, 0x0003, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_GET_PARTITION_INFO CTL_CODE(IOCTL_DISK_BASE, 0x0001, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif

#ifndef IOCTL_DISK_GET_DRIVE_LAYOUT_EX

#define IOCTL_DISK_GET_DRIVE_LAYOUT_EX CTL_CODE(IOCTL_DISK_BASE, 0x0014, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_DRIVE_GEOMETRY_EX CTL_CODE(IOCTL_DISK_BASE, 0x0028, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DISK_GEOMETRY_EX {
	DISK_GEOMETRY Geometry; // Standard disk geometry: may be faked by driver.
	LARGE_INTEGER DiskSize; // Must always be correct
	BYTE Data[1]; // Partition, Detect info
} DISK_GEOMETRY_EX, *PDISK_GEOMETRY_EX;

typedef struct _DRIVE_LAYOUT_INFORMATION_GPT {
	GUID DiskId;
	LARGE_INTEGER StartingUsableOffset;
	LARGE_INTEGER UsableLength;
	DWORD MaxPartitionCount;
} DRIVE_LAYOUT_INFORMATION_GPT, *PDRIVE_LAYOUT_INFORMATION_GPT;

//
// MBR specific drive layout information.
//

typedef struct _DRIVE_LAYOUT_INFORMATION_MBR {
	DWORD Signature;
} DRIVE_LAYOUT_INFORMATION_MBR, *PDRIVE_LAYOUT_INFORMATION_MBR;

typedef enum _PARTITION_STYLE {
	PARTITION_STYLE_MBR,
	PARTITION_STYLE_GPT,
	PARTITION_STYLE_RAW
} PARTITION_STYLE;


typedef struct _PARTITION_INFORMATION_MBR {
	BYTE PartitionType;
	BOOLEAN BootIndicator;
	BOOLEAN RecognizedPartition;
	DWORD HiddenSectors;
} PARTITION_INFORMATION_MBR, *PPARTITION_INFORMATION_MBR;

typedef struct _PARTITION_INFORMATION_GPT {
	GUID PartitionType; // Partition type. See table 16-3.
	GUID PartitionId; // Unique GUID for this partition.
	DWORD64 Attributes; // See table 16-4.
	WCHAR Name [36]; // Partition Name in Unicode.
} PARTITION_INFORMATION_GPT, *PPARTITION_INFORMATION_GPT;

typedef struct _PARTITION_INFORMATION_EX {
	PARTITION_STYLE PartitionStyle;
	LARGE_INTEGER StartingOffset;
	LARGE_INTEGER PartitionLength;
	DWORD PartitionNumber;
	BOOLEAN RewritePartition;
	union {
		PARTITION_INFORMATION_MBR Mbr;
		PARTITION_INFORMATION_GPT Gpt;
	};
} PARTITION_INFORMATION_EX, *PPARTITION_INFORMATION_EX;


typedef struct _DRIVE_LAYOUT_INFORMATION_EX {
	DWORD PartitionStyle;
	DWORD PartitionCount;
	union {
		DRIVE_LAYOUT_INFORMATION_MBR Mbr;
		DRIVE_LAYOUT_INFORMATION_GPT Gpt;
	};
	PARTITION_INFORMATION_EX PartitionEntry[1];
} DRIVE_LAYOUT_INFORMATION_EX, *PDRIVE_LAYOUT_INFORMATION_EX;

#endif

#endif // ntdiskspec_h
