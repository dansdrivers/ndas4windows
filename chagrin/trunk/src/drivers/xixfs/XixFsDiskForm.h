#ifndef __XIXFS_DISKFORM__H__
#define __XIXFS_DISKFORM__H__

#include "XixFsType.h"


/*
#if (_M_PPC || _M_MPPC)
#else 
#endif//#if (_M_PPC || _M_MPPC)
*/

/*
	Disk Information stored based on little endian
	
	Please check endian....
*/


#define LITTLETOBIGS(Data)		((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
#define BIGTOLITTLES(Data)		(USHORT)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))

#define LITTLETOBIGL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8)	\
								| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define BIGTOLITTLEL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
								| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define LITTLETOBIGLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define BIGTOLITTLELL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))


/*
 *		Vitual Cluster Numbering
 */

#define CLUSTER_SHIFT							(12)
#define CLUSTER_SIZE							(1<<(CLUSTER_SHIFT))
#define CLUSTER_ALIGNED_ADDRESS(y)				( ( (y) >> (CLUSTER_SHIFT) ) << (CLUSTER_SHIFT) )
#define CLUSTER_ALIGNED_SIZE(y)					( ( ( (y) + (CLUSTER_SIZE) - 1 ) >> (CLUSTER_SHIFT) ) << (CLUSTER_SHIFT) )
#define CLUSTER_NUMBER(y)						(	(y) >> (CLUSTER_SHIFT) )
 /*
 *		Sector based calcuration
 */
#define SECTOR_ALIGNED_ADDR(x, y)				(	( (y) / (x) ) * (x)		)
#define SECTOR_ALIGNED_SIZE(x, y)				(	( ((y) + ((x) - 1)) /(x) ) * (x)	)	
#define SECTOR_ALIGNED_COUNT(x, y)				(	( (y) + ((x) - 1) ) / (x)	)
#define IS_SECTOR_ALIGNED(x, y)					( !((y) % (x)) ) 

#define SECTORSHIFT_512							(9)
#define SECTORSIZE_512							( 1<<(SECTORSHIFT_512))
#define SECTORALIGNADDRESS_512(x)				(( (x) >> (SECTORSHIFT_512)) << (SECTORSHIFT_512))						
#define SECTORCOUNT_512(x)						(((x) + (SECTORSIZE_512 -1)) >> (SECTORSHIFT_512))
#define SECTORALIGNSIZE_512(x)					( SECTORCOUNT_512((x))*SECTORSIZE_512)
#define IS_512_SECTOR(x)						( !((x) % SECTORSIZE_512))



#define SECTORSHIFT_4096						(12)
#define SECTORSIZE_4096							( 1<<(SECTORSHIFT_4096))
#define SECTORALIGNADDRESS_4096(x)				(( (x) >> (SECTORSHIFT_4096)) << (SECTORSHIFT_4096))						
#define SECTORCOUNT_4096(x)						(((x) + (SECTORSIZE_4096 -1)) >> (SECTORSHIFT_4096))
#define SECTORALIGNSIZE_4096(x)					( SECTORCOUNT_4096((x))*SECTORSIZE_4096)
#define IS_4096_SECTOR(x)						( !((x) % SECTORSIZE_4096))



#define XIFSD_MAX_SYSTEM_RESERVED_LOT			(20)

				
#define XIFSD_READ_AHEAD_GRANULARITY			(0x10000)
#define XIFSD_MAX_HOST							(64)




// sector offset for volume record
#define XIFS_OFFSET_VOLUME_LOT_LOC				(0x00000800*(SECTORSIZE_512))
#define XIFS_VOLUME_SIGNATURE					(0x1978197306092102)
#define XIFS_CURRENT_RELEASE_VERSION			(0x0)
#define XIFS_CURRENT_VERSION					(0x1)
#define XIFS_MAX_FILE_NAME_LENGTH				(1000)

#define	XIFS_MAX_NAME_LEN						(2000)
#define XIFS_RESERVED_LOT_SIZE					(24)
#define XIFS_DEFAULT_USER						(8)
#define XIFS_DEFAULT_HOST_LOT_COUNT				(10)
#define	XIFS_MAX_XILOT_NUMBER					(1024*16)

