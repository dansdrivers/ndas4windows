// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "pix.h"
#include <atlres.h>
class CAboutDialog : 
	public CDialogImpl<CAboutDialog>
{
	CButton m_wndUpdate;
	CHyperLink m_wndHyperLink;
	CPix m_pix;

public:
	enum { IDD = IDD_ABOUTBOX };

	CAboutDialog() {}
	~CAboutDialog() {}

	BEGIN_MSG_MAP_EX(CAboutDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_PAINT(OnPaint)
		COMMAND_ID_HANDLER_EX(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER_EX(IDC_CHECK_UPDATE,OnCheckUpdate)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnPaint(HDC hDC);

	VOID OnCheckUpdate(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnCloseCmd(UINT wNotifyCode, int wID, HWND hWndCtl)
	{
		EndDialog(wID);
	}
};
