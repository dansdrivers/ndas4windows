#ifndef __JB_M_MEDIA_DISK_H_
#define __JB_M_MEDIA_DISK_H_
#include <pshpack1.h>
#define METAKEY 0xA1C9D7B30C6EE21A
#define METASTRING "0xA1C9D7B30C6EE21A"
#define DISK_LOCK	0
#define WRITE_LOCK 	2


/****************************************************************
*
*		disk structure
*		# Cluster size Parameter
*		| 8M (G meta) | 16k (Media meta)| ... | 16k   (total 8M)  |
*			Media cluseter   .... Media cluster | Ximeta Information block (2M)|
*
*		G meta
*		dummy DiskMeta Log  DiskMeataMirror LogMirror dummy
*		|1M  |  1M   | 2M |      1M       |   2M    | 1M  |
*
*
*		Media meta(8M)
*		MediaMeta  MediaMetaMirror
*		|  4M    |      4M      |
****************************************************************/
#undef SECTOR_SIZE_BIT
#undef SECTOR_SIZE
#define SECTOR_SIZE_BIT				9
#define SECTOR_SIZE				(1<<SECTOR_SIZE_BIT)

// cluster size 2^28 (256M)
#define MEDIA_CLUSTER_SIZE_BIT			28
#define MEDIA_CLUSTER_SIZE			(1<< MEDIA_CLUSTER_SIZE_BIT)
#define MEDIA_CLUSTER_SIZE_SECTOR_COUNT		(1 << (MEDIA_CLUSTER_SIZE_BIT - SECTOR_SIZE_BIT))

// default cluster count for disc --> 10G
#define DEFAULT_DISC_CLUSTER_COUNT		(10* (1 <<(30-MEDIA_CLUSTER_SIZE_BIT) ) )



// disc info start form 2^23 sector (8M)
#define MEDIA_DISC_INFO_START_ADDR_BIT		23
#define MEDIA_DISC_INFO_START_ADDR_SECTOR	(1 << (MEDIA_DISC_INFO_START_ADDR_BIT - SECTOR_SIZE_BIT))

// 2^23/2^14 --> / 2 = 2^8
// max dic info count  2^8 -->256 and 256 for Mirror  total 2^20  each info 2^4*512 size  totoal count 2^8
#define MEDIA_MAX_DISC_COUNT_BIT		8
#define MEDIA_MAX_DISC_COUNT			( 1 <<  MEDIA_MAX_DISC_COUNT_BIT )
#define MEDIA_MAX_DISC_COUNT_BYTES		( MEDIA_MAX_DISC_COUNT >> 3)




// data cluster start from 2^24 sector(16M)
#define MEDIA_DISC_DATA_START_BIT		24
#define MEDIA_DISC_DATA_START_SECTOR		(1 << ( MEDIA_DISC_DATA_START_BIT - SECTOR_SIZE_BIT))

// disc info size is base on 2^14 (16k) --> 16 sector
#define MEDIA_DISC_INFO_SIZE_BIT		14
#define MEDIA_DISC_INFO_SIZE			(1 <<  MEDIA_DISC_INFO_SIZE_BIT)
#define MEDIA_DISC_INFO_SIZE_SECTOR		(1 << (  MEDIA_DISC_INFO_SIZE_BIT - SECTOR_SIZE_BIT ))

// disk meta total size is base on 2^24 (16M)
#define MEDIA_META_INFO_SIZE_BIT		24
#define MEDIA_META_IFNO_SIZE			 (1 << MEDIA_META_INFO_SIZE_BIT)
#define MEDIA_META_INFO_SECTOR_COUNT		(1 << (MEDIA_META_INFO_SIZE_BIT - SECTOR_SIZE_BIT))

// ximeta DIV size 2^21 (2M)
#define XIMETA_MEDIA_INFO_SIZE_BIT		21
#define XIMETA_MEDIA_INFO_SIZE			(1 << XIMETA_MEDIA_INFO_SIZE_BIT)
#define XIMETA_MEDIA_INFO_SECTOR_COUNT		(1 << (XIMETA_MEDIA_INFO_SIZE_BIT - SECTOR_SIZE_BIT) )


#define MEDIA_DISK_MAGIC			0x19730621
#define MEDIA_DISK_VERSION			0x1


