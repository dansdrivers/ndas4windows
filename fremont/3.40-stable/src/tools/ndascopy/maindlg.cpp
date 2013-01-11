#include "stdatl.hpp"
#include "maindlg.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasid.h>

#define SECTOR_SIZE (512)

// copy buffer macros
#define countof(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))
#define cb_countof(ARRAY) (countof(ARRAY))
#define cb_nextof(ARRAY, POS) ((((POS) + 1) == cb_countof(ARRAY)) ? 0 : (POS) + 1)
#define cb_readables(ARRAY, START, END) (((START) <= (END)) ? (END) - (START) : cb_countof(ARRAY) + (END) - (START))

static
inline
VOID
_StringMacAddress(
	TCHAR *szText,
	size_t cchText,
	const BYTE MacAddressDevice[],
	const BYTE MacAddressLocal[])
{
	StringCchPrintf(szText, cchText, 
		_T("%02X:%02X:%02X:%02X:%02X:%02X at %02X:%02X:%02X:%02X:%02X:%02X"),
		MacAddressDevice[0],
		MacAddressDevice[1],
		MacAddressDevice[2],
		MacAddressDevice[3],
		MacAddressDevice[4],
		MacAddressDevice[5],
		MacAddressLocal[0],
		MacAddressLocal[1],
		MacAddressLocal[2],
		MacAddressLocal[3],
		MacAddressLocal[4],
		MacAddressLocal[5]
	);
}

DWORD WINAPI _ThreadProcWrite(LPVOID lpParameter)
{
	CMainDlg *pMainDlg = (CMainDlg *)lpParameter;

	return pMainDlg->ThreadProcWrite();

}

DWORD WINAPI _ThreadProcMain(LPVOID lpParameter)
{
	CMainDlg *pMainDlg = (CMainDlg *)lpParameter;

	return pMainDlg->ThreadProcMain();
}

CMainDlg::CMainDlg()
{
	m_bCopying = FALSE;
	InitializeCriticalSection(&m_cs);

	for (int i = 0; i < cb_countof(m_Buffer); i++) {
		m_Buffer[i].buffer = (BYTE *)malloc(NDAS_COPY_BUFFER_SEGMENT_SIZE);
	}

}

CMainDlg::~CMainDlg()
{
	for (int i = 0; i < cb_countof(m_Buffer); i++) {
		if (m_Buffer[i].buffer) {
			free(m_Buffer[i].buffer);
			m_Buffer[i].buffer = NULL;
		}
	}

	DeleteCriticalSection(&m_cs);
}


