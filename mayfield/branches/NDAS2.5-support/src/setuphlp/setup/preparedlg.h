#pragma once
#include "resource.h"
#include "winutil.h"
#include "sdf.h"

class CPrepareDlg : 
	public CDialogImpl<CPrepareDlg>
{
public:
	enum { IDD = IDD_PREPARE };

	BEGIN_MSG_MAP_EX(CPrepareDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnTimer(UINT nID, TIMERPROC proc);

private:
	int m_pgsMax;
	CProgressBarCtrl m_wndPgs;
	HANDLE m_hProcess;
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

	const SDF_DATA* SdfData = reinterpret_cast<const SDF_DATA*>(lParam);
	m_hProcess = RunMsiInstaller(SdfData->MsiRedist);
	if (NULL == m_hProcess)
	{
		ATLTRACE("RunMsiInstaller failed with error %d\n", GetLastError());
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
		EndDialog(IDOK);
	}
	ATLASSERT(WAIT_OBJECT_0 == waitResult || WAIT_TIMEOUT == waitResult);
}

