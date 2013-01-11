#define	__NETDISK_MANAGER__
#define __PRIMARY__
#include "LfsProc.h"


VOID
NetdiskManagerThreadProc(
	IN 	PNETDISK_MANAGER	NetdiskManager
	);


PNETDISK_MANAGER_REQUEST
AllocNetdiskManagerRequest(
	IN	BOOLEAN	Synchronous
	); 


VOID
DereferenceNetdiskManagerRequest(
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	); 


FORCEINLINE
VOID
QueueingNetdiskManagerRequest(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	);


PENABLED_NETDISK
AllocateEnabledNetdisk(
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo
	);


PENABLED_NETDISK
EnabledNetdisk_Create(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PDEVICE_OBJECT		HarddiskDeviceObject,
	IN PNETDISK_INFORMATION	NetDiskInformation,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);


BOOLEAN
EnabledNetdisk_Update(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PDEVICE_OBJECT		HarddiskDeviceObject,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);
	
	
VOID
EnabledNetdisk_Close(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


VOID
EnabledNetdisk_FileSystemShutdown(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


NTSTATUS
EnabledNetdisk_MountVolume(
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation,
	OUT	PNETDISK_ENABLE_MODE			NetdiskEnableMode
	);


VOID
EnabledNetdisk_MountVolumeComplete(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN PIRP						Irp
	);


VOID
EnabledNetdisk_ChangeMode(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
EnabledNetdisk_SurpriseRemoval(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
EnabledNetdisk_DisMountVolume(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


BOOLEAN
EnabledNetdisk_ThisVolumeHasSecondary(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


PNETDISK_PARTITION
EnabledNetdisk_GetPrimaryPartition(
	IN PENABLED_NETDISK	EnabledNetdisk,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PLARGE_INTEGER	StartingOffset
	);


VOID
EnabledNetdisk_ReturnPrimaryPartition(
	IN PENABLED_NETDISK	EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition 
	);


BOOLEAN
EnabledNetdisk_Secondary2Primary(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
EnabledNetdisk_UnplugNetdisk(
	IN PENABLED_NETDISK			EnabledNetdisk, 
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


PENABLED_NETDISK
LookUpEnabledNetdisk(
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo
	);


VOID
EnabledNetdisk_Reference (
	IN PENABLED_NETDISK	EnabledNetdisk
	);


VOID
EnabledNetdisk_Dereference(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


PNETDISK_PARTITION
AllocateNetdiskPartition(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


PNETDISK_PARTITION
LookUpNetdiskPartition(
	IN  PENABLED_NETDISK		EnabledNetdisk,
    IN  PPARTITION_INFORMATION	PartitionInformation,
	IN  PLARGE_INTEGER			StartingOffset,
	IN  PLFS_DEVICE_EXTENSION	LfsDeviceExt,	
	OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	);


BOOLEAN
IsStoppedNetdisk(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


BOOLEAN
IsStoppedPrimary(
	IN PENABLED_NETDISK	EnabledNetdisk
	);


VOID
NetdiskPartition_Reference (
	IN PNETDISK_PARTITION	NetdiskPartition
	);


VOID
NetdiskPartition_Dereference(
	PNETDISK_PARTITION	NetdiskPartition
	);


PNETDISK_MANAGER
NetdiskManager_Create (
	IN PLFS	Lfs
	)
{
	PNETDISK_MANAGER	netdiskManager;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			ntStatus;
	LARGE_INTEGER		timeOut;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
		("Primary_Create: Entered\n"));

	LfsReference (Lfs);

	netdiskManager = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(NETDISK_MANAGER),
						LFS_ALLOC_TAG
						);
	
	if (netdiskManager == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		LfsDereference (Lfs);

		return NULL;
	}

	RtlZeroMemory(
		netdiskManager,
		sizeof(NETDISK_MANAGER)
		);

	KeInitializeSpinLock(&netdiskManager->SpinLock);	
	netdiskManager->ReferenceCount	= 1;

	netdiskManager->Lfs = Lfs;

	KeInitializeSpinLock(&netdiskManager->EnabledNetdiskQSpinLock);
	InitializeListHead(&netdiskManager->EnabledNetdiskQueue);

	netdiskManager->Thread.ThreadHandle = 0;
	netdiskManager->Thread.ThreadObject = NULL;

	netdiskManager->Thread.Flags = 0;

	KeInitializeEvent(&netdiskManager->Thread.ReadyEvent, NotificationEvent, FALSE) ;

	KeInitializeSpinLock(&netdiskManager->Thread.RequestQSpinLock) ;
	InitializeListHead(&netdiskManager->Thread.RequestQueue) ;
	KeInitializeEvent(&netdiskManager->Thread.RequestEvent, NotificationEvent, FALSE) ;

	
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ntStatus = PsCreateSystemThread(
					&netdiskManager->Thread.ThreadHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					NetdiskManagerThreadProc,
					netdiskManager
					);
	
	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_UNEXPECTED);
		NetdiskManager_Close(netdiskManager);
		
		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle(
					netdiskManager->Thread.ThreadHandle,
					FILE_READ_DATA,
					NULL,
					KernelMode,
					&netdiskManager->Thread.ThreadObject,
					NULL
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		NetdiskManager_Close(netdiskManager);
		
		return NULL;
	}

	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	ntStatus = KeWaitForSingleObject(
					&netdiskManager->Thread.ReadyEvent,
					Executive,
					KernelMode,
					FALSE,
					&timeOut
					);

	ASSERT(ntStatus == STATUS_SUCCESS);

	KeClearEvent(&netdiskManager->Thread.ReadyEvent);

	if(ntStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);
		NetdiskManager_Close(netdiskManager);
		
		return NULL;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
		("NetdiskManager_Create: netdiskManager = %p\n", netdiskManager));

	return netdiskManager;
}


VOID
NetdiskManager_Close (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
	PLIST_ENTRY enabledNetdiskListEntry;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
		("NetdiskManager_Close: Entered NetdiskManager = %p\n", NetdiskManager));

	while(enabledNetdiskListEntry = 
			ExInterlockedRemoveHeadList(
					&NetdiskManager->EnabledNetdiskQueue,
					&NetdiskManager->EnabledNetdiskQSpinLock
					)
		)
	{
		PENABLED_NETDISK	enabledNetdisk;
		
		enabledNetdisk = CONTAINING_RECORD (enabledNetdiskListEntry, ENABLED_NETDISK, ListEntry);
		EnabledNetdisk_Close(enabledNetdisk);
	}
	
	if(NetdiskManager->Thread.ThreadHandle == NULL)
	{
		ASSERT(LFS_BUG);
		NetdiskManager_Dereference(NetdiskManager);

		return;
	}

	ASSERT(NetdiskManager->Thread.ThreadObject != NULL);

	if(NetdiskManager->Thread.Flags & NETDISK_MANAGER_THREAD_TERMINATED)
	{
		ObDereferenceObject(NetdiskManager->Thread.ThreadObject) ;

		NetdiskManager->Thread.ThreadHandle = NULL;
		NetdiskManager->Thread.ThreadObject = NULL;

	} else
	{
		PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
		NTSTATUS					ntStatus;
		LARGE_INTEGER				timeOut;
	
		
		netdiskManagerRequest = AllocNetdiskManagerRequest(FALSE);
		netdiskManagerRequest->RequestType = NETDISK_MANAGER_REQUEST_DOWN;

		QueueingNetdiskManagerRequest(
			NetdiskManager,
			netdiskManagerRequest
			);

		timeOut.QuadPart = - LFS_TIME_OUT ;		// 10 sec
		ntStatus = KeWaitForSingleObject(
						NetdiskManager->Thread.ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		ASSERT(ntStatus == STATUS_SUCCESS);

		if(ntStatus == STATUS_SUCCESS) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
							("NetdiskManager_Close: thread stoped\n"));

			ObDereferenceObject(NetdiskManager->Thread.ThreadObject) ;

			NetdiskManager->Thread.ThreadHandle = NULL;
			NetdiskManager->Thread.ThreadObject = NULL;
		
		} else
		{
			ASSERT(LFS_BUG);
			return;
		}
	}

	NetdiskManager_Dereference(NetdiskManager);

	return;
}


VOID
MountManager_FileSystemShutdown(
	IN PNETDISK_MANAGER		NetdiskManager
	)
{
	PLIST_ENTRY enabledNetdiskListEntry;
	KIRQL		oldIrql;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
		("MountManager_FileSystemShutdown: Entered\n"));

	KeAcquireSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql);

	for(enabledNetdiskListEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
		enabledNetdiskListEntry != &NetdiskManager->EnabledNetdiskQueue;
		enabledNetdiskListEntry = enabledNetdiskListEntry->Flink)
	{
		PENABLED_NETDISK	enabledNetdisk;
	
		
		enabledNetdisk = CONTAINING_RECORD (enabledNetdiskListEntry, ENABLED_NETDISK, ListEntry);
		EnabledNetdisk_FileSystemShutdown(enabledNetdisk);
	}

	KeReleaseSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, oldIrql);

	Primary_FileSystemShutdown(GlobalLfs.Primary);

	return;
}
	

VOID
NetdiskManager_Reference (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
    LONG result;
	
    result = InterlockedIncrement (&NetdiskManager->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
NetdiskManager_Dereference (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
    LONG result;


    result = InterlockedDecrement (&NetdiskManager->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		LfsDereference (NetdiskManager->Lfs);

		ASSERT(NetdiskManager->EnabledNetdiskQueue.Flink == NetdiskManager->EnabledNetdiskQueue.Blink);

		ExFreePoolWithTag(
			NetdiskManager,
			LFS_ALLOC_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
				("NetdiskManager_Dereference: NetdiskManager is Freed NetdiskManager = %p\n", NetdiskManager));
	}
}


VOID
NetdiskManagerThreadProc(
	IN 	PNETDISK_MANAGER	NetdiskManager
	)
{
	BOOLEAN	netdiskManagerThreadExit = FALSE;

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
				("NetdiskManagerThreadProc: Start\n"));

	NetdiskManager_Reference(NetdiskManager);

	//SetFlag(NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_INITIALIZING);
	SetFlag(NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_START);				
	KeSetEvent(&NetdiskManager->Thread.ReadyEvent, IO_DISK_INCREMENT, FALSE) ;


	while(netdiskManagerThreadExit == FALSE)
	{
		LARGE_INTEGER	timeOut ;
		NTSTATUS		waitStatus;		
		PLIST_ENTRY		netdiskManagerRequestEntry;


		timeOut.QuadPart = - LFS_TIME_OUT;

		waitStatus = KeWaitForSingleObject(
						&NetdiskManager->Thread.RequestEvent,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		if(waitStatus == STATUS_TIMEOUT) 
		{
			continue;
		}

		ASSERT(waitStatus == STATUS_SUCCESS);

		KeClearEvent(&NetdiskManager->Thread.RequestEvent);

		while(netdiskManagerThreadExit == FALSE 
				&& (netdiskManagerRequestEntry = 
						ExInterlockedRemoveHeadList(
									&NetdiskManager->Thread.RequestQueue,
									&NetdiskManager->Thread.RequestQSpinLock
									))
			) 
		{
			PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
			
			InitializeListHead(netdiskManagerRequestEntry);

			netdiskManagerRequest = CONTAINING_RECORD(netdiskManagerRequestEntry, NETDISK_MANAGER_REQUEST, ListEntry);

			if(netdiskManagerRequest->RequestType == NETDISK_MANAGER_REQUEST_DOWN)
			{				
				netdiskManagerThreadExit = TRUE;
				ASSERT(IsListEmpty(&NetdiskManager->Thread.RequestQueue));
			}
			else if(netdiskManagerRequest->RequestType == NETDISK_MANAGER_REQUEST_TOUCH_VOLUME)
			{
				HANDLE					fileHandle = NULL;	
				UNICODE_STRING			fileName;
				WCHAR					fileNameBuffer[NDFS_MAX_PATH];
				NTSTATUS				unicodeCreateStatus;

				ULONG					retryCount = 5;


				RtlInitEmptyUnicodeString(&fileName, fileNameBuffer, sizeof(fileNameBuffer));

				RtlCopyUnicodeString(
					&fileName,
					&netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName
					);
	
				unicodeCreateStatus = RtlAppendUnicodeToString(&fileName, TOUCH_VOLUME_FILE_NAME);

				if(unicodeCreateStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
				}
					
				while(unicodeCreateStatus == STATUS_SUCCESS && retryCount--)
				{
					NTSTATUS				createStatus;
					IO_STATUS_BLOCK			ioStatusBlock;
				    LARGE_INTEGER			interval;
					ACCESS_MASK				desiredAccess;
					ULONG					attributes;
					OBJECT_ATTRIBUTES		objectAttributes;
					ULONG					fileAttributes;
					ULONG					shareAccess;
					ULONG					createDisposition;
					ULONG					createOptions;


				    interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
					KeDelayExecutionThread(KernelMode, FALSE, &interval);
			    
					ioStatusBlock.Information = 0;

					desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
									| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

					ASSERT(desiredAccess == 0x0012019F);

					attributes  = OBJ_KERNEL_HANDLE;
					attributes |= OBJ_CASE_INSENSITIVE;

					InitializeObjectAttributes(
						&objectAttributes,
						&fileName,
						attributes,
						NULL,
						NULL
						);
		
					//allocationSize.LowPart  = 0;
					//allocationSize.HighPart = 0;

					fileAttributes	  = 0;		
					shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
					createDisposition = FILE_OPEN_IF;
					createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;
					//eaBuffer		  = NULL;
					//eaLength		  = 0;		
		
					createStatus = ZwCreateFile(
										&fileHandle,
										desiredAccess,
										&objectAttributes,
										&ioStatusBlock,
										NULL,
										fileAttributes,
										shareAccess,
										createDisposition,
										createOptions,
										NULL,
										0
										);
		
					SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
									("NetdiskManagerThreadProc: Touch Volume %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
					                 &netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information ));

					if(createStatus == STATUS_SUCCESS)
					{
						ASSERT(ioStatusBlock.Information == FILE_CREATED);
						ZwClose(fileHandle);
						break;
					}
					
					if(retryCount == 1)
						ASSERT(LFS_REQUIRED);
				}
				
				NetdiskPartition_Dereference(netdiskManagerRequest->NetdiskPartition);
			}			
			else
			{
				KIRQL	oldIrql;

				ASSERT(LFS_BUG);
				KeAcquireSpinLock(&NetdiskManager->SpinLock, &oldIrql) ;
				SetFlag(NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_ERROR);
				KeReleaseSpinLock(&NetdiskManager->SpinLock, oldIrql) ;
				netdiskManagerThreadExit = TRUE;
			}

			if(netdiskManagerRequest->Synchronous == TRUE)
				KeSetEvent(&netdiskManagerRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE) ;
			else
				DereferenceNetdiskManagerRequest(
					netdiskManagerRequest
					);
		}
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("NetdiskManagerThreadProc: PsTerminateSystemThread NetdiskManager = %p\n", NetdiskManager));

	SetFlag(NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_TERMINATED);

	NetdiskManager_Dereference(NetdiskManager);	
	PsTerminateSystemThread(STATUS_SUCCESS);
}


BOOLEAN
NetdiskManager_IsNetdiskPartition(
	IN  PNETDISK_MANAGER	NetdiskManager,
	IN  PDEVICE_OBJECT		DiskDeviceObject,
	OUT PENABLED_NETDISK	*EnabledNetdisk
	)
{
	BOOLEAN							result;
	NETDISK_INFORMATION				netdiskInformation;
	
//	PDEVICE_OBJECT					harddiskDeviceObject;
//	BOOLEAN							getHarddiskResult;
	
	NETDISK_ENABLE_MODE				netdiskEnableMode;
	PENABLED_NETDISK				enabledNetdisk;

	UNICODE_STRING					deviceName;
    WCHAR							deviceNameBuffer[DEVICE_NAMES_SZ];

	ASSERT(DiskDeviceObject);

    RtlInitEmptyUnicodeString( 
				&deviceName,
                deviceNameBuffer,
                sizeof(deviceNameBuffer) 
				);

    SpyGetObjectName(DiskDeviceObject, &deviceName);

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
					("NetdiskManager_IsNetdiskPartition: entered %wZ\n", &deviceName));

	result = IsNetDisk(
				DiskDeviceObject,
				&netdiskInformation
				);
	
	if(result == FALSE)
		return FALSE;

		
/*
	Direct access to PhysicalDriveXX causes conflict with SiS Ide driver(sisidex.sys) on Windows 2000.

	getHarddiskResult = GetHarddisk(
							DiskDeviceObject,
							&harddiskDeviceObject
							);

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
			("getHarddiskResult = %d\n", getHarddiskResult));
		
	if(getHarddiskResult == FALSE)
	{
		*EnabledNetdisk = NULL;
		return FALSE;
	}

*/

	if(netdiskInformation.DesiredAccess & GENERIC_WRITE)
	{
		if(netdiskInformation.GrantedAccess & GENERIC_WRITE)
			netdiskEnableMode = NETDISK_PRIMARY;
		else
			netdiskEnableMode = NETDISK_SECONDARY;
	} else
		netdiskEnableMode = NETDISK_READ_ONLY;

	enabledNetdisk = LookUpEnabledNetdisk(
							NetdiskManager,
							&netdiskInformation.NetDiskAddress,
							netdiskInformation.UnitDiskNo
							);

	if(enabledNetdisk)
	{
		if(enabledNetdisk->NetDiskInformation.EnabledTime.QuadPart != netdiskInformation.EnabledTime.QuadPart)
		{
			KIRQL	oldIrql;

			//ASSERT(IsStoppedNetdisk(enabledNetdisk));
			
			KeAcquireSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql);
			RemoveEntryList(&enabledNetdisk->ListEntry);
			KeReleaseSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, oldIrql);
			
			EnabledNetdisk_Close(enabledNetdisk);
			
			EnabledNetdisk_Dereference(enabledNetdisk);
			enabledNetdisk = NULL;
		}
	}

	if(enabledNetdisk)
	{
		BOOLEAN	updateResult;

		EnabledNetdisk_Reference(enabledNetdisk);
		updateResult = EnabledNetdisk_Update( 
							NetdiskManager,
							enabledNetdisk,
							DiskDeviceObject,
							netdiskEnableMode
							);
		EnabledNetdisk_Dereference(enabledNetdisk);

		if(updateResult != TRUE)
		{
			*EnabledNetdisk = NULL;
//			ObDereferenceObject(harddiskDeviceObject);	// by GetHarddisk
			return TRUE;
		}
	}
		
	if(enabledNetdisk == NULL)
	{		
		enabledNetdisk = EnabledNetdisk_Create( 
							NetdiskManager,
							DiskDeviceObject,
							&netdiskInformation,
							netdiskEnableMode
							);

		if(enabledNetdisk == NULL)
		{
			*EnabledNetdisk = NULL;
//			ObDereferenceObject(harddiskDeviceObject);	// by GetHarddisk
			return TRUE;
		}

		EnabledNetdisk_Reference(enabledNetdisk);
		
		ExInterlockedInsertHeadList(
			&NetdiskManager->EnabledNetdiskQueue,
			&enabledNetdisk->ListEntry,
			&NetdiskManager->EnabledNetdiskQSpinLock
			);
	}

	*EnabledNetdisk = enabledNetdisk;
//	ObDereferenceObject(harddiskDeviceObject);	// by GetHarddisk

	return TRUE;
}