DWORD CMainDlg::ThreadProcWrite()
{
	TCHAR szText[256];
	PNDAS_COPY_ITEM pCopyItem;
	PNDAS_COPY_BUFFER pBuffer;
	BOOL result;

	// select exclusive NDASItem
	Lock();
	StringCchPrintf(szText, countof(szText), _T("Writing thread %d up"), m_iThreadIndex);
	AddLog(szText);
	pCopyItem = m_ThreadCopyItems[m_iThreadIndex++];
	pCopyItem->iBufferUsing = 0;
	pCopyItem->status = NDAS_COPY_ITEM_STATUS_COPYING;
	Unlock();
	
	// open NDAS
	NDASCOMM_CONNECTION_INFO NdasConnectionInfo;
	ZeroMemory(&NdasConnectionInfo, sizeof(NdasConnectionInfo));
	NdasConnectionInfo.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	NdasConnectionInfo.WriteAccess = TRUE;
	NdasConnectionInfo.AddressType = NDASCOMM_CIT_DEVICE_ID;
	CopyMemory(NdasConnectionInfo.Address.DeviceId.Node, pCopyItem->DeviceAddress, 6);
	NdasConnectionInfo.PrivilegedOEMCode.UI64Value = 0;
	NdasConnectionInfo.OEMCode.UI64Value = 0;
	NdasConnectionInfo.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	NdasConnectionInfo.Protocol = NDASCOMM_TRANSPORT_LPX;
	pCopyItem->hNdas = NdasCommConnect(&NdasConnectionInfo); // close this handle at ThreadProcMain()

	if (NULL == pCopyItem->hNdas) {
		AddLog(_T("Connection failure!"));
		_StringMacAddress(szText, countof(szText), pCopyItem->DeviceAddress, pCopyItem->LocalAddress);
		AddLog(szText);
	}

	while (1) {
		// ready to write data?
		while (1) {
			Lock();
			if (pCopyItem->iBufferUsing != m_iBufferEnd) {
				Unlock();
				break;
			}
			Unlock();
			Sleep(50);
		}

		// we gonna write using this buffer
		pBuffer = &m_Buffer[pCopyItem->iBufferUsing];

		// write
		ATLTRACE(_T("NDAS(%p) writing at %I64dB using buffer(%p:%d)\n"),
			pCopyItem,
			pBuffer->LocationInBytes,
			pBuffer,
			pCopyItem->iBufferUsing);

		if (pCopyItem->hNdas) {
			SetLastError(0);
			result = NdasCommBlockDeviceWriteSafeBuffer(
				pCopyItem->hNdas, 
				pBuffer->LocationInBytes / SECTOR_SIZE, 
				pBuffer->LengthInBytes / SECTOR_SIZE, 
				pBuffer->buffer);

			if (FALSE == result) {
				StringCchPrintf(szText, countof(szText),
					_T("Write failed. GLE(%d) Location(%I64dB) Length(%I64dB)"),
					GetLastError(),
					pBuffer->LocationInBytes,
					pBuffer->LengthInBytes
					);
				AddLog(szText);

			}
			else {
				ATLTRACE(_T("NDAS(%p) writing at %I64dB using buffer(%p:%d) complete\n"),
					pCopyItem,
					pBuffer->LocationInBytes,
					pBuffer,
					pCopyItem->iBufferUsing);
			}
		}

		// get ready for next buffer
		Lock();
		pCopyItem->iBufferUsing = cb_nextof(m_Buffer, pCopyItem->iBufferUsing);
		Unlock();
	}

	return 0;
}

