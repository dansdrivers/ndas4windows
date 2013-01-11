/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <socketlpx.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndascomm.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasid.h>
#include <ndasbusioctl.h>

#include "ndassvcdef.h"
#include "ndasdevhb.h"
#include "ndascomobjectsimpl.hpp"

#include "ndasdevid.h"
#include "ndasunitdev.h"
#include "ndasdevhb.h"
#include "ndascfg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasdevcomm.h"
#include "ndasunitdevfactory.h"
#include "ndasobjs.h"
#include "lpxcomm.h"

#include "ndasdev.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdev.tmh"
#endif

const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE  = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER  = NDAS_OEM_CODE_DEFAULT;
const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE = { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };
const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE = { 0xCD, NDAS_VID_SEAGATE, 0xFF, 0xFF };

//////////////////////////////////////////////////////////////////////////
//
// AutoNdasHandle
//
//////////////////////////////////////////////////////////////////////////

struct AutoNdasHandleConfig
{
	static HNDAS GetInvalidValue() { return NULL; }
	static void Release(HNDAS h)
	{
		DWORD dwError = ::GetLastError();
		BOOL success = ::NdasCommDisconnect(h);
		XTLASSERT(success);
		::SetLastError(dwError);
	}
};

typedef XTL::AutoResourceT<HNDAS, AutoNdasHandleConfig> AutoNdasHandle;

//////////////////////////////////////////////////////////////////////////

inline bool 
IsSupportedHardwareVersion(UCHAR ucType, UCHAR ucVersion)
{
	return (0 == ucType) && (0 == ucVersion || 1 == ucVersion || 2 == ucVersion);
}

const NDAS_OEM_CODE& 
GetDefaultNdasOemCode(
	const NDAS_DEVICE_ID& DeviceId,
	const NDASID_EXT_DATA& NdasIdExtension)
{
	if (NDAS_VID_SEAGATE == NdasIdExtension.VID)
	{
		return NDAS_OEM_CODE_SEAGATE;
	}

	// Sample hardware range: 00:F0:0F:xx:xx:xx
	if (0x00 == DeviceId.Node[0] && 
		0xF0 == DeviceId.Node[1] && 
		0x0F == DeviceId.Node[2])
	{
		return NDAS_OEM_CODE_SAMPLE;
	} 
	// Rutter Tech NDAS Device address range:
	// 00:0B:D0:20:00:00 - 00:0B:D0:21:FF:FF
	//
	else if (
		0x00 == DeviceId.Node[0] &&
		0x0B == DeviceId.Node[1] &&
		0xD0 == DeviceId.Node[2] &&
		(0x20 == (DeviceId.Node[3] & 0xFE)))
	{
		return NDAS_OEM_CODE_RUTTER;
	}
	// Retail Devices
	return NDAS_OEM_CODE_DEFAULT;
}

class CNdasEventMonitor;

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDevice
//
//////////////////////////////////////////////////////////////////////////

//
// constructor
//

namespace
{
	__forceinline bool
	FlagIsSet(DWORD Flags, DWORD TargetFlag)
	{
		return (Flags & TargetFlag) == (TargetFlag);
	}

	struct CNdasUnitDeviceStat : _NDAS_UNITDEVICE_STAT 
	{
		CNdasUnitDeviceStat() 
		{
			::ZeroMemory(this, sizeof(NDAS_UNITDEVICE_STAT));
			this->Size = sizeof(NDAS_UNITDEVICE_STAT);
		}
	};

	struct CNdasDeviceStat : _NDAS_DEVICE_STAT 
	{
		CNdasDeviceStat() 
		{
			::ZeroMemory(this, sizeof(NDAS_DEVICE_STAT));
			this->Size = sizeof(NDAS_DEVICE_STAT);
			this->UnitDevices[0].Size = sizeof(NDAS_UNITDEVICE_STAT);
			this->UnitDevices[1].Size = sizeof(NDAS_UNITDEVICE_STAT);
		}
	};

	struct UnitDeviceNoEquals : std::unary_function<INdasUnit*,bool> 
	{
		const DWORD UnitNo;
		UnitDeviceNoEquals(DWORD UnitNo) : UnitNo(UnitNo) 
		{}
		bool operator()(INdasUnit* pNdasUnit) const 
		{
			DWORD unitNo;
			pNdasUnit->get_UnitNo(&unitNo);
			return (UnitNo == unitNo);
		}
	};

	struct UnitDeviceStatusEquals : std::unary_function<INdasUnit*,bool> {
		const NDAS_UNITDEVICE_STATUS Status;
		UnitDeviceStatusEquals(NDAS_UNITDEVICE_STATUS Status) : Status(Status) 
		{}
		bool operator()(INdasUnit* pNdasUnit) const 
		{
			NDAS_UNITDEVICE_STATUS status;
			if (SUCCEEDED(pNdasUnit->get_Status(&status)))
			{
				return Status == status;
			}
			return false;
		}
	};

	struct RegisterNdasUnit : std::unary_function<INdasUnit*,void> 
	{
		void operator()(INdasUnit* pNdasUnit) const 
		{
			HRESULT hr = pNdasUnit->RegisterToLogicalUnitManager();
			COMASSERT(hr);
		}
	};

	struct UnregisterNdasUnit : std::unary_function<INdasUnit*,void> 
	{
		void operator()(INdasUnit* pNdasUnit) const 
		{
			HRESULT hr = pNdasUnit->UnregisterFromLogicalUnitManager();
			COMASSERT(hr);
		}
	};
}

const NDAS_DEVICE_HARDWARE_INFO NullHardwareInfo = {0};

