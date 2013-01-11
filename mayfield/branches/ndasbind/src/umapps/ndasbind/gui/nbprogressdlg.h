////////////////////////////////////////////////////////////////////////////
//
// Interface of CProgressDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NBPROGRESSDLG_H_
#define _NBPROGRESSDLG_H_

#include "resource.h"
#include "TextProgressBar.h"

class CProgressDlg :
	public CDialogImpl<CProgressDlg>,
	public CWinDataExchange<CProgressDlg>
{
protected:
	CTime		m_timeBegin;
	CTimeSpan	m_timeElapsed, m_timeLeft;
	UINT		m_nCurrentStep;
	//UINT		m_nSectorPerSecond;		// Speed in sector per seconds

	CTextProgressBarCtrl m_progBar;
public:
	CProgressDlg(void);
	enum { IDD = IDD_PROGDLG };
	BEGIN_DDX_MAP(CProgressDlg)
	END_DDX_MAP()
	BEGIN_MSG_MAP(CProgressDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnCancel(UINT wNotifyCode, WORD wID, HWND hwndCtl);
};

#endif // _NBPROGRESSDLG_H_