DWORD CMainDlg::ThreadProcMain()
{
	TCHAR szText[256];
	int i;
	INT64 iReadLocationInBytes = m_pDriveInfo->StartingSectorInBytes;
	INT64 iReadLocationInBytesOld;
	DWORD dwTickCount, dwTickCountStart, dwTickCountNow;
	PNDAS_COPY_BUFFER pBuffer;
	BOOL result;

	// init critical variables before threads created
	m_iBufferStart = 0;
	m_iBufferEnd = 0;

	// create write threads
	AddLog(_T("Reading thread up"));

	for (i = 0; i < m_nThreadTotal; i++) {		
		Lock();
		AddLog(_T("Creating writing thread..."));
		m_ThreadCopyItems[i]->hThreadWrite = CreateThread(NULL, 0, _ThreadProcWrite, this, NULL, NULL);
		Unlock();
	}

	// wait until all write threads are ready
	while (1) {
		Lock();
		if (m_iThreadIndex == m_nThreadTotal) {
			Unlock();
			break;
		}
		Unlock();
		Sleep(50);
	}

	// move to start position
	LARGE_INTEGER liDistanceToMove, liNewFilePointer;
	liDistanceToMove.QuadPart = m_pDriveInfo->StartingSectorInBytes;
	result = SetFilePointerEx(m_hSrcDrive, liDistanceToMove, &liNewFilePointer, FILE_BEGIN);

	DWORD nNumberOfBytesRead;

	// pumping loop
	iReadLocationInBytesOld = iReadLocationInBytes;
	dwTickCount = GetTickCount();
	dwTickCountStart = dwTickCount;
	while (1) {
		// ready to read data?
		while (cb_nextof(m_Buffer, m_iBufferEnd) == m_iBufferStart) {
			Lock();
			for (i = 0; i < m_nThreadTotal; i++) {
				if (m_ThreadCopyItems[i]->iBufferUsing == m_iBufferStart)
					break;
			}

			if (i == m_nThreadTotal) {
				m_iBufferStart = cb_nextof(m_Buffer, m_iBufferStart);
				Unlock();
				break;
			}
			Unlock();
			Sleep(50);
		}

		// we read data into m_iBufferEnd
		pBuffer = &m_Buffer[m_iBufferEnd];
		pBuffer->LocationInBytes = iReadLocationInBytes;
		pBuffer->LengthInBytes = 
			min(NDAS_COPY_BUFFER_SEGMENT_SIZE,
			m_pDriveInfo->PartitionLengthInBytes - (iReadLocationInBytes - m_pDriveInfo->StartingSectorInBytes));

		// read
		ATLTRACE(_T("reading %I64d(%I64d). buffer %p:%d\n"),
			iReadLocationInBytes,
			pBuffer->LengthInBytes,
			pBuffer,
			m_iBufferEnd);

		result = ReadFile(
			m_hSrcDrive,
			pBuffer->buffer,
			(DWORD)pBuffer->LengthInBytes,
			&nNumberOfBytesRead,
			NULL);

		if (FALSE == result) {
			StringCchPrintf(szText, countof(szText),
				_T("ReadFile failed at byte %I64d"), iReadLocationInBytes);
			AddLog(szText);
		}

		// finally, we move m_buffer_end so that write threads can use it
		Lock();
		ATLTRACE(_T("reading %I64d(%I64d). buffer %p:%d complete\n"),
			iReadLocationInBytes,
			pBuffer->LengthInBytes,
			pBuffer,
			m_iBufferEnd);
		m_iBufferEnd = cb_nextof(m_Buffer, m_iBufferEnd);
		Unlock();

		// report status every 1 second
		dwTickCountNow = GetTickCount();
		if (dwTickCount + 1000 < dwTickCountNow) {
			StringCchPrintf(szText, countof(szText), _T("(%06d:%02d) %I64d.%I64d/%I64dGB, %I64dKBPS"),
				((dwTickCountNow - dwTickCountStart) / 1000) / 60,
				((dwTickCountNow - dwTickCountStart) / 1000) % 60,
				(iReadLocationInBytes - m_pDriveInfo->StartingSectorInBytes)/ (1000 * 1000 * 1000),
				((iReadLocationInBytes - m_pDriveInfo->StartingSectorInBytes) / (100 * 1000 * 1000)) % 10,
				m_pDriveInfo->PartitionLengthInBytes / (1000 * 1000 * 1000),
				(iReadLocationInBytes - iReadLocationInBytesOld) / (dwTickCountNow - dwTickCount) // almost KBPS
				);

			m_StaticProgress.SetWindowText(szText);

			m_ProgressCtrlCopy.SetPos((int)(iReadLocationInBytes / (1000 * 1000)));

			iReadLocationInBytesOld = iReadLocationInBytes;
			dwTickCount = dwTickCountNow;
		}

		// exit loop if copying completes
		iReadLocationInBytes += NDAS_COPY_BUFFER_SEGMENT_SIZE;
		if (iReadLocationInBytes >= (m_pDriveInfo->StartingSectorInBytes + m_pDriveInfo->PartitionLengthInBytes))
			break;
	}

	if (m_hSrcDrive) {
		CloseHandle(m_hSrcDrive);
		m_hSrcDrive = NULL;
	}

	ATLTRACE(_T("reading complete\n"));

	// wait until write thread complete
	while (1) {
		Lock();
		for (i = 0; i < m_nThreadTotal; i++) {
			if (m_ThreadCopyItems[i]->iBufferUsing != m_iBufferEnd)
				break;
		}
		Unlock();

		if (i == m_nThreadTotal) {
			break;
		}

		Sleep(50);
	}

	// close handles and kill threads	
	for (i = 0; i < m_nThreadTotal; i++) {
		PNDAS_COPY_ITEM pThreadCopyItem = m_ThreadCopyItems[i];
		if (NULL != pThreadCopyItem->hNdas) {
			NdasCommDisconnect(pThreadCopyItem->hNdas);
			pThreadCopyItem->hNdas = NULL;
		}
		TerminateThread(pThreadCopyItem->hThreadWrite, 0);
		ATLTRACE(_T("Terminate %d\n"), i);
	}

	m_ButtonStart.SetWindowText(_T("Start"));
	m_ButtonStart.EnableWindow(TRUE);
	m_bCopying = FALSE;
	AddLog(_T("Complete!"));

	ExitThread(0);

	return 0;
}

static
VOID
NDASHEARAPI_CALLBACK
_NdasHeartbeatCallback(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	CMainDlg *mainDlg = (CMainDlg *)lpContext;
	ATL::CString address;

	if (!mainDlg)
		return;

	mainDlg->CallbackNdasHeartbeat(pHeartbeat);
}