HRESULT 
CNdasDevice::Initialize(
	__in DWORD SlotNo, 
	__in const NDAS_DEVICE_ID& DeviceId,
	__in DWORD RegFlags,
	__in_opt const NDASID_EXT_DATA* NdasIdExtension)
{
	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
		"slot=%d, deviceId=%s\n", SlotNo, CNdasDeviceId(DeviceId).ToStringA());

	m_Status = NDAS_DEVICE_STATUS_DISABLED;
	m_NdasDeviceError = NDAS_DEVICE_ERROR_NONE;
	m_SlotNo = SlotNo;
	m_NdasDeviceId = DeviceId;
	m_grantedAccess = 0x00000000L;
	m_LastHeartbeatTick = 0;
	m_DiscoverErrors = 0;
	m_RegFlags = RegFlags;
	m_NdasIdExtension = NdasIdExtension ? *NdasIdExtension : NDAS_ID_EXTENSION_DEFAULT;
	m_OemCode = GetDefaultNdasOemCode(DeviceId, NdasIdExtension ? *NdasIdExtension : NDAS_ID_EXTENSION_DEFAULT);
	m_NdasDeviceStat = CNdasDeviceStat();
	m_HardwareInfo = NullHardwareInfo;

	COMVERIFY(StringCchPrintf(m_ConfigContainer, 30, _T("Devices\\%04d"), SlotNo));

	//
	// After we allocate m_szCfgContainer, we can use SetConfigValue
	//
	if (m_RegFlags & NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED)
	{
		(void) pSetConfigValue(_T("AutoRegistered"), TRUE);
	}
	else
	{
		(void) pDeleteConfigValue(_T("AutoRegistered"));		
	}

	ZeroMemory(&m_SocketAddressData, sizeof(m_SocketAddressData));

	m_LocalSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	m_LocalSocketAddress.lpSockaddr = &m_SocketAddressData.Local.Address;

	m_RemoteSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	m_RemoteSocketAddress.lpSockaddr = &m_SocketAddressData.Remote.Address;

	HRESULT hr;

	m_TaskQueueSemaphore = CreateSemaphore(NULL, 0, 1000, NULL);

	if (NULL == m_TaskQueueSemaphore)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	m_TaskThread = AtlCreateThread(pTaskThreadStart, this);

	if (NULL == m_TaskThread)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		ATLVERIFY(CloseHandle(m_TaskQueueSemaphore));
		return hr;
	}

	return S_OK;
}

void
CNdasDevice::FinalRelease()
{
	if (NULL != m_TaskThread)
	{
		pQueueTask(NdasDeviceTaskItem::HALT_TASK);
		Sleep(0);
		DWORD waitResult = WaitForSingleObject(m_TaskThread, 0);
		if (WAIT_TIMEOUT == waitResult)
		{
			XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_WARNING,
				"Wait for task thread to stop...\n");
			WaitForSingleObject(m_TaskThread, INFINITE);
		}
		ATLVERIFY(CloseHandle(m_TaskThread));
	}

	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
		"slot=%d, deviceId=%s\n",
		m_SlotNo, CNdasDeviceId(m_NdasDeviceId).ToStringA());
}

template <typename T>
BOOL
CNdasDevice::pSetConfigValue(LPCTSTR szName, T value)
{
	// We don't write to a registry if volatile 
	if (pIsVolatile()) return TRUE;

	BOOL success = _NdasSystemCfg.
		SetValueEx(m_ConfigContainer, szName, value);
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_WARNING,
			"Writing device configuration to %ls failed, error=0x%X\n",
			m_ConfigContainer, GetLastError());
	}

	return success;
}

BOOL 
CNdasDevice::pSetConfigValueSecure(
	LPCTSTR szName, LPCVOID lpValue, DWORD cbValue)
{
	// We don't write to a registry if volatile 
	if (pIsVolatile()) return TRUE;

	BOOL success = _NdasSystemCfg.
		SetSecureValueEx(m_ConfigContainer, szName, lpValue, cbValue);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_WARNING,
			"Writing data to %ls of %ls failed, error=0x%X\n", 
			szName, m_ConfigContainer, GetLastError());
	}
	return success;
}

template <typename T>
BOOL 
CNdasDevice::pSetConfigValueSecure(LPCTSTR szName, T value)
{
	return pSetConfigValueSecure(szName, &value, sizeof(T));
}

BOOL 
CNdasDevice::pDeleteConfigValue(LPCTSTR szName)
{
	BOOL success = _NdasSystemCfg.DeleteValue(m_ConfigContainer, szName);
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_WARNING,
			"Delete configuration %ws\\%ws failed, error=0x%X\n", 
			m_ConfigContainer, szName, GetLastError());
	}
	return success;
}

bool 
CNdasDevice::pIsVolatile()
{
	return (m_RegFlags & NDAS_DEVICE_REG_FLAG_VOLATILE) ? true : false;
}

//
// set status of the device (internal use only)
//

