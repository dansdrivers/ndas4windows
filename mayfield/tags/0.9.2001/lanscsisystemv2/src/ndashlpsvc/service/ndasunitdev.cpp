#include "stdafx.h"
#include "ndasdev.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASUNITDEV
#include "xdebug.h"

#include "ndasinstman.h"
#include "lsbusioctl.h"

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

#if 0
//
// Creator for Unit Devices
//
// Get the unit device information from the NDAS device
// using NdasDeviceComm, and create the proper unit device
// instance - e.g. UnitDiskDevice or UnitDVDDevice, etc
//
// At this time, only UnitDiskDevice is created.
//
PCNdasUnitDevice
CNdasUnitDevice::
CreateUnitDevice(
	PCNdasDevice pParentDevice, 
	DWORD dwUnitNo)
{
	CNdasDeviceComm devComm(pParentDevice, dwUnitNo);
	PCNdasUnitDevice pUnitDevice = NULL;

	BOOL fSuccess = devComm.Initialize();
	if (!fSuccess) {
		DPError(_FT("Communication initialization failed to unit device %d"), dwUnitNo);
		return NULL;
	}

	//
	// Discover unit device information
	//
	NDAS_UNITDEVICE_INFORMATION udi = {0};

	fSuccess = devComm.GetUnitDeviceInformation(&udi);
	if (!fSuccess) {
		DPError(_FT("GetUnitDeviceInformation failed (UN%02d): "), dwUnitNo);
		return NULL;
	}

	if (udi.MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE) {

		pUnitDevice = CNdasUnitDiskDevice::CreateUnitDiskDevice(
			devComm, 
			pParentDevice, 
			dwUnitNo, 
			&udi);

	} else if (udi.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_COMPACT_BLOCK_DEVICE) {

		// Not implemented yet
		// pUnitDevice = CreateCompactBlockDevice()
		_ASSERTE(FALSE);

	} else if (udi.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE) {

//		pUnitDevice = new CNdasUnitCDROMDevice(
//			pParentDevice,
//			dwUnitNo,
//			&udi);
		// Not implemented yet
		_ASSERTE(FALSE);


	} else if (udi.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE) {

		// Not implemented yet
		_ASSERTE(FALSE);

	} else {

		// Not implemented yet
		_ASSERTE(FALSE);

	}

	return pUnitDevice;
}
#endif 

//
// Constructor
//

CNdasUnitDevice::
CNdasUnitDevice(
	PCNdasDevice pParentDevice, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_TYPE type,
	NDAS_UNITDEVICE_SUBTYPE subType,
	PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo) :
	m_pParentDevice(pParentDevice),
	m_dwUnitNo(dwUnitNo),
	m_type(type),
	m_subType(subType),
	m_status(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED),
	m_lastError(NDAS_UNITDEVICE_ERROR_NONE)
{
	_ASSERTE(pUnitDevInfo != NULL);
	_ASSERTE(!::IsBadReadPtr(pUnitDevInfo, sizeof(NDAS_UNITDEVICE_INFORMATION)));

	::CopyMemory(
		&m_devInfo, 
		pUnitDevInfo, 
		sizeof(NDAS_UNITDEVICE_INFORMATION));

	::ZeroMemory(
		&m_PrimaryHostInfo, 
		sizeof(NDAS_UNITDEVICE_PRIMARY_HOST_INFO));

	m_szStrBuf[0] = _T('\0');
}

//
// Destructor
//

PCNdasLogicalDevice
CNdasUnitDevice::
CreateLogicalDevice()
{
	return NULL;
}

NDAS_UNITDEVICE_ID
CNdasUnitDevice::
GetUnitDeviceId()
{
	NDAS_UNITDEVICE_ID unitDeviceId = { 
		m_pParentDevice->GetDeviceId(),
			m_dwUnitNo };

		return unitDeviceId;
}

//
// Unit Device Status Setter (internal)
//

VOID
CNdasUnitDevice::
SetStatus(NDAS_UNITDEVICE_STATUS newStatus)
{
	m_status = newStatus;
}

//
// Unit Device Status Getter
//

NDAS_UNITDEVICE_STATUS
CNdasUnitDevice::
GetStatus()
{
	return m_status;
}

//
// Get the last unit device error
//
NDAS_UNITDEVICE_ERROR
CNdasUnitDevice::
GetLastError()
{
	return m_lastError;
}

//
// Get the unit device information
//
VOID 
CNdasUnitDevice::
GetUnitDevInfo(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo)
{
	_ASSERTE(pUnitDevInfo != NULL);
	::CopyMemory(
		pUnitDevInfo, 
		&m_devInfo, 
		sizeof(NDAS_UNITDEVICE_INFORMATION));
}

//
// Set 
BOOL
CNdasUnitDevice::
SetMountStatus(BOOL bMounted)
{
	// TODO: Adjust this
	if (bMounted) {
		SetStatus(NDAS_UNITDEVICE_STATUS_MOUNTED);
	} else {
		SetStatus(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED);
	}

	return TRUE;
}

PCNdasDevice
CNdasUnitDevice::
GetParentDevice()
{
	return m_pParentDevice;
}

DWORD
CNdasUnitDevice::
GetUnitNo()
{
	return m_dwUnitNo;
}

ACCESS_MASK
CNdasUnitDevice::
GetGrantedAccess()
{
	return m_pParentDevice->GetGrantedAccess();
}

ACCESS_MASK
CNdasUnitDevice::
GetAllowingAccess()
{
	ACCESS_MASK granted = GetGrantedAccess();
	//
	// depends on the implementation of the driver
	// whether allows multiple write access or not
	//
	// TODO: Refine this!
	//
	return granted;
}

VOID
CNdasUnitDevice::
SetHostUsageCount(DWORD nROHosts, DWORD nRWHosts)
{
	m_devInfo.dwROHosts = nROHosts;
	m_devInfo.dwRWHosts = nRWHosts;
}

