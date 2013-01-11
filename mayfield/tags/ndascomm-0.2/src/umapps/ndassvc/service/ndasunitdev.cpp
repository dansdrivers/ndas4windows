#include "stdafx.h"
#include "ndas/ndasmsg.h"
#include "ndas/ndasdib.h"

#include "ndasdev.h"
#include "ndashixcli.h"
#include "ndasinstman.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include "lsbusioctl.h"
#include "lfsfiltctl.h"
#include "sysutil.h"
// #include "scrc32.h"
#include "ndasdevcomm.h"
#include "ndasunitdevfactory.h"

#include "autores.h"
#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASUNITDEV
#include "xdebug.h"

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

static
NDAS_LOGICALDEVICE_TYPE
pUnitDeviceLogicalDeviceType(
	NDAS_UNITDEVICE_TYPE udType,
	NDAS_UNITDEVICE_SUBTYPE udSubtype);

static
NDAS_UNITDEVICE_ID 
pCreateUnitDeviceId(CNdasDevice& dev, DWORD unitNo);

ULONG
CNdasUnitDevice::AddRef()
{
	ximeta::CAutoLock autolock(this);
	ULONG ulCount = ximeta::CExtensibleObject::AddRef();
	DBGPRT_INFO(_FT("%s: %u\n"), ToString(), ulCount);
	return ulCount;
}

ULONG
CNdasUnitDevice::Release()
{
	{
		ximeta::CAutoLock autolock(this);
		DBGPRT_INFO(_FT("%s\n"), ToString());
	}
	ULONG ulCount = ximeta::CExtensibleObject::Release();
	DBGPRT_INFO(_FT("RefCount=%u\n"), ulCount);
	return ulCount;
}

//
// Constructor
//

CNdasUnitDevice::CNdasUnitDevice(
	CNdasDevice& parentDevice, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_TYPE type,
	NDAS_UNITDEVICE_SUBTYPE subType,
	CONST NDAS_UNITDEVICE_INFORMATION& unitDeviceInfo,
	CONST NDAS_LOGICALDEVICE_GROUP& ldGroup,
	DWORD ldSequence) :
	m_unitDeviceId(pCreateUnitDeviceId(parentDevice,dwUnitNo)),
	m_pParentDevice(&parentDevice),
	m_type(type),
	m_subType(subType),
	m_status(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED),
	m_lastError(NDAS_UNITDEVICE_ERROR_NONE),
	m_devInfo(unitDeviceInfo),
	m_ldGroup(ldGroup),
	m_ldSequence(ldSequence),
	m_pLogicalDevice(NULL),
	m_bSupposeFault(FALSE)
{
	::ZeroMemory(
		&m_PrimaryHostInfo, 
		sizeof(NDAS_UNITDEVICE_PRIMARY_HOST_INFO));

	HRESULT hr = ::StringCchPrintf(
		m_szRegContainer,
		30,
		_T("Devices\\%04d\\%04d"),
		m_pParentDevice->GetSlotNo(),
		m_unitDeviceId.UnitNo);

	_ASSERTE(SUCCEEDED(hr));

	DBGPRT_TRACE(_FT("ctor: %s\n"), ToString());
}


//
// Destructor
//
CNdasUnitDevice::~CNdasUnitDevice()
{
	DBGPRT_TRACE(_FT("dtor: %s\n"), ToString());
}

CONST NDAS_UNITDEVICE_ID&
CNdasUnitDevice::GetUnitDeviceId()
{
	ximeta::CAutoLock autolock(this);
	return m_unitDeviceId;
}

//
// Get the unit device information
//
CONST NDAS_UNITDEVICE_INFORMATION&
CNdasUnitDevice::GetUnitDevInfo()
{
	ximeta::CAutoLock autolock(this);
	return m_devInfo;
}

//
// Set 
BOOL
CNdasUnitDevice::SetMountStatus(BOOL fMounted)
{
	ximeta::CAutoLock autolock(this);
	// TODO: Adjust this
	if (fMounted) {
		SetStatus(NDAS_UNITDEVICE_STATUS_MOUNTED);
	} else {
		SetStatus(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED);
	}

	return TRUE;
}

ACCESS_MASK
CNdasUnitDevice::GetGrantedAccess()
{
	ximeta::CAutoLock autolock(this);
	return m_pParentDevice->GetGrantedAccess();
}