/*
/
/	VOLUME INFORMATION
/		Host Register LotMap Index --> 
/			Information for Host which mount this volume is recorded in this Lot
/				- Check Out Lot Map information
/				- Additional information 
/
*/

//#include <pshpack1.h>
#include <pshpack8.h>
#pragma warning(error: 4820)



#define MAX_VOLUME_LAVEL			(128)
typedef struct _XIDISK_VOLUME_INFO{
	uint64		VolumeSignature;			//8
	uint32		XifsVesion;					//12
	// changed by ILGU HONG
	//		chesung suggest
	//uint64		NumLots;				//20
	//uint32		LotSize;				//24
	
	uint32		LotSize;					// 16
	uint64		NumLots;					// 24
	uint64		HostRegLotMapIndex;			//32
	uint64		RootDirectoryLotIndex;		//40
	uint64		VolCreationTime;			//48
	uint32		VolSerialNumber;			//52
	uint32		VolLabelLength;				//56
	// CHANGED by ILGU HONG
	/*
	uint8		DiskId[6];					//62
	uint8		VolLabel[128];				//190
	uint32		LotSignature ;				//194
	uint8		Reserved[318];				//512
	uint8		Reserved2[3584];			//4096
	*/
	uint8		DiskId[16];					//72
	uint8		VolLabel[128];				//200
	uint32		LotSignature ;				//204
	// changed by ILGU HONG
	//		chesung suggest
	//uint8		Reserved[308];				//512
	uint32		_alignment_1_;				//208
	uint8		Reserved[304];				//512
	uint8		Reserved2[3584];			//4096
}XDISK_VOLUME_INFO, *PXDISK_VOLUME_INFO;

C_ASSERT(sizeof(XDISK_VOLUME_INFO) == 4096);

#define XDISK_VOLUME_INFO_SIZE		(SECTORALIGNSIZE_4096(sizeof(XDISK_VOLUME_INFO))) 


typedef struct _PACKED_BOOT_SECTOR {
    uint8	Jump[3];                                //  offset = 0
    uint8	Oem[8];                                 //  offset = 3
	uint8	reserved;								//	offset = 11
	uint8	reserved1[4];							//	offset = 12
	uint64	VolumeSignature;						//	offset = 16
    uint64  LotNumber;								//  offset = 24
	uint32	LotSignature ;							//	offset = 32
	uint32	XifsVesion;								//	offset = 36
    uint8	reserved2[472];							//	offset = 40
} PACKED_BOOT_SECTOR;                               //  sizeof = 512
typedef PACKED_BOOT_SECTOR *PPACKED_BOOT_SECTOR;
C_ASSERT(sizeof(PACKED_BOOT_SECTOR) == 512);




/*
/
/
/	LOT MAP 
/		Lot --> A unique data struct for xifs system
/		Threre are several Map 
/			--> 1. Used Lot Map (dirty)
/			--> 2. Unused Lot Map (free)
/			--> 3. Check out Lot Map (reserved for host)
/
/
*/
#define		XIFS_MAP_INVALID		(0)
#define		XIFS_MAP_USEDLOT		(1)
#define		XIFS_MAP_UNUSEDLOT		(2)
#define		XIFS_MAP_CHECKOUTLOT	(3)
#define		XIFS_MAP_SYSTEM			(4)
typedef struct _XIDISK_LOT_MAP_INFO{
	uint32		MapType;				//allocated/free/checkout/system	//4
	uint32		BitMapBytes;												//8
	uint64		NumLots;													//16
	uint8		Reserved[496];			//512
	uint8		Reserved2[3584];			//4096
}XIDISK_LOT_MAP_INFO, *PXIDISK_LOT_MAP_INFO;

C_ASSERT(sizeof(XIDISK_LOT_MAP_INFO) == 4096);

#define XIDISK_LOT_MAP_INFO_SIZE		(SECTORALIGNSIZE_4096(sizeof(XIDISK_LOT_MAP_INFO))) 

/*
/
/
/	Host Record	(512 byte aligne)
/		Stored in Host register Lot information
/		Information --> 1. Host Check out Lot Map Index
/						2. Host Used Lot Map Index
/						3. Host Unused Lot Map Index
/
/
*/
#define HOST_UMOUNT		(0x00000000)
#define HOST_MOUNTING	(0x00000001)
#define HOST_MOUNT		(0x00000002)

