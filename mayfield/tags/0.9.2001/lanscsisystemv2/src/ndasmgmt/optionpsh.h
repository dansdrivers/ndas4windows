#pragma once

namespace opt {

class CGeneralPage :
	public CPropertyPageImpl<CGeneralPage>,
	public CWinDataExchange<CGeneralPage>
{
public:
	enum { IDD = IDR_OPTION_GENERAL };

	BEGIN_MSG_MAP_EX(CGeneralPage)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
};

class COptionsPropSheet :
	public CPropertySheetImpl<COptionsPropSheet>
{
	BOOL m_bCentered;
public:
	COptionsPropSheet(
		_U_STRINGorID title = (LPCTSTR) NULL,
		UINT uStartPage = 0,
		HWND hWndParent = NULL);

	BEGIN_MSG_MAP_EX(COptionsPropSheet)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<COptionsPropSheet>)
	END_MSG_MAP()

	CGeneralPage m_pgGeneral;

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);

	void OnShowWindow(BOOL bShow, UINT nStatus)
	{
		if (bShow && !m_bCentered) {
			// Center Windows only once!
			m_bCentered = TRUE;
			CenterWindow();
		}
		SetMsgHandled(FALSE);
	}
};

}
