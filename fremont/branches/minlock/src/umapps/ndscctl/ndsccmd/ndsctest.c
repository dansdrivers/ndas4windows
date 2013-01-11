#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

//
// Workaround for including winioctl.h with ntddscsi, ntddstor
//
// DEVICE_TYPE is required from ntddstor.h
// which is defined from devioctl.h or winioctl.h
//
// We will provide DEVICE_TYPE here and undefine it right after inclusion
//
#ifndef DEVICE_TYPE
#define DEVICE_TYPE DWORD
#endif

#include <windows.h>
#include <ntddscsi.h>
#include <ntddstor.h>
#include <winioctl.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include "ndasscsictl.h"


int __cdecl _tmain(int argc, TCHAR** argv)
{
	BOOL					fSuccess;
	NDSCIOCTL_DEVICELOCK	ndasDevLockControl;
	ULONG					phyDiskNo;
	SCSI_ADDRESS			scsiAddr;
	PSTORAGE_ADAPTER_DESCRIPTOR	adapterDescriptor;



	if (argc <= 1)
	{
		_tprintf(_T("%s [physical disk number]\n"), argv[0]);
		return -1;
	}
	phyDiskNo = _tcstoul(argv[1], NULL, 10);;
	_tprintf(_T("Physical disk %d\n"), phyDiskNo);

	//
	//  Get the scsi address of the physical disk.
	//

	fSuccess = NdscCtlGetScsiAddressOfDevice(phyDiskNo, _T("CdRom"), &scsiAddr);
	if(!fSuccess) {
		_tprintf(_T("Could not get the scsi address.\n"));
		return -1;
	}
	//
	//  SCSI adapter property from the disk
	//
	adapterDescriptor =	(PSTORAGE_ADAPTER_DESCRIPTOR)NdscCtlGetAdapterStoragePropertyFromDisk(
				phyDiskNo,
				StorageAdapterProperty,
				PropertyStandardQuery);
	if (adapterDescriptor == NULL)
	{
		_tprintf(_T("NdscCtlGetAdapterStorageProperty failed, error=0x%X\n"),
			GetLastError());
		return 1;
	}
	_tprintf(_T("Version=0x%x\n"),adapterDescriptor->Version);
	_tprintf(_T("Size=%d\n"),adapterDescriptor->Size);
	_tprintf(_T("MaximumTransferLength=%u\n"),adapterDescriptor->MaximumTransferLength);
	_tprintf(_T("MaximumPhysicalPages=%u(0x%x)\n"), adapterDescriptor->MaximumPhysicalPages, adapterDescriptor->MaximumPhysicalPages);
	_tprintf(_T("AlignmentMask=%x\n"),adapterDescriptor->AlignmentMask);
	_tprintf(_T("AdapterUsesPio=%d\n"),adapterDescriptor->AdapterUsesPio);
	_tprintf(_T("AdapterScansDown=%d\n"),adapterDescriptor->AdapterScansDown);
	_tprintf(_T("CommandQueueing=%d\n"),adapterDescriptor->CommandQueueing);
	_tprintf(_T("AcceleratedTransfer=%d\n"),adapterDescriptor->AcceleratedTransfer);
	_tprintf(_T("BusType=%d\n"),adapterDescriptor->BusType);
	_tprintf(_T("BusMajorVersion=%d\n"),adapterDescriptor->BusMajorVersion);
	_tprintf(_T("BusMinorVersion=%d\n"),adapterDescriptor->BusMinorVersion);
	_tprintf(_T("\n"));
	HeapFree(GetProcessHeap(), 0, adapterDescriptor);


#if 0
	//
	// Test
	//
	_tprintf("Acquiring XIFS lock.\n");
	ndasDevLockControl.LockId = NDSCLOCK_ID_XIFS;
	ndasDevLockControl.LockOpCode = NDSCLOCK_OPCODE_ACQUIRE;
	ndasDevLockControl.AdvancedLock = FALSE;
	ndasDevLockControl.AddressRangeValid = TRUE;
	ndasDevLockControl.RequireLockAcquisition = FALSE;
	ndasDevLockControl.StartingAddress = 0;
	ndasDevLockControl.EndingAddress = 0;
	ndasDevLockControl.ContentionTimeOut = 0;
	RtlZeroMemory(ndasDevLockControl.LockData, NDSCLOCK_LOCKDATA_LENGTH);

	fSuccess = NdscCtlDeviceLockControl(scsiAddr.PortNumber, &ndasDevLockControl);
	if(!fSuccess) {
		_tprintf("Could not acquire the lock.\n");
		return -1;
	}

	_tprintf("Lock data = %d\n\n", *(PDWORD)ndasDevLockControl.LockData);
	_tprintf("Querying the lock data.\n");
//	ndasDevLockControl.RequireLockAcquisition = TRUE;
	ndasDevLockControl.LockOpCode = NDSCLOCK_OPCODE_GETDATA;
	RtlZeroMemory(ndasDevLockControl.LockData, NDSCLOCK_LOCKDATA_LENGTH);

	fSuccess = NdscCtlDeviceLockControl(scsiAddr.PortNumber, &ndasDevLockControl);
	if(!fSuccess) {
		_tprintf("Could not query the lock data.\n");
		return -1;
	}
	_tprintf("Lock data = %d\n\n", *(PDWORD)ndasDevLockControl.LockData);
	_tprintf("Querying the owner.\n");
//	ndasDevLockControl.RequireLockAcquisition = TRUE;
	ndasDevLockControl.LockOpCode = NDSCLOCK_OPCODE_QUERY_OWNER;
	RtlZeroMemory(ndasDevLockControl.LockData, NDSCLOCK_LOCKDATA_LENGTH);

	fSuccess = NdscCtlDeviceLockControl(scsiAddr.PortNumber, &ndasDevLockControl);
	if(!fSuccess) {
		_tprintf("Could not query the lock owner.\n");
		return -1;
	}
	_tprintf("port %02x:%02x address %02x:%02x:%02x:%02x:%02x:%02x\n\n", 
				ndasDevLockControl.LockData[0],
				ndasDevLockControl.LockData[1],
				ndasDevLockControl.LockData[2],
				ndasDevLockControl.LockData[3],
				ndasDevLockControl.LockData[4],
				ndasDevLockControl.LockData[5],
				ndasDevLockControl.LockData[6],
				ndasDevLockControl.LockData[7]
				);
	_tprintf("Releasing XIFS lock.\n");
	ndasDevLockControl.LockOpCode = NDSCLOCK_OPCODE_RELEASE;
	RtlZeroMemory(ndasDevLockControl.LockData, NDSCLOCK_LOCKDATA_LENGTH);

	fSuccess = NdscCtlDeviceLockControl(scsiAddr.PortNumber, &ndasDevLockControl);
	if(!fSuccess) {
		_tprintf("Could not release the lock.\n");
		return -1;
	}
	_tprintf("Lock data = %d\n", *(PDWORD)ndasDevLockControl.LockData);
#endif

	return 0;
}