typedef struct _XIDISK_HOST_RECORD {
	// changed by ILGU HONG
	//	chesung suggest
	/*
	uint32		HostState;						//4
	uint8		HostSignature[16];				//20
	uint64		HostMountTime;					//28
	uint64		HostCheckOutLotMapIndex;		//36
	uint64		HostUsedLotMapIndex;			//44
	uint64		HostUnusedLotMapIndex;			//52
	uint64		LogFileIndex;					//60
	uint8		Reserved[452];					//512
	uint8		Reserved2[3584];				//4096
	*/
	uint32		HostState;						//4
	uint32		_alignment_1_;					//8
	uint8		HostSignature[16];				//24
	uint64		HostMountTime;					//32
	uint64		HostCheckOutLotMapIndex;		//40
	uint64		HostUsedLotMapIndex;			//48
	uint64		HostUnusedLotMapIndex;			//56
	uint64		LogFileIndex;					//64
	uint8		Reserved[448];					//512
	uint8		Reserved2[3584];				//4096
}XIDISK_HOST_RECORD, *PXIDISK_HOST_RECORD;

C_ASSERT(sizeof(XIDISK_HOST_RECORD) == 4096);

#define XIDISK_HOST_RECORD_SIZE		(SECTORALIGNSIZE_4096(sizeof(XIDISK_HOST_RECORD))) 

/*
/
/		Host Info (512 byte aligne)
/			- Header of Host Register Lot
/			- Used Lot Map Index
/			- Unused Lot Map Index
/			- Check out Lot Map Index
/
/
*/
typedef struct _XIDISK_HOST_INFO{
	// changed by ILGU HONG
	//	suggest by chesung
	/*
	uint32		NumHost;						//4
	uint64		UsedLotMapIndex;				//12
	uint64		UnusedLotMapIndex;				//20
	uint64		CheckOutLotMapIndex;			//28
	uint64		LogFileIndex;					//36
	uint8		RegisterMap[16];				//52
	uint8		Reserved[460];					//512
	uint8		Reserved2[3584];				//4096
	*/
	uint32		NumHost;						//4
	uint32		_alignment_1_;					//8
	uint64		UsedLotMapIndex;				//16
	uint64		UnusedLotMapIndex;				//24
	uint64		CheckOutLotMapIndex;			//32
	uint64		LogFileIndex;					//40
	uint8		RegisterMap[16];				//56
	uint8		Reserved[456];					//512
	uint8		Reserved2[3584];				//4096
}XIDISK_HOST_INFO, *PXIDISK_HOST_INFO;

C_ASSERT(sizeof(XIDISK_HOST_INFO) == 4096);

#define XIDISK_HOST_INFO_SIZE		(SECTORALIGNSIZE_4096(sizeof(XIDISK_HOST_INFO))) 

/*
/
/
/	Lot Lock (512 Byte aligne)
/		--> Used file lock / direcotry lock
/
/
*/

#define XIDISK_LOCK_RELEASED			(0x00000000)
#define XIDISK_LOCK_ACQUIRED			(0x00000001)

typedef struct _XIDISK_LOCK {
	// Chagned by ILGU HONG
	/*
	uint32	LockState;					//	4		//file/dir/valid/invalid
	uint8	LockHostSignature[16];		//	20
	uint64	LockAcquireTime;			//	28
	uint8	LockHostMacAddress[6];		//	34			
	uint8	Reserved[478];				//	512	
	*/
	// chesung suggest 
	uint32	LockState;					//	4		//file/dir/valid/invalid
	uint32  _alignment_1_;				//	8
	uint8	LockHostSignature[16];		//	24
	uint64	LockAcquireTime;			//	32
	uint8	LockHostMacAddress[32];		//	64			
	uint8	Reserved[448];				//	512	
}XIDISK_LOCK, *PXIDISK_LOCK;

C_ASSERT(sizeof(XIDISK_LOCK) == 512);

#define XIDISK_LOCK_SIZE		(SECTORALIGNSIZE_512(sizeof(XIDISK_LOCK)))

