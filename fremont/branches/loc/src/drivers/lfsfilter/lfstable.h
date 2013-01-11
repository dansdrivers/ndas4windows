#ifndef __LFS_TABLE_H__
#define __LFS_TABLE_H__


typedef struct _LFS_TABLE LFS_TABLE, *PLFS_TABLE;;

#define LFSTABENTRY_FLAG_ACTPRIMARY				0x0000001
#define LFSTABENTRY_FLAG_VALIDPRIMADDRESS		0x0000002

typedef struct _LFSTAB_ENTRY {

	KSPIN_LOCK						SpinLock;
    LONG							ReferenceCount;
	LIST_ENTRY						LfsTabPartitionEntry ;
	ULONG							Flags ;

	LOCAL_NETDISK_PARTITION_INFO	LocalNetDiskPartitionInfo;
	LPX_ADDRESS						PrimaryAddress;

	PLFS_TABLE						LfsTable ;

}	LFSTAB_ENTRY, *PLFSTAB_ENTRY ;


struct _LFS_TABLE
{
	KSPIN_LOCK			SpinLock;
    LONG				ReferenceCount;
	LONG				EntryCount;

	LIST_ENTRY			LfsTabPartitionList ;

	PLFS				Lfs;
} ;


VOID
LfsTable_UpdatePrimaryInfo(
	IN PLFS_TABLE		LfsTable,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN UCHAR			UnitDiskNo,
	IN PLPX_ADDRESS		PrimaryAddress,
	IN UINT8				*NdscId
	);


#endif
