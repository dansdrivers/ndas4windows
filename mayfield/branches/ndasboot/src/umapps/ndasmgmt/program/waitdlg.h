#pragma once

class CWaitDialog : 
	public CDialogImpl<CWaitDialog>
{
public:

	typedef BOOL (CALLBACK* STOPWAITPROC)(LPVOID lpContext);

protected:

	UINT_PTR m_uiTimer;
	UINT m_uElapse;
	STOPWAITPROC m_pfnWaitCheck;
	LPVOID m_pfnContext;

	UINT m_nMessageID;
	UINT m_nTitleID;

	CStatic m_wndWaitMessage;

public:
	
	enum { IDD = IDD_WAIT };
	
	BEGIN_MSG_MAP_EX(CWaitDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER_EX(IDCANCEL,OnCancel)
	END_MSG_MAP()

	CWaitDialog(
		UINT nMessageID,
		UINT nTitleID,
		UINT uElapse, 
		STOPWAITPROC proc, 
		LPVOID lpContext);

	virtual ~CWaitDialog();

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	VOID OnDestroy();
	VOID OnTimer(UINT uTimer, TIMERPROC proc);
	VOID OnCancel(UINT wNotifyCode, int wID, HWND hWndCtl);
};

inline
CWaitDialog::CWaitDialog(
	UINT nMessageID,
	UINT nTitleID,
	UINT uElapse, 
	STOPWAITPROC pfnCallback, 
	LPVOID lpContext) :
	m_nMessageID(nMessageID),
	m_nTitleID(nTitleID),
	m_uiTimer(0),
	m_uElapse(uElapse),
	m_pfnWaitCheck(pfnCallback),
	m_pfnContext(lpContext)
{
}

inline
CWaitDialog::~CWaitDialog()
{
}

inline
VOID
CWaitDialog::OnDestroy()
{
	if (0 != m_uiTimer) {
		KillTimer(100);
	}
}

inline
LRESULT
CWaitDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CenterWindow();

	m_uiTimer = SetTimer(100, m_uElapse, NULL);
	m_wndWaitMessage.Attach(GetDlgItem(IDC_WAIT_MESSAGE));

	CString strTitle, strMessage;
	strTitle.LoadString(m_nTitleID);
	strMessage.LoadString(m_nMessageID);

	SetWindowText(strTitle);
	m_wndWaitMessage.SetWindowText(strMessage);

	return 1;
}

inline
VOID
CWaitDialog::OnTimer(UINT uTimer, TIMERPROC proc)
{
	if (NULL != m_pfnWaitCheck) {
		if (m_pfnWaitCheck(m_pfnContext)) {
			EndDialog(IDOK);
		}
	}
}

inline
VOID 
CWaitDialog::OnCancel(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	EndDialog(IDCANCEL);
}