/*
/
/
/	Lot Info	(512 Byte aligned)
/		Type -->		1.INVALID			(0x00000000)
/					2.VOLUME			(0x00000001)
/					3.FILE				(0x00000002)
/					4.DIRECTORY			(0x00000004)
/					5.HOSTREG			(0x00000008)
/					6.SYSTEM_MAP		(0x00000010)
/					7.BITMAP				(0x00000020)
/
/		Flags -->	1. Invalid
/					1. Begin
/					2. Body
/					3. End
/					4. unique
/
*/
typedef struct _XIDISK_LOT_INFO {
	uint32		Type;						//	4  - dir/file/root/map/host register
	uint32		Flags;						//	8	//begin/end/body
	uint64		LotIndex;					//	16
	uint64		BeginningLotIndex;			//	24
	uint64		PreviousLotIndex;			//	32
	uint64		NextLotIndex;				//	40
	uint64		LogicalStartOffset;			//	48
	uint32		StartDataOffset;			//	52
	uint32		LotTotalDataSize;			//	56
	uint32		LotSignature;				//	60
	// changed by ILGU HONG
	//uint8		Reserved[452];				//	512	
	//	chesung suggest
	uint32		_alignment_1_;				//  64
	uint8		Reserved[448];				//	512
}XIDISK_LOT_INFO, *PXIDISK_LOT_INFO;

C_ASSERT(sizeof(XIDISK_LOT_INFO) == 512);

#define XIDISK_LOT_INFO_SIZE	(SECTORALIGNSIZE_512(sizeof(XIDISK_LOT_INFO)))


/*
	Address Information
		required for random access 
		is designed for (1024*16 entry)

		Address Ma index
		|VALIDITY(1)|Address(63)|
		
*/
#define	XIDISK_ADDRMAP_VALID			(0x8000000000000000)
#define	XIDISK_ADDRMAP_INVALID			(0x0000000000000000)
#define	XIDISK_ADDRMAP_VALIDITY_MASK	(0xF000000000000000)

typedef struct _XIDISK_ADDR_MAP {
	uint64		LotNumber[XIFS_MAX_XILOT_NUMBER];
}XIDISK_ADDR_MAP, *PXIDISK_ADDR_MAP;

C_ASSERT(sizeof(XIDISK_ADDR_MAP) == 131072);

#define	XIDISK_ADDR_MAP_SIZE			(SECTORALIGNSIZE_4096(sizeof(XIDISK_ADDR_MAP)))

#define XIDISK_ADDR_MAP_SEC_512_COUNT	((uint32)(XIDISK_ADDR_MAP_SIZE/SECTORSIZE_512))
#define XIDISK_ADDR_MAP_SEC_4096_COUNT	((uint32)(XIDISK_ADDR_MAP_SIZE/SECTORSIZE_4096))
#define XIDISK_ADDR_MAP_SEC_COUNT(x)	((uint32)(XIDISK_ADDR_MAP_SIZE/	(x)	))