BOOL
CNdasDevice::pChangeStatus(
	__in NDAS_DEVICE_STATUS newStatus, 
	__in_opt const CInterfaceArray<INdasUnit>* NewNdasUnits)
{
	CAutoInstanceLock autolock(this);

	const NDAS_DEVICE_STATUS oldStatus = m_Status;

	if (oldStatus == newStatus) 
	{
		return FALSE;
	}

	switch (newStatus)
	{
	case NDAS_DEVICE_STATUS_ONLINE:
		XTLASSERT(NewNdasUnits);
		m_NdasUnits.Copy(*NewNdasUnits);
		m_DiscoverErrors = 0;
		break;
	case NDAS_DEVICE_STATUS_DISABLED:
		m_DiscoverErrors = 0;
		break;
	}

	m_Status = newStatus;

	autolock.Release();

	if (NDAS_DEVICE_STATUS_DISABLED == oldStatus && 
		NDAS_DEVICE_STATUS_OFFLINE == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Advise(this);
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (NDAS_DEVICE_STATUS_OFFLINE == oldStatus && 
		NDAS_DEVICE_STATUS_DISABLED == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Unadvise(this);
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (NDAS_DEVICE_STATUS_OFFLINE == oldStatus && 
		NDAS_DEVICE_STATUS_CONNECTING == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (NDAS_DEVICE_STATUS_CONNECTING == oldStatus && 
		NDAS_DEVICE_STATUS_OFFLINE == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		// NO EVENT MONITOR CHANGE
		pDestroyAllUnitDevices();
	}
	else if (NDAS_DEVICE_STATUS_CONNECTING == oldStatus && 
		NDAS_DEVICE_STATUS_ONLINE == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		pGetNdasEventMonitor().Attach(this);
		// NO UNIT DEVICE CHANGE
	}
	else if (NDAS_DEVICE_STATUS_ONLINE == oldStatus && 
		NDAS_DEVICE_STATUS_OFFLINE == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		pGetNdasEventMonitor().Detach(this);
		pDestroyAllUnitDevices();
	}
	else if (NDAS_DEVICE_STATUS_ONLINE == oldStatus && 
		NDAS_DEVICE_STATUS_DISABLED == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Unadvise(this);
		//pGetNdasDeviceHeartbeatListener().Detach(this);
		pGetNdasEventMonitor().Detach(this);
		pDestroyAllUnitDevices();
	}
	else
	{
		XTLASSERT(FALSE && "INVALID STATUS CHANGE");
	}

	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasDevice=%d, status changed %ls to %ls\n",
		m_SlotNo,
		NdasDeviceStatusString(oldStatus),
		NdasDeviceStatusString(newStatus));

	(void) pGetNdasEventPublisher().DeviceStatusChanged(m_SlotNo, oldStatus, newStatus);

	return TRUE;
}

STDMETHODIMP CNdasDevice::put_Enabled(__in BOOL Enabled)
{
	CAutoInstanceLock autolock(this);

	if (Enabled) 
	{
		//
		// To enable this device
		//
		if (NDAS_DEVICE_STATUS_DISABLED != m_Status) 
		{
			return S_FALSE;
		} 

		//
		// DISABLED -> DISCONNECTED
		//
		pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
	} 
	else 
	{
		//
		// To disable this device
		//
		if (NDAS_DEVICE_STATUS_DISABLED == m_Status) 
		{
			return S_FALSE;
		} 

		if (NDAS_DEVICE_STATUS_CONNECTING == m_Status)
		{
			return NDASSVC_ERROR_OPERATION_NOT_ALLOWED_WHILE_CONNECTING;
		}

		//
		// You cannot disable this device when a unit device is mounted
		//
		if (pIsAnyUnitDevicesMounted())
		{
			return NDASSVC_ERROR_CANNOT_DISABLE_MOUNTED_DEVICE;
		}

		//
		// DISCONNECTED/CONNECTED -> DISABLED
		//
		pChangeStatus(NDAS_DEVICE_STATUS_DISABLED);
	}

	XTLVERIFY( pSetConfigValue(_T("Enabled"), Enabled) );

	//
	// Clear Device Error
	//
	pSetLastDeviceError(NDAS_DEVICE_ERROR_NONE);

	return S_OK;
}

STDMETHODIMP CNdasDevice::put_Name(__in BSTR Name)
{
	CAutoInstanceLock autolock(this);

	_ATLTRY 
	{
		m_Name = Name;
	} 
	_ATLCATCH(ex)
	{
		return ex;
	}

	(void) pSetConfigValue(_T("DeviceName"), Name);
	(void) pGetNdasEventPublisher().DevicePropertyChanged(m_SlotNo);

	return S_OK;
}

STDMETHODIMP CNdasDevice::get_Name(__out BSTR* Name)
{
	CAutoInstanceLock autolock(this);

	HRESULT hr = m_Name.CopyTo(Name);
	return hr;
}

STDMETHODIMP CNdasDevice::get_NdasDeviceId(__out NDAS_DEVICE_ID* NdasDeviceId)
{
	*NdasDeviceId = m_NdasDeviceId;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_RegisterFlags(__out DWORD* Flags)
{
	*Flags = m_RegFlags;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_Status(__out NDAS_DEVICE_STATUS* Status)
{
	CAutoInstanceLock autolock(this);
	*Status = m_Status;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_DeviceError(__out NDAS_DEVICE_ERROR* Error)
{
	CAutoInstanceLock autolock(this);
	*Error = m_NdasDeviceError;
	return S_OK;
}

//
// get slot number of the NDAS device
// (don't be confused with logical device slot number.)
//

STDMETHODIMP CNdasDevice::get_SlotNo(__out DWORD* SlotNo)
{
	*SlotNo = m_SlotNo;
	return S_OK;
}

//
// get granted (registered) access permission
//

STDMETHODIMP CNdasDevice::get_GrantedAccess(__out ACCESS_MASK* Access)
{
	*Access = InterlockedCompareExchange(
		(volatile LONG*)&m_grantedAccess, 0, 0);
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_AllowedAccess(__out ACCESS_MASK* Access)
{
	//
	// depends on the implementation of the driver
	// whether allows multiple write access or not
	//
	// TODO: Refine this!
	//
	// (This comment is copied from CNdasUnit::GetAllowingAccess)
	return get_GrantedAccess(Access);
}

//
// set granted access permission
// use this to change the (valid) access of the registered device
//

STDMETHODIMP CNdasDevice::put_GrantedAccess(__in ACCESS_MASK access)
{
	// only GENERIC_READ and GENERIC_WRITE are acceptable
	ACCESS_MASK mask = (access & (GENERIC_READ | GENERIC_WRITE));
	InterlockedExchange((volatile LONG*)&m_grantedAccess, mask);

	const DWORD bufferSize = sizeof(mask) + sizeof(m_NdasDeviceId);
	BYTE buffer[bufferSize] = {0};

	::CopyMemory(buffer, &mask, sizeof(mask));
	::CopyMemory(buffer + sizeof(mask), &m_NdasDeviceId, sizeof(m_NdasDeviceId));

	(void) pSetConfigValueSecure(_T("GrantedAccess"), buffer, bufferSize);
	(void) pGetNdasEventPublisher().DevicePropertyChanged(m_SlotNo);

	return S_OK;
}

//
// on CONNECTED status only
// the local LPX address where the NDAS device was found
//

//
// remote LPX address of this device
// this may be different than the actual device ID. (Proxied?)
//

STDMETHODIMP CNdasDevice::get_RemoteAddress(__inout SOCKET_ADDRESS * SocketAddress)
{
	CAutoInstanceLock autolock(this);
	LPSOCKET_ADDRESS sourceSocketAddress = &m_RemoteSocketAddress;
	if (SocketAddress->iSockaddrLength < sourceSocketAddress->iSockaddrLength)
	{
		SocketAddress->iSockaddrLength = sourceSocketAddress->iSockaddrLength;
		return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
	}
	SocketAddress->iSockaddrLength = sourceSocketAddress->iSockaddrLength;
	CopyMemory(
		SocketAddress->lpSockaddr,
		sourceSocketAddress->lpSockaddr,
		sourceSocketAddress->iSockaddrLength);
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_LocalAddress(__inout SOCKET_ADDRESS * SocketAddress)
{
	CAutoInstanceLock autolock(this);
	LPSOCKET_ADDRESS sourceSocketAddress = &m_LocalSocketAddress;
	if (SocketAddress->iSockaddrLength < sourceSocketAddress->iSockaddrLength)
	{
		SocketAddress->iSockaddrLength = sourceSocketAddress->iSockaddrLength;
		return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
	}
	SocketAddress->iSockaddrLength = sourceSocketAddress->iSockaddrLength;
	CopyMemory(
		SocketAddress->lpSockaddr,
		sourceSocketAddress->lpSockaddr,
		sourceSocketAddress->iSockaddrLength);
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_HardwareVersion(__out DWORD* HardwareVersion)
{
	CAutoInstanceLock autolock(this);
	*HardwareVersion = m_HardwareInfo.HardwareVersion;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_HardwareType(__out DWORD* HardwareType)
{
	CAutoInstanceLock autolock(this);
	*HardwareType = m_HardwareInfo.HardwareType;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_HardwareRevision(__out DWORD* HardwareRevision)
{
	CAutoInstanceLock autolock(this);
	*HardwareRevision = m_HardwareInfo.HardwareRevision;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_HardwarePassword(__out UINT64* HardwarePassword)
{
	CAutoInstanceLock autolock(this);
	*HardwarePassword = m_OemCode.UI64Value;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_OemCode(__out NDAS_OEM_CODE* OemCode)
{
	CAutoInstanceLock autolock(this);
	*OemCode = m_OemCode;
	return S_OK;
}

STDMETHODIMP CNdasDevice::put_OemCode(__in const NDAS_OEM_CODE* OemCode)
{
	CAutoInstanceLock autolock(this);
	if (NULL == OemCode)
	{
		CAutoInstanceLock autolock(this);
		m_OemCode = GetDefaultNdasOemCode(m_NdasDeviceId, m_NdasIdExtension);
		(void) pDeleteConfigValue(_T("OEMCode"));
	}
	else
	{
		m_OemCode = *OemCode;
		XTLVERIFY(pSetConfigValueSecure(
			_T("OEMCode"), &m_OemCode, sizeof(NDAS_OEM_CODE)));
	}
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_HardwareInfo(__out NDAS_DEVICE_HARDWARE_INFO* HardwareInfo)
{
	CAutoInstanceLock autolock(this);
	*HardwareInfo = m_HardwareInfo;
	return S_OK;
}

STDMETHODIMP CNdasDevice::get_MaxTransferBlocks(__out LPDWORD MaxTransferBlocks)
{
	CAutoInstanceLock autolock(this);
	*MaxTransferBlocks = m_HardwareInfo.MaximumTransferBlocks;
	return S_OK;
}

STDMETHODIMP 
CNdasDevice::get_DeviceStat(__out NDAS_DEVICE_STAT* DeviceStat)
{
	CAutoInstanceLock autolock(this);
	*DeviceStat = m_NdasDeviceStat;
	return S_OK;
}

STDMETHODIMP 
CNdasDevice::get_NdasIdExtension(__out NDASID_EXT_DATA* IdExtension)
{
	CAutoInstanceLock autolock(this);
	*IdExtension = m_NdasIdExtension;
	return S_OK;
}

STDMETHODIMP 
CNdasDevice::get_NdasUnits(
	__inout CInterfaceArray<INdasUnit> & NdasUnits)
{
	CAutoInstanceLock autolock(this);
	NdasUnits.Copy(m_NdasUnits);
	return S_OK;
}

STDMETHODIMP 
CNdasDevice::get_NdasUnit(
	__in DWORD UnitNo, __deref_out INdasUnit** ppNdasUnit)
{
	CAutoInstanceLock autolock(this);

	*ppNdasUnit = 0;

	size_t count = m_NdasUnits.GetCount();

	for (size_t index = 0; index < count; ++index)
	{
		CComPtr<INdasUnit> pNdasUnit = m_NdasUnits.GetAt(index);
		if (UnitDeviceNoEquals(UnitNo)(pNdasUnit))
		{
			*ppNdasUnit = pNdasUnit.Detach();
			return S_OK;
		}
	}

	return E_FAIL;
}

//
// Subject to the Heartbeat Listener object
//

void
CNdasDevice::NdasHeartbeatReceived(const NDAS_DEVICE_HEARTBEAT_DATA* Data)
{
	//
	// matching device id (address) only
	//
	// LPX_ADDRESS and NDAS_DEVICE_ID are different type
	// so we cannot merely use CompareLpxAddress function here
	//
	XTLC_ASSERT_EQUAL_SIZE(Data->RemoteAddress.Node, m_NdasDeviceId.Node);
	if (0 != memcmp(Data->RemoteAddress.Node, m_NdasDeviceId.Node, sizeof(m_NdasDeviceId.Node)))
	{
		return;
	}
	OnHeartbeat(Data->LocalAddress, Data->RemoteAddress, Data->Type, Data->Version);
}

//
// status check event handler
// to reconcile the status 
//
// to be connected status, broadcast packet
// should be received within MAX_ALLOWED_HEARTBEAT_INTERVAL
//
STDMETHODIMP_(void)
CNdasDevice::OnTimer()
{
	// When just a single unit device is mounted,
	// status will not be changed to DISCONNECTED!
	if (pIsAnyUnitDevicesMounted()) 
	{
		return;
	}

	CAutoInstanceLock autolock(this);

	// No checkup is required if not connected
	if (NDAS_DEVICE_STATUS_ONLINE != m_Status) 
	{
		return;
	}

	static volatile BOOL NdasDisconnectOnDebug = FALSE; // Change this value with debugger to override setting.
	
	const DWORD MaxHeartbeatInterval = NdasServiceConfig::Get(nscMaximumHeartbeatInterval);
	DWORD dwCurrentTick = ::GetTickCount();
	DWORD dwElapsed = dwCurrentTick - m_LastHeartbeatTick;
	if (dwElapsed > MaxHeartbeatInterval) 
	{
		//
		// Do not disconnect the device when the debugger is attached
		//
		if (!NdasDisconnectOnDebug &&
			!NdasServiceConfig::Get(nscDisconnectOnDebug) &&
			::IsDebuggerPresent()) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
				"Debugger is attached. Not disconnecting.\n");
		}
		else
		{
			pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
		}
	}
	return;
}

//
// Discovered event handler
//
// on discovered, check the supported hardware type and version,
// and get the device information
//


void
CNdasDevice::OnHeartbeat(
	const LPX_ADDRESS& localAddress,
	const LPX_ADDRESS& remoteAddress,
	UCHAR Type,
	UCHAR Version)
{
	CAutoInstanceLock autolock(this);

	if (NDAS_DEVICE_STATUS_DISABLED == m_Status) 
	{
		return;
	}

	//
	// If the local address is different, ignore it.
	// because we may be receiving from multiple interfaces.
	//
	// In case of interface change, as we don't update the heartbeat tick
	// here, device will be disconnected and re-discovered.
	//
	// So, it's safe to ignore non-bound local address
	//
	// However, we can assume the local address is valid
	// only if the status is CONNECTED.
	// 
	if (NDAS_DEVICE_STATUS_ONLINE == m_Status) 
	{
		PSOCKADDR_LPX sockAddrLpx = reinterpret_cast<PSOCKADDR_LPX>(
			m_LocalSocketAddress.lpSockaddr);
		if (!IsEqualLpxAddress(sockAddrLpx->LpxAddress, localAddress))
		{
			return;
		}
	}

	//
	// Version Checking
	//
	if (!IsSupportedHardwareVersion(Type, Version))
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"Unsupported NDAS device detected, Type=%d, Version=%d.\n", 
			Type, Version);

		//
		// TODO: EVENTLOG unsupported version detected!
		//
		pSetLastDeviceError(NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION);

		return;
	}

	// Update heartbeat tick
	m_LastHeartbeatTick = ::GetTickCount();

	// Connected status ignores this heartbeat
	// We only updates Last Heartbeat Tick
	if (NDAS_DEVICE_STATUS_ONLINE == m_Status ||
		NDAS_DEVICE_STATUS_CONNECTING == m_Status)
	{
		return;
	}

	XTLASSERT(NDAS_DEVICE_STATUS_OFFLINE == m_Status);
	if (NDAS_DEVICE_STATUS_OFFLINE != m_Status)
	{
		return;
	}

	// Now the device is in DISCONNECTED status

	// Failure Count to prevent possible locking of the service
	// because of the discover failure timeout
	DWORD fatalErrorThreshold = NdasServiceConfig::Get(nscHeartbeatFailLimit);
	if (m_DiscoverErrors >= fatalErrorThreshold) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE
		//
		pSetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE);
		return;
	}

	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_VERBOSE,
		"Discovered %s at local %s.\n", 
		CLpxAddress(remoteAddress).ToStringA(), 
		CLpxAddress(localAddress).ToStringA());

	//
	// Save the local and remote address
	//

	//
	// Only address node fields are copied
	//

	ZeroMemory(
		&m_SocketAddressData.Local.LpxAddress,
		sizeof(SOCKADDR_LPX));

	m_SocketAddressData.Local.LpxAddress.sin_family = AF_LPX;

	CopyMemory(
		m_SocketAddressData.Local.LpxAddress.LpxAddress.Node,
		localAddress.Node,
		sizeof(localAddress.Node));

	m_LocalSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	m_LocalSocketAddress.lpSockaddr = &m_SocketAddressData.Local.Address;


	ZeroMemory(
		&m_SocketAddressData.Remote.LpxAddress,
		sizeof(SOCKADDR_LPX));

	m_SocketAddressData.Remote.LpxAddress.sin_family = AF_LPX;

	CopyMemory(
		m_SocketAddressData.Remote.LpxAddress.LpxAddress.Node,
		remoteAddress.Node,
		sizeof(remoteAddress.Node));

	m_RemoteSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	m_RemoteSocketAddress.lpSockaddr = &m_SocketAddressData.Remote.Address;

	//
	// Queue the discover routine
	//
	pQueueDiscoverTask();
}

HRESULT
CNdasDevice::pQueueNdasUnitCreationTask(DWORD UnitNo)
{
	return pQueueTask(NdasDeviceTaskItem::NDAS_UNIT_CREATE_TASK, UnitNo);
}

HRESULT 
CNdasDevice::pTaskThreadStart()
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	//
	// Separate thread should hold the reference
	//
	CComPtr<INdasDevice> pNdasDevice(this);

	while (TRUE)
	{
		DWORD waitResult = WaitForSingleObject(m_TaskQueueSemaphore, INFINITE);
		ATLASSERT(WAIT_OBJECT_0 == waitResult);

		m_TaskQueueLock.LockInstance();
		NdasDeviceTaskItem taskItem = m_TaskQueue.back();
		m_TaskQueue.pop();
		m_TaskQueueLock.UnlockInstance();

		switch (taskItem.TaskType)
		{
		case NdasDeviceTaskItem::HALT_TASK:
			return S_OK;
		case NdasDeviceTaskItem::DISCOVER_TASK:
			pDiscoverThreadStart();
			break;
		case NdasDeviceTaskItem::NDAS_UNIT_CREATE_TASK:
			pNdasUnitCreationThreadStart(taskItem.UnitNo);
			break;
		default:
			ATLASSERT(FALSE && "Invalid task item is queued\n");
		}
	}
}

HRESULT
CNdasDevice::pQueueTask(NdasDeviceTaskItem::Type Type, DWORD UnitNo /* = 0 */)
{
	CAutoLock<CLock> autoQueueLock(&m_TaskQueueLock);
	try
	{
		m_TaskQueue.push(NdasDeviceTaskItem(Type, UnitNo));
	}
	catch (...)
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"*** caught exception ***");
		return E_OUTOFMEMORY;
	}
	BOOL success = ReleaseSemaphore(m_TaskQueueSemaphore, 1, NULL);
	ATLASSERT(success);
	if (!success)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return S_OK;
}

HRESULT 
CNdasDevice::pQueueDiscoverTask()
{
	HRESULT hr;

	CAutoInstanceLock autoInstanceLock(this);

	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
		"Queuing the discover task...\n");

	XTLASSERT(NDAS_DEVICE_STATUS_OFFLINE == m_Status);

	pChangeStatus(NDAS_DEVICE_STATUS_CONNECTING);

	hr = pQueueTask(NdasDeviceTaskItem::DISCOVER_TASK);
	if (FAILED(hr))
	{
		pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
		return hr;
	}

	return S_OK;
}

HRESULT
CNdasDevice::pDiscoverThreadStart()
{
	NDAS_DEVICE_HARDWARE_INFO ndasHardwareInfo;

	HRESULT hr = pRetrieveHardwareInfo(ndasHardwareInfo);

	if (FAILED(hr)) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		CAutoInstanceLock autoInstanceLock(this);
		pSetLastDeviceError(NDAS_DEVICE_ERROR_GET_HARDWARE_INFO_FAILED);
		++m_DiscoverErrors;
		pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
		return hr;
	}

	LockInstance();
	m_HardwareInfo = ndasHardwareInfo;
	UnlockInstance();

	NDAS_DEVICE_STAT ndasDeviceStat;
	hr = pUpdateStats(ndasDeviceStat);

	if (FAILED(hr))
	{
		CAutoInstanceLock autoInstanceLock(this);
		pSetLastDeviceError(NDAS_DEVICE_ERROR_GET_HARDWARE_STAT_FAILED);
		++m_DiscoverErrors;
		pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
		return hr;
	}

	LockInstance();
	m_NdasDeviceStat = ndasDeviceStat;
	pSetLastDeviceError(NDAS_DEVICE_ERROR_NONE);
	m_DiscoverErrors = 0;
	UnlockInstance();

	CInterfaceArray<INdasUnit> ndasUnits;

	for (DWORD i = 0; i < ndasDeviceStat.NumberOfUnitDevices; i++) 
	{
		if (ndasDeviceStat.UnitDevices[i].IsPresent)
		{
			// we can ignore unit device creation errors here
			CComPtr<INdasUnit> pNdasUnit;
			hr = pCreateNdasUnit(i, &pNdasUnit);
			if (SUCCEEDED(hr))
			{
				ndasUnits.Add(pNdasUnit);
			}
		}
	}

	//
	// Status should be changed to ONLINE before registering NdasUnits
	//

	BOOL changed = pChangeStatus(
		NDAS_DEVICE_STATUS_ONLINE, &ndasUnits);

	//
	// If the status was not changed, we discard the created NdasUnits.
	// Which means, we only register ndas units when the status was changed
	// to online.
	//

	if (changed)
	{
		AtlForEach(ndasUnits, RegisterNdasUnit());
	}

	return S_OK;
}

HRESULT 
CNdasDevice::pNdasUnitCreationThreadStart(DWORD UnitNo)
{
	CComPtr<INdasUnit> pExistingNdasUnit;
	LockInstance();
	size_t count = m_NdasUnits.GetCount();
	for (size_t i = 0; i < count; ++i)
	{
		pExistingNdasUnit = m_NdasUnits.GetAt(i);
		DWORD unitNo;
		COMVERIFY(pExistingNdasUnit->get_UnitNo(&unitNo));
		if (UnitNo == unitNo)
		{
			m_NdasUnits.RemoveAt(i);
			break;
		}
		pExistingNdasUnit.Release();
	}
	UnlockInstance();

	if (pExistingNdasUnit)
	{
		UnregisterNdasUnit()(pExistingNdasUnit);
	}

	NDAS_DEVICE_STATUS status;
	COMVERIFY(this->get_Status(&status));
	if (NDAS_DEVICE_STATUS_ONLINE != status)
	{
		return S_FALSE;
	}

	// we can ignore unit device creation errors here
	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pCreateNdasUnit(UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"pCreateNdasUnit failed, hr=0x%X\n", hr);
		return hr;
	}

	LockInstance();
	COMVERIFY(this->get_Status(&status));
	if (NDAS_DEVICE_STATUS_ONLINE != status)
	{
		UnlockInstance();
		return S_FALSE;
	}
	m_NdasUnits.Add(pNdasUnit);
	UnlockInstance();

	RegisterNdasUnit()(pNdasUnit);

	return S_OK;
}

