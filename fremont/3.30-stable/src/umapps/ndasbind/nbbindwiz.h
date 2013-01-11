#pragma once
#include "nblistviewctrl.h"
#include "nbdev.h"

namespace nbbwiz  {

	class CWizard;
	class CCompletionPage;

	///////////////////////////////////////////////////////////////////////
	//
	// Shared Wizard Data
	//
	///////////////////////////////////////////////////////////////////////

	typedef struct _WIZARD_DATA {
		NBUnitDevicePtrList listUnitDevicesSingle;
		NBUnitDevicePtrList listUnitDevicesBind;

		NDAS_MEDIA_TYPE m_nBindType;
		UINT m_nDiskCount;
		UINT32 m_BindResult;
		DWORD dwBindLastError;
		

		CWizard* ppsh;
		CCompletionPage* ppspComplete;
	} WIZARD_DATA, *PWIZARD_DATA;
	
	///////////////////////////////////////////////////////////////////////
	//
	// Intro Page
	//
	///////////////////////////////////////////////////////////////////////

	class CIntroPage :
		public CPropertyPageImpl<CIntroPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

	public:
		enum { IDD = IDD_BIND_WIZARD_INTRO };

		BEGIN_MSG_MAP_EX(CIntroPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CIntroPage>)
		END_MSG_MAP()

		CIntroPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		BOOL OnSetActive();
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Bind Type / Disk Number Page
	//
	///////////////////////////////////////////////////////////////////////

	class CBindTypePage :
		public CPropertyPageImpl<CBindTypePage>,
		public CWinDataExchange<CBindTypePage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		int m_nBindType;
		int m_nDiskCount;

	public:
		enum { IDD = IDD_BIND_WIZARD_TYPE };

		BEGIN_MSG_MAP_EX(CBindTypePage)
			MSG_WM_INITDIALOG(OnInitDialog)
			MSG_WM_COMMAND(OnCommand)
			CHAIN_MSG_MAP(CPropertyPageImpl<CBindTypePage>)
		END_MSG_MAP()

		BEGIN_DDX_MAP(CBindTypePage)
			DDX_RADIO(IDC_BIND_TYPE_AGGR, m_nBindType)
			DDX_UINT(IDC_COMBO_DISKCOUNT, m_nDiskCount);
		END_DDX_MAP()

		CBindTypePage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		BOOL OnSetActive();
		INT OnWizardNext();
		LRESULT OnCommand(UINT msg, int nID, HWND hWnd);

		VOID UpdateControls (UINT32 nType);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Select disk Page
	//
	///////////////////////////////////////////////////////////////////////

	class CSelectDiskPage :
		public CPropertyPageImpl<CSelectDiskPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		CNBListViewCtrl m_wndListSingle;
		CNBListViewCtrl m_wndListBind;
	public:
		enum { IDD = IDD_BIND_WIZARD_SELECT_DISK };

		BEGIN_MSG_MAP_EX(CSelectDiskPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			MESSAGE_HANDLER_EX(WM_USER_NB_VIEW_LDBLCLK, OnDblClkList)
			COMMAND_ID_HANDLER_EX(IDC_BTN_ADD, OnClickAddRemove)
			COMMAND_ID_HANDLER_EX(IDC_BTN_REMOVE, OnClickAddRemove)
			COMMAND_ID_HANDLER_EX(IDC_BTN_REMOVE_ALL, OnClickAddRemove)
			NOTIFY_CODE_HANDLER_EX(LVN_ITEMCHANGED, OnListSelChanged)
			CHAIN_MSG_MAP(CPropertyPageImpl<CSelectDiskPage>)
		END_MSG_MAP()

		CSelectDiskPage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		BOOL OnSetActive();
		INT OnWizardNext();

		LRESULT OnListSelChanged(LPNMHDR /*lpNMHDR*/);
		void OnClickAddRemove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/);
		void UpdateControls();
		LRESULT OnDblClkList(UINT uMsg, WPARAM wParam, LPARAM lParam);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Completion Page
	//
	///////////////////////////////////////////////////////////////////////

	class CCompletionPage :
		public CPropertyPageImpl<CCompletionPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

	public:
		enum { IDD = IDD_BIND_WIZARD_COMPLETE };

		BEGIN_MSG_MAP_EX(CCompletionPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CCompletionPage>)
		END_MSG_MAP()

		CCompletionPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		BOOL OnSetActive();
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Registration Wizard Property Sheet
	//
	///////////////////////////////////////////////////////////////////////

	class CWizard :
		public CPropertySheetImpl<CWizard>
	{
		BOOL m_fCentered;
		WIZARD_DATA m_wizData;
	public:

		BEGIN_MSG_MAP(CWizard)
			MSG_WM_SHOWWINDOW(OnShowWindow)
			CHAIN_MSG_MAP(CPropertySheetImpl<CWizard>)
		END_MSG_MAP()

		// Property pages
		CIntroPage m_pgIntro;
		CBindTypePage m_pgBindType;
		CSelectDiskPage m_pgSelectDisk;
		CCompletionPage m_pgComplete;

		CWizard(HWND hWndParent = NULL);
		VOID OnShowWindow(BOOL fShow, UINT nStatus);

		void SetSingleDisks (NBUnitDevicePtrList &listUnitDevicesSingle);
	};

}

