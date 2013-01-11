// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "imagectrl.h"
#include <ndas/ndupdate.h>

class CAboutDialog : 
	public CDialogImpl<CAboutDialog>
{
	CButton m_wndUpdate;
	CHyperLink m_wndHyperLink;
	CImageCtrl m_wndImage;

public:

	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP_EX(CAboutDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_CHECK_UPDATE,OnCheckUpdate)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnCheckUpdate(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCloseCmd(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
	{
		EndDialog(wID);
	}

private:

	typedef BOOL (WINAPI* NDUPDATE_NdasUpdateDoUpdate)(HWND, LPCTSTR, PNDUPDATE_SYSTEM_INFO);
	XTL::AutoModuleHandle m_hUpdateDll;
	NDUPDATE_NdasUpdateDoUpdate m_pfnDoUpdate;
};
