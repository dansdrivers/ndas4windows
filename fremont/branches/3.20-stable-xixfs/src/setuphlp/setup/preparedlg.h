#pragma once
#include "resource.h"
#include "winutil.h"

class CPrepareDlg : 
	public CDialogImpl<CPrepareDlg>
{
public:
	enum { IDD = IDD_PREPARE };

	BEGIN_MSG_MAP_EX(CPrepareDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_TIMER(OnTimer)
	END_MSG_MAP()

	CPrepareDlg() : m_hProcess(NULL) {}
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnTimer(UINT nID, TIMERPROC proc);
	void OnDestroy();
	int GetExitCode();
private:
	int m_pgsMax;
	CProgressBarCtrl m_wndPgs;
	HANDLE m_hProcess;
	int m_exitCode;
};

inline
LRESULT
CPrepareDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CenterWindow();

#ifndef PBS_MARQUEE
#define PBS_MARQUEE             0x08
#endif
#ifndef PBM_SETMARQUEE
#define PBM_SETMARQUEE          (WM_USER+10)
#endif

	m_wndPgs.Attach(GetDlgItem(IDC_PROGRESS1));
	m_wndPgs.ModifyStyle(0, PBS_MARQUEE | PBS_SMOOTH, FALSE);
	m_wndPgs.SendMessage(PBM_SETMARQUEE, TRUE, 100);

	m_pgsMax = 1000;
	m_wndPgs.SetRange(0, m_pgsMax);
	// m_wndPgs.SetStep(50);
	m_wndPgs.SetStep(10);
	SetTimer(0, 330, NULL);

	LPTSTR CommandLine = reinterpret_cast<LPTSTR>(lParam);
	m_exitCode = -1;
	m_hProcess = LaunchExecutable(CommandLine);
	if (NULL == m_hProcess)
	{
		ATLTRACE("LaunchExecutable failed with error %d\n", GetLastError());
		EndDialog(IDCANCEL);
	}
	return TRUE;
}

inline
void 
CPrepareDlg::OnTimer(UINT nID, TIMERPROC proc)
{
	int step = m_wndPgs.StepIt();
	DWORD waitResult = WaitForSingleObject(m_hProcess, 0);
	if (waitResult == WAIT_OBJECT_0)
	{
		DWORD exitCode;
		if (GetExitCodeProcess(m_hProcess, &exitCode))
		{
			ATLTRACE("ExitCode=%d\n", exitCode);
		}
		else
		{
			ATLTRACE("Failed to retrieve exit code of the process=%p\n", m_hProcess);
		}
		m_exitCode = exitCode;
		EndDialog(IDOK);
	}
	ATLASSERT(WAIT_OBJECT_0 == waitResult || WAIT_TIMEOUT == waitResult);
}

inline void
CPrepareDlg::OnDestroy()
{
	if (m_hProcess)
	{
		::CloseHandle(m_hProcess);
		m_hProcess = NULL;
	}
}

inline int
CPrepareDlg::GetExitCode()
{
	return m_exitCode;
}
