#pragma once
#include "editchainpaste.h"

namespace ndrwiz  {

	class CWizard;
	class CCompletionPage;

	///////////////////////////////////////////////////////////////////////
	//
	// Shared Wizard Data
	//
	///////////////////////////////////////////////////////////////////////
	
	typedef struct _WIZARD_DATA {
		TCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR szDeviceKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];
		TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
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
		CButton m_wndDontShow;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_INTRO };

		BEGIN_MSG_MAP_EX(CIntroPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_ID_HANDLER_EX(IDC_DONT_SHOW_REGWIZ,OnCheckDontShow)
			CHAIN_MSG_MAP(CPropertyPageImpl<CIntroPage>)
		END_MSG_MAP()

		CIntroPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		BOOL OnSetActive();
		VOID OnCheckDontShow(UINT uCode, INT nCtrlID, HWND hwndCtrl);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Device Name Page
	//
	///////////////////////////////////////////////////////////////////////

	class CDeviceNamePage :
		public CPropertyPageImpl<CDeviceNamePage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;
		CEdit m_wndDevName;
		BOOL m_bValidName;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_DEVICE_NAME };

		BEGIN_MSG_MAP_EX(CDeviceNamePage)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDC_DEV_NAME,EN_CHANGE, DevName_OnChange)
			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceNamePage>)
		END_MSG_MAP()

		CDeviceNamePage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		LRESULT DevName_OnChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
		BOOL ValidateDeviceName();
		// Notification handler
		BOOL OnSetActive();
		INT OnWizardNext();
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Device ID Page
	//
	///////////////////////////////////////////////////////////////////////

	class CDeviceIdPage :
		public CPropertyPageImpl<CDeviceIdPage>
	{
		CEdit m_DevId1;
		CEdit m_DevId2;
		CEdit m_DevId3;
		CEdit m_DevId4;
		CEdit m_DevKey;
		
		TCHAR m_szDevId[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR m_szDevKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];

		CEditChainPaste m_wndPasteChains[5];

		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		BOOL m_bNextEnabled;
	public:
		enum { IDD = IDD_DEVREG_WIZARD_DEVICE_ID };

		BEGIN_MSG_MAP_EX(CDeviceIdPage)

			MSG_WM_INITDIALOG(OnInitDialog)

			COMMAND_HANDLER_EX(IDC_DEV_ID_1,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_2,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_3,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_4,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_KEY,EN_CHANGE, DevId_OnChange)

			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceIdPage>)

			CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[0])
			CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[1])
			CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[2])
			CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[3])

		END_MSG_MAP()

		CDeviceIdPage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		BOOL OnSetActive();
		INT OnWizardNext();

		VOID UpdateDevId();
		VOID UpdateDevKey();

		LRESULT DevId_OnChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// Mount Device Page
	//
	///////////////////////////////////////////////////////////////////////

	class CDeviceMountPage :
		public CPropertyPageImpl<CDeviceMountPage>
	{
		enum State
		{ ST_INIT, ST_PREPARING, ST_USERCONFIRM, ST_MOUNTING } m_state;

		ndas::LogicalDevice* m_pLogDevice;
		BOOL m_fMountRW;

		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		CButton m_wndMountRW;
		CButton m_wndMountRO;
		CButton m_wndDontMount;
		CStatic m_wndMountStatus;
		CStatic m_wndMountWarning;
		CStatic m_wndMountQuestion;

		HANDLE m_hPrepareMountThread;
		HANDLE m_hDoMountThread;

		HCURSOR m_hOldCursor;

		UINT PrepareMount();
		UINT DoMount();
		DWORD pPrepareMountProc();
		DWORD pDoMountProc();
		static DWORD WINAPI spPrepareMountProc(LPVOID lpParam);
		static DWORD WINAPI spDoMountProc(LPVOID lpParam);

		VOID ReportWorkDone(State newState);

		VOID pShowMountOptions(BOOL fShow);

	public:
		enum { IDD = IDD_DEVREG_WIZARD_MOUNT };

		BEGIN_MSG_MAP_EX(CDeviceMountPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceMountPage>)
		END_MSG_MAP()

		CDeviceMountPage(HWND hWndParent, PWIZARD_DATA pData);
		virtual ~CDeviceMountPage();
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);

		BOOL OnSetActive();
		BOOL OnQueryCancel();
		INT OnWizardNext();

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
		CStatic m_wndCompletionMsg;
		UINT m_uIDCompletionMsg;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_COMPLETE };

		BEGIN_MSG_MAP_EX(CCompletionPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CCompletionPage>)
			END_MSG_MAP()

			CCompletionPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		BOOL OnSetActive();

		VOID SetCompletionMessage(UINT nID);
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
		CCompletionPage m_pgComplete;
		CDeviceIdPage m_pgDeviceId;
		CDeviceNamePage m_pgDeviceName;
		CDeviceMountPage m_pgMount;

		CWizard(HWND hWndParent = NULL);
		VOID OnShowWindow(BOOL fShow, UINT nStatus);
	};

}

