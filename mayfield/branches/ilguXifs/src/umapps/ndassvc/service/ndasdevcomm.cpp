#include "stdafx.h"
#include "ndasdevcomm.h"
#include "lpxcomm.h"
#include "ndasdev.h"
#include <autores.h>
#include "xdebug.h"

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


//BOOL 
//CNdasDeviceComm::Initialize(BOOL bWriteAccess)
//{
//	m_bWriteAccess = bWriteAccess;
//
//	InitializeLANSCSIPath();
//
//	LPX_ADDRESS local, remote;
//
//	local = m_pDevice->GetLocalLpxAddress();
//	remote = m_pDevice->GetRemoteLpxAddress();
//
//	SOCKET sock = CreateLpxConnection(&remote, &local);
//
//	if (INVALID_SOCKET == sock) {
//		DBGPRT_ERR_EX(_FT("CreateLpxConnection failed: "));
//		return FALSE;
//	}
//
//
//	m_lspath.HWType = m_pDevice->GetHWType();
//	m_lspath.HWVersion = m_pDevice->GetHWVersion();
//	m_lspath.connsock = sock;
//	INT iResult = Login(&m_lspath, LOGIN_TYPE_NORMAL);
//	if (0 != iResult) {
//		// TODO: LANDISK_ERROR_BADKEY?
//		DBGPRT_ERR_EX(_FT("Login failed (ret %d): "), iResult);
//		::closesocket(sock);
//		return FALSE;
//	}
//
//	m_bInitialized = TRUE;
//
//	return TRUE;
//}

BOOL 
CNdasDeviceComm::GetUnitDeviceInformation(PNDAS_UNITDEVICE_HARDWARE_INFO pUnitDevInfo)
{
	XTLASSERT(NULL != m_hNdas && "CNdasDeviceComm is not connected");
	XTLASSERT(pUnitDevInfo != NULL);

	BOOL fSuccess = NdasCommGetUnitDeviceHardwareInfo(m_hNdas, pUnitDevInfo);
	if (!fSuccess)
	{
		DBGPRT_ERR_EX(_FT("NdasCommGetUnitDeviceInfo failed: "));
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
		DBGPRT_ERR_EX(_FT("NdasCommBlockDeviceRead failed: "));
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
		DBGPRT_ERR_EX(_FT("NdasCommBlockDeviceRead failed: "));
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
	AutoProcessHeap autoHeap = pBindAddrList;

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
		DBGPRT_ERR_EX(_FT("NdasCommConnect failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasDeviceComm::Disconnect()
{
	BOOL fSuccess = ::NdasCommDisconnect(m_hNdas);
	return fSuccess;
}
