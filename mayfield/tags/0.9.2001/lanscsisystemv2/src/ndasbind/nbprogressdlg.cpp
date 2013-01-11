////////////////////////////////////////////////////////////////////////////
//
// Implementation of CProgressDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "nbprogressdlg.h"

CProgressDlg::CProgressDlg(void)
{
}

LRESULT CProgressDlg::OnInitDialog(HWND hWndCtl, LPARAM lParam)
{
	CenterWindow();
	m_progBar.SubclassWindow( GetDlgItem(IDC_PROGBAR) );
	DoDataExchange(FALSE);
	return 0;
}

void CProgressDlg::OnCancel(UINT wNotifyCode, WORD wID, HWND hwndCtl)
{
	EndDialog(IDCANCEL);
}