BOOL
CNdasUnitDevice::
GetHostUsageCount(
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts, 
	BOOL bUpdate)
{
	_ASSERTE(!IsBadWritePtr(lpnROHosts, sizeof(DWORD)));
	_ASSERTE(!IsBadWritePtr(lpnRWHosts, sizeof(DWORD)));

/*
	if (bUpdate) {
		BOOL fSuccess = m_pParentDevice->UpdateDeviceInfo();
		if (!fSuccess) {
			DPErrorEx(_FT("Update device status failed: "));
			return FALSE;
		}
	}
*/

	*lpnROHosts = m_devInfo.dwROHosts;
	*lpnRWHosts = m_devInfo.dwRWHosts;

	DPInfo(_FT("Host Usage Count: RO %d, RW %d.\n"), 
		m_devInfo.dwROHosts, 
		m_devInfo.dwRWHosts);

	return TRUE;
}

VOID 
CNdasUnitDevice::
UpdatePrimaryHostInfo(
	PNDAS_UNITDEVICE_PRIMARY_HOST_INFO pPrimaryHostInfo)
{
	::CopyMemory(
		&m_PrimaryHostInfo,
		pPrimaryHostInfo,
		sizeof(NDAS_UNITDEVICE_PRIMARY_HOST_INFO));

	m_PrimaryHostInfo.LastUpdate = ::GetTickCount();

	DPInfo(_FT("Primary Host Usage Updated: %s, Timestamp %d\n"), 
		this->ToString(),
		m_PrimaryHostInfo.LastUpdate);

}

LPCTSTR
CNdasUnitDevice::
ToString()
{
	if (m_szStrBuf[0] == _T('0')) {
		HRESULT hr = ::StringCchPrintf(
			m_szStrBuf, 
			CCH_STR_BUF,
			_T("%s[%02d]"),
			m_pParentDevice->ToString(),
			m_dwUnitNo);
		_ASSERTE(SUCCEEDED(hr));
	}
	return m_szStrBuf;
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitDiskDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

#if 0
//
// static functions
//
PCNdasUnitDiskDevice
CNdasUnitDiskDevice::
CreateUnitDiskDevice(
	CNdasDeviceComm& devComm,
	PCNdasDevice pParentDevice,
	DWORD dwUnitNo,
	PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo)
{
	//
	// Read DIB
	//
	// Read DIB should success, even if the unit disk does not contain
	// the DIB. If it fails, there should be some communication error.
	//

	BOOL fSuccess = FALSE;
	PNDAS_DIB_V2 pDIBv2 = NULL;

	//
	// ReadDIB is to allocate pDIBv2
	// We should free later if pDIBv2 != NULL
	//
	fSuccess = ReadDIB(pParentDevice, dwUnitNo, &pDIBv2, devComm);

	if (!fSuccess) {
		DPErrorEx(_FT("Reading or creating DIBv2 failed: "));
		DPError(_FT("Creating unit disk device instance failed.\n"));
		return NULL;
	}

	//
	// From now on, we have to HeapFree(pDIBv2)
	//

	NDAS_UNITDEVICE_DISK_TYPE diskType;

	switch (pDIBv2->iMediaType) {
	case NMT_SINGLE:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_SINGLE;
	case NMT_MIRROR:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER;
	case NMT_AGGREGATE:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED;
	case NMT_RAID_01:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_RAID_1;
	case NMT_VDVD:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD;

	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	default:
		//
		// Error! Invalid media type
		//
		DPErrorEx(_FT("Invalid media type %d(0x%08X)"), 
			pDIBv2->iMediaType,
			pDIBv2->iMediaType);
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		return NULL;
	}

	PCNdasUnitDiskDevice pUnitDiskDevice = new CNdasUnitDiskDevice(
		pParentDevice, 
		dwUnitNo,
		diskType,
		pUnitDevInfo,
		pDIBv2);

	::HeapFree(::GetProcessHeap(), 0, (LPVOID) pDIBv2);

	return pUnitDiskDevice;
}

BOOL
CNdasUnitDiskDevice::
ReadDIB(
	PCNdasDevice pParentDevice, 
	DWORD dwUnitNo,
	PNDAS_DIB_V2* ppDIBv2,
	CNdasDeviceComm& devComm)
{
	BOOL fSuccess = FALSE;

	//
	// ppDIBv2 will be set only if this function succeed.
	//

	PNDAS_DIB_V2 pDIBv2 = reinterpret_cast<PNDAS_DIB_V2>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512));

	if (NULL == pDIBv2) {
		//
		// Out of memory!
		//
		return FALSE;
	}

	fSuccess = devComm.ReadDiskBlock(
		reinterpret_cast<PBYTE>(pDIBv2),
		NDAS_BLOCK_LOCATION_DIB_V2);

	//
	// Regardless of the existence,
	// Disk Block should be read.
	// Failure means communication error or disk error
	//
	if (!fSuccess) {
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		DPError(_FT("ReadDiskBlock failed.\n"));
		return FALSE;
	}

	//
	// check signature
	//
	if(DISK_INFORMATION_SIGNATURE_V2 != pDIBv2->Signature ) {
		//
		// Read DIBv1
		//
		
		fSuccess = ReadDIBv1AndConvert(
			pParentDevice, 
			dwUnitNo, 
			pDIBv2, 
			devComm);

		if (!fSuccess) {
			::HeapFree(::GetProcessHeap(), 0, pDIBv2);
			return FALSE;
		}

		*ppDIBv2 = pDIBv2;
		return TRUE;
	}

	//
	// check version
	//
	if(IS_HIGHER_VERSION_V2(*pDIBv2))
	{
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		DPError(_FT("Unsupported version V2 failed"));
		return FALSE;
	}

	//
	// TODO: Lower version process (future code) ???
	//
	if(0)
	{
		DPError(_FT("lower version V2 detected"));
	}

	//
	// read additional locations if needed
	//
	if (pDIBv2->nUnitCount > MAX_UNITS_IN_V2) {

		UINT32 nTrailSectorCount = 
			GET_TRAIL_SECTOR_COUNT_V2(pDIBv2->nUnitCount);

		SIZE_T dwBytes = sizeof(NDAS_DIB_V2) + 512 * nTrailSectorCount;

		pDIBv2 = reinterpret_cast<PNDAS_DIB_V2>(
			::HeapReAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY, 
				pDIBv2, 
				dwBytes));

		for(DWORD i = 0; i < nTrailSectorCount; i++) {

			fSuccess = devComm.ReadDiskBlock(
				reinterpret_cast<PBYTE>(pDIBv2) + sizeof(NDAS_DIB_V2) + 512 * i,
				NDAS_BLOCK_LOCATION_ADD_BIND + i);

			if(!fSuccess) {
				DPError(_FT("Reading additional block failed %d"), 
					NDAS_BLOCK_LOCATION_ADD_BIND + i);
				::HeapFree(::GetProcessHeap, 0, pDIBv2);
				return FALSE;
			}
		}
	}

	// Virtual DVD check. Not supported ATM.

	*ppDIBv2 = pDIBv2;

	return TRUE;
}