STDMETHODIMP_(void) 
CNdasDevice::UnitDismountCompleted(__in INdasUnit* pNdasUnit)
{
	CAutoInstanceLock autolock(this);
	// When just a single unit device is mounted,
	// status will not be changed to DISCONNECTED!
	if (FAILED(UpdateStats()))
	{
		if (!pIsAnyUnitDevicesMounted()) 
		{
			pChangeStatus(NDAS_DEVICE_STATUS_OFFLINE);
		}
	}
}

STDMETHODIMP 
CNdasDevice::UpdateStats()
{
	NDAS_DEVICE_STAT dstat = {0};
	dstat.Size = sizeof(NDAS_DEVICE_STAT);

	HRESULT hr = pUpdateStats(dstat);
	if (FAILED(hr))
	{
		return hr;
	}

	LockInstance();
	m_NdasDeviceStat = dstat;
	UnlockInstance();

	return S_OK;
}

HRESULT 
CNdasDevice::pGetLocalSocketAddressList(
	__deref_out PSOCKET_ADDRESS_LIST* SocketAddressList)
{
	*SocketAddressList = NULL;

	CAutoInstanceLock autolock(this);

	CHeapPtr<SOCKET_ADDRESS_LIST> socketAddressList;
	bool allocated = socketAddressList.AllocateBytes(
		sizeof(SOCKET_ADDRESS_LIST) + 
		m_LocalSocketAddress.iSockaddrLength);

	if (!allocated)
	{
		return E_OUTOFMEMORY;
	}

	socketAddressList->iAddressCount = 1;
	socketAddressList->Address[0].iSockaddrLength = m_LocalSocketAddress.iSockaddrLength;
	socketAddressList->Address[0].lpSockaddr = 
		ByteOffset<SOCKADDR>(socketAddressList, sizeof(SOCKET_ADDRESS_LIST));

	CopyMemory(
		socketAddressList->Address[0].lpSockaddr,
		m_LocalSocketAddress.lpSockaddr,
		m_LocalSocketAddress.iSockaddrLength);

	*SocketAddressList = socketAddressList.Detach();

	return S_OK;
}

