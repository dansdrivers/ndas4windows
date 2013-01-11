#include "stdafx.h"
#include "ndasdevcomm.h"
#include "lpxcomm.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdevcomm.tmh"
#endif

LONG DbgLevelCommDev = DBG_LEVEL_COMM_DEV;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelCommDev) {								\
        XTLTRACE1(TRACE_LEVEL_ERROR, "|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		XTLTRACE1(TRACE_LEVEL_ERROR, x,__VA_ARGS__);							\
    } 														\
} while(0)

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDeviceComm class
//
//////////////////////////////////////////////////////////////////////////

CNdasDeviceComm::CNdasDeviceComm(
	INdasDevice* pNdasDevice,
	DWORD dwUnitNo) :
	m_hNdas(NULL),
	m_pNdasDevice(pNdasDevice),
	m_dwUnitNo(dwUnitNo),
	m_bWriteAccess(FALSE)
{
}

CNdasDeviceComm::~CNdasDeviceComm()
{
	if (m_hNdas)
	{
		XTLVERIFY(SUCCEEDED(Disconnect()));
	}
}

HRESULT
CNdasDeviceComm::GetNdasUnitInfo(PNDAS_UNITDEVICE_HARDWARE_INFO pUnitDevInfo)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pUnitDevInfo != NULL);

	BOOL success = NdasCommGetUnitDeviceHardwareInfo(m_hNdas, pUnitDevInfo);
	
	if (!success) {

		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		
		NdasUiDbgCall( 1, "NdasCommGetUnitDeviceHardwareInfo failed, hr=0x%X\n", hr );
		return hr;
	}

	return S_OK;
}

HRESULT
CNdasDeviceComm::GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pDiskInfoBlock != NULL && "INVALID PARAMETER");

	//
	// Read Last Sector for NDAS_UNITDISK_INFORMATION_BLOCK
	//
	BOOL success = NdasCommBlockDeviceRead(
		m_hNdas, -1, 1, 
		reinterpret_cast<LPBYTE>(pDiskInfoBlock));

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommBlockDeviceRead for GetDiskInfoBlock failed, hr=0x%X\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
CNdasDeviceComm::ReadDiskBlock(PVOID Buffer, INT64 LogicalBlockAddress, DWORD TransferBlocks)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(Buffer != NULL);
	XTLASSERT(TransferBlocks >= 1 && TransferBlocks <= 128);
	XTLASSERT(!::IsBadWritePtr(Buffer, TransferBlocks * 512));

	BOOL success = NdasCommBlockDeviceRead(
		m_hNdas, LogicalBlockAddress, TransferBlocks, Buffer);

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommBlockDeviceRead failed, LBA=0x%I64x hr=0x%X\n", 
			LogicalBlockAddress, hr);

		return hr;
	}

	return S_OK;
}

HRESULT
CNdasDeviceComm::Connect(BOOL bWriteAccess)
{
	if (m_hNdas) {

		ATLASSERT(FALSE);
		ATLVERIFY( Disconnect() );
	}

	// Binding Address List
	SOCKADDR_LPX localSockAddrLpx;
	SOCKET_ADDRESS localSocketAddress;
	localSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
	localSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localSockAddrLpx);

	COMVERIFY(m_pNdasDevice->get_LocalAddress(&localSocketAddress));

	SOCKET_ADDRESS_LIST localAddressList;
	localAddressList.iAddressCount = 1;
	localAddressList.Address[0] = localSocketAddress;

	// Connection Information
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	COMVERIFY(m_pNdasDevice->get_NdasDeviceId(&ci.Address.DeviceId));
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	COMVERIFY(m_pNdasDevice->get_HardwarePassword(&ci.OEMCode.UI64Value));
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = m_dwUnitNo;
	ci.WriteAccess = bWriteAccess;
	ci.BindingSocketAddressList = &localAddressList;

	m_hNdas = ::NdasCommConnect(&ci);

	if (NULL == m_hNdas)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommConnect failed, hr=0x%X\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
CNdasDeviceComm::Disconnect()
{
	BOOL success = ::NdasCommDisconnect(m_hNdas);
	if (!success)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommDisconnect failed, hr=0x%X\n", hr);
		return hr;
	}
	return S_OK;
}

