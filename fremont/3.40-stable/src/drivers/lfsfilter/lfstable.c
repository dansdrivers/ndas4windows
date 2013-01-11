#include "LfsProc.h"


static
BOOLEAN
LfsTable_Lookup (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN  BOOLEAN					VaildOnly,
	OUT	PLFSTAB_ENTRY			*LfsTabEntry
	); 

static
BOOLEAN
LfsTable_LookupEx (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN  BOOLEAN					Primary,
	OUT	PLFSTAB_ENTRY			*LfsTabEntry
	);

static
NTSTATUS
LfsTable_CreateEntry(
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN PLPX_ADDRESS			BindAddress,
	IN BOOLEAN				Primary,
	OUT PLFSTAB_ENTRY		*Entry
	) ;

static
VOID
LfsTable_ReferenceEntry (
		IN PLFSTAB_ENTRY	LfsTableEntry
	) ;

static
VOID
LfsTable_DereferenceEntry (
		IN PLFSTAB_ENTRY	LfsTableEntry
	) ;


PLFS_TABLE
LfsTable_Create(
	IN PLFS	Lfs
	)
{
	PLFS_TABLE	lfsTable;


	LfsReference (Lfs);

	lfsTable = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(LFS_TABLE),
						LFS_ALLOC_TAG
						);
	
	if (lfsTable == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		LfsDereference (Lfs);
		return NULL;
	}

	RtlZeroMemory(
		lfsTable,
		sizeof(LFS_TABLE)
		);

	KeInitializeSpinLock(&lfsTable->SpinLock);
	InitializeListHead(&lfsTable->LfsTabPartitionList) ;
	lfsTable->ReferenceCount = 1;
	lfsTable->Lfs = Lfs;
	

	
	return lfsTable;
}


VOID
LfsTable_Close (
	IN PLFS_TABLE	LfsTable
	)
{
	LfsTable_Dereference(LfsTable);

	return;
}


VOID
LfsTable_Reference (
	IN PLFS_TABLE	LfsTable
	)
{
    LONG result;

    result = InterlockedIncrement (&LfsTable->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
LfsTable_Dereference (
	IN PLFS_TABLE	LfsTable
	)
{
    LONG result;


    result = InterlockedDecrement (&LfsTable->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		ASSERT(LfsTable->EntryCount == 0);

		LfsDereference (LfsTable->Lfs);

		ExFreePoolWithTag(
			LfsTable,
			LFS_ALLOC_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
				("LfsTable_Dereference: Lfs Table is Freed\n"));
	}
}


VOID
LfsTable_InsertNetDiskPartitionInfoUser (
	IN PLFS_TABLE				LfsTable,
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN PLPX_ADDRESS				BindAddress,
	BOOLEAN						Primary
	) 
{
	NTSTATUS		ntStatus;
	PLFSTAB_ENTRY	Entry;

	BOOLEAN			found;
	PLFSTAB_ENTRY	entry;

	found = LfsTable_LookupEx( LfsTable, NetDiskPartitionInfo, Primary, &entry );

	if (found == TRUE) {

		NDAS_ASSERT(FALSE);

		LfsTable_DereferenceEntry (entry);

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_ERROR, ("InsertNetDiskPartitionInfoUser: Found=%u\n", found) );
	}

	ntStatus = LfsTable_CreateEntry( NetDiskPartitionInfo, BindAddress, Primary, &Entry );

	if (!NT_SUCCESS(ntStatus)) {

		NDAS_ASSERT(FALSE);
		return;
	}

	Entry->LfsTable = LfsTable;

	ExInterlockedInsertHeadList( &LfsTable->LfsTabPartitionList, &Entry->LfsTabPartitionEntry, &LfsTable->SpinLock );

	InterlockedIncrement( &LfsTable->EntryCount );

	LfsTable_Reference( LfsTable );

	SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LfsTable_InsertNetDiskPartitionInfoUser: inserted a Partition entry.\n") );
}


VOID
LfsTable_DeleteNetDiskPartitionInfoUser (
	IN PLFS_TABLE				LfsTable,
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN BOOLEAN					Primary
	) 
{
	PLFSTAB_ENTRY	entry;
	BOOLEAN			Found;

	Found = LfsTable_LookupEx( LfsTable, NetDiskPartitionInfo, Primary, &entry );

	if (Found) {

		LfsTable_DereferenceEntry(entry);
		LfsTable_DereferenceEntry(entry);

		entry = NULL;
	
	} else {

		NDAS_ASSERT(FALSE);

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_ERROR,
					   ("LFS: LfsTable_DeleteNetDiskPartitionInfoUser: Could not find NetDisk:%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
						NetDiskPartitionInfo->NetdiskAddress.Node[0],
						NetDiskPartitionInfo->NetdiskAddress.Node[1],
						NetDiskPartitionInfo->NetdiskAddress.Node[2],
						NetDiskPartitionInfo->NetdiskAddress.Node[3],
						NetDiskPartitionInfo->NetdiskAddress.Node[4],
						NetDiskPartitionInfo->NetdiskAddress.Node[5],
						NTOHS(NetDiskPartitionInfo->NetdiskAddress.Port)) );
	}
}