class CWorkerDialog : 
	public CDialogImpl<CWorkerDialog>
{
public:

	typedef DWORD (WINAPI* WORKERPROC)(
		CWorkerDialog* workerDlg,
		LPVOID lpContext);

protected:

	HANDLE m_hCancelEvent;
	WORKERPROC m_pfnWorker;
	LPVOID m_pfnContext;

	DWORD m_dwWorkerThreadID;
	HANDLE m_hWorkerThread;

	UINT m_nMessageID;
	UINT m_nTitleID;
	UINT m_uiResponse;

	CButton m_wndCancel;
	CStatic m_wndWaitMessage;

	static DWORD WINAPI spThreadProc(LPVOID lpContext)
	{
		CWorkerDialog* pThis = reinterpret_cast<CWorkerDialog*>(lpContext);
		DWORD dwRet = pThis->m_pfnWorker(pThis,pThis->m_pfnContext);
		pThis->EndDialog(pThis->m_uiResponse);
		::ExitThread(dwRet);
		return dwRet;
	}


public:
	
	enum { IDD = IDD_WAIT };

#define WM_WORKDLG_SET_BUTTON_TEXT (WM_USER + 0x00E1)
#define WM_WORKDLG_SET_MESSAGE	(WM_USER + 0x00E2)
#define WM_WORKDLG_REPORT_ERROR	(WM_USER + 0x00E3)

	BEGIN_MSG_MAP_EX(CWorkerDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER_EX(IDCANCEL,OnCancel)
		MESSAGE_HANDLER_EX(WM_WORKDLG_SET_BUTTON_TEXT, OnSetButtonText)
		MESSAGE_HANDLER_EX(WM_WORKDLG_SET_MESSAGE, OnSetMessage)
		MESSAGE_HANDLER_EX(WM_WORKDLG_REPORT_ERROR, OnReportError)
	END_MSG_MAP()

	CWorkerDialog(
		UINT nMessageID,
		UINT nTitleID,
		WORKERPROC pfnWorker);

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	VOID OnDestroy();
	VOID OnCancel(UINT wNotifyCode, int wID, HWND hWndCtl);

	LRESULT OnSetButtonText(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSetMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnReportError(UINT uMsg, WPARAM wParam, LPARAM lParam);

	BOOL SetButtonText(UINT nTextID);
	BOOL SetMessage(UINT nMessageID);
	BOOL ReportError(UINT nMessageID, DWORD dwError);

	VOID SetResponse(UINT uiResponse) { m_uiResponse = uiResponse; }
	HANDLE GetCancelEvent() { return m_hCancelEvent; }
};

inline
LRESULT 
CWorkerDialog::OnSetButtonText(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString str;
	BOOL fSuccess = str.LoadString((UINT)wParam);
	ATLASSERT(fSuccess);
	m_wndCancel.SetWindowText(str);

	return TRUE;
}

inline
LRESULT 
CWorkerDialog::OnSetMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString str;
	BOOL fSuccess = str.LoadString((UINT)wParam);
	ATLASSERT(fSuccess);
	m_wndWaitMessage.SetWindowText(str);

	return TRUE;
}

inline
LRESULT 
CWorkerDialog::OnReportError(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ShowErrorMessageBox((UINT)wParam,NULL,m_hWnd,(UINT)lParam);
	return TRUE;
}

inline
CWorkerDialog::CWorkerDialog(
	UINT nMessageID, 
	UINT nTitleID,
	WORKERPROC pfnWorker) :
	m_nMessageID(nMessageID),
	m_nTitleID(nTitleID),
	m_hCancelEvent(NULL),
	m_hWorkerThread(NULL),
	m_pfnWorker(pfnWorker),
	m_uiResponse(IDCLOSE)
{
}

inline
LRESULT
CWorkerDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	BOOL fSuccess;
	
	CenterWindow();

	m_wndCancel.Attach(GetDlgItem(IDCANCEL));
	m_wndWaitMessage.Attach(GetDlgItem(IDC_WAIT_MESSAGE));

	CString strTitle, strMessage;
	fSuccess = strTitle.LoadString(m_nTitleID);
	ATLASSERT(fSuccess);
	fSuccess = strMessage.LoadString(m_nMessageID);
	ATLASSERT(fSuccess);

	SetWindowText(strTitle);
	m_wndWaitMessage.SetWindowText(strMessage);

	if (NULL == m_hCancelEvent) {
		m_hCancelEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hCancelEvent) {
		}
	}

	ATLASSERT(NULL != m_pfnWorker);
	m_hWorkerThread = ::CreateThread(
		NULL, 
		0, 
		spThreadProc, 
		this, 
		0, 
		&m_dwWorkerThreadID);

	return 1;
}

inline
VOID
CWorkerDialog::OnDestroy()
{
	if (NULL != m_hWorkerThread) {
		::CloseHandle(m_hWorkerThread);
		// ::WaitForSingleObject(m_hWorkerThread, INFINITE);
	}

	if (NULL != m_hCancelEvent) {
		::CloseHandle(m_hCancelEvent);
	}
}

inline
VOID
CWorkerDialog::OnCancel(UINT, int, HWND)
{
	if (NULL != m_hCancelEvent) {
		m_wndCancel.EnableWindow(FALSE);
		::SetEvent(m_hCancelEvent);
	}
}

inline
BOOL 
CWorkerDialog::SetButtonText(UINT nTextID)
{
	return PostMessage(WM_WORKDLG_SET_BUTTON_TEXT, nTextID, 0);
}

inline
BOOL 
CWorkerDialog::SetMessage(UINT nMessageID)
{
	return PostMessage(WM_WORKDLG_SET_MESSAGE, nMessageID, 0);
}

inline
BOOL
CWorkerDialog::ReportError(UINT nMessageID, DWORD dwError)
{
	return PostMessage(
		WM_WORKDLG_REPORT_ERROR, 
		(WPARAM)nMessageID, 
		(LPARAM)dwError);
}
