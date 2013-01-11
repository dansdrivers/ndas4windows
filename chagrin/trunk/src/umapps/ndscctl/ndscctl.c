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
#include "xdbgprn.h"

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif


const DWORD DeviceFileAccess = GENERIC_READ;
const DWORD DeviceFileShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

//////////////////////////////////////////////////////////////////////////
//
// Device enumeration helper
//

HANDLE
pOpenDiskDeviceByNumber(DWORD DiskNumber)
{
	// Device Name Format: \\.\PhysicalDriveXX
	TCHAR deviceName[24];
	HANDLE hDisk;
	HRESULT	result;

	result = StringCchPrintf(
		deviceName, RTL_NUMBER_OF(deviceName), 
		_T("\\\\.\\PhysicalDrive%d"), DiskNumber);

	hDisk = CreateFile(
		deviceName, 
		DeviceFileAccess, 
		DeviceFileShareMode,
		NULL, 
		OPEN_EXISTING, 
		0, 
		NULL);
	if (hDisk == INVALID_HANDLE_VALUE)
	{
		DebugPrint(3, _T("Can not open %s\n"), deviceName);
	}

	return hDisk;
}

PSTORAGE_DESCRIPTOR_HEADER
pStorageQueryProperty(
	HANDLE hDevice,
	STORAGE_PROPERTY_ID PropertyId,
	STORAGE_QUERY_TYPE  QueryType)
{
	DWORD bufferLength = sizeof(STORAGE_DESCRIPTOR_HEADER);
	STORAGE_PROPERTY_QUERY spquery;
	PSTORAGE_DESCRIPTOR_HEADER buffer;
	DWORD returnedLength;
	BOOL fSuccess;

	ZeroMemory(&spquery, sizeof(spquery));
	spquery.PropertyId = PropertyId;
	spquery.QueryType = QueryType;


	buffer =HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength);
	if (buffer == NULL)
	{
		return NULL;
	}

	fSuccess = DeviceIoControl(
		hDevice, IOCTL_STORAGE_QUERY_PROPERTY, 
		&spquery, sizeof(spquery),
		buffer, bufferLength,
		&returnedLength, NULL);
	if (!fSuccess)
	{
		DebugPrint(3,
			_T("IOCTL_STORAGE_QUERY_PROPERTY(HEADER) failed, error=0x%X\n"),
			GetLastError());
		HeapFree(GetProcessHeap(), 0, buffer);
		return NULL;
	}


	// We only retrieved the header, now we reallocate the buffer
	// required for the actual query
	bufferLength = buffer->Size; 
	HeapFree(GetProcessHeap(), 0, buffer);
	buffer = NULL;

	buffer =HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength);
	if (buffer == NULL)
	{
		return NULL;
	}
	
	// now we can query the actual property with the proper size
	fSuccess = DeviceIoControl(
		hDevice, IOCTL_STORAGE_QUERY_PROPERTY, 
		&spquery, sizeof(spquery),
		buffer, bufferLength,
		&returnedLength, NULL);
	if (!fSuccess)
	{
		DebugPrint(3, 
			_T("IOCTL_STORAGE_QUERY_PROPERTY(DATA) failed, error=0x%X\n"),
			GetLastError());
		HeapFree(GetProcessHeap(), 0, buffer);
		return NULL;
	}

	return buffer;
}

BOOL
pScsiGetAddress(
	HANDLE hDevice, 
	SCSI_ADDRESS* ScsiAddress)
{
	DWORD returnedLength;
	BOOL fSuccess;
	
	fSuccess = DeviceIoControl(
		hDevice, IOCTL_SCSI_GET_ADDRESS,
		NULL, 0,
		ScsiAddress, sizeof(SCSI_ADDRESS),
		&returnedLength, NULL);

	return fSuccess;
}