VOID
LfsTable_CleanCachePrimaryAddress (
	IN PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo
	)
{
	PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	KIRQL			oldIrql2;	
	PLFSTAB_ENTRY	entry;
	ULONG			foundCnt;

	foundCnt = 0;

	KeAcquireSpinLock( &LfsTable->SpinLock, &oldIrql );

	listEntry = LfsTable->LfsTabPartitionList.Flink;

	while (listEntry != &LfsTable->LfsTabPartitionList) {

		entry = CONTAINING_RECORD( listEntry, LFSTAB_ENTRY, LfsTabPartitionEntry );

		listEntry = listEntry->Flink;

		if (!RtlEqualMemory(&entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress,
							&NetDiskPartitionInfo->NetdiskAddress, 
							sizeof(LPX_ADDRESS))) {
								
			continue;
		}
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo != NetDiskPartitionInfo->UnitDiskNo) {
			
			continue;
		}
		
		if (!RtlEqualMemory(entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NdscId,
							NetDiskPartitionInfo->NdscId,
							NDSC_ID_LENGTH)) {
								
			continue;
		}
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.StartingOffset.QuadPart != NetDiskPartitionInfo->StartingOffset.QuadPart) {	//	Partition Starting Byte Offset
		
			continue;
		}

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_CleanCachePrimaryAddress: Found\n") );

		KeAcquireSpinLock( &entry->SpinLock, &oldIrql2 );
		entry->NumberOfPrimaryAddress = 0;
		KeReleaseSpinLock( &entry->SpinLock, oldIrql2 );

		foundCnt++;
	}

	KeReleaseSpinLock( &LfsTable->SpinLock, oldIrql );

	NDAS_ASSERT( foundCnt <= 2 );
#if DBG
	SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,("CleanCachePrimaryAddress: FoundCnt=%u\n", foundCnt) );
#endif

	return;	
}

#define QUERYPRIMARYADDRESS_MAX_TRIAL		2
#define	QUERYPRIMARYADDRESS_DELAY_INTERVAL	(1 * NANO100_PER_SEC)

#if 0

NTSTATUS
LfsTable_QueryPrimaryAddress (
	IN PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	OUT PLPX_ADDRESS			PrimaryAddress
	) 
{
	NTSTATUS		ntStatus = STATUS_SUCCESS;
	PLFSTAB_ENTRY	entry;
	BOOLEAN			Found;
	KIRQL			oldIrql;
	LONG			Trial;
	LARGE_INTEGER	Interval;

	NDAS_ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );
	NDAS_ASSERT( PrimaryAddress );

	Trial = 0;

	while (Trial < QUERYPRIMARYADDRESS_MAX_TRIAL) {

		Interval.QuadPart = -QUERYPRIMARYADDRESS_DELAY_INTERVAL;
		
		ntStatus = KeDelayExecutionThread( KernelMode, TRUE, &Interval );

		if (!NT_SUCCESS(ntStatus)) {
		
			SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_QueryPrimaryAddress: KeDelayExecutionThread() failed.\n") );
		}

		Found = LfsTable_Lookup( LfsTable, NetDiskPartitionInfo, FALSE, &entry );

		if (Found) {

			KeAcquireSpinLock( &entry->SpinLock, &oldIrql );

			if (entry->Flags & LFSTABENTRY_FLAG_VALIDPRIMADDRESS) {

				RtlCopyMemory( PrimaryAddress, &entry->PrimaryAddress, sizeof(LPX_ADDRESS) );
				
				entry->Flags &= ~LFSTABENTRY_FLAG_VALIDPRIMADDRESS;
				
				KeReleaseSpinLock( &entry->SpinLock, oldIrql );

				LfsTable_DereferenceEntry(entry);

				break;

			} else {
				
				KeReleaseSpinLock( &entry->SpinLock, oldIrql );

				LfsTable_DereferenceEntry(entry);
			}
		}

		Found = FALSE;
		Trial ++;

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_QueryPrimaryAddress: Retry #%d\n", Trial) );
	}

	if (!Found) {
	
		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_QueryPrimaryAddress: Could not resolve the primary address.\n") );
		ntStatus = STATUS_NO_SUCH_DEVICE;
	}

	return ntStatus;
}