NTSTATUS
NetdiskManager_MountVolume(
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation,
	OUT	PNETDISK_ENABLE_MODE			NetdiskEnableMode
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);
	
	return EnabledNetdisk_MountVolume(
				EnabledNetdisk,
				FileSystemType,
				LfsDeviceExt,
				DiskDeviceObject,
				NetdiskPartionInformation,
				NetdiskEnableMode
				);
}


VOID
NetdiskManager_MountVolumeComplete(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN PIRP						Irp
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	EnabledNetdisk_MountVolumeComplete(
		EnabledNetdisk,
		LfsDeviceExt,
		Irp
		);

	return;
}


VOID
NetdiskManager_ChangeMode(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	if(EnabledNetdisk == NULL)
	{
		ASSERT(BooleanFlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP));
		return;
	}
	
	EnabledNetdisk_ChangeMode(
		EnabledNetdisk,
		LfsDeviceExt
		);

	return;
}


VOID
NetdiskManager_SurpriseRemoval(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	EnabledNetdisk_SurpriseRemoval(
		EnabledNetdisk,
		LfsDeviceExt
		);
	
	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_ERROR,
				("NetdiskManager_SurpriseRemoval: IsStoppedNetdisk(EnabledNetdisk) = %d\n", 
				IsStoppedNetdisk(EnabledNetdisk)));

	EnabledNetdisk_Dereference(EnabledNetdisk);		// by NetdiskManager_IsNetdiskPartition

	return;
}

	
VOID
NetdiskManager_DisMountVolume(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);


	EnabledNetdisk_DisMountVolume(
		EnabledNetdisk,
		LfsDeviceExt
		);
	
	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("NetdiskManager_DisMountVolume: IsStoppedNetdisk(EnabledNetdisk) = %d\n", IsStoppedNetdisk(EnabledNetdisk)));

	EnabledNetdisk_Dereference(EnabledNetdisk);		// by NetdiskManager_IsNetdiskPartition

	return;
}


