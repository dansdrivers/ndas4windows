#pragma once
#include "propertytree.h"

#ifndef _WTL_NEW_PAGE_NOTIFY_HANDLERS
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS
#endif

typedef struct _APP_OPT_VALUE_DEF {

	typedef enum _APP_OPT_VAL_TYPE {
		AOV_BOOL,
		AOV_DWORD
	} APP_OPT_VALUE_TYPE;

	typedef union _APP_OPT_VALUE {
		BOOL fValue;
		DWORD dwValue;
	} APP_OPT_VALUE;

	LPCTSTR szValueName;
	UINT nResID;
	APP_OPT_VALUE_TYPE ovType;
	APP_OPT_VALUE DefaultValue;
	APP_OPT_VALUE CurrentValue;
	APP_OPT_VALUE NewValue;

} APP_OPT_VALUE_DEF, *PAPP_OPT_VALUE_DEF;

class CAppOptGeneralPage :
	public CPropertyPageImpl<CAppOptGeneralPage>,
	public CWinDataExchange<CAppOptGeneralPage>
{

	typedef enum _TRI_STATE {
		TriStateUnknown,
		TriStateYes,
		TriStateNo
	} TRI_STATE;

	LANGID m_wConfigLangID;
	CButton m_wndAlertDisconnect;
	CButton m_wndAlertReconnect;
	CButton m_wndRemountOnBoot;
	CComboBox m_wndUILangList;
	CImageList m_imageList;
	CButton m_wndDisableAutoPlay;
	TRI_STATE m_disableAutoPlay;

public:

	enum { IDD = IDR_OPTION_GENERAL };

	BEGIN_MSG_MAP_EX(CAppOptGeneralPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CAppOptGeneralPage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	INT OnApply();

	void GetOptions();
	void SetOptions();
};

class CAppOptAdvancedPage :
	public CPropertyPageImpl<CAppOptAdvancedPage>
{
	CPropertyTreeCtrl m_tree;
	CImageList m_images;

	HTREEITEM m_tiGroupConfirm;
	HTREEITEM m_tiGroupMisc;

	HTREEITEM m_tiGroupSuspend;
	HTREEITEM m_tiGroupSuspendAllow;

	HTREEITEM m_tiGroupAutoPnp;

	DWORD m_dwSuspend;
	DWORD m_dwAutoPnp;

public:
	enum { IDD = IDD_OPTION_ADVANCED };

	BEGIN_MSG_MAP_EX(CAppOptAdvancedPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		NOTIFY_ID_HANDLER_EX(IDC_OPTION_TREE,OnTreeNotify)
		CHAIN_MSG_MAP(CPropertyPageImpl<CAppOptAdvancedPage>)
		REFLECT_NOTIFICATIONS() // CPropertyTree suppport
	END_MSG_MAP()
	
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	INT OnApply();
	LRESULT OnTreeNotify(LPNMHDR pnmh);
};

class CAppOptPropSheet :
	public CPropertySheetImpl<CAppOptPropSheet>
{
public:
	
	BEGIN_MSG_MAP_EX(CAppOptPropSheet)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<CAppOptPropSheet>)
	END_MSG_MAP()

	CAppOptPropSheet(
		_U_STRINGorID title = (LPCTSTR) NULL,
		UINT uStartPage = 0,
		HWND hWndParent = NULL);

	CAppOptGeneralPage m_pgGeneral;
	CAppOptAdvancedPage  m_pgAdvanced;

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnShowWindow(BOOL bShow, UINT nStatus);

private:
	BOOL m_bCentered;

};