BOOL 
CNdasUnitDiskDevice::
ReadDIBv1AndConvert(
	PCNdasDevice pParentDevice,
	DWORD dwUnitNo,
	PNDAS_DIB_V2 pDIBv2,
	CNdasDeviceComm &devComm)
{
	BOOL fSuccess = FALSE;
	NDAS_DIB DIBv1 = {0};
	PNDAS_DIB pDIBv1 = &DIBv1;

	fSuccess = devComm.ReadDiskBlock(
		reinterpret_cast<PBYTE>(pDIBv1), 
		NDAS_BLOCK_LOCATION_DIB_V1);

	if (!fSuccess) {
		DPErrorEx(_FT("Reading DIBv1 block failed: "));
		return FALSE;
	}

	//
	// If there is no DIB in the disk,
	// create a pseudo DIBv2
	//
	if (NDAS_DIB_SIGNATURE != pDIBv1->Signature ||
		IS_NDAS_DIBV1_WRONG_VERSION(*pDIBv1)) 
	{
		//
		// Create a pseudo DIBv2
		//
		InitializeDIBv2(pDIBv2, devComm.GetDiskSectorCount());

		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			pParentDevice->GetDeviceId().Node, 
			6);

		pDIBv2->UnitDisks[0].UnitNumber = dwUnitNo;

		return TRUE;
	}

	//
	// Convert V1 to V2
	//
	fSuccess = ConvertDIBv1toDIBv2(
		pDIBv1, 
		pDIBv2, 
		devComm.GetDiskSectorCount());

	if (!fSuccess) {

		//
		// Create a pseudo DIBv2 again!
		//
		InitializeDIBv2(pDIBv2, devComm.GetDiskSectorCount());

		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			pParentDevice->GetDeviceId().Node, 
			6);

		pDIBv2->UnitDisks[0].UnitNumber = dwUnitNo;

		return TRUE;
	}

	return TRUE;
}