void
CMainDlg::CallbackNdasHeartbeat(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat)
{
	if (pHeartbeat->Version < 2) {
		return;
	}

	TCHAR szListBoxString[256];

	// test MAC filtering code
	if (pHeartbeat->DeviceAddress.Node[3] == 0xFE &&
		pHeartbeat->DeviceAddress.Node[4] == 0x03 &&
		pHeartbeat->DeviceAddress.Node[5] == 0x78) {
	} else {
//		return;
	}

	_StringMacAddress(
		szListBoxString,
		countof(szListBoxString),
		pHeartbeat->DeviceAddress.Node,
		pHeartbeat->LocalAddress.Node);

	// add if the NDAS isn't listed
	if (m_ListBoxNDASList.FindStringExact(-1, szListBoxString) == LB_ERR) {
		Lock();

		int iListBoxIndex = m_ListBoxNDASList.AddString(szListBoxString);
		if (LB_ERR == iListBoxIndex) {
			Unlock();
			return;
		}

		NDAS_COPY_ITEM* pCopyItem = new NDAS_COPY_ITEM;
		ZeroMemory(pCopyItem, sizeof(NDAS_COPY_ITEM));
		pCopyItem->status = NDAS_COPY_ITEM_STATUS_READY;
		CopyMemory(pCopyItem->DeviceAddress, pHeartbeat->DeviceAddress.Node, 6);
		CopyMemory(pCopyItem->LocalAddress, pHeartbeat->LocalAddress.Node, 6);

		m_ListBoxNDASList.SetItemDataPtr(iListBoxIndex, pCopyItem);

		Unlock();
	}
}

LRESULT
CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	// Initialize controls
	m_ComboBoxDriveList.Attach(GetDlgItem(IDC_COMBO_DRIVES));
	m_ListBoxNDASList.Attach(GetDlgItem(IDC_LIST_NDAS));
	m_StaticProgress.Attach(GetDlgItem(IDC_STATIC_PROGRESS));
	m_ButtonStart.Attach(GetDlgItem(IDC_BUTTON_START));
	m_EditLog.Attach(GetDlgItem(IDC_EDIT_LOG));
	m_ProgressCtrlCopy.Attach(GetDlgItem(IDC_PROGRESS_COPY));

	FillDriveList();
	InitNDASHear();

	AddLog(_T("NEVER forget to stop NDAS service."));
	AddLog(_T("Initialized. Select local drive as source and remote NDAS as target."));
	AddLog(_T("And click 'Start' to copy."));

	return TRUE;
}

