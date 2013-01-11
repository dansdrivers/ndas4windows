#include "stdafx.h"
#include "ndasdevcomm.h"
#include "lpxcomm.h"
#include "ndasdev.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdevcomm.tmh"
#endif

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDeviceComm class
//
//////////////////////////////////////////////////////////////////////////

CNdasDeviceComm::CNdasDeviceComm(
	CNdasDevicePtr pDevice,
	DWORD dwUnitNo) :
	m_hNdas(NULL),
	m_pDevice(pDevice),
	m_dwUnitNo(dwUnitNo),
	m_bWriteAccess(FALSE)
{
}

CNdasDeviceComm::~CNdasDeviceComm()
{
	if (m_hNdas)
	{
		BOOL fSuccess = Disconnect();
		XTLASSERT(fSuccess && "Disconnect should succeed");
	}
}

BOOL 
CNdasDeviceComm::GetUnitDeviceInformation(PNDAS_UNITDEVICE_HARDWARE_INFO pUnitDevInfo)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pUnitDevInfo != NULL);

	BOOL fSuccess = NdasCommGetUnitDeviceHardwareInfo(m_hNdas, pUnitDevInfo);
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommGetUnitDeviceHardwareInfo failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pDiskInfoBlock != NULL && "INVALID PARAMETER");

	//
	// Read Last Sector for NDAS_UNITDISK_INFORMATION_BLOCK
	//
	BOOL fSuccess = NdasCommBlockDeviceRead(
		m_hNdas, -1, 1, 
		reinterpret_cast<LPBYTE>(pDiskInfoBlock));

	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommBlockDeviceRead for GetDiskInfoBlock failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock, INT32 i32BlockSize)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pBlockBuffer != NULL);
	XTLASSERT(i32BlockSize >= 1 && i32BlockSize <= 128);
	XTLASSERT(!::IsBadWritePtr(pBlockBuffer, i32BlockSize * 512));

	BOOL fSuccess = NdasCommBlockDeviceRead(
		m_hNdas, i64DiskBlock, i32BlockSize, pBlockBuffer);

	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommBlockDeviceRead failed, block=0x%I64x error=0x%X\n", 
			i64DiskBlock, GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::Connect(BOOL bWriteAccess)
{
	// Binding Address List
	LPSOCKET_ADDRESS_LIST pBindAddrList = 
		CreateLpxSocketAddressList(&m_pDevice->GetLocalLpxAddress());

	if (NULL == pBindAddrList)
	{
		return FALSE;
	}

	// Should free the allocated heap (pHostLpxAddrList) on return
	XTL::AutoProcessHeap autoHeap = pBindAddrList;

	// Connection Information
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.Address.DeviceId = m_pDevice->GetDeviceId();
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.OEMCode.UI64Value = m_pDevice->GetHardwarePassword();
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = m_dwUnitNo;
	ci.WriteAccess = bWriteAccess;
	ci.BindingSocketAddressList = pBindAddrList;

	m_hNdas = ::NdasCommConnect(&ci);

	if (NULL == m_hNdas)
	{
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommConnect failed, error=0x%X\n", 
			GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::Disconnect()
{
	BOOL fSuccess = ::NdasCommDisconnect(m_hNdas);
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASCOMM, TRACE_LEVEL_ERROR,
			"NdasCommDisconnect failed, error=0x%X\n", 
			GetLastError());
	}
	return fSuccess;
}