VOID 
CNdasUnitDiskDevice::
InitializeDIBv2(
	PNDAS_DIB_V2 pDIBv2, 
	UINT64 nDiskSectorCount)
{
	_ASSERTE(!::IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	::ZeroMemory(pDIBv2, sizeof(NDAS_DIB_V2));

	pDIBv2->Signature = DISK_INFORMATION_SIGNATURE_V2;
	pDIBv2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIBv2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIBv2->sizeXArea = 2 * 1024 * 2;
	pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
	pDIBv2->iSectorsPerBit = 128;
	pDIBv2->sizeUserSpace -= pDIBv2->sizeUserSpace % pDIBv2->iSectorsPerBit;
	pDIBv2->iMediaType = NMT_SINGLE;
	pDIBv2->iSequence = 0;
	pDIBv2->nUnitCount = 1;
	pDIBv2->FlagDirty = 0;

}

BOOL
CNdasUnitDiskDevice::
ConvertDIBv1toDIBv2(
	const NDAS_DIB* pDIBv1, 
	PNDAS_DIB_V2 pDIBv2, 
	UINT64 nDiskSectorCount)
{
	_ASSERTE(!::IsBadReadPtr(pDIBv1, sizeof(NDAS_DIB)));
	_ASSERTE(!::IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	InitializeDIBv2(pDIBv2, nDiskSectorCount);

	switch(pDIBv1->DiskType) {
	case NDAS_DIB_DISK_TYPE_SINGLE:
		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			pDIBv1->EtherAddress, 
			6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;
		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;
		// not yet

	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->PeerUnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->PeerUnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->UnitNumber;

		pDIBv2->sizeUserSpace = 0; // same as that of master
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->PeerUnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->PeerUnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->UnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
	default:
		// _ASSERTE(FALSE);
		return FALSE;
	}

	return TRUE;
}
#endif

//
// Constructor
//


CNdasUnitDiskDevice::
CNdasUnitDiskDevice( 
	PCNdasDevice pParentDevice, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_DISK_TYPE diskType,
	DWORD dwAssocSequence,
	DWORD nAssocUnitDevices,
	PNDAS_UNITDEVICE_ID pAssocUnitDevices,
	ULONG ulUserBlocks,
	PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo,
	PVOID pAddTargetInfo) :
	m_dwAssocSequence(dwAssocSequence),
	m_nAssocUnitDevices(nAssocUnitDevices),
	m_pAssocUnitDevices(pAssocUnitDevices),
	m_ulUserBlocks(ulUserBlocks),
	m_pAddTargetInfo(pAddTargetInfo),
	CNdasUnitDevice(
		pParentDevice, dwUnitNo, 
		NDAS_UNITDEVICE_TYPE_DISK,
		CreateNdasUnitDeviceSubType(diskType),
		pUnitDevInfo)
{
	//
	// We don't have to make a copy of pAssocUnitDevices, pAddTargetInfo
	// as the Creator allocates them in the heap
	// 
#if 0
	//
	// Copy pAssocUnitDevices
	//
	if (nAssocUnitDevices > 0) {
		m_pAssocUnitDevices = new NDAS_UNITDEVICE_ID[nAssocUnitDevices];
		::CopyMemory(
			m_pAssocUnitDevices, 
			pAssocUnitDevices, 
			sizeof(NDAS_UNITDEVICE_ID) * nAssocUnitDevices);
	}
#endif
}

#if 0
CNdasUnitDiskDevice::
CNdasUnitDiskDevice(
	PCNdasDevice pParentDevice, 
	DWORD dwUnitNo,
	NDAS_UNITDEVICE_DISK_TYPE diskType,
	PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo,
	PNDAS_DIB_V2 pDIBv2) :
	m_pAssocUnitDevices(NULL),
	m_nAssocUnitDevices(0),
	m_dwAssocSequence(0),
	m_pAddTargetInfo(NULL),
	m_ulUserBlocks(0),
	CNdasUnitDevice(
		pParentDevice, dwUnitNo, 
		NDAS_UNITDEVICE_TYPE_DISK,
		CreateNdasUnitDeviceSubType(diskType),
		pUnitDevInfo)
{
	_ASSERTE(!::IsBadReadPtr(pParentDevice, sizeof(CNdasDevice)));
	_ASSERTE(!::IsBadReadPtr(pUnitDevInfo, sizeof(NDAS_UNITDEVICE_INFORMATION)));
	_ASSERTE(!::IsBadReadPtr(pDIBv2, sizeof(NDAS_DIB_V2))); // dynamic size

	ProcessDiskInfoBlock_(pDIBv2);
}
#endif

//
// Destructor
//

CNdasUnitDiskDevice::
~CNdasUnitDiskDevice()
{
	if (NULL != m_pAssocUnitDevices) {
		delete [] m_pAssocUnitDevices;
		m_pAssocUnitDevices = NULL;
	}

	if (NULL != m_pAddTargetInfo) {
		delete [] m_pAddTargetInfo;
		m_pAddTargetInfo = NULL;
	}
}

ULONG
CNdasUnitDiskDevice::
GetUserBlockCount()
{
	// Reserve 2 MB for internal use (2 * 2 * 1024 blocks)
	// ULONG ulDiskBlocks = (ULONG) m_devInfo.SectorCount - (2 * 1024 * 2); 

	//	return ulDiskBlocks;
	return m_ulUserBlocks;
}

ULONG
CNdasUnitDiskDevice::
GetPhysicalBlockCount()
{
	return m_devInfo.SectorCount;
}

PVOID 
CNdasUnitDiskDevice::
GetAddTargetInfo()
{
	return m_pAddTargetInfo;
}


VOID
CNdasUnitDiskDevice::
ProcessDiskInfoBlock_(PNDAS_DIB_V2 pDib)
{
	//
	// Verify DIB information
	//
	if (DISK_INFORMATION_SIGNATURE_V2 != pDib->Signature) {
		//
		// DIB signature mismatch implies a single disk
		// We do not make use of DIB for a single disk anymore.
		//
		m_diskType = NDAS_UNITDEVICE_DISK_TYPE_SINGLE;
		m_dwAssocSequence = 0;
		m_pAssocUnitDevices = new NDAS_UNITDEVICE_ID[1];
		_ASSERTE(NULL != m_pAssocUnitDevices);
		m_pAssocUnitDevices[0] = GetUnitDeviceId();
		return;
	}

	//
	// Now the signature is a valid one.
	//
	if (IS_HIGHER_VERSION_V2(*pDib))
	{
		m_diskType = NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN;
		return;
	}

	//
	// Now the version is a supported one (0.1)
	//
	switch (pDib->iMediaType) {
	case NMT_SINGLE:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
		m_diskType = NDAS_UNITDEVICE_DISK_TYPE_SINGLE;
		break;
	case NMT_MIRROR:
		m_diskType = (0 == pDib->iSequence) ? 
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER : 
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE;
		break;
	case NMT_AGGREGATE:
		m_diskType = NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED;
		break;
	case NMT_RAID_01:
		{
			PINFO_RAID_1 pInfo;

			m_diskType = NDAS_UNITDEVICE_DISK_TYPE_RAID_1;
			if(NULL != m_pAddTargetInfo)
			{
				delete m_pAddTargetInfo;
				m_pAddTargetInfo = NULL;
			}
			m_pAddTargetInfo = ::LocalAlloc(LPTR, sizeof(INFO_RAID_1));
			_ASSERTE(NULL != m_pAddTargetInfo);

			pInfo = (PINFO_RAID_1)m_pAddTargetInfo;

			pInfo->SectorsPerBit = pDib->iSectorsPerBit;
			pInfo->OffsetFlagInfo = (UINT32)(&((PNDAS_DIB_V2)NULL)->FlagDirty);
			pInfo->SectorBitmapStart = m_devInfo.SectorCount - 0x0f00;
			pInfo->SectorInfo = m_devInfo.SectorCount - 0x0002;
			pInfo->SectorLastWrittenInfo = m_devInfo.SectorCount - 0x1000;
		}
		break;
	default:
		m_diskType = NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN;
		return;
	}

	m_ulUserBlocks = pDib->sizeUserSpace;

	m_dwAssocSequence = pDib->iSequence;
	m_nAssocUnitDevices = pDib->nUnitCount;
	m_pAssocUnitDevices = new NDAS_UNITDEVICE_ID[m_nAssocUnitDevices];
	_ASSERTE(NULL != m_pAssocUnitDevices);

	for(int i = 0; i < m_nAssocUnitDevices; i++)
	{
		::CopyMemory(
			&m_pAssocUnitDevices[i].DeviceId, 
			pDib->UnitDisks[i].MACAddr, 
			sizeof(NDAS_DEVICE_ID));

		m_pAssocUnitDevices[i].UnitNo = pDib->UnitDisks[i].UnitNumber;
	}

	return;

	/*
	pDib->DiskType;
	pDib->PeerAddress;
	pDib->PeerUnitNumber;
	pDib->Sequence; // not used 
	pDib->EtherAddress; // ignored
	pDib->UnitNumber; // ignored
	pDib->UsageType; // obsolete
	pDib->Checksum; // obsolete
	pDib->MajorVersion;
	pDib->MinorVersion;
	*/
}

BOOL
CNdasUnitDiskDevice::
RegisterToLDM()
{
	//
	// register to LDM
	//

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	PCNdasLogicalDeviceManager pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	PCNdasLogicalDevice pLogDevice = NULL;

	NDAS_LOGICALDEVICE_TYPE logDevType;

	switch (GetSubType().DiskDeviceType) {
	case NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED:
		logDevType = NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED;
		break;
	case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER:
	case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE:
		logDevType = NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED;
		break;
	case NDAS_UNITDEVICE_DISK_TYPE_SINGLE:
		logDevType = NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
		break;
	case NDAS_UNITDEVICE_DISK_TYPE_RAID_1:
		logDevType = NDAS_LOGICALDEVICE_TYPE_DISK_RAID_1;
		break;
	case NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN:
	default:
		return FALSE;
	}

	if (NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE == logDevType) {
		//
		// call single disk registration
		//
		pLogDevice = pLdm->Register(
			logDevType,
			GetUnitDeviceId(),
			0,
			GetUnitDeviceId(),
			1);

	} else {

		//
		// bound disk registration
		//
		pLogDevice = pLdm->Register(
			logDevType,
			GetUnitDeviceId(),
			m_dwAssocSequence,
			m_pAssocUnitDevices[0],
			m_nAssocUnitDevices);
	}

	if (NULL == pLogDevice) {
		DPErrorEx(_FT("Registration to LDM failed: "));
		return FALSE;
	}

	if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED == pLogDevice->GetStatus()) {
		BOOL fSuccess = pLogDevice->Initialize();
		if (!fSuccess) {
			DPWarningEx(_FT("Logical Device Initialization failed: "));
		}
	}

	return TRUE;
}
#if 0
BOOL
CNdasUnitDiskDevice::
UpdateDeviceInfo()
{
	CNdasDeviceComm devComm(m_pParentDevice, m_dwUnitNo);

	BOOL fSuccess = devComm.Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Communication initialization failed to %s(%d): "), 
			CNdasDeviceId(m_pParentDevice->GetDeviceId()).ToString(), 
			m_dwUnitNo);
		return FALSE;
	}

	NDAS_UNITDEVICE_INFORMATION udi = {0};
	PNDAS_DIB_V2 pDIB_V2 = NULL;

	::ZeroMemory(&udi, sizeof(udi));

	fSuccess = devComm.GetUnitDeviceInformation(&udi);
	if (!fSuccess) {
		DPErrorEx(_FT("Getting unit device information failed from %s(%d): "), 
			CNdasDeviceId(m_pParentDevice->GetDeviceId()).ToString(),
			m_dwUnitNo);
		return FALSE;
	}

	//	fSuccess = devComm.GetDiskInfoBlock(&dib);
	fSuccess = ReadDIB(m_pParentDevice, m_dwUnitNo, &pDIB_V2, devComm);
	if (!fSuccess) {
		return FALSE;
	}

	// ProcessDiskInfoBlock_(&dib);
	ProcessDiskInfoBlock_(pDIB_V2);

	if (pDIB_V2) {
		::LocalFree(pDIB_V2);
	}

	return TRUE;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitCDROMDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