BOOLEAN
NetdiskManager_ThisVolumeHasSecondary(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	if(EnabledNetdisk == NULL)
	{
		ASSERT(BooleanFlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP));
		return FALSE;
	}

	return EnabledNetdisk_ThisVolumeHasSecondary(
				EnabledNetdisk,
				LfsDeviceExt
				);
}


BOOLEAN
NetdiskManager_Secondary2Primary(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	if(EnabledNetdisk == NULL)
	{
		ASSERT(BooleanFlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP));
		return FALSE;
	}

	return EnabledNetdisk_Secondary2Primary(
				EnabledNetdisk,
				LfsDeviceExt
				);
}


VOID
NetdiskManager_UnplugNetdisk(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	EnabledNetdisk_UnplugNetdisk(
		EnabledNetdisk,
		LfsDeviceExt
		);
}


PNETDISK_PARTITION
MountManager_GetPrimaryPartition(
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PLARGE_INTEGER	StartingOffset
	)
{	
	PENABLED_NETDISK	enabledNetdisk;
	PNETDISK_PARTITION	netdiskPartition;


	UNREFERENCED_PARAMETER(NetdiskManager);

	enabledNetdisk = LookUpEnabledNetdisk(
							NetdiskManager,
							NetDiskAddress,
							UnitDiskNo
							);

	if(enabledNetdisk == NULL)
		return NULL;

	netdiskPartition = EnabledNetdisk_GetPrimaryPartition(
							enabledNetdisk,
							NetDiskAddress,
							UnitDiskNo,
							StartingOffset
							);

	EnabledNetdisk_Dereference(enabledNetdisk);		// by LookUpEnabledNetdisk

	return netdiskPartition;
}


VOID
MountManager_ReturnPrimaryPartition(
	IN PNETDISK_MANAGER		NetdiskManager, 
	IN PNETDISK_PARTITION	NetdiskPartition 
	)
{
	UNREFERENCED_PARAMETER(NetdiskManager);

	EnabledNetdisk_ReturnPrimaryPartition(
		NetdiskPartition->EnabledNetdisk,
		NetdiskPartition
		);

	return;
}


BOOLEAN
IsStoppedNetdisk(
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PNETDISK_PARTITION	netdiskPartition;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;


	KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) 
	{
		NETDISK_ENABLE_MODE	volumeIndex;

		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT(netdiskPartition->ReferenceCount > 0);

		for(volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
		{
			if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt)
			{
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
				return FALSE;
			}
			if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt)
			{
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
				return FALSE;
			}
		}
	}

	KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);

	return TRUE;
}


BOOLEAN
IsStoppedPrimary(
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PNETDISK_PARTITION	netdiskPartition;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;


	KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) 
	{
		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT(netdiskPartition->ReferenceCount > 0);

		if(netdiskPartition->NetdiskVolume[NETDISK_PRIMARY][0].LfsDeviceExt
			|| netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][0].LfsDeviceExt)
		{
			KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			return FALSE;
		}
		if(netdiskPartition->NetdiskVolume[NETDISK_PRIMARY][1].LfsDeviceExt
			|| netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][1].LfsDeviceExt)
		{
			KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			return FALSE;
		}
	}

	KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);

	return TRUE;
}


PNETDISK_MANAGER_REQUEST
AllocNetdiskManagerRequest(
	IN	BOOLEAN	Synchronous
) 
{
	PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;


 	netdiskManagerRequest = ExAllocatePoolWithTag(
								NonPagedPool,
								sizeof(NETDISK_MANAGER_REQUEST),
								NETDISK_MANAGER_REQUEST_TAG
								);

	if(netdiskManagerRequest == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(netdiskManagerRequest, sizeof(NETDISK_MANAGER_REQUEST));

	netdiskManagerRequest->ReferenceCount = 1;
	InitializeListHead(&netdiskManagerRequest->ListEntry);
	
	netdiskManagerRequest->Synchronous = Synchronous;
	KeInitializeEvent(&netdiskManagerRequest->CompleteEvent, NotificationEvent, FALSE);

#if DBG
	InterlockedIncrement(&LfsObjectCounts.NetdiskManagerRequestCount);
#endif

	return netdiskManagerRequest;
}


VOID
DereferenceNetdiskManagerRequest(
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	) 
{
	LONG	result ;

	result = InterlockedDecrement(&NetdiskManagerRequest->ReferenceCount) ;

	ASSERT(result >= 0) ;

	if(0 == result)
	{
		ExFreePoolWithTag(
			NetdiskManagerRequest,
			NETDISK_MANAGER_REQUEST_TAG
			);

#if DBG
		InterlockedDecrement(&LfsObjectCounts.NetdiskManagerRequestCount);
#endif

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
				("DereferenceNetdiskManagerRequest: NetdiskManagerRequest freed\n")) ;
	}	
}


FORCEINLINE
VOID
QueueingNetdiskManagerRequest(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	)
{
	ExInterlockedInsertTailList(
		&NetdiskManager->Thread.RequestQueue,
		&NetdiskManagerRequest->ListEntry,
		&NetdiskManager->Thread.RequestQSpinLock
		);

	KeSetEvent(&NetdiskManager->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE);
	
	return;
}