/*
/
/
/	Dir Info	(512 Btye aligned --> 5*5*512)
/		State	1. DELETED
/				2. CREATE
/
/		Type	1.INVALID				(0x00000000)
/				2.FILE					(0x00000010)
/				3.DIRECTORY				(0x00000020)
/				4.ROOT_DIRECTORY		(0x00000040)
/				5.PAGE_FILE				(0x00000080)
/				6.SYSTEM_FILE			(0x00000100)
/				7.PIPE_FILE				(0x00000200)
/				8.DEVICE_FILE			(0x00000400)
/				9.SF_LINK				(0x00000004)
/				10.HD_LINK				(0x00000008)
/
/
/		
/
*/
typedef struct _XIDISK_DIR_INFO {
	// Changed by ILGU HONG
	//	suggested by chesung
	/*
	uint32	DirInfoSignature1;			//	4
	uint32	State;						//	8	
	uint32	Type;						//	12
	uint64 	ParentDirLotIndex;			//	20
	uint64	FileSize;					//	28	
	uint64	AllocationSize;				//	36
	uint32	FileAttribute;				//	40
	uint64	LotIndex;					//	48
	uint64	AddressMapIndex;			//	56
	uint32 	LinkCount;					//	60			
	uint64 	Create_time;				// 	68	
	uint64 	Change_time;				//	76	
	uint64	Modified_time;				// 	84
	uint64 	Access_time;				// 	92
	uint32 	AccessFlags;				// 	96
	uint32	ACLState;					// 	100
	uint8	OwnHostId[16];				//	116
	uint32	NameSize;					//	120
	uint32 	childCount;					//	124	
	uint8	ChildMap[128];				//	252
	uint8	Reserved[256];				// 	508
	uint8 	Name[2048];					//	2556
	uint32	DirInfoSignature2;			//	2560	(512*5)
	uint8	Reserved2[1536];			//	4096	(512*8)
	*/
	uint32	DirInfoSignature1;			//	4
	uint32	State;						//	8	
	uint32	Type;						//	12
	uint32	FileAttribute;				//	16
	uint32 	LinkCount;					//	20
	uint32 	AccessFlags;				// 	24
	uint32	ACLState;					// 	28
	uint32	NameSize;					//	32
	uint64 	ParentDirLotIndex;			//	40
	uint64	FileSize;					//	48	
	uint64	AllocationSize;				//	56
	uint64	LotIndex;					//	64
	uint64	AddressMapIndex;			//	72
	uint64 	Create_time;				// 	80	
	uint64 	Change_time;				//	88	
	uint64	Modified_time;				// 	96
	uint64 	Access_time;				// 	104
	uint8	OwnHostId[16];				//	120
	uint32 	childCount;					//	124
	uint32	_alignment_1_;				//	128
	uint8	ChildMap[128];				//	256
	uint8	Reserved[256];				// 	512
	uint8 	Name[2048];					//	2560	(512*5)
	uint32	DirInfoSignature2;			//	2564
	uint32	_alignment_2_;				//	2568
	uint8	Reserved2[1528];			//	4096	(512*8)
}XIDISK_DIR_INFO, *PXIDISK_DIR_INFO;


C_ASSERT(sizeof(XIDISK_DIR_INFO) == 4096);

#define XIDISK_DIR_INFO_SIZE	(SECTORALIGNSIZE_4096(sizeof(XIDISK_DIR_INFO)))

// Added by ILGU HONG 12082006
/*
/
/	Directroy entry hash value table
/		used for fast searching.
/			- hash value of directory entry string
/
/
*/
#define MAX_DIR_ENTRY		1024
typedef struct _XIDISK_HASH_VALUE_TABLE{
	uint16		EntryHashVal[MAX_DIR_ENTRY];
	uint8		Reserved[2048];
}XIDISK_HASH_VALUE_TABLE, *PXIDISK_HASH_VALUE_TABLE;


C_ASSERT(sizeof(XIDISK_HASH_VALUE_TABLE) == 4096);

#define XIDISK_HASH_VALUE_TABLE_SIZE	(SECTORALIGNSIZE_4096(sizeof(XIDISK_HASH_VALUE_TABLE)))
// Added by ILGU HONG 12082006 END


/*
/
/
/	Directory Link Information	(512 Byte aligned  4*512)
/		--> Used for Dir information
/
/		Type	1.INVALID				(0x00000000)
/				2.FILE					(0x00000010)
/				3.DIRECTORY				(0x00000020)
/				4.ROOT_DIRECTORY		(0x00000040)
/				5.PAGE_FILE				(0x00000080)
/				6.SYSTEM_FILE			(0x00000100)
/				7.PIPE_FILE				(0x00000200)
/				8.DEVICE_FILE			(0x00000400)
/				9.SF_LINK				(0x00000004)
/				10.HD_LINK				(0x00000008)
/
/		State	1. DELETED
/				2. CREATE
/
/
*/
typedef struct _XIDISK_CHILD_INFORMATION{
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	uint32		Childsignature;				//	4
	uint32		Type;						//	8
	uint32		State;						//	12 -delete/create/linked/...
	uint64		StartLotIndex;				//	20
	uint8		HostSignature[16];			//	36
	uint32		NameSize;					//	40
	uint32		ChildIndex;					//	44
	uint8 		Name[2000];					//	2044
	uint32		Childsignature2;			//	2048
	uint8		Reserved[2048];				//	4096
	*/
	uint32		Childsignature;				//	4
	uint32		Type;						//	8
	uint32		State;						//	12 -delete/create/linked/...
	uint32		NameSize;					//	16
	uint64		StartLotIndex;				//	24
	uint8		HostSignature[16];			//	40
	uint32		ChildIndex;					//	44
	uint32		_alignment_1_;				//	48
	uint8 		Name[2000];					//	2048
	uint8		Reserved[2040];				//	4088
	uint32		Childsignature2;			//	4092
	uint32		_alignment_2_;				//	4096
}XIDISK_CHILD_INFORMATION, *PXIDISK_CHILD_INFORMATION;