#define MAX_ADDR_ALLOC_COUNT		40
/***************************************************************

	G meta information

typedef struct  _ON_DISK_META
{
	unsigned int	MAGIC_NUMBER;
	unsigned int	VERSION;
	unsigned int	age;
	unsigned int	nr_Enable_disc;
	unsigned int	nr_DiscInfo;
	unsigned int	nr_DiscInfo_byte;
	unsigned int	nr_DiscCluster;
	unsigned int	nr_AvailableCluster;
	unsigned int	nr_DiscCluster_byte;
	unsigned int	RESERVED1[23];
	unsigned int	RESERVED2[16];
	unsigned char	DiscInfo_bitmap[64];
	unsigned int	RESERVED3[16];
	unsigned char FreeCluster_bitmap[64];
	unsigned int	RESERVED4[16];
	unsigned char DirtyCluster_bitmap[64];
}ON_DISK_META, *PON_DISK_META;

****************************************************************/
//	1M
#define DISK_META_START_BIT			20
#define DISK_META_START				(1 << (DISK_META_START_BIT - SECTOR_SIZE_BIT))

//	2M
#define DISK_LOG_START_BIT			21
#define DISK_LOG_START 				(1<< (DISK_LOG_START_BIT - SECTOR_SIZE_BIT))
#define DISK_LOG_DATA_START			(DISK_LOG_START + 1)


//#define SECTOR_PER_LOG				4
//#define DISK_LOG_DATA_SIZE			64

#define DISK_LOG_DATA_SECTOR_COUNT		2

#define DISK_META_READ_SECTOR_COUNT			3
#define DISK_META_WRITE_DIRTY_SECTOR_COUNT  		1
#define DISK_META_WRITE_FREE_SECTOR_COUNT 		1
#define DISK_META_HEAD_WRITE_SECTOR_COUNT   		1
#define DISK_META_WRITE_SECTOR_COUNT			3


#define DISK_LOG_HEAD_READ_SECTOR_COUNT			1 
#define DISK_LOG_HEAD_WRITE_SECTOR_COUNT		1

#define DISK_LOG_DATA_READ_SECTOR_COUNT			2
#define DISK_LOG_DATA_WRITE_SECTOR_COUNT		2

// 	4M
#define DISK_META_MIRROR_START_BIT		22
#define DISK_META_MIRROR_START			(1<< (DISK_META_MIRROR_START_BIT - SECTOR_SIZE_BIT))

//	5M
#define DISK_LOG_MIRROR_START			( DISK_META_MIRROR_START + (1<<(20-SECTOR_SIZE_BIT)) )

#define DISK_INFO_VALID				0x00000001
#define DISK_INFO_INVALID			0x00000002

typedef struct _DISK_INFO
{
	unsigned int start_disc_num;
	unsigned int disc_count;
	unsigned int status;
}DISK_INFO, *PDISK_INFO;

/*
typedef struct _BITEMAP
{
	unsigned char bitmap[64];
}BITEMAP , *PBITEMAP;
*/
typedef unsigned char BITEMAP;
typedef unsigned char * PBITEMAP;


typedef struct  DISK_META
{
	unsigned int	MAGIC_NUMBER;					
	unsigned int	VERSION;								// offset 4
	unsigned int	age;										// offset 8
	unsigned int	nr_Enable_disc;							// offset 12
	unsigned int	nr_DiscInfo;								// offset 16
	unsigned int	nr_DiscInfo_byte;							// offset 20
	unsigned int	nr_DiscCluster;							// offset 24
	unsigned int	nr_AvailableCluster;						// offset 28
	unsigned int	nr_DiscCluster_byte;						// offset 32
}DISK_META, *PDISK_META;


typedef struct  _ON_DISK_FREE
{
	BITEMAP	FreeCluster_bitmap[512];
}ON_DISK_FREE, * PON_DISK_FREE;



typedef struct  _ON_DISK_DIRTY
{
	BITEMAP	DirtyCluster_bitmap[512];
}ON_DISK_DIRTY, * PON_DISK_DIRTY;


typedef struct  _ON_DISK_META
{
	unsigned int	MAGIC_NUMBER;					
	unsigned int	VERSION;				// offset 4
	unsigned int	age;					// offset 8
	unsigned int	nr_Enable_disc;				// offset 12
	unsigned int	nr_DiscInfo;				// offset 16
	unsigned int	nr_DiscInfo_byte;			// offset 20
	unsigned int	nr_DiscCluster;				// offset 24
	unsigned int	nr_AvailableCluster;			// offset 28
	unsigned int	nr_DiscCluster_byte;			// offset 32
	unsigned int	RESERVED1[23];				// offset 36
	unsigned int	RESERVED2[32];				// offset 128
	BITEMAP		DiscInfo_bitmap[256];			// offset 256
	BITEMAP		FreeCluster_bitmap[512];		// offset 512
	BITEMAP		DirtyCluster_bitmap[512];		// offset 1024
}ON_DISK_META, *PON_DISK_META;