ACCESS_MASK
CNdasUnitDevice::GetAllowingAccess()
{
	ximeta::CAutoLock autolock(this);
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
CNdasUnitDevice::SetHostUsageCount(DWORD nROHosts, DWORD nRWHosts)
{
	ximeta::CAutoLock autolock(this);
	m_devInfo.dwROHosts = nROHosts;
	m_devInfo.dwRWHosts = nRWHosts;
}

ULONG
CNdasUnitDevice::GetUserBlockCount()
{
	ximeta::CAutoLock autolock(this);
	return m_devInfo.SectorCount;
}

ULONG
CNdasUnitDevice::GetPhysicalBlockCount()
{
	ximeta::CAutoLock autolock(this);
	return m_devInfo.SectorCount;
}

BOOL
CNdasUnitDevice::GetActualHostUsageCount(
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts, 
	BOOL bUpdate)
{
	ximeta::CAutoLock autolock(this);

	DWORD nROHosts, nRWHosts;
	BOOL fSuccess = GetHostUsageCount(&nROHosts, &nRWHosts, bUpdate);
	if (!fSuccess) {
		return FALSE;
	}

	//
	// Following cases does not need further consideration
	// - RW = 0, RO = any
	// - RW = 1, RO = 0
	// Otherwise, we need HIX to discover the actual count
	//
	if (nRWHosts == 0 || nROHosts == 0) {
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return TRUE;
	}

	DWORD nHosts = nROHosts + nRWHosts;
	NDAS_UNITDEVICE_ID unitDeviceId = GetUnitDeviceId();

	//
	// BUG: Workaround for weird lock
	// We should unlock the global lock (yes, it's global lock at the moment)
	// for local HIX server to work!
	//

	autolock.Release();

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	fSuccess = hixdisc.Initialize();
	if (!fSuccess) {
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return TRUE;
	}

	fSuccess = hixdisc.Discover(
		unitDeviceId,
		NHIX_UDA_READ_ACCESS, // read bit is set - all hosts
		nHosts,
		2000);

	if (!fSuccess) {
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return TRUE;
	}

	DWORD nRepliedHosts = hixdisc.GetHostCount(unitDeviceId);
	for (DWORD i = 0; i < nRepliedHosts; ++i) {
		NHIX_UDA uda = 0;
		fSuccess = hixdisc.GetHostData(unitDeviceId,i,&uda);
		_ASSERTE(fSuccess); // index must be valid!
		if (uda == NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS)
		{
			--nROHosts;
			++nRWHosts;
		}
	}

	*lpnROHosts = nROHosts;
	*lpnRWHosts = nRWHosts;

	return TRUE;
}

BOOL
CNdasUnitDevice::GetHostUsageCount(
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts, 
	BOOL fUpdate)
{
	ximeta::CAutoLock autolock(this);

	_ASSERTE(!IsBadWritePtr(lpnROHosts, sizeof(DWORD)));
	_ASSERTE(!IsBadWritePtr(lpnRWHosts, sizeof(DWORD)));

	if (fUpdate) {
		BOOL fSuccess = m_pParentDevice->UpdateDeviceInfo();
		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("Update device status failed: "));
			return FALSE;
		}
	}

	*lpnROHosts = m_devInfo.dwROHosts;
	*lpnRWHosts = m_devInfo.dwRWHosts;

	DBGPRT_INFO(_FT("Host Usage Count: RO %d, RW %d.\n"), 
		m_devInfo.dwROHosts, 
		m_devInfo.dwRWHosts);

	return TRUE;
}

VOID
CNdasUnitDevice::UpdatePrimaryHostInfo(
	CONST NDAS_UNITDEVICE_PRIMARY_HOST_INFO& info)
{
	ximeta::CAutoLock autolock(this);

	::CopyMemory(
		&m_PrimaryHostInfo,
		&info,
		sizeof(NDAS_UNITDEVICE_PRIMARY_HOST_INFO));

	m_PrimaryHostInfo.LastUpdate = ::GetTickCount();

	DBGPRT_NOISE(
		_FT("Primary Host Usage Updated: %s, Timestamp %d\n"), 
		this->ToString(),
		m_PrimaryHostInfo.LastUpdate);
}

VOID 
CNdasUnitDevice::SetStatus(NDAS_UNITDEVICE_STATUS newStatus)
{ 
	ximeta::CAutoLock autolock(this);

	m_status = newStatus; 
}