#endif

NTSTATUS
LfsTable_QueryPrimaryAddressList (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	OUT PLPX_ADDRESS			PrimaryAddressList,
	OUT PLONG					NumberOfPrimaryAddress
	)
{
	PLFSTAB_ENTRY	entry;
	KIRQL			oldIrql;
	LONG			trial;
	LARGE_INTEGER	interval;
	BOOLEAN			found;

	NDAS_ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	NDAS_ASSERT( PrimaryAddressList && NumberOfPrimaryAddress );

	*NumberOfPrimaryAddress = 0;

	trial = 0;

	while (trial < QUERYPRIMARYADDRESS_MAX_TRIAL) {

		NTSTATUS status;

		interval.QuadPart = -QUERYPRIMARYADDRESS_DELAY_INTERVAL;
		
		status = KeDelayExecutionThread( KernelMode, TRUE, &interval );

		if (!NT_SUCCESS(status)) {
		
			NDAS_ASSERT(FALSE);
		}

		found = LfsTable_Lookup( LfsTable, NetDiskPartitionInfo, TRUE, &entry );

		if (found) {

			KeAcquireSpinLock( &entry->SpinLock, &oldIrql );

			NDAS_ASSERT( entry->NumberOfPrimaryAddress );

			RtlCopyMemory( PrimaryAddressList, entry->PrimaryAddressList, sizeof(LPX_ADDRESS) * entry->NumberOfPrimaryAddress );	
			*NumberOfPrimaryAddress = entry->NumberOfPrimaryAddress;
				
			KeReleaseSpinLock( &entry->SpinLock, oldIrql );

			LfsTable_DereferenceEntry(entry);
		
			break;
		}  

		trial ++;

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_QueryPrimaryAddress: Retry #%d\n", trial) );
	}

	if (found == FALSE) {
	
		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, ("LFS: LfsTable_QueryPrimaryAddress: Could not resolve the primary address\n") );
		return STATUS_NO_SUCH_DEVICE;
	}

	return STATUS_SUCCESS;
}

VOID
LfsTable_UpdatePrimaryInfo (
	IN PLFS_TABLE		LfsTable,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN UCHAR			UnitDiskNo,
	IN PLPX_ADDRESS		PrimaryAddress,
	IN UINT8			*NdscId
	) 
{
	PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	KIRQL			oldIrql2;
	PLFSTAB_ENTRY	entry;

	LONG			i;

	KeAcquireSpinLock( &LfsTable->SpinLock, &oldIrql );

	listEntry = LfsTable->LfsTabPartitionList.Flink ;
	
	while (listEntry != &LfsTable->LfsTabPartitionList) {

		entry = CONTAINING_RECORD(listEntry, LFSTAB_ENTRY, LfsTabPartitionEntry);
		
		listEntry = listEntry->Flink;

		if (!RtlEqualMemory(&entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node,
							NetDiskAddress->Node, 
							LPXADDR_NODE_LENGTH)) {
								  
			continue;
		}
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Port != NetDiskAddress->Port) {
			
			continue;
		}	
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo != UnitDiskNo) {
			
			continue;
		}
			
		if (!RtlEqualMemory( entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NdscId,
							 NdscId,
							 NDSC_ID_LENGTH)) {

			continue;
		}

		for (i = 0; i < entry->NumberOfPrimaryAddress; i++) {

			if (RtlEqualMemory(&entry->PrimaryAddressList[i].Node, PrimaryAddress->Node, LPXADDR_NODE_LENGTH) &&
				entry->PrimaryAddressList[i].Port == PrimaryAddress->Port) {

				break;
			}
		}

		if (i != entry->NumberOfPrimaryAddress) {

			continue;
		}

		if (entry->NumberOfPrimaryAddress == MAX_SOCKETLPX_INTERFACE) {

			NDAS_ASSERT(FALSE);
			continue;
		}

		KeAcquireSpinLock( &entry->SpinLock, &oldIrql2 );

		RtlCopyMemory( &entry->PrimaryAddressList[entry->NumberOfPrimaryAddress], PrimaryAddress, sizeof(LPX_ADDRESS) );
		entry->NumberOfPrimaryAddress ++;
		
		KeReleaseSpinLock( &entry->SpinLock, oldIrql2 );

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO, 
					   ("[LFS] LfsTable_UpdatePrimaryInfo: : %02x:%02x:%02x:%02x:%02x:%02x/%d.\n",
						PrimaryAddress->Node[0], PrimaryAddress->Node[1], PrimaryAddress->Node[2],
						PrimaryAddress->Node[3], PrimaryAddress->Node[4], PrimaryAddress->Node[5],
						NTOHS(PrimaryAddress->Port)) );

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
					   ("LFS: LfsTable_UpdatePrimaryInfo: entry = %p entry->NumberOfPrimaryAddress = %d, NetDisk:%02x:%02x:%02x:%02x:%02x:%02x/%d UnitDisk:%d\n",
					    entry, entry->NumberOfPrimaryAddress,
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[0],
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[1],
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[2],
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[3],
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[4],
						entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Node[5],
						NTOHS(entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress.Port),
						(int)entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo) );
	}

	KeReleaseSpinLock( &LfsTable->SpinLock, oldIrql );
}


