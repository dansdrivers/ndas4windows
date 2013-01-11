#include "stdafx.h"
#include <xtl/xtltrace.h>
#include <lfsfiltctl.h>
#include <ndasbusioctl.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasdib.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndascntenc.h>

#include "ndasdevid.h"
#include "ndashixcli.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include "sysutil.h"
#include "ndasdevcomm.h"
#include "ndasunitdevfactory.h"

#include "ndassvcdef.h"
#include "ndasobjs.h"
#include "ndascomobjectsimpl.hpp"

#include "ndasunitdev.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasunitdev.tmh"
#endif

DWORD 
pGetNdasUserId(DWORD UnitNo, ACCESS_MASK access)
{
	const DWORD RO_SET[] = { 0x00000001, 0x00000002 };
	const DWORD RW_SET[] = { 0x00010001, 0x00020002 };

	const DWORD* userIdSet = (GENERIC_WRITE & access) ? RW_SET : RO_SET;

	XTLASSERT(UnitNo < RTL_NUMBER_OF(RO_SET));

	DWORD index = min(UnitNo, RTL_NUMBER_OF(RO_SET) - 1);

	return userIdSet[index];
}


DWORD
pReadMaxRequestBlockLimitConfig(DWORD hardwareVersion)
{
	const LPCTSTR MAX_REQUEST_BLOCK_LIMIT_KEY = _T("MaxRequestBlockLimit");
	const DWORD MAX_REQUEST_BLOCK_LIMIT_DEFAULT = 128;
	BOOL success;
	DWORD dwMaxRequestBlockLimit;
	TCHAR szMaxRequestBlockLimitKey[64];
	HRESULT hr = ::StringCchPrintf(
		szMaxRequestBlockLimitKey, 
		RTL_NUMBER_OF(szMaxRequestBlockLimitKey),
		_T("%s.%d"),
		MAX_REQUEST_BLOCK_LIMIT_KEY,
		hardwareVersion);
	XTLASSERT(SUCCEEDED(hr));

	success = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		szMaxRequestBlockLimitKey,
		&dwMaxRequestBlockLimit);
	if (!success || 0 == dwMaxRequestBlockLimit)
	{
		// MaxRequestBlockLimit.{Version} is not specified
		// Locate MaxRequestBlockLimit
		success = _NdasSystemCfg.GetValueEx(
			_T("ndassvc"),
			MAX_REQUEST_BLOCK_LIMIT_KEY,
			&dwMaxRequestBlockLimit);
		if (!success || 0 == dwMaxRequestBlockLimit)
		{
			dwMaxRequestBlockLimit = MAX_REQUEST_BLOCK_LIMIT_DEFAULT;
		}
	}
	return dwMaxRequestBlockLimit;
}


//////////////////////////////////////////////////////////////////////////
//
// CNdasUnit class implementation
//
//////////////////////////////////////////////////////////////////////////

namespace
{

NDAS_LOGICALDEVICE_TYPE
pUnitDeviceLogicalDeviceType(
	NDAS_UNITDEVICE_TYPE udType,
	NDAS_UNITDEVICE_SUBTYPE udSubtype);

NDAS_UNITDEVICE_ID 
pCreateUnitDeviceId(
	INdasDevice* pNdasDevice,
	DWORD unitNo);

}

//
// Constructor
//


