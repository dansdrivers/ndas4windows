#pragma once
#include "resource.h"

#include <ndas/ndascomm.h>
#include <ndas/ndashear.h>

#include <winioctl.h>

#define NDAS_COPY_THREAD_LIMIT 32
#define NDAS_COPY_BUFFER_SEGMENT_SIZE (10 * 1024 * 1024)
#define NDAS_COPY_BUFFER_SEGMENT_COUNT (4)

typedef enum _NDAS_COPY_ITEM_STATUS {
	NDAS_COPY_ITEM_STATUS_DISABLED = 0,	
	NDAS_COPY_ITEM_STATUS_READY = 1,
	NDAS_COPY_ITEM_STATUS_COPYING = 2,
	NDAS_COPY_ITEM_STATUS_COMPLETE = 3,
} NDAS_COPY_ITEM_STATUS, *PNDAS_COPY_ITEM_STATUS;

typedef struct _NDAS_COPY_ITEM {
	NDAS_COPY_ITEM_STATUS status;
	BYTE DeviceAddress[6], LocalAddress[6];
	HANDLE hThreadWrite;
	int iBufferUsing;
	HNDAS hNdas;
} NDAS_COPY_ITEM, *PNDAS_COPY_ITEM;

typedef struct _NDAS_COPY_BUFFER {
	INT64 LocationInBytes;
	INT64 LengthInBytes;
	BYTE *buffer;
} NDAS_COPY_BUFFER, *PNDAS_COPY_BUFFER;

typedef struct _NDAS_COPY_DRIVE_INFO {
	DWORD dwDrive;
	BOOL bIsPartition;
	DWORD dwPartition;
	INT64 StartingSectorInBytes;
	INT64 PartitionLengthInBytes;
} NDAS_COPY_DRIVE_INFO, *PNDAS_COPY_DRIVE_INFO;

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	CMainDlg();
	~CMainDlg();
public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_BUTTON_START, OnCmdStart)
		COMMAND_ID_HANDLER_EX(IDC_BUTTON_RESET_LIST, OnCmdResetList)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);

	void OnCloseCmd(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
	{
#ifdef _DEBUG
		if  (m_bCopying && IDYES != MessageBox(
			_T("Copying disk. If you stop dialog, the process may crash. Is it ok?"),
			_T("Warning"),
			MB_YESNO)) {
				return;
		}
#endif
		for (int i = 0; i < m_ComboBoxDriveList.GetCount(); i++) {
			free(m_ComboBoxDriveList.GetItemDataPtr(i));
		}
		m_ComboBoxDriveList.ResetContent();

		ResetNDASList();

		EndDialog(wID);
	}

	void OnCmdStart(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdResetList(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);	

private:
	// controls
	CComboBox m_ComboBoxDriveList;
	CListBox m_ListBoxNDASList;
	CStatic m_StaticProgress;
	CButton m_ButtonStart;
	CEdit m_EditLog;
	CProgressBarCtrl m_ProgressCtrlCopy;

	void AddLog(TCHAR* szLog);
	void ResetNDASList();

	// NDAS hear
	HANDLE m_hNdasHear;
	BOOL InitNDASHear();
public:
	void CallbackNdasHeartbeat(CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat);

private:
	void FillDriveList();

	// lock
	CRITICAL_SECTION m_cs;
	inline void Lock() {
		EnterCriticalSection(&m_cs);
	}
	inline void Unlock() {
		LeaveCriticalSection(&m_cs);
	}

	// copy thread
	BOOL m_bCopying;
	int m_iThreadIndex; // used to allocate unique write job to each thread.
	int m_nThreadTotal; // not greater than NDAS_COPY_THREAD_LIMIT
	PNDAS_COPY_ITEM m_ThreadCopyItems[NDAS_COPY_THREAD_LIMIT];
public:
	DWORD ThreadProcWrite();
	DWORD ThreadProcMain();

private:
	// source drive or partition information
	HANDLE m_hSrcDrive;
	PNDAS_COPY_DRIVE_INFO m_pDriveInfo;

	// circular buffer for producer - consumer thread model
	// Data ready from m_iBufferStart to m_iBufferEnd -1.
	// Data empty when m_iBufferStart == m_iBufferEnd
	int m_iBufferStart, m_iBufferEnd;
	NDAS_COPY_BUFFER m_Buffer[NDAS_COPY_BUFFER_SEGMENT_COUNT];
};

