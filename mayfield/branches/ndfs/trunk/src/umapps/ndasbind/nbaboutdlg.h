// nbaboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "imagectrl.h"

class CAboutDlg : 
	public CDialogImpl<CAboutDlg>,
	public CWinDataExchange<CAboutDlg>
{
protected:
	
	CHyperLink m_wndLink;
	CImageCtrl m_wndImage;

public:
	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP_EX(CAboutDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_RANGE_HANDLER_EX(IDOK,IDNO,OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWndCtrl, LPARAM lParam);
	void OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/)
	{
		EndDialog(wID);
	}
};