CNdasUnitCDROMDevice::
CNdasUnitCDROMDevice(
	PCNdasDevice pParentDevice,
	DWORD dwUnitNo,
	NDAS_UNITDEVICE_CDROM_TYPE cdromType,
	PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo) :
	CNdasUnitDevice(
		pParentDevice, 
		dwUnitNo, 
		NDAS_UNITDEVICE_TYPE_CDROM,
		CreateNdasUnitDeviceSubType(cdromType),
		pUnitDevInfo)
{
}

//
// Destructor
//

CNdasUnitCDROMDevice::
~CNdasUnitCDROMDevice()
{

}


//////////////////////////////////////////////////////////////////////////
//
// NDAS Unit Device Instance Creator
//
//////////////////////////////////////////////////////////////////////////


CNdasUnitDeviceCreator::
CNdasUnitDeviceCreator(PCNdasDevice pDevice, DWORD dwUnitNo) :
	m_pDevice(pDevice),
	m_dwUnitNo(dwUnitNo),
	m_devComm(pDevice, dwUnitNo)
{
	::ZeroMemory(&m_unitDevInfo, sizeof(NDAS_UNITDEVICE_INFORMATION));
}

PCNdasUnitDevice
CNdasUnitDeviceCreator::
CreateUnitDevice()
{
	PCNdasUnitDevice pUnitDevice = NULL;

	BOOL fSuccess = m_devComm.Initialize();
	if (!fSuccess) {
		DPError(_FT("Communication initialization to %s[%d] failed: "),
			m_pDevice->ToString(), 
			m_dwUnitNo);
		return NULL;
	}

	//
	// Discover unit device information
	//
	fSuccess = m_devComm.GetUnitDeviceInformation(&m_unitDevInfo);
	if (!fSuccess) {
		DPErrorEx(_FT("GetUnitDeviceInformation of %s[%d] failed: "), 
			m_pDevice->ToString(), 
			m_dwUnitNo);
		return NULL;
	}


	if (m_unitDevInfo.MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE) {

		pUnitDevice = CreateUnitDiskDevice();

	} else if (m_unitDevInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_COMPACT_BLOCK_DEVICE) {

		// Not implemented yet
		// pUnitDevice = CreateCompactBlockDevice()
		_ASSERTE(FALSE);

	} else if (m_unitDevInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE) {

		//		pUnitDevice = new CNdasUnitCDROMDevice(
		//			pParentDevice,
		//			dwUnitNo,
		//			&m_unitDevInfo);
		// Not implemented yet
		_ASSERTE(FALSE);


	} else if (m_unitDevInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE) {

		// Not implemented yet
		_ASSERTE(FALSE);

	} else {

		// Not implemented yet
		_ASSERTE(FALSE);

	}

	return pUnitDevice;
}