PENABLED_NETDISK
EnabledNetdisk_Create(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PDEVICE_OBJECT		HarddiskDeviceObject,
	IN PNETDISK_INFORMATION	NetDiskInformation,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	PENABLED_NETDISK			enabledNetdisk;

	NTSTATUS					ntStatus;
	PDRIVE_LAYOUT_INFORMATION	driveLayoutInformation;
	ULONG						driveLayoutInformationSize;
	ULONG						partitionIndex;


	enabledNetdisk = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(ENABLED_NETDISK),
						NETDISK_MANAGER_ENABLED_TAG
						);
	
	if (enabledNetdisk == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	NetdiskManager_Reference(NetdiskManager);

	RtlZeroMemory(
		enabledNetdisk,
		sizeof(ENABLED_NETDISK)
		);
	

	KeInitializeSpinLock(&enabledNetdisk->SpinLock);
	enabledNetdisk->ReferenceCount = 1;
	InitializeListHead(&enabledNetdisk->ListEntry);
	enabledNetdisk->NetdiskManager = NetdiskManager;

	KeInitializeSpinLock(&enabledNetdisk->NetdiskPartitionQSpinLock);
	InitializeListHead(&enabledNetdisk->NetdiskPartitionQueue);

	enabledNetdisk->NetDiskInformation = *NetDiskInformation;
	enabledNetdisk->NetdiskEnableMode = NetdiskEnableMode;

	driveLayoutInformationSize = sizeof(DRIVE_LAYOUT_INFORMATION);
	
	while(1)
	{
		driveLayoutInformationSize += sizeof(PARTITION_INFORMATION); 

		driveLayoutInformation = ExAllocatePoolWithTag( 
									NonPagedPool, 
									driveLayoutInformationSize,
									NETDISK_MANAGER_TAG
									);

		if (driveLayoutInformation == NULL)
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);
			return enabledNetdisk;
		}
		
		ntStatus = LfsFilterDeviceIoControl( 
						HarddiskDeviceObject,
			            IOCTL_DISK_GET_DRIVE_LAYOUT,
						NULL,
						0,
						driveLayoutInformation,
						driveLayoutInformationSize,
						NULL 
						);

		if(ntStatus == STATUS_SUCCESS)
		{
			break;
		}
		
		ExFreePoolWithTag(
			driveLayoutInformation,
			NETDISK_MANAGER_TAG
			);

		if(ntStatus == STATUS_BUFFER_TOO_SMALL)
		{
			continue;
		}

		ASSERT(LFS_UNEXPECTED);
		return enabledNetdisk;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
			("EnabledNetdisk_Create: driveLayoutInformation->PartitionCount = %d\n", 
				driveLayoutInformation->PartitionCount));

	enabledNetdisk->DummyNetdiskPartition.EnabledNetdisk	= enabledNetdisk;
	enabledNetdisk->DummyNetdiskPartition.Flags				= NETDISK_PARTITION_ENABLED;
	enabledNetdisk->DummyNetdiskPartition.NetdiskPartitionInformation.NetDiskInformation
			= enabledNetdisk->NetDiskInformation;

	for(partitionIndex=0; partitionIndex<driveLayoutInformation->PartitionCount; partitionIndex++)
	{
		PNETDISK_PARTITION	netdiskPartition;
		NETDISK_ENABLE_MODE	volumeIndex;


		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
			("EnabledNetdisk_Create: StartingOffset = %I64x, PartitionNumber = %d\n", 
				driveLayoutInformation->PartitionEntry[partitionIndex].StartingOffset,
				driveLayoutInformation->PartitionEntry[partitionIndex].PartitionNumber));
	
		netdiskPartition = AllocateNetdiskPartition(
							enabledNetdisk
							);

		if(netdiskPartition == NULL)
		{
			break;
		}

		NetdiskPartition_Reference(netdiskPartition);

		netdiskPartition->Flags = NETDISK_PARTITION_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetDiskInformation
			= enabledNetdisk->NetDiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformation 
			= driveLayoutInformation->PartitionEntry[partitionIndex];

		for(volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
		{
			netdiskPartition->NetdiskVolume[volumeIndex][0].VolumeState		= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt	= NULL;
			netdiskPartition->NetdiskVolume[volumeIndex][1].VolumeState		= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt	= NULL;
			//netdiskPartition->NetdiskVolume[volumeIndex].MountStatus		= STATUS_SUCCESS;
		}

		ExInterlockedInsertHeadList(					// must be inserted head Why ?
			&enabledNetdisk->NetdiskPartitionQueue,
			&netdiskPartition->ListEntry,
			&enabledNetdisk->NetdiskPartitionQSpinLock
			);
	
		NetdiskPartition_Dereference(netdiskPartition);
	}

	enabledNetdisk->DriveLayoutInformation = driveLayoutInformation;

#if DBG
	InterlockedIncrement(&LfsObjectCounts.EnabledNetdiskCount);
#endif

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
		("EnabledNetdisk_Create enabledNetdisk = %p, enabledNetdiskCount = %d\n", enabledNetdisk, LfsObjectCounts.EnabledNetdiskCount));

	return enabledNetdisk;
}


BOOLEAN
EnabledNetdisk_Update(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PDEVICE_OBJECT		HarddiskDeviceObject,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS					ntStatus;
	PDRIVE_LAYOUT_INFORMATION	driveLayoutInformation;
	ULONG						driveLayoutInformationSize;
	ULONG						partitionIndex;


	UNREFERENCED_PARAMETER(NetdiskManager);


	switch(EnabledNetdisk->NetdiskEnableMode)
	{
	case NETDISK_READ_ONLY:
			
		ASSERT(NetdiskEnableMode == NETDISK_READ_ONLY);

		break;

	case NETDISK_SECONDARY:
			
		if(NetdiskEnableMode == NETDISK_PRIMARY)
		{
			EnabledNetdisk->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		}
		else
			ASSERT(NetdiskEnableMode == NETDISK_SECONDARY);

		break;

	case NETDISK_PRIMARY:
	case NETDISK_SECONDARY2PRIMARY:
			
		ASSERT(NetdiskEnableMode == NETDISK_PRIMARY);

		break;

	default:

		ASSERT(LFS_BUG);
		break;
	}

	driveLayoutInformationSize = sizeof(DRIVE_LAYOUT_INFORMATION)
								+ (EnabledNetdisk->DriveLayoutInformation->PartitionCount-1) * sizeof(PARTITION_INFORMATION);
	
	while(1)
	{
		driveLayoutInformationSize += sizeof(PARTITION_INFORMATION); 

		driveLayoutInformation = ExAllocatePoolWithTag( 
									NonPagedPool, 
									driveLayoutInformationSize,
									NETDISK_MANAGER_TAG
									);

		if (driveLayoutInformation == NULL)
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);
			return FALSE;
		}
		
		ntStatus = LfsFilterDeviceIoControl( 
						HarddiskDeviceObject,
			            IOCTL_DISK_GET_DRIVE_LAYOUT,
						NULL,
						0,
						driveLayoutInformation,
						driveLayoutInformationSize,
						NULL 
						);

		if(ntStatus == STATUS_SUCCESS)
		{
			break;
		}
		
		ExFreePoolWithTag(
			driveLayoutInformation,
			NETDISK_MANAGER_TAG
			);

		if(ntStatus == STATUS_BUFFER_TOO_SMALL)
		{
			continue;
		}

		ASSERT(LFS_UNEXPECTED);
		return FALSE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
			("EnabledNetdisk_Update: driveLayoutInformation->PartitionCount = %d\n", 
				driveLayoutInformation->PartitionCount));

	for(partitionIndex=0; partitionIndex<driveLayoutInformation->PartitionCount; partitionIndex++)
	{
		PNETDISK_PARTITION		netdiskPartition;
		NETDISK_ENABLE_MODE		volumeIndex;
		PPARTITION_INFORMATION	partitionInformation;
	
	
		partitionInformation = &driveLayoutInformation->PartitionEntry[partitionIndex];

		netdiskPartition = LookUpNetdiskPartition(
							EnabledNetdisk,
							NULL,
							&partitionInformation->StartingOffset,
							NULL,
							NULL
							);

		if(netdiskPartition)
		{
			KIRQL				oldIrql;
			NETDISK_ENABLE_MODE	volumeIndex2;

			
			if(netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart == partitionInformation->StartingOffset.QuadPart
				&& netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionLength.QuadPart == partitionInformation->PartitionLength.QuadPart
				/* && netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionType == partitionInformation->PartitionType */
				)
			{
				//ASSERT(netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionType == partitionInformation->PartitionType);
				ASSERT(netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionNumber == partitionInformation->PartitionNumber);
				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionNumber 
					= partitionInformation->PartitionNumber;
				
				NetdiskPartition_Dereference(netdiskPartition);	// by LookUpNetdiskPartition
				continue;
			}

#if DBG
			for(volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++)
			{
				if(netdiskPartition->NetdiskVolume[volumeIndex2][0].LfsDeviceExt)
				{
					break;

					EnabledNetdisk_SurpriseRemoval(EnabledNetdisk, netdiskPartition->NetdiskVolume[volumeIndex2][0].LfsDeviceExt);
					continue;
				}
				if(netdiskPartition->NetdiskVolume[volumeIndex2][1].LfsDeviceExt)
				{
					break;

					EnabledNetdisk_SurpriseRemoval(EnabledNetdisk, netdiskPartition->NetdiskVolume[volumeIndex2][1].LfsDeviceExt);
					continue;
				}
			}

			ASSERT(volumeIndex2 == (NETDISK_SECONDARY2PRIMARY+1));
#endif
			
			for(volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++)
			{
				if(netdiskPartition->NetdiskVolume[volumeIndex2][0].LfsDeviceExt)
				{	
					// It must not be happened ! 
					EnabledNetdisk_SurpriseRemoval(EnabledNetdisk, netdiskPartition->NetdiskVolume[volumeIndex2][0].LfsDeviceExt);
					continue;
				}
				if(netdiskPartition->NetdiskVolume[volumeIndex2][1].LfsDeviceExt)
				{
					// It must not be happened ! 
					EnabledNetdisk_SurpriseRemoval(EnabledNetdisk, netdiskPartition->NetdiskVolume[volumeIndex2][1].LfsDeviceExt);
					continue;
				}
			}
	
			KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);
			RemoveEntryList(&netdiskPartition->ListEntry);
			KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			
			InitializeListHead(&netdiskPartition->ListEntry);

			if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject)
			{
				ObDereferenceObject(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject);
				netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
			}

			NetdiskPartition_Dereference(netdiskPartition);	// close It
			NetdiskPartition_Dereference(netdiskPartition);	// by LookUpNetdiskPartition
			
			netdiskPartition = NULL;
		}

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
			("EnabledNetdisk_Update: StartingOffset = %I64x, PartitionNumber = %d\n", 
				driveLayoutInformation->PartitionEntry[partitionIndex].StartingOffset,
				driveLayoutInformation->PartitionEntry[partitionIndex].PartitionNumber));

		netdiskPartition = AllocateNetdiskPartition(
							EnabledNetdisk
							);

		if(netdiskPartition == NULL)
		{
			return FALSE;
		}

		NetdiskPartition_Reference(netdiskPartition);

		netdiskPartition->Flags = NETDISK_PARTITION_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetDiskInformation
			= EnabledNetdisk->NetDiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformation 
			= driveLayoutInformation->PartitionEntry[partitionIndex];

		for(volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
		{
			netdiskPartition->NetdiskVolume[volumeIndex][0].VolumeState		= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt	= NULL;
			//netdiskPartition->NetdiskVolume[volumeIndex].MountStatus		= STATUS_SUCCESS;
			netdiskPartition->NetdiskVolume[volumeIndex][1].VolumeState		= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt	= NULL;
			//netdiskPartition->NetdiskVolume[volumeIndex].MountStatus		= STATUS_SUCCESS;
		}

		ExInterlockedInsertHeadList(					// must be inserted head Why ?
			&EnabledNetdisk->NetdiskPartitionQueue,
			&netdiskPartition->ListEntry,
			&EnabledNetdisk->NetdiskPartitionQSpinLock
			);
	
		NetdiskPartition_Dereference(netdiskPartition);
	}

	if(EnabledNetdisk->DriveLayoutInformation)
		ExFreePoolWithTag(
			EnabledNetdisk->DriveLayoutInformation,
			NETDISK_MANAGER_TAG
			);
	else
		ASSERT(LFS_BUG);

	EnabledNetdisk->DriveLayoutInformation = driveLayoutInformation;

	return TRUE;
}