BOOL
CNdasUnitDevice::RegisterToLDM()
{
	ximeta::CAutoLock autolock(this);

	CNdasLogicalDeviceManager* pLDM = pGetNdasLogicalDeviceManager();

	m_pLogicalDevice = pLDM->Register(*this);

	if (NULL == m_pLogicalDevice) {
		DBGPRT_ERR_EX(_FT("Failed to register a unit device to the LDM: "));
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasUnitDevice::UnregisterFromLDM()
{
	ximeta::CAutoLock autolock(this);

	CNdasLogicalDeviceManager* pLDM = pGetNdasLogicalDeviceManager();
	BOOL fSuccess = pLDM->Unregister(*this);
	m_pLogicalDevice->Release();
	_ASSERTE(fSuccess);
	return fSuccess;
}

LPCTSTR
CNdasUnitDevice::ToString()
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchPrintf(
		m_lpStrBuf,
		m_cchStrBuf,
		_T("%s.%02d"),
		m_pParentDevice->ToString(),
		m_unitDeviceId.UnitNo);

	_ASSERTE(SUCCEEDED(hr));

	return m_lpStrBuf;
}

VOID
CNdasUnitDevice::SetFault(BOOL bFault)
{
	m_bSupposeFault = bFault;
}

BOOL
CNdasUnitDevice::IsFault()
{
	return m_bSupposeFault;
}

UINT64 
CNdasUnitDevice::GetDevicePassword()
{
	return m_pParentDevice->GetHWPassword();
}

DWORD 
CNdasUnitDevice::GetDeviceUserID(ACCESS_MASK access)
{
	static const DWORD RO_SET[] = { 0x00000001, 0x00000002 };
	static const DWORD RW_SET[] = { 0x00010001, 0x00020002 };

	if (GENERIC_WRITE & access)
	{
		return 0 == m_unitDeviceId.UnitNo ?  RW_SET[0] : RW_SET[1];
	}
	else
	{
		return 0 == m_unitDeviceId.UnitNo ? RO_SET[0] : RO_SET[1];
	}
}

BOOL
CNdasUnitDevice::CheckNDFSCompatibility()
{
	ximeta::CAutoLock autolock(this);
	//
	// Unit devices other than disks are not allowed for NDFS
	//
	if (m_type != NDAS_UNITDEVICE_TYPE_DISK) {
		return FALSE;
	}

	//
	// LfsFilter compatibility check.
	// NDFS Major version should be same
	// 
	WORD wHostNDFSVerMajor;
	BOOL fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&wHostNDFSVerMajor, NULL);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Getting LFS Filter Version failed:"));
		return FALSE;
	}

	//
	// Primary Host Info is valid for 30 seconds
	//
	DWORD dwMaxTickAllowance = 30 * 1000;
	if (0 != m_PrimaryHostInfo.NDFSCompatVersion &&
		::GetTickCount() < m_PrimaryHostInfo.LastUpdate + dwMaxTickAllowance) 
	{
		// primary host info is valid
		return (wHostNDFSVerMajor == m_PrimaryHostInfo.NDFSCompatVersion);
	}

	//
	// No Primary Host Info is available (IX)
	// Use HIX to discover
	//
	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	fSuccess = hixdisc.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("HIXDiscover init failed: "));
		return FALSE;
	}

	NDAS_UNITDEVICE_ID udid = GetUnitDeviceId();

	hixdisc.Discover(udid,NHIX_UDA_SHRW_PRIM,1,1000);
	DWORD nHosts = hixdisc.GetHostCount(udid);
	if (0 == nHosts) {
		DBGPRT_ERR_EX(_FT("GetHostCount failed: "));
		return FALSE;
	}

	GUID hostGuid;
	fSuccess = hixdisc.GetHostData(udid,0,NULL,&hostGuid,NULL,NULL);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("GetHostData failed: "));
		return FALSE;
	}

	CNdasHostInfoCache* phic = pGetNdasHostInfoCache();
	_ASSERTE(phic); // phic is not null (by pGetNdasHostInfoCache)
	CONST NDAS_HOST_INFO* pHostInfo = phic->GetHostInfo(&hostGuid);
	if (NULL == pHostInfo) {
		DBGPRT_ERR_EX(_FT("GetHostInfo failed: "));
		return FALSE;
	}

	//
	// ReservedVerInfo contains NDFS Version Information
	//
	if (pHostInfo->ReservedVerInfo.VersionMajor != wHostNDFSVerMajor) {
		DBGPRT_ERR(_FT("Host NDFS %d, Primary NDFS %d failed: "),
			wHostNDFSVerMajor, pHostInfo->ReservedVerInfo.VersionMajor);
		return FALSE;
	}

	//
	// Primary and this host's NDFS compat version is same
	//

	return TRUE;

}