HRESULT 
CNdasDevice::pRetrieveHardwareInfo(
	__out NDAS_DEVICE_HARDWARE_INFO& HardwareInfo)
{
	//
	// NDAS Device does not change, so we do not need to lock the object
	//
	NDAS_DEVICE_ID ndasDeviceId = m_NdasDeviceId;

	//
	// Create a Socket Address List from SOCKADDR_LPX.
	// This will prevent unnecessary connection overhead,
	// when there are more than one network interfaces
	//

	CHeapPtr<SOCKET_ADDRESS_LIST> localAddressList;
	HRESULT hr = pGetLocalSocketAddressList(&localAddressList);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"pGetLocalSocketAddressList failed, hr=0x%X\n", hr);
		return hr;
	}

	// Connection information
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
	get_OemCode(&ci.OEMCode);
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = 0;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	//
	// Device Id is never changed
	//
	ci.Address.DeviceId = ndasDeviceId;
	ci.BindingSocketAddressList = localAddressList;

	// Connect to NDAS device (discover mode)
	AutoNdasHandle hNdas = ::NdasCommConnect(&ci);
	if (NULL == (HNDAS) hNdas)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"Discovery failed, hr=0x%X\n", hr);

		pSetLastDeviceError(NDAS_DEVICE_ERROR_CONNECTION_FAILED);

		return hr;
	}

	//
	// On discover login, only GetDeviceInfo is available
	//

	//
	// Discover
	//
	NDAS_DEVICE_HARDWARE_INFO ndasHardwareInfo = {0};
	ndasHardwareInfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
	BOOL success = NdasCommGetDeviceHardwareInfo(hNdas, &ndasHardwareInfo);
	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"NdasCommGetDeviceHardwareInfo failed, hr=0x%X\n", hr);

		pSetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);

		return hr;
	}

	HardwareInfo = ndasHardwareInfo;

	return S_OK;
}