VOID
EnabledNetdisk_Close(
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PLIST_ENTRY	netdiskPartitionListEntry;

	InitializeListHead(&EnabledNetdisk->ListEntry);

	while(netdiskPartitionListEntry = 
				ExInterlockedRemoveHeadList(
						&EnabledNetdisk->NetdiskPartitionQueue,
						&EnabledNetdisk->NetdiskPartitionQSpinLock
						)
			) 
	{
		PNETDISK_PARTITION	netdiskPartition;
		NETDISK_ENABLE_MODE	volumeIndex;
		
		netdiskPartition = CONTAINING_RECORD (netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
		InitializeListHead(&netdiskPartition->ListEntry);
		
		for(volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
		{
			if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt)
			{
				//ASSERT(LFS_BUG);
				//LfsDeviceExt_Dereference(netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt);
				//netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt = NULL;
			}
			if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt)
			{
				//ASSERT(LFS_BUG);
				//LfsDeviceExt_Dereference(netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt);
				//netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt = NULL;
			}
		}

		if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject)
		{
			ObDereferenceObject(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject);
			netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
		}

		NetdiskPartition_Dereference(netdiskPartition);	// close it
	}
	
	EnabledNetdisk_Dereference(EnabledNetdisk);
}


VOID
EnabledNetdisk_FileSystemShutdown(
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PLIST_ENTRY	netdiskPartitionListEntry;

	for(netdiskPartitionListEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
		netdiskPartitionListEntry != &EnabledNetdisk->NetdiskPartitionQueue;
		netdiskPartitionListEntry = netdiskPartitionListEntry->Flink)
	{	
		PNETDISK_PARTITION	netdiskPartition;
		
		netdiskPartition = CONTAINING_RECORD (netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
		SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_SHUTDOWN);
	}
	return;
}


NTSTATUS
EnabledNetdisk_MountVolume(
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation,
	OUT	PNETDISK_ENABLE_MODE			NetdiskEnableMode
	)
{
	NTSTATUS				ntStatus;
	PNETDISK_PARTITION		netdiskPartition;
	PARTITION_INFORMATION	partitionInformation;
	NETDISK_ENABLE_MODE		netdiskEnableMode;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_MountVolume: LfsDeviceExt = %p\n", LfsDeviceExt));

	ntStatus = LfsFilterDeviceIoControl( 
						DiskDeviceObject,
			            IOCTL_DISK_GET_PARTITION_INFO,
						NULL,
						0,
						&partitionInformation,
						sizeof(PARTITION_INFORMATION),
						NULL 
						);

	if(ntStatus != STATUS_SUCCESS)
	{
		ASSERT(LFS_UNEXPECTED);
		return ntStatus;
	}

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_ERROR,
		("EnabledNetdisk_MountVolume DiskDeviceObject = %p, partitionInformation.StartingOffset.QuadPart = %I64x\n", 
		DiskDeviceObject, partitionInformation.StartingOffset.QuadPart));

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						&partitionInformation,
						NULL,
						NULL,
						NULL
						);

	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return STATUS_UNSUCCESSFUL;
	}

	if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject)
	{
		NETDISK_ENABLE_MODE	volumeIndex;

		
		for(volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
		{
			if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt)
			{
				break;
			}
			if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt)
			{
				break;
			}
		}
		if(volumeIndex != (NETDISK_SECONDARY2PRIMARY+1))
			ASSERT(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == DiskDeviceObject);

		ObDereferenceObject(DiskDeviceObject);
	}


	ObReferenceObject(DiskDeviceObject);
	netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = DiskDeviceObject;
	netdiskPartition->FileSystemType = FileSystemType;

	LfsDeviceExt_Reference(LfsDeviceExt);

	netdiskEnableMode = EnabledNetdisk->NetdiskEnableMode;

	if(netdiskPartition->NetdiskVolume[netdiskEnableMode][0].LfsDeviceExt != NULL)
	{
		ASSERT(netdiskPartition->NetdiskVolume[netdiskEnableMode][1].LfsDeviceExt == NULL);
		netdiskPartition->NetdiskVolume[netdiskEnableMode][1].VolumeState 
			= netdiskPartition->NetdiskVolume[netdiskEnableMode][0].VolumeState;
		netdiskPartition->NetdiskVolume[netdiskEnableMode][1].LfsDeviceExt 
			= netdiskPartition->NetdiskVolume[netdiskEnableMode][0].LfsDeviceExt;
		netdiskPartition->NetdiskVolume[netdiskEnableMode][1].MountStatus
			= netdiskPartition->NetdiskVolume[netdiskEnableMode][0].MountStatus;
	}
	netdiskPartition->NetdiskVolume[netdiskEnableMode][0].VolumeState	= VolumeMounting;
	netdiskPartition->NetdiskVolume[netdiskEnableMode][0].LfsDeviceExt	= LfsDeviceExt;
	netdiskPartition->NetdiskVolume[netdiskEnableMode][0].MountStatus	= STATUS_SUCCESS;

	SpyGetObjectName(
		netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, 
		&netdiskPartition->NetdiskPartitionInformation.VolumeName);
	
	*NetdiskPartionInformation = netdiskPartition->NetdiskPartitionInformation;
    RtlInitEmptyUnicodeString(
				&NetdiskPartionInformation->VolumeName,
                NetdiskPartionInformation->VolumeNameBuffer,
                sizeof(NetdiskPartionInformation->VolumeNameBuffer) 
				);

	if(RtlAppendUnicodeStringToString(
		&NetdiskPartionInformation->VolumeName,
		&netdiskPartition->NetdiskPartitionInformation.VolumeName
		) != STATUS_SUCCESS)
	{
		ASSERT(LFS_UNEXPECTED);
	}	
	*NetdiskEnableMode = netdiskEnableMode;
	
	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&EnabledNetdisk->NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	if(netdiskEnableMode == NETDISK_SECONDARY) 
	{
		LfsTable_InsertNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				&EnabledNetdisk->NetDiskInformation.BindAddress,
				FALSE
				);
	}
	else if(netdiskEnableMode == NETDISK_PRIMARY || netdiskEnableMode == NETDISK_SECONDARY2PRIMARY)
	{
		LfsTable_InsertNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				&EnabledNetdisk->NetDiskInformation.BindAddress,
				TRUE
				);
	}
		
	SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTING);
	
	return STATUS_SUCCESS;
}