PCNdasUnitDiskDevice 
CNdasUnitDeviceCreator::
CreateUnitDiskDevice()
{
	//
	// Read DIB
	//
	// Read DIB should success, even if the unit disk does not contain
	// the DIB. If it fails, there should be some communication error.
	//

	BOOL fSuccess = FALSE;
	PNDAS_DIB_V2 pDIBv2 = NULL;

	//
	// ReadDIB is to allocate pDIBv2
	// We should free later if pDIBv2 != NULL
	//
	fSuccess = ReadDIB(&pDIBv2);

	if (!fSuccess) {
		DPErrorEx(_FT("Reading or creating DIBv2 failed: "));
		DPError(_FT("Creating unit disk device instance failed.\n"));
		return NULL;
	}

	//
	// From now on, we have to HeapFree(pDIBv2)
	//

	NDAS_UNITDEVICE_DISK_TYPE diskType;
	LPVOID pAddTargetInfo = NULL;

	switch (pDIBv2->iMediaType) {
	case NMT_SINGLE:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_SINGLE;
		break;
	case NMT_MIRROR:
		diskType = (0 == pDIBv2->iSequence) ?
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER :
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE;
		break;
	case NMT_AGGREGATE:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED;
		break;
	case NMT_RAID_01:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_RAID_1;
		{
			//
			// do not allocate with "new INFO_RAID_1"
			// destructor will delete with "delete [] m_pAddTargetInfo"
			//

			pAddTargetInfo = new BYTE[sizeof(INFO_RAID_1)];
			PINFO_RAID_1 pIR = reinterpret_cast<PINFO_RAID_1>(pAddTargetInfo);

			pIR->SectorsPerBit = pDIBv2->iSectorsPerBit;
			pIR->OffsetFlagInfo = (UINT32)(&((PNDAS_DIB_V2)NULL)->FlagDirty);
			pIR->SectorBitmapStart = m_unitDevInfo.SectorCount - 0x0f00;
			pIR->SectorInfo = m_unitDevInfo.SectorCount - 0x0002;
			pIR->SectorLastWrittenInfo = m_unitDevInfo.SectorCount - 0x1000;
		}
		break;
	case NMT_VDVD:
		diskType = NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD;
		break;
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	default:
		//
		// Error! Invalid media type
		//
		DPErrorEx(_FT("Invalid media type %d(0x%08X)"), 
			pDIBv2->iMediaType,
			pDIBv2->iMediaType);
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		return NULL;
	}

	NDAS_UNITDEVICE_ID udid;
	udid.DeviceId;
	udid.UnitNo;

	ULONG ulUserBlocks = pDIBv2->sizeUserSpace;
	DWORD dwAssocSequence = pDIBv2->iSequence;
	DWORD nAssocUnitDevices = pDIBv2->nUnitCount;
	PNDAS_UNITDEVICE_ID pAssocUnitDevices = new NDAS_UNITDEVICE_ID[nAssocUnitDevices];
	_ASSERTE(NULL != pAssocUnitDevices);

	for(DWORD i = 0; i < nAssocUnitDevices; i++)
	{
		::CopyMemory(
			&pAssocUnitDevices[i].DeviceId, 
			pDIBv2->UnitDisks[i].MACAddr, 
			sizeof(NDAS_DEVICE_ID));

		pAssocUnitDevices[i].UnitNo = pDIBv2->UnitDisks[i].UnitNumber;
	}

	PCNdasUnitDiskDevice pUnitDiskDevice = 
		new CNdasUnitDiskDevice(
			m_pDevice, 
			m_dwUnitNo,
			diskType,
			dwAssocSequence,
			nAssocUnitDevices,
			pAssocUnitDevices,
			ulUserBlocks,
			&m_unitDevInfo,
			pAddTargetInfo);

	::HeapFree(::GetProcessHeap(), 0, (LPVOID) pDIBv2);

	return pUnitDiskDevice;
}


BOOL
CNdasUnitDeviceCreator::
ReadDIB(PNDAS_DIB_V2* ppDIBv2)
{
	BOOL fSuccess = FALSE;

	//
	// ppDIBv2 will be set only if this function succeed.
	//

	PNDAS_DIB_V2 pDIBv2 = reinterpret_cast<PNDAS_DIB_V2>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 512));

	if (NULL == pDIBv2) {
		//
		// Out of memory!
		//
		return FALSE;
	}

	fSuccess = m_devComm.ReadDiskBlock(
		reinterpret_cast<PBYTE>(pDIBv2),
		NDAS_BLOCK_LOCATION_DIB_V2);

	//
	// Regardless of the existence,
	// Disk Block should be read.
	// Failure means communication error or disk error
	//
	if (!fSuccess) {
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		DPError(_FT("ReadDiskBlock failed.\n"));
		return FALSE;
	}

	//
	// check signature
	//
	if(DISK_INFORMATION_SIGNATURE_V2 != pDIBv2->Signature ) {
		//
		// Read DIBv1
		//
		
		fSuccess = ReadDIBv1AndConvert(pDIBv2);

		if (!fSuccess) {
			::HeapFree(::GetProcessHeap(), 0, pDIBv2);
			return FALSE;
		}

		*ppDIBv2 = pDIBv2;
		return TRUE;
	}

	//
	// check version
	//
	if(IS_HIGHER_VERSION_V2(*pDIBv2))
	{
		::HeapFree(::GetProcessHeap(), 0, pDIBv2);
		DPError(_FT("Unsupported version V2 failed"));
		return FALSE;
	}

	//
	// TODO: Lower version process (future code) ???
	//
	if(0)
	{
		DPError(_FT("lower version V2 detected"));
	}

	//
	// read additional locations if needed
	//
	if (pDIBv2->nUnitCount > MAX_UNITS_IN_V2) {

		UINT32 nTrailSectorCount = 
			GET_TRAIL_SECTOR_COUNT_V2(pDIBv2->nUnitCount);

		SIZE_T dwBytes = sizeof(NDAS_DIB_V2) + 512 * nTrailSectorCount;

		pDIBv2 = reinterpret_cast<PNDAS_DIB_V2>(
			::HeapReAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY, 
				pDIBv2, 
				dwBytes));

		for(DWORD i = 0; i < nTrailSectorCount; i++) {

			fSuccess = m_devComm.ReadDiskBlock(
				reinterpret_cast<PBYTE>(pDIBv2) + sizeof(NDAS_DIB_V2) + 512 * i,
				NDAS_BLOCK_LOCATION_ADD_BIND + i);

			if(!fSuccess) {
				DPError(_FT("Reading additional block failed %d"), 
					NDAS_BLOCK_LOCATION_ADD_BIND + i);
				::HeapFree(::GetProcessHeap, 0, pDIBv2);
				return FALSE;
			}
		}
	}

	// Virtual DVD check. Not supported ATM.

	*ppDIBv2 = pDIBv2;

	return TRUE;
}