//////////////////////////////////////////////////////////////////////////
//
//	Static functions.
//

static
BOOLEAN
LfsTable_LookupEx (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN  BOOLEAN					Primary,
	OUT	PLFSTAB_ENTRY			*LfsTabEntry
	) 
{
	PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	PLFSTAB_ENTRY	entry;
	BOOLEAN			found;

	found = FALSE;

	KeAcquireSpinLock( &LfsTable->SpinLock, &oldIrql );
	
	listEntry = LfsTable->LfsTabPartitionList.Flink;
	
	while (listEntry != &LfsTable->LfsTabPartitionList) {

		entry = CONTAINING_RECORD( listEntry, LFSTAB_ENTRY, LfsTabPartitionEntry );
		listEntry = listEntry->Flink;
		
		if (!RtlEqualMemory(&entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress,
							&NetDiskPartitionInfo->NetdiskAddress, 
							sizeof(LPX_ADDRESS))) {
			
			continue;
		}
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo != NetDiskPartitionInfo->UnitDiskNo) {
			
			continue;
		}
		
		if (!RtlEqualMemory(entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NdscId, NetDiskPartitionInfo->NdscId, NDSC_ID_LENGTH)) {
			
			continue;
		}

		if (NetDiskPartitionInfo->StartingOffset.QuadPart && 
			entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.StartingOffset.QuadPart != NetDiskPartitionInfo->StartingOffset.QuadPart) {

			continue;
		}

		if (entry->ActAsPrimary != Primary) {

			continue;
		}

		LfsTable_ReferenceEntry(entry);

		*LfsTabEntry = entry;
		found = TRUE;

		break;
	}

	KeReleaseSpinLock( &LfsTable->SpinLock, oldIrql );

#if DBG

	if (!found) {
	
		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
					  ("LFS: LfsTable_Lookup: updated the primary address of  NetDisk:%02x:%02x:%02x:%02x:%02x:%02x/%d UnitDisk:%d\n",
					   NetDiskPartitionInfo->NetdiskAddress.Node[0], NetDiskPartitionInfo->NetdiskAddress.Node[1],
					   NetDiskPartitionInfo->NetdiskAddress.Node[2], NetDiskPartitionInfo->NetdiskAddress.Node[3],
					   NetDiskPartitionInfo->NetdiskAddress.Node[4], NetDiskPartitionInfo->NetdiskAddress.Node[5],
					   NTOHS(NetDiskPartitionInfo->NetdiskAddress.Port), (int)NetDiskPartitionInfo->UnitDiskNo) );
	}

#endif

	return found;
}