VOID
EnabledNetdisk_MountVolumeComplete(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN PIRP						Irp
	)
{
	PNETDISK_PARTITION	netdiskPartition;
	NETDISK_ENABLE_MODE	volumeIndex;
	ULONG				lfsDeviceExtIndex;

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_MountVolumeComplete: LfsDeviceExt = %p\n", LfsDeviceExt));

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return;
	}

	if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 0;
	else if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 1;
	else
		ASSERT(LFS_BUG);

	ASSERT(lfsDeviceExtIndex == 0);

	ASSERT(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState == VolumeMounting);
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState	= VolumeMounted;
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].MountStatus	= Irp->IoStatus.Status;
	
	ClearFlag(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTING);

	if(NT_SUCCESS(Irp->IoStatus.Status))
	{
		SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTED);
		ClearFlag(netdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED);
	}
	else if(Irp->IoStatus.Status == STATUS_FILE_CORRUPT_ERROR
			|| Irp->IoStatus.Status == STATUS_DISK_CORRUPT_ERROR)
	{
		SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED);
	}

	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition

	return;
}


VOID
EnabledNetdisk_ChangeMode(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	PNETDISK_PARTITION		netdiskPartition;
	NETDISK_ENABLE_MODE		volumeIndex;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	ULONG					lfsDeviceExtIndex;

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_ChangeMode: LfsDeviceExt = %p\n", LfsDeviceExt));

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return;
	}

	if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 0;
	else if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 1;
	else
		ASSERT(LFS_BUG);

	ASSERT(lfsDeviceExtIndex == 0);

	ASSERT(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState == VolumeMounted);
	ASSERT(volumeIndex == NETDISK_SECONDARY);
	
	netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][lfsDeviceExtIndex].LfsDeviceExt 
		= netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt;
	netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][lfsDeviceExtIndex].MountStatus	
		= netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].MountStatus;
	netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][lfsDeviceExtIndex].VolumeState
		= netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState;
	
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt = NULL;
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].MountStatus = VolumeEnabled;
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].MountStatus = 0;
	
	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&EnabledNetdisk->NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	LfsTable_DeleteNetDiskPartitionInfoUser(
			GlobalLfs.LfsTable,
			&netdiskPartitionInfo,
			FALSE
			);
	
	LfsTable_InsertNetDiskPartitionInfoUser(
			GlobalLfs.LfsTable,
			&netdiskPartitionInfo,
			&EnabledNetdisk->NetDiskInformation.BindAddress,
			TRUE
			);

	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition

	return;
}


VOID
EnabledNetdisk_SurpriseRemoval(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	PNETDISK_PARTITION		netdiskPartition;
	NETDISK_ENABLE_MODE		volumeIndex;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	ULONG					lfsDeviceExtIndex;

	
	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_SurpriseRemoval: LfsDeviceExt = %p\n", LfsDeviceExt));

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		//ASSERT(LFS_BUG);
		return;
	}

	if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 0;
	else if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 1;
	else
		ASSERT(LFS_BUG);

	ASSERT(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState == VolumeMounted);
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState = VolumeSurpriseRemoved;
	LfsDeviceExt_Dereference(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt);
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt = NULL;

	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&EnabledNetdisk->NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	if(volumeIndex == NETDISK_SECONDARY) 
	{
		LfsTable_DeleteNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				FALSE
				);
	}
	else if(volumeIndex == NETDISK_PRIMARY || volumeIndex == NETDISK_SECONDARY2PRIMARY)
	{
		LfsTable_DeleteNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				TRUE
				);
	}
	
	if(BooleanFlagOn(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTED))
		SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_SURPRISE_REMOVAL);
	
	if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject)
	{
		ObDereferenceObject(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject);
		netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
	}
	
	NetdiskPartition_Dereference(netdiskPartition);		// by EnabledNetdisk_MountVolume
	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition

	return;
}


VOID
EnabledNetdisk_DisMountVolume(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	PNETDISK_PARTITION		netdiskPartition;
	NETDISK_ENABLE_MODE		volumeIndex;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	NETDISK_ENABLE_MODE		volumeIndex2;
	ULONG					lfsDeviceExtIndex;

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_DisMountVolume: LfsDeviceExt = %p\n", LfsDeviceExt));
	
	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		//ASSERT(LFS_BUG);
		return;
	}

	if(netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 0;
	else if(netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt == LfsDeviceExt)
		lfsDeviceExtIndex = 1;
	else
		ASSERT(LFS_BUG);

	ASSERT(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState == VolumeMounted);
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].VolumeState = VolumeDismounted;
	LfsDeviceExt_Dereference(netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt);
	netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].LfsDeviceExt = NULL;

	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&EnabledNetdisk->NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	if(volumeIndex == NETDISK_SECONDARY) 
	{
		LfsTable_DeleteNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				FALSE
				);
	}
	else if(volumeIndex == NETDISK_PRIMARY || volumeIndex == NETDISK_SECONDARY2PRIMARY)
	{
		LfsTable_DeleteNetDiskPartitionInfoUser(
				GlobalLfs.LfsTable,
				&netdiskPartitionInfo,
				TRUE
				);
	}
			
	for(volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++)
	{
		if(netdiskPartition->NetdiskVolume[volumeIndex2][0].LfsDeviceExt)
			break;
		if(netdiskPartition->NetdiskVolume[volumeIndex2][1].LfsDeviceExt)
			break;
	}

	if(volumeIndex2 == (NETDISK_SECONDARY2PRIMARY+1))
	{
		if(BooleanFlagOn(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTED))
			SetFlag(netdiskPartition->Flags, NETDISK_PARTITION_DISMOUNTED);

		if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject)
		{
			ObDereferenceObject(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject);
			netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
		}
	}
	
	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
	NetdiskPartition_Dereference(netdiskPartition);		// by EnabledNetdisk_MountVolume
	
	if(volumeIndex == NETDISK_SECONDARY2PRIMARY 
		&& netdiskPartition->NetdiskVolume[volumeIndex][lfsDeviceExtIndex].MountStatus == STATUS_SUCCESS
		&& EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY
		&& IsStoppedNetdisk(EnabledNetdisk) == FALSE 
		&& IsStoppedPrimary(EnabledNetdisk) == TRUE)
	{
		PLIST_ENTRY				netdiskPartitionListEntry;
		KIRQL					oldIrql;


		KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);
	
		for(netdiskPartitionListEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
			netdiskPartitionListEntry != &EnabledNetdisk->NetdiskPartitionQueue;
			netdiskPartitionListEntry = netdiskPartitionListEntry->Flink)
		{	
			PNETDISK_PARTITION			netdiskPartition2;
			PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
		
			netdiskPartition2 = CONTAINING_RECORD (netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
			if(netdiskPartition2->NetdiskVolume[NETDISK_SECONDARY][0].LfsDeviceExt
				|| netdiskPartition2->NetdiskVolume[NETDISK_SECONDARY][1].LfsDeviceExt)
			{
				netdiskManagerRequest = AllocNetdiskManagerRequest(FALSE);
				netdiskManagerRequest->RequestType = NETDISK_MANAGER_REQUEST_TOUCH_VOLUME;
				NetdiskPartition_Reference(netdiskPartition2);
				netdiskManagerRequest->NetdiskPartition = netdiskPartition2;

				QueueingNetdiskManagerRequest(
					GlobalLfs.NetdiskManager,
					netdiskManagerRequest
					);
			}
		}

		KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
	}

	return;
}