BOOL
pGetScsiAddressForDisk(
	IN HANDLE hDisk,
	OUT PSCSI_ADDRESS ScsiAddress)
{
	PSTORAGE_ADAPTER_DESCRIPTOR	adapterDescriptor;
	SCSI_ADDRESS scsiAddress = {0};
	BOOL fSuccess;

	//
	// Query Storage Property
	//
	DebugPrint(3, _T("DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)\n"));

	adapterDescriptor =	(PSTORAGE_ADAPTER_DESCRIPTOR)pStorageQueryProperty(hDisk, StorageAdapterProperty, PropertyStandardQuery);
	if (adapterDescriptor == NULL)
	{
		DebugPrint(3, _T("pStorageQueryAdapterProperty failed, error=0x%X\n"),
			GetLastError());
		return FALSE;
	}

	//
	// Ignore non-SCSI device
	//

	if (BusTypeScsi != adapterDescriptor->BusType) 
	{
		DebugPrint(3, _T("Ignoring non-scsi bus\n"));
		return FALSE;
	}

	HeapFree(GetProcessHeap(), 0, adapterDescriptor);
	adapterDescriptor = NULL;

	//
	// Query SCSI Address, given that the physical drive is a SCSI device
	//

	DebugPrint(3, _T("DeviceIoControl(IOCTL_SCSI_GET_ADDRESS)\n"));

	fSuccess = pScsiGetAddress(hDisk, &scsiAddress);
	if (!fSuccess) 
	{
		DebugPrint(3, _T("pScsiGetAddress failed, error=0x%X\n"),
			GetLastError());
		return FALSE;
	}

	DebugPrint(3, _T("SCSIAddress: Len: %d, PortNumber: %d, ")
		_T("PathId: %d, TargetId: %d, Lun: %d\n"),
		(DWORD) scsiAddress.Length,
		(DWORD) scsiAddress.PortNumber,
		(DWORD) scsiAddress.PathId,
		(DWORD) scsiAddress.TargetId,
		(DWORD) scsiAddress.Lun);

	//
	// Return the result
	//
	*ScsiAddress = scsiAddress;
	return TRUE;
}

HANDLE
pOpenScsiPortDeviceByNumber(
	DWORD ScsiPortNumber)
{
	TCHAR scsiPortName[MAX_PATH];
	HANDLE hScsiPort;

	//
	// Make up SCSI Port Name
	//

	StringCchPrintf(
		scsiPortName, MAX_PATH,
		_T("\\\\.\\Scsi%d:"), ScsiPortNumber);

	//
	// Open SCSI Port Device
	//

	/* SCSI miniport IOCTL required GENERIC_WRITE for now, really? */
	hScsiPort = CreateFile(
		scsiPortName,
		DeviceFileAccess | GENERIC_WRITE, 
		DeviceFileShareMode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hScsiPort == INVALID_HANDLE_VALUE) 
	{
		_tprintf(_T("CreateFile(%s) failed, error=0x%X\n"), 
			scsiPortName, GetLastError());
		return INVALID_HANDLE_VALUE;
	}

	return hScsiPort;
}