BOOL 
CNdasUnitDeviceCreator::
ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2)
{
	BOOL fSuccess = FALSE;
	NDAS_DIB DIBv1 = {0};
	PNDAS_DIB pDIBv1 = &DIBv1;

	fSuccess = m_devComm.ReadDiskBlock(
		reinterpret_cast<PBYTE>(pDIBv1), 
		NDAS_BLOCK_LOCATION_DIB_V1);

	if (!fSuccess) {
		DPErrorEx(_FT("Reading DIBv1 block failed: "));
		return FALSE;
	}

	//
	// If there is no DIB in the disk,
	// create a pseudo DIBv2
	//
	if (NDAS_DIB_SIGNATURE != pDIBv1->Signature ||
		IS_NDAS_DIBV1_WRONG_VERSION(*pDIBv1)) 
	{
		//
		// Create a pseudo DIBv2
		//
		InitializeDIBv2(pDIBv2, m_devComm.GetDiskSectorCount());

		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			m_pDevice->GetDeviceId().Node, 
			6);

		pDIBv2->UnitDisks[0].UnitNumber = m_dwUnitNo;

		return TRUE;
	}

	//
	// Convert V1 to V2
	//
	fSuccess = ConvertDIBv1toDIBv2(
		pDIBv1, 
		pDIBv2, 
		m_devComm.GetDiskSectorCount());

	if (!fSuccess) {

		//
		// Create a pseudo DIBv2 again!
		//
		InitializeDIBv2(pDIBv2, m_devComm.GetDiskSectorCount());

		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			m_pDevice->GetDeviceId().Node, 
			6);

		pDIBv2->UnitDisks[0].UnitNumber = m_dwUnitNo;

		return TRUE;
	}

	return TRUE;
}