C_ASSERT(sizeof(XIDISK_CHILD_INFORMATION) == 4096);

#define XIDISK_CHILD_RECORD_SIZE	(SECTORALIGNSIZE_4096(sizeof(XIDISK_CHILD_INFORMATION)))
// CHANGED by ILGU HONG 12082006
//#define GetChildOffset(ChildIndex)		((ChildIndex)*XIDISK_CHILD_RECORD_SIZE)
#define GetChildOffset(ChildIndex)		( ( (ChildIndex) + 1 )*XIDISK_CHILD_RECORD_SIZE )
// CHANGED by ILGU HONG 12082006 END
#define	XILOT		ULONGLONG

/*
/
/	File Info	(512 Btye aligned --> 5*5*512)
/		State	1. DELETED
/				2. CREATE
/
/		Type	1.INVALID				(0x00000000)
/				2.FILE					(0x00000010)
/				3.DIRECTORY				(0x00000020)
/				4.ROOT_DIRECTORY		(0x00000040)
/				5.PAGE_FILE				(0x00000080)
/				6.SYSTEM_FILE			(0x00000100)
/				7.PIPE_FILE				(0x00000200)
/				8.DEVICE_FILE			(0x00000400)
/				9.SF_LINK				(0x00000004)
/				10.HD_LINK				(0x00000008)
/
*/
typedef struct _XIDISK_FILE_INFO {
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	uint32	FileInfoSignature1;			//	4
	uint32	State;						//	8	
	uint32	Type;						//	12
	uint64 	ParentDirLotIndex;			//	20
	uint64	FileSize;					//	28	
	uint64	AllocationSize;				//	36
	uint32	FileAttribute;				//	40
	uint64	LotIndex;					//	48
	uint64	AddressMapIndex;			//	56
	uint32 	LinkCount;					//	60			
	uint64 	Create_time;				// 	68	
	uint64 	Change_time;				//	76	
	uint64	Modified_time;				// 	84
	uint64 	Access_time;				// 	92
	uint32 	AccessFlags;				// 	96
	uint32	ACLState;					// 	100
	uint8	OwnHostId[16];				//	116
	uint32	NameSize;					//	120
	uint8	Reserved[388];				//	508
	uint8	Name[2048];					// 	2556
	uint32	FileInfoSignature2;			//	2560	(512*5)
	uint8	Reserved2[1536];			//	4096	(512*8)
	*/
	uint32	FileInfoSignature1;			//	4
	uint32	State;						//	8	
	uint32	Type;						//	12
	uint32	FileAttribute;				//	16
	uint32 	LinkCount;					//	20
	uint32 	AccessFlags;				// 	24
	uint32	ACLState;					// 	28
	uint32	NameSize;					//	32
	uint64 	ParentDirLotIndex;			//	40
	uint64	FileSize;					//	48	
	uint64	AllocationSize;				//	56
	uint64	LotIndex;					//	64
	uint64	AddressMapIndex;			//	72
	uint64 	Create_time;				// 	80	
	uint64 	Change_time;				//	88	
	uint64	Modified_time;				// 	96
	uint64 	Access_time;				// 	104
	uint8	OwnHostId[16];				//	120
	uint8	Reserved[392];				// 	512
	uint8 	Name[2048];					//	2560	(512*5)
	uint32	FileInfoSignature2;			//	2564
	uint32	_alignment_1_;				//	2568
	uint8	Reserved2[1528];			//	4096	(512*8)
}XIDISK_FILE_INFO, *PXIDISK_FILE_INFO;


C_ASSERT(sizeof(XIDISK_FILE_INFO) == 4096);

