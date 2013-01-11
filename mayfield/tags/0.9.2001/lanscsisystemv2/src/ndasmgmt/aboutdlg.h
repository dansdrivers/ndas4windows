// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pix.h"

class CAboutDialog : 
	public CDialogImpl<CAboutDialog>
{
	CHyperLink m_wndHyperLink;
	CPix m_pix;

public:
	enum { IDD = IDD_ABOUTBOX };

	CAboutDialog() {}
	~CAboutDialog() {}

	BEGIN_MSG_MAP_EX(CAboutDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_PAINT(OnPaint)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnPaint(HDC hDC);

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
