#ifndef __LFS_TABLE_H__
#define __LFS_TABLE_H__


typedef struct _LFS_TABLE LFS_TABLE, *PLFS_TABLE;;

typedef struct _LFSTAB_ENTRY {

	KSPIN_LOCK						SpinLock;
    LONG							ReferenceCount;
	LIST_ENTRY						LfsTabPartitionEntry;

	BOOLEAN							ActAsPrimary;

	LOCAL_NETDISK_PARTITION_INFO	LocalNetDiskPartitionInfo;

	LPX_ADDRESS						PrimaryAddressList[MAX_SOCKETLPX_INTERFACE];
	ULONG							Flags[MAX_SOCKETLPX_INTERFACE];
	LONG							NumberOfPrimaryAddress;

	PLFS_TABLE						LfsTable;

} LFSTAB_ENTRY, *PLFSTAB_ENTRY;


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