void CMainDlg::FillDriveList()
{
	HANDLE hDevice = NULL;
	BOOL bResult;
	TCHAR szPath[256];
	DISK_GEOMETRY_EX dg;
	DWORD junk;

	TCHAR szText[256];
	int iItemIndex;

	NDAS_COPY_DRIVE_INFO* pDriveInfo = NULL;

	m_ComboBoxDriveList.ResetContent();

	for (int iDrive = 0; ; iDrive++) {
		// open drive
		StringCchPrintf(szPath, countof(szPath), _T("\\\\.\\PhysicalDrive%d"), iDrive );
		hDevice = CreateFile(szPath,
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			0,
			OPEN_EXISTING,
			0,
			0);
		if (hDevice == INVALID_HANDLE_VALUE)
			break;

		// get drive geometry
		bResult = DeviceIoControl(hDevice, // the device we are querying
			IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // operation to perform
			NULL, 0, // no input buffer, so we pass zero
			&dg, sizeof(dg), // the output buffer
			&junk, // discard the count of bytes returned
			NULL);

		BYTE bLayoutInfo[20240];
		bResult = DeviceIoControl(hDevice, // the device we are querying
			IOCTL_DISK_GET_DRIVE_LAYOUT_EX, // operation to perform
			NULL, 0, // no input buffer, so we pass zero
			bLayoutInfo, sizeof(bLayoutInfo), // the output buffer
			&junk, // discard the count of bytes returned
			NULL);

		CloseHandle(hDevice);
		hDevice = NULL;

		if (!bResult) {
#ifdef _DEBUG
			MessageBox(
				_T("DeviceIoControl : IOCTL_DISK_GET_DRIVE_GEOMETRY_EX"),
				_T("Error"),
				MB_OK | MB_ICONEXCLAMATION);
#else
			StringCchPrintf(szText, countof(szText),
				_T("DeviceIoControl : IOCTL_DISK_GET_DRIVE_GEOMETRY_EX drive(%d)"),
				iDrive);
			AddLog(szText);
#endif
			break;
		}

		if (SECTOR_SIZE != dg.Geometry.BytesPerSector) {
			StringCchPrintf(szText, countof(szText),
				_T("Bytes per sector in drive %d is %d. Skip this drive."),
				iDrive,
				dg.Geometry.BytesPerSector);
#ifdef _DEBUG
			MessageBox(
				szText,
				_T("Warning"),
				MB_OK | MB_ICONEXCLAMATION);
#else
			AddLog(szText);
#endif

			continue;
		}

		// add to m_ComboBoxDriveList 
		StringCchPrintf(szText, countof(szText), _T("Drive %d (%I64dGB)"), iDrive, dg.DiskSize.QuadPart / (1000 * 1000 * 1000));
		iItemIndex = m_ComboBoxDriveList.AddString(szText);
		if (CB_ERR == iItemIndex)
			break;

		pDriveInfo = new NDAS_COPY_DRIVE_INFO;
		ZeroMemory(pDriveInfo, sizeof(NDAS_COPY_DRIVE_INFO));
		pDriveInfo->dwDrive = iDrive;
		pDriveInfo->bIsPartition = FALSE;
		pDriveInfo->StartingSectorInBytes = 0;
		pDriveInfo->PartitionLengthInBytes = dg.DiskSize.QuadPart;

		m_ComboBoxDriveList.SetItemDataPtr(iItemIndex, pDriveInfo);
		pDriveInfo = NULL;

#ifdef _DEBUG
		// add 'copy MBR only'
		StringCchPrintf(szText, countof(szText), _T("Drive %d MBR"), iDrive);
		iItemIndex = m_ComboBoxDriveList.AddString(szText);
		if (CB_ERR == iItemIndex)
			break;

		pDriveInfo = new NDAS_COPY_DRIVE_INFO;
		ZeroMemory(pDriveInfo, sizeof(NDAS_COPY_DRIVE_INFO));
		pDriveInfo->dwDrive = iDrive;
		pDriveInfo->bIsPartition = FALSE;
		pDriveInfo->StartingSectorInBytes = 0;
		pDriveInfo->PartitionLengthInBytes = 512;

		m_ComboBoxDriveList.SetItemDataPtr(iItemIndex, pDriveInfo);
		pDriveInfo = NULL;

		// add partitions
		PDRIVE_LAYOUT_INFORMATION_EX pLI = (PDRIVE_LAYOUT_INFORMATION_EX)bLayoutInfo;
		for( DWORD iPartition = 0; iPartition < pLI->PartitionCount; iPartition++ )
		{
			PARTITION_INFORMATION_EX* pi = &(pLI->PartitionEntry[iPartition]);

			StringCchPrintf(szText, countof(szText), _T("Drive %d Partition %d (%I64dGB)"),
				iDrive, iPartition, pi->PartitionLength.QuadPart / (1000 * 1000 * 1000));
			iItemIndex = m_ComboBoxDriveList.AddString(szText);

			pDriveInfo = new NDAS_COPY_DRIVE_INFO;
			ZeroMemory(pDriveInfo, sizeof(NDAS_COPY_DRIVE_INFO));
			pDriveInfo->dwDrive = iDrive;
			pDriveInfo->bIsPartition = TRUE;
			pDriveInfo->dwPartition = iPartition;
			pDriveInfo->StartingSectorInBytes = pi->StartingOffset.QuadPart;
			pDriveInfo->PartitionLengthInBytes = pi->PartitionLength.QuadPart;

			m_ComboBoxDriveList.SetItemDataPtr(iItemIndex, pDriveInfo);
			pDriveInfo = NULL;
		}
#endif
	}

	m_ComboBoxDriveList.SetCurSel(0);
}