VOID 
CNdasUnitDeviceCreator::
InitializeDIBv2(
	PNDAS_DIB_V2 pDIBv2, 
	UINT64 nDiskSectorCount)
{
	_ASSERTE(!::IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	::ZeroMemory(pDIBv2, sizeof(NDAS_DIB_V2));

	pDIBv2->Signature = DISK_INFORMATION_SIGNATURE_V2;
	pDIBv2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIBv2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIBv2->sizeXArea = 2 * 1024 * 2;
	pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
	pDIBv2->iSectorsPerBit = 128;
	pDIBv2->sizeUserSpace -= pDIBv2->sizeUserSpace % pDIBv2->iSectorsPerBit;
	pDIBv2->iMediaType = NMT_SINGLE;
	pDIBv2->iSequence = 0;
	pDIBv2->nUnitCount = 1;
	pDIBv2->FlagDirty = 0;

}

BOOL
CNdasUnitDeviceCreator::
ConvertDIBv1toDIBv2(
	const NDAS_DIB* pDIBv1, 
	PNDAS_DIB_V2 pDIBv2, 
	UINT64 nDiskSectorCount)
{
	_ASSERTE(!::IsBadReadPtr(pDIBv1, sizeof(NDAS_DIB)));
	_ASSERTE(!::IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	InitializeDIBv2(pDIBv2, nDiskSectorCount);

	switch(pDIBv1->DiskType) {
	case NDAS_DIB_DISK_TYPE_SINGLE:
		::CopyMemory(
			pDIBv2->UnitDisks[0].MACAddr, 
			pDIBv1->EtherAddress, 
			6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;
		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;
		// not yet

	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->PeerUnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->PeerUnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->UnitNumber;

		pDIBv2->sizeUserSpace = 0; // same as that of master
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->UnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->PeerUnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
		// master information
		CopyMemory(pDIBv2->UnitDisks[0].MACAddr, pDIBv1->PeerAddress, 6);
		pDIBv2->UnitDisks[0].UnitNumber = pDIBv1->PeerUnitNumber;

		// slave information
		CopyMemory(pDIBv2->UnitDisks[1].MACAddr, pDIBv1->EtherAddress, 6);
		pDIBv2->UnitDisks[1].UnitNumber = pDIBv1->UnitNumber;

		pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
		break;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
	default:
		// _ASSERTE(FALSE);
		return FALSE;
	}

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDeviceComm class
//
//////////////////////////////////////////////////////////////////////////

CNdasDeviceComm::
CNdasDeviceComm(PCNdasDevice pDevice, DWORD dwUnitNo) :
	m_pDevice(pDevice),
	m_dwUnitNo(dwUnitNo),
	m_bWriteAccess(FALSE),
	m_bInitialized(FALSE)
{
}

CNdasDeviceComm::
~CNdasDeviceComm()
{
	if (m_bInitialized) {
		Cleanup();
	}
}

BOOL 
CNdasDeviceComm::
Initialize(BOOL bWriteAccess)
{
	m_bWriteAccess = bWriteAccess;

	InitializeLANSCSIPath();

	LPX_ADDRESS local, remote;

	local = m_pDevice->GetLocalLpxAddress();
	remote = m_pDevice->GetRemoteLpxAddress();

	SOCKET sock = CreateLpxConnection(&remote, &local);

	if (INVALID_SOCKET == sock) {
		DPErrorExWsa(_FT("CreateLpxConnection failed: "));
		return FALSE;
	}


	m_lspath.HWType = m_pDevice->GetHWType();
	m_lspath.HWVersion = m_pDevice->GetHWVersion();

	m_lspath.connsock = sock;
	INT iResult = Login(&m_lspath, LOGIN_TYPE_NORMAL);
	if (0 != iResult) {
		// TODO: LANDISK_ERROR_BADKEY?
		DPErrorExWsa(_FT("Login failed (ret %d): "), iResult);
		::closesocket(sock);
		return FALSE;
	}

	m_bInitialized = TRUE;

	return TRUE;
}

BOOL
CNdasDeviceComm::
Cleanup()
{
	INT iResult = 0;
	if (m_bInitialized) {
		iResult = ::closesocket(m_lspath.connsock);
	}
	m_bInitialized = FALSE;
	return (iResult == 0);
}

BOOL 
CNdasDeviceComm::
GetUnitDeviceInformation(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo)
{
	_ASSERTE(m_bInitialized && "CNdasDeviceComm is not initialized");
	_ASSERTE(pUnitDevInfo != NULL);

	INT iResult = GetDiskInfo(&m_lspath, m_dwUnitNo);
	if (0 != iResult) {
		// TODO: LANDISK_ERROR_BADKEY?
		DPError(_FT("GetDiskInfo failed with error %d\n"), iResult);
		return FALSE;
	}

	PTARGET_DATA pTargetData = &m_lspath.PerTarget[m_dwUnitNo];

	pUnitDevInfo->bLBA = pTargetData->bLBA;
	pUnitDevInfo->bLBA48 = pTargetData->bLBA48;
	pUnitDevInfo->bPIO = pTargetData->bPIO;
	pUnitDevInfo->bDMA = pTargetData->bDma;
	pUnitDevInfo->bUDMA = pTargetData->bUDma;
	pUnitDevInfo->MediaType = pTargetData->MediaType;

	pUnitDevInfo->dwROHosts = pTargetData->NRROHost;
	pUnitDevInfo->dwRWHosts = pTargetData->NRRWHost;
	pUnitDevInfo->SectorCount = pTargetData->SectorCount;

#ifdef UNICODE

	//
	// TODO: What if FwRev, Model, SerialNo is not null terminated?
	//

	_ASSERTE(sizeof(pUnitDevInfo->szModel) / sizeof(pUnitDevInfo->szModel[0]) == sizeof(pTargetData->Model));
	_ASSERTE(sizeof(pUnitDevInfo->szSerialNo) / sizeof(pUnitDevInfo->szSerialNo[0]) == sizeof(pTargetData->SerialNo));
	_ASSERTE(sizeof(pUnitDevInfo->szFwRev) / sizeof(pUnitDevInfo->szFwRev[0]) == sizeof(pTargetData->FwRev));

	::MultiByteToWideChar(
		CP_ACP, 0, 
		pTargetData->Model, sizeof(pTargetData->Model), 
		pUnitDevInfo->szModel, sizeof(pUnitDevInfo->szModel) / sizeof(pUnitDevInfo->szModel[0]));

	::MultiByteToWideChar(
		CP_ACP, 0, 
		pTargetData->SerialNo, sizeof(pTargetData->SerialNo), 
		pUnitDevInfo->szSerialNo, sizeof(pUnitDevInfo->szSerialNo) / sizeof(pUnitDevInfo->szSerialNo[0]));

	::MultiByteToWideChar(
		CP_ACP, 0, 
		pTargetData->FwRev, sizeof(pTargetData->FwRev), 
		pUnitDevInfo->szFwRev, sizeof(pUnitDevInfo->szFwRev) / sizeof(pUnitDevInfo->szFwRev[0]));

#else

	_ASSERTE(sizeof(pUnitDevInfo->szModel) == sizeof(pTargetData->Model));
	_ASSERTE(sizeof(pUnitDevInfo->szFwRev) == sizeof(pTargetData->FwRev));
	_ASSERTE(sizeof(pUnitDevInfo->szSerialNo) == sizeof(pTargetData->SerialNo));

	::CopyMemory(pUnitDevInfo->szModel, pTargetData->Model, sizeof(pTargetData->Model));
	::CopyMemory(pUnitDevInfo->szFwRev, pTargetData->FwRev, sizeof(pTargetData->FwRev));
	::CopyMemory(pUnitDevInfo->szSerialNo, pTargetData->SerialNo, sizeof(pTargetData->SerialNo));

#endif

	DPInfo(_FT("Model        : %s\n"), pUnitDevInfo->szModel);
	DPInfo(_FT("Serial No    : %s\n"), pUnitDevInfo->szSerialNo);
	DPInfo(_FT("Firmware Rev : %s\n"), pUnitDevInfo->szFwRev);

	return TRUE;
}

BOOL
CNdasDeviceComm::
GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
{
	_ASSERTE(m_bInitialized && "CNdasDeviceComm is not initialized");
	_ASSERTE(pDiskInfoBlock != NULL);

	//
	// Read Last Sector for NDAS_UNITDISK_INFORMATION_BLOCK
	//
	unsigned _int8 ui8IdeResponse;

	PTARGET_DATA pTargetData = &m_lspath.PerTarget[m_dwUnitNo];
	UINT64 ui64DiskBlock = pTargetData->SectorCount - 1;

	INT iResult = IdeCommand(
		&m_lspath, m_dwUnitNo, 0, 
		WIN_READ, 
		ui64DiskBlock, 1, 0,
		(PCHAR) pDiskInfoBlock, &ui8IdeResponse);

	if (0 != iResult) {
		DPError(_FT("IdeCommand failed with error %d, ide response %d.\n"), iResult, ui8IdeResponse);
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::
ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock)
{
	_ASSERTE(m_bInitialized && "CNdasDeviceComm is not initialized");
	_ASSERTE(pBlockBuffer != NULL);
	_ASSERTE(!::IsBadWritePtr(pBlockBuffer, 512));

	//
	// Read Last Sector for NDAS_UNITDISK_INFORMATION_BLOCK
	//
	UINT8 ui8IdeResponse;

	PTARGET_DATA pTargetData = &m_lspath.PerTarget[m_dwUnitNo];

	INT64 i64AbsoluteBlock = (i64DiskBlock >= 0) ? 
		i64DiskBlock : 
		(INT64)pTargetData->SectorCount + i64DiskBlock;

	INT iResult = IdeCommand(
		&m_lspath, 
		m_dwUnitNo, 
		0, 
		WIN_READ, 
		i64AbsoluteBlock, 
		1, 
		0,
		(PCHAR) pBlockBuffer, &ui8IdeResponse);

	if (0 != iResult) {
		DPError(_FT("IdeCommand failed with error %d, ide response %d.\n"), iResult, ui8IdeResponse);
		return FALSE;
	}

	return TRUE;
}

UINT64
CNdasDeviceComm::
GetDiskSectorCount()
{
	return m_lspath.PerTarget[m_dwUnitNo].SectorCount;
}

BOOL
CNdasDeviceComm::
WriteDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
{
	//
	// will not be implemented this feature here
	//
	_ASSERTE(m_bInitialized && "CNdasDeviceComm is not initialized!");
	_ASSERTE(m_bWriteAccess && "CNdasDeviceComm has not write access!");
	_ASSERTE(pDiskInfoBlock != NULL);

	return FALSE;
}

VOID
CNdasDeviceComm::
InitializeLANSCSIPath()
{
	::ZeroMemory(&m_lspath, sizeof(m_lspath));
	m_lspath.iUserID = GetUserId(); // should be set when login
	m_lspath.iPassword = m_pDevice->GetHWPassword();
}

INT32
CNdasDeviceComm::
GetUserId()
{
	if (m_bWriteAccess) {
		if (m_dwUnitNo == 0) return FIRST_TARGET_RW_USER;
		if (m_dwUnitNo == 1) return SECOND_TARGET_RW_USER;
	} else {
		if (m_dwUnitNo == 0) return FIRST_TARGET_RO_USER;
		if (m_dwUnitNo == 1) return SECOND_TARGET_RO_USER;
	}
	return 0;
}