#define ACTION_DEFAULT					0x00000000
#define	ACTION_WS1_DISKFREE_DISCINFO_START		0x00000001
#define	ACTION_WS1_DISKFREE_DISCINFO_END		0x00000002			// write step 1 complete
#define ACTION_WS2_DISKDIRTY_START			0x00000010
#define ACTION_WS2_DISKDIRTY_END			0x00000000			// vitual action																				
#define ACTION_DS1_DISCENTRY_START			0x00000100
#define	ACTION_DS1_DISCENTRY_END			0x00000000			// vitual action				
#define	ACTION_DS2_REAL_ERASE_START			0x00000400
#define ACTION_DS2_REAL_ERASE_END			0x00000000

#define ACTION_UP_DISK_META				0x00001000
#define ACTION_UP_RECOVERY				0x10000000

#define ACTION_ERROR_MASK				0x10001511

#define	ACTION_UP_DISK_MASK				0x10000000
#define ACTION_WRITE_STEP_MASK				0x00000033
#define ACTION_WRITE_STEP1_MASK				0x00000003
#define ACTION_WRITE_STEP2_MASK				0x00000030
#define ACTION_DELETE_MASK				0x00000500



#define DISK_LOG_VALID					0x00000000
#define DISK_LOG_INVALID				0x00000001




#define LOG_ACT_NON					0x00000000
#define LOG_ACT_WRITE_META_ALLOC_S			0x00000001
#define LOG_ACT_WRITE_META_ALLOC_E			0x00000002
#define LOG_ACT_WRITE_META_END_S			0x00000004
#define LOG_ACT_WRITE_META_END_E			0x00000008
#define LOG_ACT_DELET_META_S				0x00000010
#define LOG_ACT_DELET_META_E				0x00000020
#define LOG_ACT_RECOVERY_S				0x00000100
#define LOG_ACT_RECOVERY_E				0x00000200
typedef struct _DISC_HISTORY
{
	unsigned int 	hostid;				
	unsigned int  	action;
}DISC_HISTORY, *PDISC_HISTORY;

typedef struct _DISC_HOST_STATUS
{
	unsigned short IsSet;
	unsigned int hostid;
	unsigned short refcount;
}DISC_HOST_STATUS, *PDISC_HOST_STATUS;

#define MAX_DISC_LOG_HISTORY				48
#define HOST_HASH_VALUE					61
typedef struct _ON_DISK_LOG
{
	unsigned int 	valid;					
	unsigned int 	action;				// offset 4
	unsigned int 	prevActionStatus;		// offset 8
	unsigned int 	hostId;				// offset 12				
	unsigned __int64 time;				// offset 16
	unsigned int	 age;				// offset 24
	unsigned int 	latest_disc_history;		// offset 28
	unsigned int	refcount;			// offset 32
	unsigned int	validcount;			// offset 36
	unsigned int 	RESERVED;			// offset 40
	unsigned int	nr_DiscCluster;			// offset 44
	unsigned short	Addr[40];			// offset 48
	DISC_HISTORY	History[48];			// offset 128
	DISC_HOST_STATUS	Host[61];		// offset 512
	unsigned int	RESERVED2[6];			// offset 1000
}ON_DISK_LOG, *PON_DISK_LOG;


#define	ACTION_STATUS_DEFAULT				0x00000000
#define ACTION_STATUS_WRITE				0x00000001
#define	ACTION_STATUS_DELETE				0x00000002	
#define	ACTION_STATUS_RECOVERY				0x00000003

typedef struct _DISK_HISTORY
{
	unsigned int 	diskId;				
	unsigned int  	actionstatus;
}DISK_HISTORY, *PDISK_HISTORY;


#define MAX_LOG_HISTORY					61
typedef struct _ON_DISK_LOG_HEADER
{
	unsigned int latest_age;		// offset	0  // currange age // must be 2X		
	unsigned int latest_log_action;		// offset	4  // latest action
	unsigned int latest_log_history;	// offset 	8  // history index
	unsigned int latest_index;		// offset 	12 // latset disc index
	unsigned int RESERVED[2];		// offset 	16
	DISK_HISTORY	history[61];		// offset 	24
}ON_DISK_LOG_HEADER, *PON_DISK_LOG_HEADER;


/***************************************************************
	Media meta information

typedef struct _ON_DISC_ENTRY {
	unsigned int		MAGIC_NUMBER;
	unsigned int		index;
	unsigned int		loc;
	unsigned int		status;
	unsigned int		age;
	unsigned int		refcount;
	unsigned int		action;
	unsigned _int64	time;
	unsigned int		nr_DiscSector;
	unsigned int		nr_DiscCluster;
	unsigned int		nr_DiscCluster_bimap;
	unsigned int		pt_log;
	unsigned int		pt_title;
	unsigned int		pt_information;
	unsigned int		pt_key;
	unsigned char		RESERVED1[1];
	unsigned char 	RESERVED2[16];
	unsigned char		RESERVED3[16];
	unsigned char 	RESERVED4[16];
	unsigned char		RESERVED5[16];
	unsigned char		RESERVED6[16];
	unsigned char		RESERVED7[16];
	unsigned char		DiscCluster_bitmap[64];
}ON_DISC_ENTRY, *PON_DISC_ENTRY;

****************************************************************/
#define DISC_META_START				0
#define DISC_TITLE_START			1
#define DISC_ADD_INFO_START			2
#define DISC_KEY_START				3