HRESULT 
CNdasUnitImpl::Initialize(
	__in INdasDevice* pNdasDevice, 
	__in DWORD UnitNo,
	__in NDAS_UNITDEVICE_TYPE Type,
	__in NDAS_UNITDEVICE_SUBTYPE SubType,
	__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo,
	__in const NDAS_LOGICALDEVICE_GROUP& LuDefinition,
	__in DWORD LuSequence)
{
	m_pParentNdasDevice = pNdasDevice;
	m_unitDeviceId = pCreateUnitDeviceId(pNdasDevice,UnitNo);
	m_type = Type;
	m_subType = SubType;
	m_status = NDAS_UNITDEVICE_STATUS_NOT_MOUNTED;
	m_lastError = NDAS_UNITDEVICE_ERROR_NONE;
	m_udinfo = HardwareInfo;
	m_ldGroup = LuDefinition;
	m_ldSequence = LuSequence;

	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasUnit=%p, %s\n", this, CNdasUnitDeviceId(m_unitDeviceId).ToStringA());

	DWORD slotNo;
	COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));

	COMVERIFY( StringCchPrintf(
		m_szRegContainer,
		30,
		_T("Devices\\%04d\\%04d"),
		slotNo,
		m_unitDeviceId.UnitNo));

	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_ParentNdasDevice(__out INdasDevice** ppNdasDevice)
{
	CComPtr<INdasDevice> pNdasDevice = m_pParentNdasDevice;
	ATLASSERT(pNdasDevice.p);
	*ppNdasDevice = pNdasDevice.Detach();
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_NdasLogicalUnit(__out INdasLogicalUnit** ppNdasLogicalUnit)
{
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit(m_pNdasLogicalUnit);
	if (!pNdasLogicalUnit)
	{
		return E_FAIL;
	}
	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_Status(__out NDAS_UNITDEVICE_STATUS* Status)
{
	CAutoInstanceLock autolock(this);
	*Status = m_status;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_Error(__out NDAS_UNITDEVICE_ERROR* Error)
{
	CAutoInstanceLock autolock(this);
	*Error = m_lastError;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_NdasUnitId(__out NDAS_UNITDEVICE_ID* NdasUnitId)
{
	*NdasUnitId = m_unitDeviceId;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_UnitNo(__out DWORD* UnitNo)
{
	*UnitNo = m_unitDeviceId.UnitNo;
	return S_OK;
}

STDMETHODIMP 
CNdasUnitImpl::get_LogicalUnitSequence(__out DWORD* Sequence)
{
	*Sequence = m_ldSequence;
	return S_OK;
}

STDMETHODIMP 
CNdasUnitImpl::get_LogicalUnitDefinition(__out NDAS_LOGICALDEVICE_GROUP* LuDefinition)
{
	*LuDefinition = m_ldGroup;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_HardwareInfo(__out PNDAS_UNITDEVICE_HARDWARE_INFO HardwareInfo)
{
	XTLASSERT(!IsBadWritePtr(HardwareInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFO)));
	CAutoInstanceLock autolock(this);
	CopyMemory(HardwareInfo, &m_udinfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFO));
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_UnitStat(__out NDAS_UNITDEVICE_STAT* UnitStat)
{
	NDAS_DEVICE_STAT dstat;
	m_pParentNdasDevice->get_DeviceStat(&dstat);
	*UnitStat = dstat.UnitDevices[m_unitDeviceId.UnitNo];
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_AllowedAccess(__out ACCESS_MASK* Access)
{
	return m_pParentNdasDevice->get_AllowedAccess(Access);
}

STDMETHODIMP
CNdasUnitImpl::get_GrantedAccess(__out ACCESS_MASK* Access)
{
	return m_pParentNdasDevice->get_GrantedAccess(Access);
}

STDMETHODIMP
CNdasUnitImpl::get_UserBlocks(__out UINT64 * Blocks)
{
	CAutoInstanceLock autolock(this);
	*Blocks = m_udinfo.SectorCount.QuadPart;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_PhysicalBlocks(__out UINT64 * Blocks)
{
	CAutoInstanceLock autolock(this);
	*Blocks = m_udinfo.SectorCount.QuadPart;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::GetActualHostUsageCount(
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts, 
	BOOL bUpdate)
{
	DWORD nROHosts = 0;
	DWORD nRWHosts = 0;

	HRESULT hr = GetHostUsageCount(&nROHosts, &nRWHosts, bUpdate);
	if (FAILED(hr))
	{
		return hr;
	}

	//
	// Following cases does not need further consideration
	// - RW = 0, RO any
	// - RW = 1, RO = 0
	// Neither RO or RW is not NDAS_HOST_COUNT_UNKNOWN
	// Otherwise, we need HIX to discover the actual count
	//
	if ((nRWHosts == 0 || nROHosts == 0) &&
		(NDAS_HOST_COUNT_UNKNOWN != nRWHosts) &&
		(NDAS_HOST_COUNT_UNKNOWN != nROHosts))
	{
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return S_OK;
	}

	DWORD nHosts = 
		(NDAS_HOST_COUNT_UNKNOWN == nRWHosts || NDAS_HOST_COUNT_UNKNOWN == nROHosts) ? 
		NDAS_MAX_CONNECTION_V11 : nROHosts + nRWHosts;

	NDAS_UNITDEVICE_ID unitDeviceId;
	hr = get_NdasUnitId(&unitDeviceId);
	if (FAILED(hr))
	{
		return hr;
	}

	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	BOOL success = hixdisc.Initialize();
	if (!success) 
	{
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return S_OK;
	}

	success = hixdisc.Discover(
		unitDeviceId,
		NHIX_UDA_READ_ACCESS, // read bit is set - all hosts
		nHosts,
		2000);

	if (!success) 
	{
		*lpnROHosts = nROHosts;
		*lpnRWHosts = nRWHosts;
		return S_OK;
	}

	if (NDAS_HOST_COUNT_UNKNOWN == nRWHosts || NDAS_HOST_COUNT_UNKNOWN == nROHosts)
	{
		DWORD rawROHosts = nROHosts;
		DWORD rawRWHosts = nRWHosts;

		// If any host count is unknown, use HIX counter only
		nROHosts = 0;
		nRWHosts = 0;
		DWORD nRepliedHosts = hixdisc.GetHostCount(unitDeviceId);
		for (DWORD i = 0; i < nRepliedHosts; ++i) 
		{
			NHIX_UDA uda = 0;
			success = hixdisc.GetHostData(unitDeviceId,i,&uda);
			XTLASSERT(success); // index must be valid!
			if (NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS == uda ||
				NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS == uda ||
				NHIX_UDA_READ_WRITE_ACCESS == uda)
			{
				++nRWHosts;
			}
			else if (uda == NHIX_UDA_READ_ACCESS)
			{
				++nROHosts;
			}
			else
			{
				XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_WARNING,
					"Invalid uda=0x%08X\n", uda);
			}
		}
		//
		// If HIX counter is not available either, we should at least show
		// the original counter.
		//
		if (NDAS_HOST_COUNT_UNKNOWN != rawRWHosts && rawRWHosts > nRWHosts)
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_WARNING,
				"RWHost adjusted to Raw=%d from HIX=%d\n", rawRWHosts, nRWHosts);
			nRWHosts = rawRWHosts;
		}
	}
	else
	{
		// Otherwise, use SharedRW counter
		DWORD nRepliedHosts = hixdisc.GetHostCount(unitDeviceId);
		for (DWORD i = 0; i < nRepliedHosts; ++i) 
		{
			NHIX_UDA uda = 0;
			success = hixdisc.GetHostData(unitDeviceId,i,&uda);
			XTLASSERT(success); // index must be valid!
			if (uda == NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS)
			{
				--nROHosts;
				++nRWHosts;
			}
		}
	}

	*lpnROHosts = nROHosts;
	*lpnRWHosts = nRWHosts;

	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"Actual Host Usage Count: RO=%d, RW=%d.\n",
		*lpnROHosts, *lpnRWHosts);

	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::GetHostUsageCount(
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts, 
	BOOL fUpdate)
{
	XTLASSERT(!IsBadWritePtr(lpnROHosts, sizeof(DWORD)));
	XTLASSERT(!IsBadWritePtr(lpnRWHosts, sizeof(DWORD)));

	CComPtr<INdasDevice> pNdasDevice = m_pParentNdasDevice;

	if (fUpdate) 
	{
		HRESULT hr = pNdasDevice->UpdateStats();
		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				"Update device status failed, hr=0x%X\n", hr);
			return hr;
		}
	}

	NDAS_DEVICE_STAT dstat;
	HRESULT hr = pNdasDevice->get_DeviceStat(&dstat);
	if (FAILED(hr))
	{
		return hr;
	}

	*lpnROHosts = dstat.UnitDevices[m_unitDeviceId.UnitNo].ROHostCount;
	*lpnRWHosts = dstat.UnitDevices[m_unitDeviceId.UnitNo].RWHostCount;

	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"Host Usage Count: RO=%d, RW=%d.\n", 
		*lpnROHosts, *lpnRWHosts);

	return S_OK;
}

void 
CNdasUnitImpl::pSetStatus(NDAS_UNITDEVICE_STATUS newStatus)
{ 
	CAutoInstanceLock autolock(this);

	m_status = newStatus; 
}

STDMETHODIMP
CNdasUnitImpl::RegisterToLogicalUnitManager()
{
	HRESULT hr;

	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	hr = pManager->Register(this, &pNdasLogicalUnit);

	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"NdasLogicalUnitManager::Register failed, NdasUnit=%p, hr=0x%X\n",
			this, hr);
		return hr;
	}

	LockInstance();
	m_pNdasLogicalUnit = pNdasLogicalUnit;
	UnlockInstance();

	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::UnregisterFromLogicalUnitManager()
{
	HRESULT hr;

	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));

	hr = pManager->Unregister(this);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"NdasLogicalUnitManager::Deregister failed, NdasUnit=%p, hr=0x%X\n", 
			this, hr);
		return hr;
	}

	LockInstance();
	m_pNdasLogicalUnit.Release();
	UnlockInstance();
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_NdasDevicePassword(
	__out UINT64* Password)
{
	return m_pParentNdasDevice->get_HardwarePassword(Password);
}

