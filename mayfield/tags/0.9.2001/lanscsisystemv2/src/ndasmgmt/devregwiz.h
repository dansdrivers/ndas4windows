#pragma once

namespace ndrwiz  {

	const MOUNT_OPT_RW = 0x0001;
	const MOUNT_OPT_RO = 0x0002;
	const MOUNT_OPT_NONE = 0x0000;

	typedef struct _WIZARD_DATA {
		TCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR szDeviceKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];
		TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		DWORD dwGrantedMount;
		DWORD dwMountOpt;
	} WIZARD_DATA, *PWIZARD_DATA;

	class CIntroPage :
		public CPropertyPageImpl<CIntroPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_INTRO };

		BEGIN_MSG_MAP_EX(CIntroPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CIntroPage>)
		END_MSG_MAP()

		CIntroPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		int OnSetActive();

	};

	class CCompletionPage :
		public CPropertyPageImpl<CCompletionPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_COMPLETE };

		BEGIN_MSG_MAP_EX(CCompletionPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CCompletionPage>)
		END_MSG_MAP()

		CCompletionPage(HWND hWndParent, PWIZARD_DATA pData);

		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		int OnSetActive();
	};

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

		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		BOOL m_bNextEnabled;
	public:
		enum { IDD = IDD_DEVREG_WIZARD_DEVICE_ID };

		BEGIN_MSG_MAP_EX(CDeviceIdPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceIdPage>)
			COMMAND_HANDLER_EX(IDC_DEV_ID_1,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_2,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_3,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_ID_4,EN_CHANGE, DevId_OnChange)
			COMMAND_HANDLER_EX(IDC_DEV_KEY,EN_CHANGE, DevId_OnChange)
		END_MSG_MAP()

		CDeviceIdPage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		int OnSetActive();
		int OnWizardNext();

		void UpdateDevId();
		void UpdateDevKey();

		LRESULT DevId_OnChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
	};

	class CDeviceNamePage :
		public CPropertyPageImpl<CDeviceNamePage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

		CEdit m_DevName;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_DEVICE_NAME };

		BEGIN_MSG_MAP_EX(CDeviceNamePage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceNamePage>)
		END_MSG_MAP()

		CDeviceNamePage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		// Notification handler
		int OnSetActive();
	};

	class CDeviceMountPage :
		public CPropertyPageImpl<CDeviceMountPage>
	{
		PWIZARD_DATA m_pWizData;
		HWND m_hWndParent;

	public:
		enum { IDD = IDD_DEVREG_WIZARD_MOUNT };

		BEGIN_MSG_MAP_EX(CDeviceMountPage)
			MSG_WM_INITDIALOG(OnInitDialog)
			CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceMountPage>)
		END_MSG_MAP()

		CDeviceMountPage(HWND hWndParent, PWIZARD_DATA pData);
		LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
		int OnSetActive();
	};

	class CWizard :
		public CPropertySheetImpl<CWizard>
	{
		WIZARD_DATA m_wizData;
	public:
		BEGIN_MSG_MAP(CWizard)
			CHAIN_MSG_MAP(CPropertySheetImpl<CWizard>)
		END_MSG_MAP()

		// Property pages
		CIntroPage m_pgIntro;
		CCompletionPage m_pgComplete;
		CDeviceIdPage m_pgDeviceId;
		CDeviceNamePage m_pgDeviceName;
		CDeviceMountPage m_pgMount;

		CWizard(HWND hWndParent = NULL);
	};

}