BOOL CMainDlg::InitNDASHear()
{
	// Initialize the NDAS hear
	BOOL fSuccess = NdasHeartbeatInitialize();
	if (!fSuccess) 
	{
		_tprintf(_T("Failed to init listener : %d\n"), ::GetLastError());

		return FALSE;
	}
	
	m_hNdasHear = NdasHeartbeatRegisterNotification(_NdasHeartbeatCallback, this);

	if (NULL == m_hNdasHear) {
		_tprintf(_T("Failed to register handler : %d\n"), ::GetLastError());

		return FALSE;
	}

	return TRUE;
}

void CMainDlg::OnCmdStart(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	TCHAR szText[256];
	DWORD dwThreadId;
	m_iThreadIndex = 0;

	m_nThreadTotal = m_ListBoxNDASList.GetSelCount();

	if (0 == m_nThreadTotal) {
		StringCchPrintf(szText, countof(szText), _T("Select at least one NDAS hard drive"));
		MessageBox(
			szText,
			_T("Copy Failure"),
			MB_OK | MB_ICONEXCLAMATION);

		return;
	}

	if (m_nThreadTotal > NDAS_COPY_THREAD_LIMIT) {
		StringCchPrintf(szText, countof(szText), _T("Select NDAS hard drive less than %d"), NDAS_COPY_THREAD_LIMIT);
		MessageBox(
			szText,
			_T("Copy Failure"),
			MB_OK | MB_ICONEXCLAMATION);

		return;
	}
	AddLog(_T("Initializing copy process..."));

	// collect select NDAS list
	int sel[NDAS_COPY_THREAD_LIMIT];
	m_ListBoxNDASList.GetSelItems(NDAS_COPY_THREAD_LIMIT, sel);
	AddLog(_T("Adding copy information"));
	for (int i = 0; i < m_nThreadTotal; i++) {	
		m_ThreadCopyItems[i] = (NDAS_COPY_ITEM *)m_ListBoxNDASList.GetItemDataPtr(sel[i]);

		_StringMacAddress(szText, countof(szText), m_ThreadCopyItems[i]->DeviceAddress, m_ThreadCopyItems[i]->LocalAddress);
		AddLog(szText);
	}

	m_pDriveInfo = (NDAS_COPY_DRIVE_INFO *)m_ComboBoxDriveList.GetItemDataPtr(m_ComboBoxDriveList.GetCurSel());

	// open drive
	TCHAR szPath[256];
	StringCchPrintf(szPath, countof(szPath), _T("\\\\.\\PhysicalDrive%d"), m_pDriveInfo->dwDrive );
	m_hSrcDrive = CreateFile(szPath,
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		0,
		0); // close this handle in ThreadProcMain()

	if (m_hSrcDrive == INVALID_HANDLE_VALUE) {
		TCHAR szText[256];
		StringCchPrintf(szText, countof(szText), _T("Failed to open local drive : GetLastError(%d)"), GetLastError());

		MessageBox(
			szText,
			_T("Copy Failure"),
			MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	m_bCopying = TRUE;
	m_ButtonStart.EnableWindow(FALSE);
	m_ButtonStart.SetWindowText(_T("Copying..."));
	m_ProgressCtrlCopy.SetRange32(0, (int)(m_pDriveInfo->PartitionLengthInBytes / (1000 * 1000)));
	AddLog(_T("Creating reading thread..."));
	CreateThread(NULL, 0, _ThreadProcMain, this, NULL, &dwThreadId);
}

void CMainDlg::OnCmdResetList(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	if (m_bCopying) {
		AddLog(_T("Not now"));
		return;
	}

	AddLog(_T("Resetting NDAS list"));
	Lock();
	ResetNDASList();
	Unlock();
}

void CMainDlg::ResetNDASList()
{
	for (int i = 0; i < m_ListBoxNDASList.GetCount(); i++) {
		free(m_ListBoxNDASList.GetItemDataPtr(i));
	}
	m_ListBoxNDASList.ResetContent();
}

void CMainDlg::AddLog(TCHAR* szLog)
{
	m_EditLog.SetSel(-1, -1);
	m_EditLog.ReplaceSel(szLog);
	m_EditLog.SetSel(-1, -1);
	m_EditLog.ReplaceSel(_T("\r\n"));
}