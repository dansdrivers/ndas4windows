// nbaboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TreeListView.h"
#include "pix.h"

template<class T, class TBase = CStatic, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CJpegImageCtrlImpl : 
	public CWindowImpl<T, TBase, TWinTraits>
{
protected:
	UINT m_nImageID;
	CPix m_pix;
public:
	typedef CJpegImageCtrlImpl<T, TBase, TWinTraits> thisClass;
	typedef CWindowImpl<T, TBase, TWinTraits> superClass;

	BEGIN_MSG_MAP_EX( thisClass )
		MSG_WM_PAINT(OnPaint)
	END_MSG_MAP()

	void SetImage(UINT nImageID)
	{
		m_pix.LoadFromResource(
			_Module.GetResourceInstance(), 
			nImageID, 
			_T("IMAGE"));
	}

	void OnPaint(HDC /*hDC*/)
	{
		CPaintDC dc(m_hWnd);
		CRect rectClient;
		GetClientRect( rectClient );

		CDCHandle dcHandle(dc);
		m_pix.Render( dcHandle, rectClient );
	}
};

class CJpegImageCtrl : 
	public CJpegImageCtrlImpl<CJpegImageCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, GetWndClassName());
};

class CAboutDlg : 
	public CDialogImpl<CAboutDlg>,
	public CWinDataExchange<CAboutDlg>
{
protected:
	CHyperLink m_wndLink;
	CJpegImageCtrl m_wndImage;

	CTreeListViewCtrl m_tree;
public:
	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