STDMETHODIMP
CNdasUnitImpl::get_NdasDeviceUserId(
	__in ACCESS_MASK Access, __out DWORD* UserId)
{
	*UserId = pGetNdasUserId(m_unitDeviceId.UnitNo, Access);
	return S_OK;
}

STDMETHODIMP 
CNdasUnitImpl::CheckNDFSCompatibility()
{
	CAutoInstanceLock autolock(this);

	HRESULT hr;

	//
	// Unit devices other than disks are not allowed for NDFS
	//
	if (m_type != NDAS_UNITDEVICE_TYPE_DISK) 
	{
		return E_FAIL;
	}

	//
	// LfsFilter compatibility check.
	// NDFS Major version should be same
	// 
	WORD wHostNDFSVerMajor;
	BOOL success = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL,
		&wHostNDFSVerMajor, NULL);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"Getting LFS Filter Version failed, error=0x%X\n", hr);
		return hr;
	}

	//
	// Primary Host Info is valid for 30 seconds
	//
#if 0
	DWORD dwMaxTickAllowance = 30 * 1000;
	if (0 != m_PrimaryHostInfo.NDFSCompatVersion &&
		::GetTickCount() < m_PrimaryHostInfo.LastUpdate + dwMaxTickAllowance) 
	{
		// primary host info is valid
		return (wHostNDFSVerMajor == m_PrimaryHostInfo.NDFSCompatVersion);
	}