#define DISC_META_SIZE_BIT			12
#define DISC_META_SIZE				(1 << DISC_META_SIZE_BIT)
#define DISC_META_SECTOR_COUNT			(1 << (DISC_META_SIZE_BIT - SECTOR_SIZE_BIT))		
#define DISC_META_HEAD_SECTOR_COUNT		1

#define DISC_META_MIRROR_START_SHIFT_BIT	22
#define DISC_META_MIRROR_SHIFT_SECTOR_BIT	( DISC_META_MIRROR_START_SHIFT_BIT - SECTOR_SIZE_BIT )
#define DISC_META_MIRROR_START			( 1 << DISC_META_MIRROR_SHIFT_SECTOR_BIT )




//#define MAX_DISC_COUNT				128
#define MAX_ADDR_ENTRIES			128

#define DISC_LIST_ASSIGNED			0x00010000
#define DISC_LIST_EMPTY				0x00020000
#define DISC_LIST_WRITING			0x00040000

#define	MAX_DISC_LIST				200
typedef struct _DISC_LIST
{
	unsigned int	valid;
	unsigned int	pt_loc;
	unsigned int	sector_count;
	unsigned short	encrypt;
	short	minor;
}DISC_LIST, *PDISC_LIST;


#define DISC_STATUS_ERASING			0x00000001
#define DISC_STATUS_WRITING			0x00000002
#define DISC_STATUS_VALID			0x00000004
#define DISC_STATUS_VALID_END			0x00000008
#define DISC_STATUS_INVALID			0x00000000



typedef struct _DISC_ENTRY {
	unsigned int		MAGIC_NUMBER;
	unsigned int		index;				// offset 4
	unsigned int		loc;				// offset 8
	unsigned int		status;				// offset 12
	unsigned int		age;				// offset 16
	unsigned int		RESERVED;			// offset 20
	unsigned int		action;				// offset 24
	unsigned __int64	time;					// offset 28
	unsigned int		nr_DiscSector;			// offset 36
	unsigned int		nr_DiscCluster;			// offset 40
	unsigned int		nr_DiscCluster_bitmap;		// offset 44
	unsigned int		pt_title;			// offset 48
	unsigned int		pt_information;			// offset 52
	unsigned int		pt_key;				// offset 56
	unsigned int		encrypt;			// offset 60
	unsigned char		HINT[32];			// offset 64
}DISC_ENTRY, *PDISC_ENTRY;



typedef struct _ON_DISC_ENTRY {
	unsigned int		MAGIC_NUMBER;
	unsigned int		index;					// offset 4
	unsigned int		loc;					// offset 8
	unsigned int		status;					// offset 12
	unsigned int		age;					// offset 16
	unsigned int		RESERVED;				// offset 20
	unsigned int		action;					// offset 24
	unsigned __int64	time;						// offset 28
	unsigned int		nr_DiscSector;				// offset 36
	unsigned int		nr_DiscCluster;				// offset 40
	unsigned int		nr_DiscCluster_bitmap;			// offset 44
	unsigned int		pt_title;				// offset 48
	unsigned int		pt_information;				// offset 52
	unsigned int		pt_key;					// offset 56
	unsigned int		encrypt;				// offset 60
	unsigned char		HINT[32];				// offset 64
	unsigned int 		RESERVED2[8];				// offset 96
	unsigned int		RESERVED3[16];				// offset 128
	unsigned int 		RESERVED4[16];				// offset 192
	unsigned int		RESERVED5[16];				// offset 256
	unsigned int		RESERVED6[16];				// offset 320
	unsigned int		RESERVED7[12];				// offset 384
	unsigned short		Addr[40];				// offset 432
}ON_DISC_ENTRY, *PON_DISC_ENTRY;

typedef struct _DISC_MAP_LIST
{
	unsigned int	StartSector;
	unsigned int	nr_SecCount;
	unsigned int	Lg_StartSector;
}DISC_MAP_LIST, *PDISC_MAP_LIST;
#include <poppack.h>
/********************************************************************
	
	Adapter management structure
typedef struct _DISC_INFO
{
	int disk_id;
	int	disc_id;
	unsigned int status;
}DISC_INFO, *PDISC_INFO;

*********************************************************************/



#define MAX_META_BUFFER				(8*SECTOR_SIZE)


#endif // #ifndef __JB_M_MEDIA_DISK_H_