BOOLEAN
EnabledNetdisk_Secondary2Primary(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	PNETDISK_PARTITION	netdiskPartition;
	NETDISK_ENABLE_MODE	volumeIndex;

	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			upgradeStatus;

	BOOLEAN				result;
	NETDISK_INFORMATION	netdiskInformation;
	NETDISK_ENABLE_MODE	netdiskEnableMode;


	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_Secondary2Primary: LfsDeviceExt = %p\n", LfsDeviceExt));
	
	if(EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY)
		return TRUE;

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return FALSE;
	}
	
	ASSERT(EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY);

	upgradeStatus = NDCtrlUpgradeToWrite(
							EnabledNetdisk->NetDiskInformation.SlotNo,
							&ioStatusBlock
							) ;

	if(!NT_SUCCESS(upgradeStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("[LFS] SecondaryToPrimaryThreadProc: NDCtrlUpgradeToWrite() function failed.\n"));
	} else if(!NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("[LFS] SecondaryToPrimaryThreadProc: Upgrading failed. NetWorking failed.\n"));
	} else {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("[LFS] SecondaryToPrimaryThreadProc: Primary mode.\n"));
	}

	result = IsNetDisk(
				netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
				&netdiskInformation
				);
	
	if(result == FALSE)
	{
		NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
		return FALSE;	
	}
	
	if(netdiskInformation.DesiredAccess & GENERIC_WRITE)
	{
		if(netdiskInformation.GrantedAccess & GENERIC_WRITE)
			netdiskEnableMode = NETDISK_PRIMARY;
		else
			netdiskEnableMode = NETDISK_SECONDARY;
	} else
		netdiskEnableMode = NETDISK_READ_ONLY;

	if(netdiskInformation.EnabledTime.QuadPart != EnabledNetdisk->NetDiskInformation.EnabledTime.QuadPart)
	{
		NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
		return FALSE;	
	}

	if(netdiskEnableMode == NETDISK_PRIMARY)
	{
		EnabledNetdisk->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
		return TRUE;
	}
	else
	{
		NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
		return FALSE;
	}
}


VOID
EnabledNetdisk_UnplugNetdisk(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	KIRQL				oldIrql;
	PNETDISK_PARTITION	netdiskPartition;
	NETDISK_ENABLE_MODE	volumeIndex;
	BOOLEAN				result;
	NETDISK_INFORMATION	netdiskInformation;
	NTSTATUS			unplugStatus;
	IO_STATUS_BLOCK		ioStatusBlock;
	

	KeAcquireSpinLock(&EnabledNetdisk->SpinLock, &oldIrql);
	if(BooleanFlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING)
		|| BooleanFlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGED)
		|| BooleanFlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_SURPRISE_REMOVAL))
	{
		KeReleaseSpinLock(&EnabledNetdisk->SpinLock, oldIrql);
		return;
	}

	SetFlag(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING);
	KeReleaseSpinLock(&EnabledNetdisk->SpinLock, oldIrql);

	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		KeAcquireSpinLock(&EnabledNetdisk->SpinLock, &oldIrql);
		ClearFlag(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING);
		KeReleaseSpinLock(&EnabledNetdisk->SpinLock, oldIrql);
		return;
	}

	result = IsNetDisk(
				netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
				&netdiskInformation
				);
	
	if(result == TRUE)
	{
		unplugStatus = NDCtrlUnplug(
						EnabledNetdisk->NetDiskInformation.SlotNo,
						&ioStatusBlock
						);

	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
					("EnabledNetdisk_UnplugNetdisk:	result = %d unplugStatus = %x\n", result, unplugStatus));
	
	KeAcquireSpinLock(&EnabledNetdisk->SpinLock, &oldIrql);
	ClearFlag(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING);
	KeReleaseSpinLock(&EnabledNetdisk->SpinLock, oldIrql);

	return;
}


BOOLEAN
EnabledNetdisk_ThisVolumeHasSecondary(
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	PNETDISK_PARTITION		netdiskPartition;
	NETDISK_ENABLE_MODE		volumeIndex;
	ULONG					value;


	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						NULL,
						LfsDeviceExt,
						&volumeIndex
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return FALSE;
	}

	ASSERT(netdiskPartition->NetdiskVolume[volumeIndex][0].VolumeState == VolumeMounted
		|| netdiskPartition->NetdiskVolume[volumeIndex][1].VolumeState == VolumeMounted);
	
	InterlockedIncrement(&netdiskPartition->ExportCount);
	value = InterlockedDecrement(&netdiskPartition->ExportCount);

	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition

	return value ? TRUE : FALSE;
}


NTSTATUS
WakeUpVolume(
	PNETDISK_PARTITION	NetdiskPartition
	)
{
	HANDLE					fileHandle = NULL;	
	UNICODE_STRING			fileName;
	WCHAR					fileNameBuffer[NDFS_MAX_PATH];
	IO_STATUS_BLOCK			ioStatusBlock;
	NTSTATUS				createStatus;

	ACCESS_MASK				desiredAccess;
	ULONG					attributes;
	OBJECT_ATTRIBUTES		objectAttributes;
	LARGE_INTEGER			allocationSize;
	ULONG					fileAttributes;
	ULONG					shareAccess;
	ULONG					createDisposition;
	ULONG					createOptions;
	PFILE_FULL_EA_INFORMATION eaBuffer;
    ULONG					eaLength;			

    RtlInitEmptyUnicodeString( 
			&fileName,
            fileNameBuffer,
            sizeof(fileNameBuffer) 
			);

	RtlCopyUnicodeString(
			&fileName,
			&NetdiskPartition->NetdiskPartitionInformation.VolumeName
			);

	
	ioStatusBlock.Information = 0;

	createStatus = RtlAppendUnicodeToString(
						&fileName,
						WAKEUP_VOLUME_FILE_NAME
						);

	if(createStatus != STATUS_SUCCESS)
	{
		ASSERT(LFS_UNEXPECTED);
		return createStatus;
	}

	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
						| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT(desiredAccess == 0x0012019F);

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes(
			&objectAttributes,
			&fileName,
			attributes,
			NULL,
			NULL
			);
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN_IF;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;
	eaBuffer		  = NULL;
	eaLength		  = 0;
				
	createStatus = ZwCreateFile(
						&fileHandle,
						desiredAccess,
						&objectAttributes,
						&ioStatusBlock,
						&allocationSize,
						fileAttributes,
						shareAccess,
						createDisposition,
						createOptions,
						eaBuffer,
						0
						);
		
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
					("WakeUpVolume:	Remount volume %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
							&NetdiskPartition->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information ));

	if(createStatus == STATUS_SUCCESS)
	{
#if 0
		CHAR			buffer[5];

		ASSERT(ioStatusBlock.Information == FILE_CREATED);
		ZwReadFile(
			fileHandle,
			NULL,
			NULL,
			NULL,
			&ioStatusBlock,
			buffer,
			5,
			NULL,
			NULL
		    );
#endif
		ZwClose(fileHandle);
	}

	return createStatus;
}


PNETDISK_PARTITION
EnabledNetdisk_GetPrimaryPartition(
	IN PENABLED_NETDISK	EnabledNetdisk,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PLARGE_INTEGER	StartingOffset
	)
{
	PNETDISK_PARTITION	netdiskPartition;
	NTSTATUS			wakeUpVolumeStatus;
	NETDISK_INFORMATION	netdiskInformation;


	UNREFERENCED_PARAMETER(NetDiskAddress);
	UNREFERENCED_PARAMETER(UnitDiskNo);


	if(StartingOffset == NULL)
	{
		EnabledNetdisk_Reference(EnabledNetdisk);
		return &EnabledNetdisk->DummyNetdiskPartition;
	}
	
	netdiskPartition = LookUpNetdiskPartition(
						EnabledNetdisk,
						NULL,
						StartingOffset,
						NULL,
						NULL
						);
	if(netdiskPartition == NULL)
	{
		ASSERT(LFS_BUG);
		return NULL;
	}
	
	if(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject
		&& IsNetDisk(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, &netdiskInformation))
	{
		if(EnabledNetdisk->NetDiskInformation.EnabledTime.QuadPart != netdiskInformation.EnabledTime.QuadPart)
		{			
			return NULL;
		}
	}

	if(IsStoppedNetdisk(EnabledNetdisk))
		return NULL;

	ASSERT(EnabledNetdisk->NetdiskEnableMode == NETDISK_PRIMARY
			|| EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY);

	if(BooleanFlagOn(netdiskPartition->Flags, NETDISK_PARTITION_MOUNTED))
	{
		if(netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode][0].LfsDeviceExt
			|| netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode][1].LfsDeviceExt)
		{
			KIRQL	oldIrql;

			KeAcquireSpinLock(&netdiskPartition->SpinLock, &oldIrql);
			InterlockedIncrement(&netdiskPartition->ExportCount);
			KeReleaseSpinLock(&netdiskPartition->SpinLock, oldIrql);

			return netdiskPartition;
		}

		wakeUpVolumeStatus = WakeUpVolume(netdiskPartition);
		if(wakeUpVolumeStatus == STATUS_SUCCESS)
		{
			KIRQL	oldIrql;

			//ASSERT(netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode].LfsDeviceExt);

			if(netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode][0].LfsDeviceExt
				|| netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode][1].LfsDeviceExt)
			{		
				KeAcquireSpinLock(&netdiskPartition->SpinLock, &oldIrql);
				InterlockedIncrement(&netdiskPartition->ExportCount);
				KeReleaseSpinLock(&netdiskPartition->SpinLock, oldIrql);
		
				return netdiskPartition;
			}
			else
			{
				/* Secondary is Purgeing */
				NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
				return NULL;
			}
		}
	}
	else if(BooleanFlagOn(netdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED))
	{
		KIRQL	oldIrql;

		KeAcquireSpinLock(&netdiskPartition->SpinLock, &oldIrql);
		InterlockedIncrement(&netdiskPartition->ExportCount);
		KeReleaseSpinLock(&netdiskPartition->SpinLock, oldIrql);

		return netdiskPartition;
	}

	NetdiskPartition_Dereference(netdiskPartition);		// by LookUpNetdiskPartition
	netdiskPartition = NULL;
	
	return netdiskPartition;
}