#endif

	//
	// No Primary Host Info is available (IX)
	// Use HIX to discover
	//
	CNdasHIXDiscover hixdisc(pGetNdasHostGuid());
	success = hixdisc.Initialize();
	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"HIXDiscover init failed, hr=0x%X\n", hr);

		return hr;
	}

	NDAS_UNITDEVICE_ID ndasUnitId;
	get_NdasUnitId(&ndasUnitId);

	DWORD timeout = NdasServiceConfig::Get(nscWriteShareCheckTimeout);

	success = hixdisc.Discover(ndasUnitId,NHIX_UDA_SHRW_PRIM,1,timeout);
	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"hixdisc.Discover failed, hr=0x%X\n", hr);
		return hr;
	}

	DWORD nHosts = hixdisc.GetHostCount(ndasUnitId);
	if (0 == nHosts) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"GetHostCount failed, hr=0x%X\n", hr);

		return hr;
	}

	GUID hostGuid;
	success = hixdisc.GetHostData(ndasUnitId,0,NULL,&hostGuid,NULL,NULL);
	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"GetHostData failed, hr=0x%X\n", hr);

		return hr;
	}

	CNdasHostInfoCache* phic = pGetNdasHostInfoCache();
	XTLASSERT(phic); // phic is not null (by pGetNdasHostInfoCache)
	const NDAS_HOST_INFO* pHostInfo = phic->GetHostInfo(&hostGuid);
	if (NULL == pHostInfo) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, 
			"GetHostInfo failed, hr=0x%X\n", hr);

		return hr;
	}

	//
	// ReservedVerInfo contains NDFS Version Information
	//
	if (pHostInfo->ReservedVerInfo.VersionMajor != wHostNDFSVerMajor) 
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Host NDFS %d, Primary NDFS %d failed.\n",
			wHostNDFSVerMajor, pHostInfo->ReservedVerInfo.VersionMajor);

		return E_FAIL;
	}

	//
	// Primary and this host's NDFS compatible version is same
	//

	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_OptimalMaxTransferBlocks(__out DWORD * Blocks)
{
	DWORD defaultMaxTransferBlocks;
	m_pParentNdasDevice->get_MaxTransferBlocks(&defaultMaxTransferBlocks);

	DWORD hardwareVersion;
	m_pParentNdasDevice->get_HardwareVersion(&hardwareVersion);

	DWORD optimalMaxTransferBlocks = defaultMaxTransferBlocks;
	DWORD maxTransferBlockLimit = pReadMaxRequestBlockLimitConfig(hardwareVersion);

	if (optimalMaxTransferBlocks > maxTransferBlockLimit)
	{
		optimalMaxTransferBlocks = maxTransferBlockLimit;
	}

	if (optimalMaxTransferBlocks > defaultMaxTransferBlocks)
	{
		optimalMaxTransferBlocks = defaultMaxTransferBlocks;
	}

	*Blocks = optimalMaxTransferBlocks;

	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_Type(__out NDAS_UNITDEVICE_TYPE* Type)
{
	*Type = m_type;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_SubType(__out NDAS_UNITDEVICE_SUBTYPE* SubType)
{
	*SubType = m_subType;
	return S_OK;
}

STDMETHODIMP
CNdasUnitImpl::get_BlockAclSize(__in DWORD SkipLength, __out DWORD* TotalSize)
{
	return 0;
}

STDMETHODIMP
CNdasUnitImpl::FillBlockAcl(__in PVOID BlockAcl)
{
	return E_NOTIMPL;
}
 
STDMETHODIMP_(void)
CNdasUnitImpl::MountCompleted()
{
	pSetStatus(NDAS_UNITDEVICE_STATUS_MOUNTED);
}

STDMETHODIMP_(void)
CNdasUnitImpl::DismountCompleted()
{
	pSetStatus(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED);

	CComQIPtr<INdasDevicePnpSink> pNdasDevicePnpSink = m_pParentNdasDevice;
	ATLASSERT(pNdasDevicePnpSink.p);
	pNdasDevicePnpSink->UnitDismountCompleted(this);
}

void 
CNdasUnit::FinalRelease()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasUnit=%p, %s\n", this, CNdasUnitDeviceId(m_unitDeviceId).ToStringA());
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasUnitDiskDevice class implementation
//
//////////////////////////////////////////////////////////////////////////

void 
CNdasDiskUnit::FinalRelease()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasDiskUnit=%p, %s\n", this, CNdasUnitDeviceId(m_unitDeviceId).ToStringA());
}

HRESULT 
CNdasDiskUnit::Initialize(
	__in INdasDevice* pNdasDevice, 
	__in DWORD UnitNo, 
	__in NDAS_DISK_UNIT_TYPE DiskType, 
	__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo, 
	__in const NDAS_LOGICALDEVICE_GROUP& LuDefinition, 
	__in DWORD LuSequence,
	__in UINT64 UserBlocks,
	__in PVOID pRaidInfo,
	__in const NDAS_CONTENT_ENCRYPT& Encryption,
	__in NDAS_DIB_V2* pDIBv2,
	__in BLOCK_ACCESS_CONTROL_LIST *pBlockAcl)
{
	HRESULT hr = CNdasUnitImpl::Initialize(
		pNdasDevice, UnitNo, NDAS_UNITDEVICE_TYPE_DISK, 
		CreateSubType(DiskType),
		HardwareInfo,
		LuDefinition,
		LuSequence);

	if (FAILED(hr))
	{
		return hr;
	}

	ATLASSERT(NULL != pDIBv2);

	m_ulUserBlocks = UserBlocks;
	m_pNdasLogicalUnitRaidInfo.Attach(pRaidInfo);
	m_contentEncrypt = Encryption;
	m_pDIBv2.Attach(pDIBv2);
	m_pBACL.Attach(pBlockAcl);
	m_diskType = DiskType;

	//
	// m_pDIBv2 and m_pAddTargetInfo will be deleted 
	// by this class on destruction
	//
	XTLASSERT(
	(NDAS_CONTENT_ENCRYPT_METHOD_NONE == m_contentEncrypt.Method &&
	m_contentEncrypt.KeyLength == 0) ||
	(NDAS_CONTENT_ENCRYPT_METHOD_NONE != m_contentEncrypt.Method &&
	m_contentEncrypt.KeyLength > 0));

	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_VERBOSE, 
		"%s\n", CNdasUnitDeviceId(m_unitDeviceId).ToStringA());

	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::get_Dib(__out NDAS_DIB_V2* Dibv2)
{
	*Dibv2 = *m_pDIBv2;
	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::IsDibUnchanged()
{
	CAutoInstanceLock autolock(this);

	CNdasUnitDeviceFactory ndasUnitFactory(m_pParentNdasDevice, m_unitDeviceId.UnitNo);

	CComPtr<INdasUnit> pCurrentNdasUnit;
	HRESULT hr = ndasUnitFactory.CreateUnitDevice(&pCurrentNdasUnit);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Recreating an NdasUnit failed, hr=0x%X\n", hr);
		return E_FAIL;
	}

	NDAS_UNITDEVICE_TYPE currentType;
	COMVERIFY(pCurrentNdasUnit->get_Type(&currentType));
	if (m_type != currentType)
	{
		return E_FAIL;
	}

	CComQIPtr<INdasDiskUnit> pCurrentNdasDiskUnit = pCurrentNdasUnit;
	ATLASSERT(pCurrentNdasDiskUnit.p);

	NDAS_DIB_V2 currentDibV2;
	COMVERIFY(pCurrentNdasDiskUnit->get_Dib(&currentDibV2));

	ATLASSERT(NULL != m_pDIBv2);

	int cmp = memcmp(m_pDIBv2, &currentDibV2, sizeof(NDAS_DIB_V2));

	if (0 != cmp)
	{
		return E_FAIL;
	}

	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::get_UserBlocks(__out UINT64 * Blocks)
{
	// Reserve 2 MB for internal use (2 * 2 * 1024 blocks)
	// ULONG ulDiskBlocks = (ULONG) m_devInfo.SectorCount - (2 * 1024 * 2); 
	//	return ulDiskBlocks;
	*Blocks = m_ulUserBlocks;
	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::get_RaidInfo(__out PVOID* Info)
{
	*Info = m_pNdasLogicalUnitRaidInfo;
	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::get_ContentEncryption(__out NDAS_CONTENT_ENCRYPT* Encryption)
{
	*Encryption = m_contentEncrypt;
	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::IsBitmapClean()
{
	CAutoInstanceLock autolock(this);

	CNdasDeviceComm devComm(m_pParentNdasDevice, m_unitDeviceId.UnitNo);

	HRESULT hr = devComm.Connect(FALSE);
	if (FAILED(hr)) 
	{
		return hr;
	}

	NDAS_UNITDEVICE_HARDWARE_INFO udinfo = {0};
	udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
	hr = devComm.GetNdasUnitInfo(&udinfo);
	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"GetUnitDeviceInformation of %s failed, error=0x%X\n", 
			CNdasUnitDeviceId(m_unitDeviceId).ToStringA(), hr);
		return hr;
	}

	BYTE BitmapData[128 * 512] = {0};

	// 1MB from NDAS_BLOCK_LOCATION_BITMAP
	for (INT i = 0; i < 16; i++)  
	{
		hr = devComm.ReadDiskBlock(
			BitmapData, 
			NDAS_BLOCK_LOCATION_BITMAP + (i * 128), 
			128);

		if (FAILED(hr)) 
		{
			return hr;
		}

		PULONG pBitmapData = (PULONG)BitmapData;
		for (INT j = 0; j < 128 * 512 / 4; ++j) 
		{
			if (*pBitmapData) 
			{
				return E_FAIL;
			}
			pBitmapData++;
		}
	}	

	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::get_BlockAclSize(__in DWORD ElementsToSkip, __out DWORD* TotalSize)
{
	*TotalSize = 0;

	if (NULL == m_pBACL)
	{
		*TotalSize = 0;
		return S_OK;
	}

	XTLASSERT(m_pBACL->ElementCount >= ElementsToSkip);

	*TotalSize = 
		FIELD_OFFSET(NDAS_BLOCK_ACL, BlockACEs) + 
		sizeof(NDAS_BLOCK_ACE) * m_pBACL->ElementCount - ElementsToSkip;

	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"GetBACLSize() m_pBACL->ElementCount(%d). Size(%d)\n",
		m_pBACL->ElementCount, *TotalSize);

	return S_OK;
}

STDMETHODIMP 
CNdasDiskUnit::FillBlockAcl(__in PVOID BlockAcl)
{
	PNDAS_BLOCK_ACL pNdasBlockAcl = (NDAS_BLOCK_ACL *)BlockAcl;
	PNDAS_BLOCK_ACE pNdasBlockAce = NULL;
	PBLOCK_ACCESS_CONTROL_LIST_ELEMENT pBACLE = NULL;

	if (!m_pBACL)
	{
		return S_FALSE;
	}

	if (!BlockAcl)
	{
		return S_FALSE;
	}

	UINT nBACLESkipped = 0;
	for (UINT i = 0; i < m_pBACL->ElementCount; i++)
	{
		pBACLE = &m_pBACL->Elements[i];
		if (!(pBACLE->AccessMask & BACL_ACCESS_MASK_PC_SYSTEM))
		{
			nBACLESkipped++;
			continue;
		}
		pNdasBlockAce = &pNdasBlockAcl->BlockACEs[i - nBACLESkipped];
		pNdasBlockAce->AccessMode |= 
			(pBACLE->AccessMask & BACL_ACCESS_MASK_WRITE) ? NBACE_ACCESS_WRITE : 0;
		pNdasBlockAce->AccessMode |= 
			(pBACLE->AccessMask & BACL_ACCESS_MASK_READ) ? NBACE_ACCESS_READ : 0;
		pNdasBlockAce->BlockStartAddr = pBACLE->ui64StartSector;
		pNdasBlockAce->BlockEndAddr =
			pBACLE->ui64StartSector + pBACLE->ui64SectorCount -1;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
			"FillBACL() pNdasBlockAce : %x %I64d ~ %I64d\n",
			pNdasBlockAce->AccessMode,
			pNdasBlockAce->BlockStartAddr,
			pNdasBlockAce->BlockEndAddr);
	}

	DWORD blockAclLength;
	COMVERIFY(get_BlockAclSize(nBACLESkipped, &blockAclLength));

	pNdasBlockAcl->Length = blockAclLength;
	pNdasBlockAcl->BlockACECnt = m_pBACL->ElementCount - nBACLESkipped;

	return S_OK;
}

const NDAS_CONTENT_ENCRYPT NDAS_CONTENT_ENCRYPT_NONE = {
	NDAS_CONTENT_ENCRYPT_METHOD_NONE
};

//////////////////////////////////////////////////////////////////////////
//
// Null Unit Disk Device
//
//////////////////////////////////////////////////////////////////////////

void 
CNdasNullDiskUnit::FinalRelease()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasNullDiskUnit=%p, %s\n", this, CNdasUnitDeviceId(m_unitDeviceId).ToStringA());
}

HRESULT 
CNdasNullDiskUnit::Initialize(
	__in INdasDevice* pNdasDevice, 
	__in DWORD UnitNo,
	__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo,
	__in NDAS_UNITDEVICE_ERROR Error)
{
	HRESULT hr = CNdasDiskUnit::Initialize(
		pNdasDevice, 
		UnitNo, 
		NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN,
		HardwareInfo,
		NDAS_LOGICALDEVICE_GROUP_NONE,
		0,
		0,
		NULL,
		NDAS_CONTENT_ENCRYPT_NONE,
		NULL,
		NULL);
	if (FAILED(hr))
	{
		return hr;
	}

	m_status = NDAS_UNITDEVICE_STATUS_UNKNOWN;
	m_lastError = Error;

	return S_OK;
}

STDMETHODIMP 
CNdasNullDiskUnit::RegisterToLogicalUnitManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullRegister\n");
	return S_OK;
}