DWORD 
CNdasUnitDevice::GetOptimalMaxRequestBlock()
{
	ximeta::CAutoLock autolock(this);

	sysutil::NDIS_MEDIUM ndisMedium;
	sysutil::NDIS_PHYSICAL_MEDIUM ndisPhysicalMedium;
	LONGLONG llLinkSpeed;

	m_pParentDevice->Lock();

	DWORD dwDefaultMRB = m_pParentDevice->GetMaxRequestBlocks();
	DWORD dwOptimalMRB;
	const LPX_ADDRESS localAddr = m_pParentDevice->GetLocalLpxAddress();

	m_pParentDevice->Unlock();

	BOOL fSuccess = sysutil::GetNetConnCharacteristics(
		6,
		localAddr.Node,
		&llLinkSpeed,
		&ndisMedium,
		&ndisPhysicalMedium);

	if (!fSuccess) {
		//
		// TODO: Do we have to more defensive to lower to 32?
		//
		return dwDefaultMRB;
	}

	DWORD lsMbps = llLinkSpeed / 10000;

	//
	// Criteria
	//
	// Actually NdisMedium cannot be used for detecting Wireless LAN
	// Wireless WAN is completely different to Wireless LAN
	// 
	// If the physical medium is WirelessLan
	// MRB is less than 64
	// Wireless link speed is quite variable.
	// 54Mbps, 48Mbps, 36MBps, 24Mbps,...
	// 11Mbps, 5.5MBps, 2MBps, 1MBps...
	// 
	// The criteria is at this time, if the link speed is more than
	// 11MBps. MRB will be 32, otherwise 16
	// 
	// Otherwise (NdisPhysicalMediumMax (INVALID_VALUE) and 802_3
	// can be regarded as Ethernet Medium
	//
	// 100Mbps or higher: 128 (or 104 in V2.0)
	//
	// 

	// Otherwise        : 64
	//

	// Wireless
	if (ndisMedium == sysutil::NdisMedium802_3 &&
		ndisPhysicalMedium == sysutil::NdisPhysicalMediumWirelessLan)
	{
		if (lsMbps >= 11) {
			dwOptimalMRB = 32;
		} else {
			dwOptimalMRB = 16;
		}
	}
	// Otherwise
	else
	{
		if (lsMbps >= 100) {
			dwOptimalMRB = 128; // Limit is set below.
		} else {
			dwOptimalMRB = 32;
		}
	}

	//
	// There is a known bug in a giga chipset:
	// In a certain situation that DMA request for IDE device is overflowed,
	// it is suspected that FIFO queue data in NDAS chip is overwritten 
	// by the subsequent data. 
	// Limiting Max Request Block to 104 will alleviate or prevent
	// such a situation.
	//
	const LPCTSTR MAX_REQUEST_BLOCK_LIMIT_KEY = _T("MaxRequestBlockLimit");
	const DWORD MAX_REQUEST_BLOCK_LIMIT_DEFAULT = 
		(2 == m_pParentDevice->GetHWVersion()) ? 104 : 128;
	
	DWORD dwMaxRequestBlockLimit;
	TCHAR szMaxRequestBlockLimitKey[64];
	HRESULT hr = ::StringCchPrintf(
		szMaxRequestBlockLimitKey, 
		RTL_NUMBER_OF(szMaxRequestBlockLimitKey),
		_T("%s.%d"),
		MAX_REQUEST_BLOCK_LIMIT_KEY,
		m_pParentDevice->GetHWVersion());
	_ASSERTE(SUCCEEDED(hr));

	fSuccess = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		szMaxRequestBlockLimitKey,
		&dwMaxRequestBlockLimit);
	if (!fSuccess || 0 == dwMaxRequestBlockLimit)
	{
		// MaxRequestBlockLimit.{Version} is not specified
		// Locate MaxRequestBlockLimit
		fSuccess = _NdasSystemCfg.GetValueEx(
			_T("ndassvc"),
			MAX_REQUEST_BLOCK_LIMIT_KEY,
			&dwMaxRequestBlockLimit);
		if (!fSuccess || 0 == dwMaxRequestBlockLimit)
		{
			dwMaxRequestBlockLimit = MAX_REQUEST_BLOCK_LIMIT_DEFAULT;
		}
	}

	if (dwOptimalMRB > dwMaxRequestBlockLimit)
	{
		dwOptimalMRB = dwMaxRequestBlockLimit;
	}

	if (dwOptimalMRB > dwDefaultMRB)
	{
		dwOptimalMRB = dwDefaultMRB;
	}

	return dwOptimalMRB;
}