static
BOOLEAN
LfsTable_Lookup (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN  BOOLEAN					VaildOnly,
	OUT	PLFSTAB_ENTRY			*LfsTabEntry
	) 
{
	PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	PLFSTAB_ENTRY	entry;
	BOOLEAN			found;

	found = FALSE;

	KeAcquireSpinLock( &LfsTable->SpinLock, &oldIrql );
	
	listEntry = LfsTable->LfsTabPartitionList.Flink;
	
	while (listEntry != &LfsTable->LfsTabPartitionList) {

		entry = CONTAINING_RECORD( listEntry, LFSTAB_ENTRY, LfsTabPartitionEntry );
		listEntry = listEntry->Flink;
		
		if (!RtlEqualMemory(&entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetdiskAddress,
							&NetDiskPartitionInfo->NetdiskAddress, 
							sizeof(LPX_ADDRESS))) {
			
			continue;
		}
		
		if (entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo != NetDiskPartitionInfo->UnitDiskNo) {
			
			continue;
		}
		
		if (!RtlEqualMemory(entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NdscId, NetDiskPartitionInfo->NdscId, NDSC_ID_LENGTH)) {
			
			continue;
		}

		if (NetDiskPartitionInfo->StartingOffset.QuadPart && 
			entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.StartingOffset.QuadPart != NetDiskPartitionInfo->StartingOffset.QuadPart) {

			continue;
		}

		if (VaildOnly && entry->NumberOfPrimaryAddress == 0) {

			continue;
		}

		LfsTable_ReferenceEntry(entry);

		*LfsTabEntry = entry;
		found = TRUE;

		break;
	}

	KeReleaseSpinLock( &LfsTable->SpinLock, oldIrql );

#if DBG

	if (!found) {
	
		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
					  ("LFS: LfsTable_Lookup: updated the primary address of  NetDisk:%02x:%02x:%02x:%02x:%02x:%02x/%d UnitDisk:%d\n",
					   NetDiskPartitionInfo->NetdiskAddress.Node[0], NetDiskPartitionInfo->NetdiskAddress.Node[1],
					   NetDiskPartitionInfo->NetdiskAddress.Node[2], NetDiskPartitionInfo->NetdiskAddress.Node[3],
					   NetDiskPartitionInfo->NetdiskAddress.Node[4], NetDiskPartitionInfo->NetdiskAddress.Node[5],
					   NTOHS(NetDiskPartitionInfo->NetdiskAddress.Port), (int)NetDiskPartitionInfo->UnitDiskNo) );
	}

#endif

	return found;
}

static
NTSTATUS
LfsTable_CreateEntry(
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN PLPX_ADDRESS			BindAddress,
	IN BOOLEAN				Primary,
	OUT PLFSTAB_ENTRY		*Entry
	) {
	NTSTATUS		ntStatus = STATUS_SUCCESS ;
	PLFSTAB_ENTRY	entry ;

	entry = (PLFSTAB_ENTRY)ExAllocatePoolWithTag(NonPagedPool, sizeof(LFSTAB_ENTRY), LFSTAB_ENTRY_TAG) ;
	if(!entry) {

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
				("LfsTable_CreateEntry: ExAllocatePoolWithTag() failed.\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES ;
		goto cleanup ;
	}

	//
	// Initialize a table entry
	//	NOTE: we do not use the following fileds for the table entry
	//				BindAddress, DesiredAccess, GrantedAccess, MessageSecurity, RwDataSecurity, SlotNo

	RtlZeroMemory(entry, sizeof(LFSTAB_ENTRY));

	entry->ReferenceCount = 1;

	entry->ActAsPrimary = Primary;
	
	InitializeListHead(&entry->LfsTabPartitionEntry);
	KeInitializeSpinLock(&entry->SpinLock);
	RtlCopyMemory(&entry->LocalNetDiskPartitionInfo.BindAddress, BindAddress, sizeof(LPX_ADDRESS));
	RtlCopyMemory(&entry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo, NetDiskPartitionInfo, sizeof(NETDISK_PARTITION_INFO));

	*Entry = entry ;

cleanup:

	return ntStatus ;
}

static
VOID
LfsTable_ReferenceEntry (
		IN PLFSTAB_ENTRY	LfsTableEntry
	)
{
    LONG result;

    result = InterlockedIncrement (&LfsTableEntry->ReferenceCount);

    ASSERT (result >= 1);
}

static
VOID
LfsTable_DereferenceEntry (
		IN PLFSTAB_ENTRY	LfsTableEntry
	)
{
    LONG result;
	PLFS_TABLE	LfsTable = LfsTableEntry->LfsTable ;
	KIRQL		oldIrql ;


    result = InterlockedDecrement (&LfsTableEntry->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		KeAcquireSpinLock(&LfsTable->SpinLock, &oldIrql) ;
		RemoveEntryList(&LfsTableEntry->LfsTabPartitionEntry) ;
		KeReleaseSpinLock(&LfsTable->SpinLock, oldIrql) ;

		InterlockedDecrement(&LfsTable->EntryCount) ;

		LfsTable_Dereference(LfsTable) ;

		ExFreePoolWithTag(
			LfsTableEntry,
			LFSTAB_ENTRY_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_TABLE_INFO,
				("LfsTable_DereferenceEntry: Lfs Table is Freed\n"));
	}
}