STDMETHODIMP 
CNdasNullDiskUnit::UnregisterFromLogicalUnitManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullUnregister\n");
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// Null Unit Device
//
//////////////////////////////////////////////////////////////////////////

void 
CNdasNullUnit::FinalRelease()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasNullUnit=%p, %s\n", this, CNdasUnitDeviceId(m_unitDeviceId).ToStringA());
}

HRESULT 
CNdasNullUnit::Initialize(
	__in INdasDevice* pNdasDevice, __in DWORD UnitNo)
{
	HRESULT hr = CNdasUnitImpl::Initialize(
		pNdasDevice, 
		UnitNo, 
		NDAS_UNITDEVICE_TYPE_UNKNOWN,
		NDAS_UNITDEVICE_SUBTYPE_NONE,
		NDAS_UNITDEVICE_HARDWARE_INFO_NONE,
		NDAS_LOGICALDEVICE_GROUP_NONE,
		0);

	if (FAILED(hr))
	{
		return hr;
	}

	m_status = NDAS_UNITDEVICE_STATUS_UNKNOWN;
	m_lastError = NDAS_UNITDEVICE_ERROR_IDENTIFY_FAILURE;

	return S_OK;
}

STDMETHODIMP 
CNdasNullUnit::RegisterToLogicalUnitManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullRegister\n");
	return S_OK;
}