CONST NDAS_LOGICALDEVICE_GROUP&
CNdasUnitDevice::GetLDGroup()
{
	ximeta::CAutoLock autolock(this);
	return m_ldGroup;
}

DWORD
CNdasUnitDevice::GetLDSequence()
{
	ximeta::CAutoLock autolock(this);
	return m_ldSequence;
}

CNdasDevice* 
CNdasUnitDevice::GetParentDevice()
{ 
	ximeta::CAutoLock autolock(this);
	_ASSERTE(NULL != m_pParentDevice);
	m_pParentDevice->AddRef();
	return m_pParentDevice; 
}

DWORD 
CNdasUnitDevice::GetUnitNo()
{ 
	ximeta::CAutoLock autolock(this);
	return m_unitDeviceId.UnitNo; 
}

NDAS_UNITDEVICE_TYPE 
CNdasUnitDevice::GetType()
{ 
	ximeta::CAutoLock autolock(this);
	return m_type; 
}

NDAS_UNITDEVICE_SUBTYPE 
CNdasUnitDevice::GetSubType()
{ 
	ximeta::CAutoLock autolock(this);
	return m_subType; 
}

NDAS_UNITDEVICE_STATUS 
CNdasUnitDevice::GetStatus()
{ 
	ximeta::CAutoLock autolock(this);
	return m_status; 
}

NDAS_UNITDEVICE_ERROR 
CNdasUnitDevice::GetLastError()
{ 
	ximeta::CAutoLock autolock(this);
	return m_lastError; 
}