HRESULT 
CNdasDevice::pUpdateStats(
	__out NDAS_DEVICE_STAT& DeviceStat)
{
	HRESULT hr;

	NDAS_DEVICE_ID ndasDeviceId = m_NdasDeviceId;
	NDAS_OEM_CODE ndasOemCode;
	COMVERIFY(this->get_OemCode(&ndasOemCode));

	CHeapPtr<SOCKET_ADDRESS_LIST> localAddressList;
	hr = pGetLocalSocketAddressList(&localAddressList);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"pGetLocalSocketAddressList failed, hr=0x%X\n", hr);
		return hr;
	}

	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.Address.DeviceId = ndasDeviceId;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ci.OEMCode = ndasOemCode;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = 0; /* Use 0 for discover */
	ci.BindingSocketAddressList = localAddressList;

	NDAS_DEVICE_STAT dstat = {0};
	dstat.Size = sizeof(NDAS_DEVICE_STAT);

	BOOL success = NdasCommGetDeviceStat(&ci, &dstat);
	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"NdasCommGetDeviceStat failed, hr=0x%X\n", hr);

		return hr;
	}

	DeviceStat = dstat;

	return hr;
}

//
// set last error of the device
//
void
CNdasDevice::pSetLastDeviceError(NDAS_DEVICE_ERROR deviceError)
{
	CAutoInstanceLock autolock(this);
	m_NdasDeviceError = deviceError;
}