STDMETHODIMP 
CNdasNullUnit::UnregisterFromLogicalUnitManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullUnregister\n");
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Function Implementations
//
//////////////////////////////////////////////////////////////////////////

namespace
{

NDAS_UNITDEVICE_ID 
pCreateUnitDeviceId(
	INdasDevice* pNdasDevice,
	DWORD unitNo)
{	
	NDAS_UNITDEVICE_ID ndasUnitId;
	COMVERIFY(pNdasDevice->get_NdasDeviceId(&ndasUnitId.DeviceId));
	ndasUnitId.UnitNo = unitNo;
	return ndasUnitId;
}

NDAS_LOGICALDEVICE_TYPE
pUnitDeviceLogicalDeviceType(
	NDAS_UNITDEVICE_TYPE udType,
	NDAS_UNITDEVICE_SUBTYPE udSubtype)
{
	switch (udType) 
	{
	case NDAS_UNITDEVICE_TYPE_DISK:
		switch (udSubtype.DiskDeviceType) 
		{
		case NDAS_UNITDEVICE_DISK_TYPE_SINGLE:
			return NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
		case NDAS_UNITDEVICE_DISK_TYPE_CONFLICT:
			return NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB;
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
		case NDAS_UNITDEVICE_DISK_TYPE_RAID1_R2:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID1_R3:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID4:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID4_R2:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID4_R3:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3;
		case NDAS_UNITDEVICE_DISK_TYPE_RAID5:
			return NDAS_LOGICALDEVICE_TYPE_DISK_RAID5;
		}
		break;
	case NDAS_UNITDEVICE_TYPE_CDROM:
		return NDAS_LOGICALDEVICE_TYPE_DVD;
	case NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK:
		return NDAS_LOGICALDEVICE_TYPE_FLASHCARD;
	case NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY:
		return NDAS_LOGICALDEVICE_TYPE_MO;
	}
	XTLASSERT(FALSE);
	return NDAS_LOGICALDEVICE_TYPE_UNKNOWN;
}

} // namespace