CNdasLogicalDevice*
CNdasUnitDevice::GetLogicalDevice()
{
	ximeta::CAutoLock autolock(this);
	if (NULL != m_pLogicalDevice) m_pLogicalDevice->AddRef();
	return m_pLogicalDevice;
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitDiskDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

//
// Constructor
//

CNdasUnitDiskDevice::CNdasUnitDiskDevice(
	CNdasDevice& parentDevice, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_DISK_TYPE diskType,
	CONST NDAS_UNITDEVICE_INFORMATION& unitDevInfo,
	CONST NDAS_LOGICALDEVICE_GROUP& ldGroup,
	DWORD ldSequence,
	ULONG ulUserBlocks,
	PVOID pAddTargetInfo,
	CONST NDAS_CONTENT_ENCRYPT& contentEncrypt,
	NDAS_DIB_V2* pDIBv2) :
	m_ulUserBlocks(ulUserBlocks),
	m_pAddTargetInfo(pAddTargetInfo),
	m_contentEncrypt(contentEncrypt),
	m_pDIBv2(pDIBv2),
	m_diskType(diskType),
	CNdasUnitDevice(
		parentDevice, 
		dwUnitNo, 
		NDAS_UNITDEVICE_TYPE_DISK,
		CreateNdasUnitDeviceSubType(diskType),
		unitDevInfo,
		ldGroup,
		ldSequence)
{
	//
	// m_pDIBv2 and m_pAddTargetInfo will be deleted 
	// by this class on destruction
	//
	_ASSERTE(
		(NDAS_CONTENT_ENCRYPT_METHOD_NONE == m_contentEncrypt.Method &&
		m_contentEncrypt.KeyLength == 0) ||
		(NDAS_CONTENT_ENCRYPT_METHOD_NONE != m_contentEncrypt.Method &&
		m_contentEncrypt.KeyLength > 0));

	DBGPRT_TRACE(_FT("%s\n"), ToString());
}

//
// Destructor
//

CNdasUnitDiskDevice::~CNdasUnitDiskDevice()
{
	DBGPRT_TRACE(_FT("%s\n"), ToString());

	if (NULL != m_pAddTargetInfo) {
		(VOID) ::HeapFree(::GetProcessHeap(), 0, m_pAddTargetInfo);
		m_pAddTargetInfo = NULL;
	}
	if (NULL != m_pDIBv2) {
		(VOID) ::HeapFree(::GetProcessHeap(), 0, m_pDIBv2);
		m_pDIBv2  = NULL;
	}
}

BOOL
CNdasUnitDiskDevice::HasSameDIBInfo()
{
	CNdasUnitDeviceCreator udCreator(*GetParentDevice(), GetUnitNo());

	CRefObjPtr<CNdasUnitDevice> pUnitDeviceNow = udCreator.CreateUnitDevice();

	if(NULL == pUnitDeviceNow.p) {
		return FALSE;
	}

	if (GetType() != pUnitDeviceNow->GetType()) {
		return FALSE;
	}

	CNdasUnitDiskDevice* pUnitDiskDeviceNow = 
		reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDeviceNow.p);

	if(!HasSameDIBInfo(*pUnitDiskDeviceNow)) {
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasUnitDiskDevice::HasSameDIBInfo(
	CNdasUnitDiskDevice &NdasUnitDiskDevice)
{
	ximeta::CAutoLock autolock(this);
	//
	// TODO: Changed to the actual size!!
	//
	return (0 == memcmp(m_pDIBv2, NdasUnitDiskDevice.m_pDIBv2,
		sizeof(NDAS_DIB_V2))) ? TRUE : FALSE;
}

ULONG
CNdasUnitDiskDevice::GetUserBlockCount()
{
	ximeta::CAutoLock autolock(this);
	// Reserve 2 MB for internal use (2 * 2 * 1024 blocks)
	// ULONG ulDiskBlocks = (ULONG) m_devInfo.SectorCount - (2 * 1024 * 2); 

	//	return ulDiskBlocks;
	return m_ulUserBlocks;
}

PVOID 
CNdasUnitDiskDevice::GetAddTargetInfo()
{
	ximeta::CAutoLock autolock(this);
	return m_pAddTargetInfo;
}

CONST NDAS_CONTENT_ENCRYPT&
CNdasUnitDiskDevice::GetEncryption()
{
	ximeta::CAutoLock autolock(this);
	return m_contentEncrypt;
}

BOOL
CNdasUnitDiskDevice::IsBitmapClean()
{
	ximeta::CAutoLock autolock(this);

	CNdasDeviceComm devComm(*m_pParentDevice, m_unitDeviceId.UnitNo);

	BOOL fSuccess = devComm.Initialize(FALSE);
	if(!fSuccess) {
		return FALSE;
	}

	NDAS_UNITDEVICE_INFORMATION unitDevInfo;
	fSuccess = devComm.GetUnitDeviceInformation(&unitDevInfo);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("GetUnitDeviceInformation of %s failed: "), this->ToString());
		return FALSE;
	}

	BYTE BitmapData[128 * 512] = {0};

	// 1MB from NDAS_BLOCK_LOCATION_BITMAP
	for(INT i = 0; i < 16; i++)  {
		fSuccess = devComm.ReadDiskBlock(BitmapData, 
			NDAS_BLOCK_LOCATION_BITMAP + (i * 128), 128);

		if(!fSuccess) {
			return FALSE;
		}

		INT j = 0;
		PULONG pBitmapData = (PULONG)BitmapData;
		for(; j < 128 * 512 / 4; ++j) {
			if(*pBitmapData) {
				return FALSE;
			}
			pBitmapData++;
		}
	}	

	return TRUE;
}

static
NDAS_UNITDEVICE_ID 
pCreateUnitDeviceId(CNdasDevice& dev, DWORD unitNo)
{
	NDAS_UNITDEVICE_ID deviceID = {dev.GetDeviceId(), unitNo};
	return deviceID;
}

static
NDAS_LOGICALDEVICE_TYPE
pUnitDeviceLogicalDeviceType(
	NDAS_UNITDEVICE_TYPE udType,
	NDAS_UNITDEVICE_SUBTYPE udSubtype)
{
	switch (udType) {
	case NDAS_UNITDEVICE_TYPE_DISK:
		switch (udSubtype.DiskDeviceType) {
		case NDAS_UNITDEVICE_DISK_TYPE_SINGLE:
			return NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
		case NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD:
			return NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD;
		case NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED:
			return NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED;
		case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER:
		case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE:
			return NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID0:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID0;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID1:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID4:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4;
		}
		break;
	case NDAS_UNITDEVICE_TYPE_CDROM:
		return NDAS_LOGICALDEVICE_TYPE_DVD;
	case NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK:
		return NDAS_LOGICALDEVICE_TYPE_FLASHCARD;
	case NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY:
		return NDAS_LOGICALDEVICE_TYPE_MO;
	}
	_ASSERTE(FALSE);
	return NDAS_LOGICALDEVICE_TYPE_UNKNOWN;
}