void
CNdasDevice::pDestroyAllUnitDevices()
{
	LockInstance();
	CInterfaceArray<INdasUnit> ndasUnits;
	get_NdasUnits(ndasUnits);
	m_NdasUnits.RemoveAll();
	UnlockInstance();

	// Status cannot be changed to DISCONNECTED 
	// if any of unit devices are mounted.
	AtlForEach(ndasUnits, UnregisterNdasUnit());
}

HRESULT 
CNdasDevice::pCreateNdasUnit(
	__in DWORD UnitNo, __deref_out INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = 0;

	if (UnitNo >= MAX_NDAS_UNITDEVICE_COUNT)
	{
		return E_INVALIDARG;
	}

	CComPtr<INdasUnit> pExistingNdasUnit;
	HRESULT hr = get_NdasUnit(UnitNo, &pExistingNdasUnit);
	if (SUCCEEDED(hr))
	{
		XTLASSERT(FALSE && "Unit Device already exists");
		return E_FAIL;
	}

	DWORD failureThreshold = NdasServiceConfig::Get(nscUnitDeviceIdentifyRetryMax);
	DWORD retryGap = NdasServiceConfig::Get(nscUnitDeviceIdentifyRetryGap);

	for (DWORD i = 0; i < failureThreshold + 1; ++i)
	{
		NDAS_DEVICE_STAT ndasDeviceStat;
		hr = pUpdateStats(ndasDeviceStat);
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
				"pUpdateStats failed, hr=0x%X.\n", hr);
			return hr;
		}

		LockInstance();
		m_NdasDeviceStat = ndasDeviceStat;
		UnlockInstance();

		if (!ndasDeviceStat.UnitDevices[UnitNo].IsPresent)
		{
			XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
				"HWINFO does not contain unit %d.\n", UnitNo);
			return E_FAIL;
		}

		CNdasUnitDeviceFactory ndasUnitFactory(this, UnitNo);
		CComPtr<INdasUnit> pNdasUnit;
		HRESULT hr = ndasUnitFactory.CreateUnitDevice(&pNdasUnit);
		if (SUCCEEDED(hr))
		{
			*ppNdasUnit = pNdasUnit.Detach();
			return hr;
		}
		::Sleep(retryGap);
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_ERROR,
			"Creating a unit device instance failed (%d out of %d), hr=0x%X\n", 
			i, failureThreshold, hr);
	}

	return E_FAIL;
}