VOID
EnabledNetdisk_ReturnPrimaryPartition(
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition 
	)
{
	UNREFERENCED_PARAMETER(EnabledNetdisk);
	
	if(NetdiskPartition == &EnabledNetdisk->DummyNetdiskPartition)
	{
		EnabledNetdisk_Dereference(EnabledNetdisk);
		return;
	}

	InterlockedDecrement(&NetdiskPartition->ExportCount);
	NetdiskPartition_Dereference(NetdiskPartition);  // by EnabledNetdisk_GetPrimaryPartition

	return;
}


PENABLED_NETDISK
LookUpEnabledNetdisk(
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo
	)
{	
	PENABLED_NETDISK	enabledNetdisk;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;


	KeAcquireSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql);

    for (enabledNetdisk = NULL, listEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
         listEntry != &NetdiskManager->EnabledNetdiskQueue;
         enabledNetdisk = NULL, listEntry = listEntry->Flink)
	{
		enabledNetdisk = CONTAINING_RECORD (listEntry, ENABLED_NETDISK, ListEntry);
		ASSERT(enabledNetdisk->ReferenceCount > 0);

		if(!RtlEqualMemory(
				&enabledNetdisk->NetDiskInformation.NetDiskAddress.Node,
				NetDiskAddress->Node,
				6
				))
		{
			continue;
		}

		if(enabledNetdisk->NetDiskInformation.NetDiskAddress.Port != NetDiskAddress->Port)
		{
			continue;
		}

		if(enabledNetdisk->NetDiskInformation.UnitDiskNo != UnitDiskNo)
		{
			continue;
		}

		EnabledNetdisk_Reference(enabledNetdisk);
		
		break;
	}

	KeReleaseSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, oldIrql);

	return enabledNetdisk;
}


VOID
EnabledNetdisk_Reference (
	PENABLED_NETDISK	EnabledNetdisk
	)
{
    LONG result;
	
    result = InterlockedIncrement (&EnabledNetdisk->ReferenceCount);
    ASSERT (result >= 0);
}


VOID
EnabledNetdisk_Dereference(
	PENABLED_NETDISK	EnabledNetdisk
	)
{
    LONG result;


    result = InterlockedDecrement(&EnabledNetdisk->ReferenceCount);
    
	if(result < 0)
	{
		ASSERT (LFS_BUG);
		return;
	}

    if (result == 0) 
	{
		PNETDISK_MANAGER	netdiskManager = EnabledNetdisk->NetdiskManager;

		ASSERT(EnabledNetdisk->ListEntry.Flink == EnabledNetdisk->ListEntry.Blink);

		if(EnabledNetdisk->DriveLayoutInformation)
			ExFreePoolWithTag(
				EnabledNetdisk->DriveLayoutInformation,
				NETDISK_MANAGER_TAG
				);

		ExFreePoolWithTag(
			EnabledNetdisk,
			NETDISK_MANAGER_ENABLED_TAG
			);

#if DBG
		InterlockedDecrement(&LfsObjectCounts.EnabledNetdiskCount);
#endif

		NetdiskManager_Dereference(netdiskManager);

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
				("EnabledNetdisk_Dereference: EnabledNetdisk = %p, EnabledNetdiskCount = %d\n", EnabledNetdisk, LfsObjectCounts.EnabledNetdiskCount)) ;
	}
}


PNETDISK_PARTITION
AllocateNetdiskPartition(
	IN PENABLED_NETDISK		EnabledNetdisk
	) 
{
	PNETDISK_PARTITION	netdiskPartition;


	netdiskPartition = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(NETDISK_PARTITION),
						NETDISK_MANAGER_PARTITION_TAG
						);
	
	if (netdiskPartition == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	EnabledNetdisk_Reference(EnabledNetdisk);

	RtlZeroMemory(
		netdiskPartition,
		sizeof(NETDISK_PARTITION)
		);
	

	KeInitializeSpinLock(&netdiskPartition->SpinLock);
	netdiskPartition->ReferenceCount = 1;
	InitializeListHead(&netdiskPartition->ListEntry);
	netdiskPartition->EnabledNetdisk = EnabledNetdisk;
	
    RtlInitEmptyUnicodeString( 
				&netdiskPartition->NetdiskPartitionInformation.VolumeName,
                netdiskPartition->NetdiskPartitionInformation.VolumeNameBuffer,
                sizeof(netdiskPartition->NetdiskPartitionInformation.VolumeNameBuffer) 
				);
	
#if DBG
	InterlockedIncrement(&LfsObjectCounts.NetdiskPartitionCount);
#endif

	SPY_LOG_PRINT(LFS_DEBUG_NETDISK_MANAGER_NOISE,
		("AllocateNetdiskPartition netdiskPartition = %p, netdiskPartition = %d\n", netdiskPartition, LfsObjectCounts.NetdiskPartitionCount));

	return netdiskPartition;
}


PNETDISK_PARTITION
LookUpNetdiskPartition(
	IN  PENABLED_NETDISK		EnabledNetdisk,
    IN  PPARTITION_INFORMATION	PartitionInformation,
	IN  PLARGE_INTEGER			StartingOffset,
	IN  PLFS_DEVICE_EXTENSION	LfsDeviceExt,	
	OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{	
	PNETDISK_PARTITION	netdiskPartition;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;


	KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) 
	{
		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT(netdiskPartition->ReferenceCount > 0);

		if(LfsDeviceExt)
		{
			NETDISK_ENABLE_MODE	volumeIndex;
				
			for(volumeIndex=0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++)
			{
				if(LfsDeviceExt == netdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt)
					break;

				if(LfsDeviceExt == netdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt)
					break;
			}
			if(volumeIndex != (NETDISK_SECONDARY2PRIMARY+1))
			{
				if(NetdiskEnableMode)
					*NetdiskEnableMode = volumeIndex;
				NetdiskPartition_Reference(netdiskPartition);
				break;
			}
		}	

		if(PartitionInformation 
			&& PartitionInformation->StartingOffset.QuadPart 
				== netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart)
		{
			ASSERT(PartitionInformation->PartitionNumber 
				== netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionNumber);
			ASSERT(PartitionInformation->RewritePartition 
					== netdiskPartition->NetdiskPartitionInformation.PartitionInformation.RewritePartition);
			
			NetdiskPartition_Reference(netdiskPartition);
			break;
		}

		if(StartingOffset == NULL)
			continue;

		if(StartingOffset->QuadPart 
			== netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart)
		{
			NetdiskPartition_Reference(netdiskPartition);
			break;
		}
	}

	KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);

	return netdiskPartition;
}


VOID
NetdiskPartition_Reference (
	PNETDISK_PARTITION	NetdiskPartition
	)
{
    LONG result;
	
    result = InterlockedIncrement (&NetdiskPartition->ReferenceCount);
    ASSERT (result >= 0);
}


VOID
NetdiskPartition_Dereference(
	PNETDISK_PARTITION	NetdiskPartition
	)
{
    LONG result;


    result = InterlockedDecrement(&NetdiskPartition->ReferenceCount);
    
	if(result < 0)
	{
		ASSERT (LFS_BUG);
		return;
	}

    if (result == 0) 
	{
		PENABLED_NETDISK	enabledNetdisk = NetdiskPartition->EnabledNetdisk;
#if DBG
		NETDISK_ENABLE_MODE	volumeIndex;

		ASSERT(NetdiskPartition->ListEntry.Flink == NetdiskPartition->ListEntry.Blink);
				
		for(volumeIndex = 0; volumeIndex < NETDISK_SECONDARY2PRIMARY+1; volumeIndex++)
		{
			ASSERT(NetdiskPartition->NetdiskVolume[volumeIndex][0].LfsDeviceExt == NULL);
			ASSERT(NetdiskPartition->NetdiskVolume[volumeIndex][1].LfsDeviceExt == NULL);
		}
		ASSERT(NetdiskPartition->ExportCount == 0);
#endif

		ExFreePoolWithTag(
			NetdiskPartition,
			NETDISK_MANAGER_PARTITION_TAG
			);

#if DBG
		InterlockedDecrement(&LfsObjectCounts.NetdiskPartitionCount);
#endif

		EnabledNetdisk_Dereference(enabledNetdisk);

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO,
				("NetdiskPartition_Dereference: NetdiskPartition = %p, NetdiskPartitionCount = %d\n", NetdiskPartition, LfsObjectCounts.NetdiskPartitionCount)) ;
	}
}