#define XIDISK_FILE_INFO_SIZE	(SECTORALIGNSIZE_4096(sizeof(XIDISK_FILE_INFO)))
/*
/
/
/	Xifs file system structure
/
/
*/
typedef struct _XIDISK_COMMON_LOT_HEADER {
	XIDISK_LOCK					Lock;			//	512
	XIDISK_LOT_INFO				LotInfo;		// 	1024
	uint8						Reserved[3072];	//	4096
}XIDISK_COMMON_LOT_HEADER, *PXIDISK_COMMON_LOT_HEADER;

C_ASSERT(sizeof(XIDISK_COMMON_LOT_HEADER) == 4096);

#define XIDISK_COMMON_LOT_HEADER_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_COMMON_LOT_HEADER)))

typedef struct _XIDISK_DIR_HEADER_LOT {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_DIR_INFO				DirInfo;		//	4096*2
}XIDISK_DIR_HEADER_LOT, *PXIDISK_DIR_HEADER_LOT;

C_ASSERT(sizeof(XIDISK_DIR_HEADER_LOT) == 8192);

#define XIDISK_DIR_HEADER_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_DIR_HEADER_LOT)))

typedef struct _XIDISK_DIR_HEADER {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_DIR_INFO 			DirInfo;		//	4096*2
	XIDISK_ADDR_MAP				AddrInfo;		//	16*8*1024 + 4096*2
}XIDISK_DIR_HEADER, *PXIDISK_DIR_HEADER;

C_ASSERT(sizeof(XIDISK_DIR_HEADER) == 139264);

#define XIDISK_DIR_HEADER_SIZE			(SECTORALIGNSIZE_4096(sizeof(XIDISK_DIR_HEADER)))

typedef struct _XIDISK_FILE_HEADER_LOT {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_FILE_INFO			FileInfo;		//	4096
}XIDISK_FILE_HEADER_LOT, *PXIDISK_FILE_HEADER_LOT;

C_ASSERT(sizeof(XIDISK_FILE_HEADER_LOT) == 8192);

#define XIDISK_FILE_HEADER_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_FILE_HEADER_LOT)))

typedef struct _XIDISK_FILE_HEADER {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_FILE_INFO 			FileInfo;		//	4096*2
	XIDISK_ADDR_MAP				AddrInfo;		//	16*8*1024 + 4096*2
}XIDISK_FILE_HEADER, *PXIDISK_FILE_HEADER;

C_ASSERT(sizeof(XIDISK_FILE_HEADER) == 139264);

#define XIDISK_FILE_HEADER_SIZE			(SECTORALIGNSIZE_4096(sizeof(XIDISK_FILE_HEADER)))

typedef struct _XIDISK_DATA_LOT{
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
}XIDISK_DATA_LOT, *PXIDISK_DATA_LOT;

C_ASSERT(sizeof(XIDISK_DATA_LOT) == 4096);

#define XIDISK_DATA_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_DATA_LOT)))

typedef struct _XIDISK_VOLUME_LOT {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XDISK_VOLUME_INFO			VolInfo;		//	4096 * 2		
}XIDISK_VOLUME_LOT, *PXIDISK_VOLUME_LOT;

C_ASSERT(sizeof(XIDISK_VOLUME_LOT) == 8192);

#define XIDISK_VOLUME_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_VOLUME_LOT)))

typedef struct _XIDISK_MAP_LOT {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_LOT_MAP_INFO			Map;			//	4096 * 2
}XIDISK_MAP_LOT, *PXIDISK_MAP_LOT;
#define XIDISK_MAP_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_MAP_LOT)))

typedef struct _XIDISK_HOST_REG_LOT {
	XIDISK_COMMON_LOT_HEADER	LotHeader;		//	4096
	XIDISK_HOST_INFO			HostInfo;		//	4096 * 2
}XIDISK_HOST_REG_LOT, *PXIDISK_HOST_REG_LOT;

C_ASSERT(sizeof(XIDISK_HOST_REG_LOT) == 8192);

#define XIDISK_HOST_REG_LOT_SIZE 	(SECTORALIGNSIZE_4096(sizeof(XIDISK_HOST_REG_LOT)))






#pragma warning(default: 4820)
#include <poppack.h>

#endif //#ifndef __XIXFS_DISKFORM__H__