STDMETHODIMP 
CNdasDevice::InvalidateNdasUnit(INdasUnit* pNdasUnit)
{
	NDAS_UNITDEVICE_STATUS status;
	COMVERIFY(pNdasUnit->get_Status(&status));
	if (NDAS_UNITDEVICE_STATUS_MOUNTED == status)
	{
		return E_FAIL;
	}

	DWORD unitNo;
	pNdasUnit->get_UnitNo(&unitNo);

	XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasDevice=%d, Invalidating Unit Device %d\n", m_SlotNo, unitNo);

	if (NDAS_DEVICE_STATUS_ONLINE != m_Status) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICE, TRACE_LEVEL_INFORMATION,
			"NdasDevice=%d, Non-connected device ignored\n", m_SlotNo);
		return E_FAIL;
	}

	pQueueNdasUnitCreationTask(unitNo);

	return S_OK;
}

bool
CNdasDevice::pIsAnyUnitDevicesMounted()
{
	CInterfaceArray<INdasUnit> ndasUnits;
	get_NdasUnits(ndasUnits);

	bool result = false;
	size_t count = ndasUnits.GetCount();
	for (size_t index = 0; index < count; ++index)
	{
		CComPtr<INdasUnit> pNdasUnit = ndasUnits.GetAt(index);
		result = UnitDeviceStatusEquals(NDAS_UNITDEVICE_STATUS_MOUNTED)(pNdasUnit);
		if (result)
		{
			break;
		}
	}
	return result;
}
