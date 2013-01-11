#include "stdafx.h"
#include "ndasdevcomm.h"
#include "lpxcomm.h"
#include "ndasdev.h"
#include "xdebug.h"

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDeviceComm class
//
//////////////////////////////////////////////////////////////////////////

CNdasDeviceComm::CNdasDeviceComm(
	CNdasDevice& device,
	DWORD dwUnitNo) :
	m_pDevice(&device),
	m_dwUnitNo(dwUnitNo),
	m_bWriteAccess(FALSE),
	m_bInitialized(FALSE)
{
}

CNdasDeviceComm::~CNdasDeviceComm()
{
	if (m_bInitialized) {
		Cleanup();
	}
}

BOOL 
CNdasDeviceComm::Initialize(BOOL bWriteAccess)
{
	m_bWriteAccess = bWriteAccess;

	InitializeLANSCSIPath();

	LPX_ADDRESS local, remote;

	local = m_pDevice->GetLocalLpxAddress();
	remote = m_pDevice->GetRemoteLpxAddress();

	SOCKET sock = CreateLpxConnection(&remote, &local);

	if (INVALID_SOCKET == sock) {
		DPErrorEx(_FT("CreateLpxConnection failed: "));
		return FALSE;
	}


	m_lspath.HWType = m_pDevice->GetHWType();
	m_lspath.HWVersion = m_pDevice->GetHWVersion();

	m_lspath.connsock = sock;
	INT iResult = Login(&m_lspath, LOGIN_TYPE_NORMAL);
	if (0 != iResult) {
		// TODO: LANDISK_ERROR_BADKEY?
		DPErrorEx(_FT("Login failed (ret %d): "), iResult);
		::closesocket(sock);
		return FALSE;
	}

	m_bInitialized = TRUE;

	return TRUE;
}

BOOL
CNdasDeviceComm::Cleanup()
{
	INT iResult = 0;
	if (m_bInitialized) {
		iResult = ::closesocket(m_lspath.connsock);
	}
	m_bInitialized = FALSE;
	return (iResult == 0);
}

BOOL 
CNdasDeviceComm::GetUnitDeviceInformation(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo)
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
	// What if FwRev, Model, SerialNo is not null terminated?
	// -> solved by terminating null between fields

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
CNdasDeviceComm::GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
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
CNdasDeviceComm::ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock, INT32 i32BlockSize)
{
	_ASSERTE(m_bInitialized && "CNdasDeviceComm is not initialized");
	_ASSERTE(pBlockBuffer != NULL);
	_ASSERTE(i32BlockSize >= 1 && i32BlockSize <= 128);
	_ASSERTE(!::IsBadWritePtr(pBlockBuffer, i32BlockSize * 512));

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
		(INT16)i32BlockSize, 
		0,
		(PCHAR) pBlockBuffer, &ui8IdeResponse);

	if (0 != iResult) {
		DPError(_FT("IdeCommand failed with error %d, ide response %d.\n"), iResult, ui8IdeResponse);
		return FALSE;
	}

	return TRUE;
}

UINT64
CNdasDeviceComm::GetDiskSectorCount()
{
	return m_lspath.PerTarget[m_dwUnitNo].SectorCount;
}

BOOL
CNdasDeviceComm::WriteDiskInfoBlock(PNDAS_DIB pDiskInfoBlock)
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
CNdasDeviceComm::InitializeLANSCSIPath()
{
	::ZeroMemory(&m_lspath, sizeof(m_lspath));
	m_lspath.iUserID = GetUserId(); // should be set when login
	m_lspath.iPassword = m_pDevice->GetHWPassword();
}

INT32
CNdasDeviceComm::GetUserId()
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