BOOL
pIoctlScsiPort(
	HANDLE	hScsiPort,
	ULONG	SrbIoctlCode,
	PUCHAR	Buffer,
	ULONG	BufferLength
){
	//
	// Make up IOCTL In-parameter
	//
	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	DWORD cbInBuffer = cbHeader + BufferLength;
	PBYTE lpInBuffer;
	DWORD cbReturned = 0;
	PSRB_IO_CONTROL pSrbIoControl;
	BOOL fSuccess;

	lpInBuffer = HeapAlloc(GetProcessHeap(), 0, cbInBuffer);

	pSrbIoControl = (PSRB_IO_CONTROL) lpInBuffer;
	pSrbIoControl->HeaderLength = cbHeader;
	CopyMemory(pSrbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	pSrbIoControl->Timeout = 10;
	pSrbIoControl->ControlCode = SrbIoctlCode;
	pSrbIoControl->Length = BufferLength;

	RtlCopyMemory(lpInBuffer + cbHeader, Buffer, BufferLength);

	//
	// Send the SRB IOCTL.
	//

	fSuccess = DeviceIoControl(
		hScsiPort,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	RtlCopyMemory(Buffer, lpInBuffer + cbHeader, BufferLength);

	if (!fSuccess) 
	{
		DebugPrint(0, _T("DeviceIoControl(IOCTL_SCSI_MINIPORT, ")
			_T("%x) failed, error=0x%X\n"), SrbIoctlCode, GetLastError());
		HeapFree(GetProcessHeap(), 0, lpInBuffer);
		return FALSE;
	}

	if (pSrbIoControl->ReturnCode != 1 /* SRB_STATUS_SUCCESS */ ) 
	{
		DebugPrint(0, _T("DeviceIoControl(IOCTL_SCSI_MINIPORT, %x) failed, ")
			_T("returnCode=0x%X, error=0x%X\n"),
			SrbIoctlCode, pSrbIoControl->ReturnCode, GetLastError());
		HeapFree(GetProcessHeap(), 0, lpInBuffer);
		return FALSE;
	}

	HeapFree(GetProcessHeap(), 0, lpInBuffer);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Export functions
//

NDSCCTLAPI
BOOL
WINAPI
NdscCtlGetScsiAddressOfDisk(
	IN DWORD			PhysicalDiskNumber,
	OUT PSCSI_ADDRESS	ScsiAddress
){
	HANDLE	phyDisk;
	BOOL	fSuccess;

	phyDisk = pOpenDiskDeviceByNumber(PhysicalDiskNumber);
	if(phyDisk == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	fSuccess = pGetScsiAddressForDisk(phyDisk, ScsiAddress);
	if(!fSuccess) {
		CloseHandle(phyDisk);
		return FALSE;
	}

	CloseHandle(phyDisk);
	return TRUE;
}

NDSCCTLAPI
PSTORAGE_DESCRIPTOR_HEADER
WINAPI
NdscCtlGetAdapterStoragePropertyFromDisk(
	IN DWORD			PhysicalDiskNumber,
	IN STORAGE_PROPERTY_ID PropertyId,
	IN STORAGE_QUERY_TYPE  QueryType
){
	HANDLE	phyDisk;
	BOOL	fSuccess;
	PSTORAGE_DESCRIPTOR_HEADER	desc;

	phyDisk = pOpenDiskDeviceByNumber(PhysicalDiskNumber);
	if(phyDisk == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	desc = (PSTORAGE_DESCRIPTOR_HEADER)pStorageQueryProperty(phyDisk, PropertyId, QueryType);
	if (desc == NULL)
	{
		DebugPrint(3, _T("pStorageQueryAdapterProperty failed, error=0x%X\n"),
			GetLastError());
		return NULL;
	}

	CloseHandle(phyDisk);
	return desc;
}

NDSCCTLAPI
PSTORAGE_DESCRIPTOR_HEADER
WINAPI
NdscCtlGetAdapterStoragePropertyFromScsiPort(
	IN DWORD			ScsiPortNumber,
	IN STORAGE_PROPERTY_ID PropertyId,
	IN STORAGE_QUERY_TYPE  QueryType
){
	HANDLE	scsiPort;
	BOOL	fSuccess;
	PSTORAGE_DESCRIPTOR_HEADER	desc;

	scsiPort = pOpenScsiPortDeviceByNumber(ScsiPortNumber);
	if(scsiPort == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	desc = (PSTORAGE_DESCRIPTOR_HEADER)pStorageQueryProperty(scsiPort, PropertyId, QueryType);
	if (desc == NULL)
	{
		DebugPrint(3, _T("pStorageQueryAdapterProperty failed, error=0x%X\n"),
			GetLastError());
		return NULL;
	}

	CloseHandle(scsiPort);
	return desc;
}

NDSCCTLAPI
BOOL
WINAPI
NdscCtlDeviceLockControl(
	IN DWORD						ScsiPortNumber,
	IN OUT PNDSCIOCTL_DEVICELOCK	NdasDevLockControl
){
	HANDLE	scsiPort;
	BOOL	fSuccess;

	scsiPort = pOpenScsiPortDeviceByNumber(ScsiPortNumber);
	if(scsiPort == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	fSuccess = pIoctlScsiPort(	scsiPort,
								NDASSCSI_IOCTL_DEVICE_LOCK,
								(PUCHAR)NdasDevLockControl,
								sizeof(NDSCIOCTL_DEVICELOCK));

	CloseHandle(scsiPort);
	return fSuccess;
}

