#pragma once
#include "editchainpaste.h"

class CRegWizard;
class CRegWizCompletionPage;

///////////////////////////////////////////////////////////////////////
//
// Shared Wizard Data
//
///////////////////////////////////////////////////////////////////////

typedef struct _REG_WIZARD_DATA {
	TCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
	TCHAR szDeviceKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];
	TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	CRegWizard* ppsh;
	CRegWizCompletionPage* ppspComplete;
} REG_WIZARD_DATA, *PREG_WIZARD_DATA;

///////////////////////////////////////////////////////////////////////
//
// Intro Page
//
///////////////////////////////////////////////////////////////////////

class CRegWizIntroPage :
	public CPropertyPageImpl<CRegWizIntroPage>
{
	PREG_WIZARD_DATA m_pWizData;
	HWND m_hWndParent;

public:
	enum { IDD = IDD_DEVREG_WIZARD_INTRO };

	BEGIN_MSG_MAP_EX(CRegWizIntroPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CRegWizIntroPage>)
	END_MSG_MAP()

	CRegWizIntroPage(HWND hWndParent, PREG_WIZARD_DATA pData);

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	// Notification handler
	BOOL OnSetActive();
};

///////////////////////////////////////////////////////////////////////
//
// Device Name Page
//
///////////////////////////////////////////////////////////////////////

class CRegWizDeviceNamePage :
	public CPropertyPageImpl<CRegWizDeviceNamePage>
{
	PREG_WIZARD_DATA m_pWizData;
	HWND m_hWndParent;
	CEdit m_wndDevName;
	BOOL m_bValidName;

public:
	enum { IDD = IDD_DEVREG_WIZARD_DEVICE_NAME };

	BEGIN_MSG_MAP_EX(CRegWizDeviceNamePage)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEV_NAME,EN_CHANGE, DevName_OnChange)
		CHAIN_MSG_MAP(CPropertyPageImpl<CRegWizDeviceNamePage>)
	END_MSG_MAP()

	CRegWizDeviceNamePage(HWND hWndParent, PREG_WIZARD_DATA pData);
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

class CRegWizDeviceIdPage :
	public CPropertyPageImpl<CRegWizDeviceIdPage>
{
	CEdit m_DevId1;
	CEdit m_DevId2;
	CEdit m_DevId3;
	CEdit m_DevId4;
	CEdit m_DevKey;
	
	TCHAR m_szDevId[NDAS_DEVICE_STRING_ID_LEN + 1];
	TCHAR m_szDevKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];

	CEditChainPaste m_wndPasteChains[5];

	PREG_WIZARD_DATA m_pWizData;
	HWND m_hWndParent;

	BOOL m_bNextEnabled;
public:
	enum { IDD = IDD_DEVREG_WIZARD_DEVICE_ID };

	BEGIN_MSG_MAP_EX(CRegWizDeviceIdPage)

		MSG_WM_INITDIALOG(OnInitDialog)

		COMMAND_HANDLER_EX(IDC_DEV_ID_1,EN_CHANGE, DevId_OnChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_2,EN_CHANGE, DevId_OnChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_3,EN_CHANGE, DevId_OnChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_4,EN_CHANGE, DevId_OnChange)
		COMMAND_HANDLER_EX(IDC_DEV_KEY,EN_CHANGE, DevId_OnChange)

		CHAIN_MSG_MAP(CPropertyPageImpl<CRegWizDeviceIdPage>)
		
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[0])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[1])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[2])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[3])

	END_MSG_MAP()

	CRegWizDeviceIdPage(HWND hWndParent, PREG_WIZARD_DATA pData);
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	// Notification handler
	BOOL OnSetActive();
	INT OnWizardNext();

	void UpdateDevId();
	void UpdateDevKey();

	LRESULT DevId_OnChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
};

///////////////////////////////////////////////////////////////////////
//
// Mount Device Page
//
///////////////////////////////////////////////////////////////////////

class CRegWizDeviceMountPage :
	public CPropertyPageImpl<CRegWizDeviceMountPage>
{
	enum State
	{ ST_INIT, ST_PREPARING, ST_USERCONFIRM, ST_MOUNTING } m_state;

	ndas::LogicalDevicePtr m_pLogDevice;
	BOOL m_fMountRW;

	PREG_WIZARD_DATA m_pWizData;
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

	CAnimateCtrl m_aniWait;

	UINT PrepareMount();
	DWORD PrepareMountThreadStart();
	
	void ReportWorkDone(State newState);
	void pShowMountOptions(BOOL fShow);

public:
	enum { IDD = IDD_DEVREG_WIZARD_MOUNT };

	BEGIN_MSG_MAP_EX(CRegWizDeviceMountPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CRegWizDeviceMountPage>)
	END_MSG_MAP()

	CRegWizDeviceMountPage(HWND hWndParent, PREG_WIZARD_DATA pData);
	virtual ~CRegWizDeviceMountPage();
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);

	BOOL OnSetActive();
	BOOL OnQueryCancel();
	INT OnWizardNext();

	void OnReset();

private:
	static DWORD WINAPI PrepareMountThreadProc(LPVOID lpParam)
	{
		CRegWizDeviceMountPage* pThis = 
			reinterpret_cast<CRegWizDeviceMountPage*>(lpParam);
		return pThis->PrepareMountThreadStart();
	}
};

///////////////////////////////////////////////////////////////////////
//
// Completion Page
//
///////////////////////////////////////////////////////////////////////

class CRegWizCompletionPage :
	public CPropertyPageImpl<CRegWizCompletionPage>
{
	PREG_WIZARD_DATA m_pWizData;
	HWND m_hWndParent;
	CStatic m_wndCompletionMsg;
	UINT m_uIDCompletionMsg;
	CHyperLink m_wndExport;

public:
	enum { IDD = IDD_DEVREG_WIZARD_COMPLETE };

	BEGIN_MSG_MAP_EX(CRegWizCompletionPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CRegWizCompletionPage>)
		COMMAND_ID_HANDLER_EX(IDC_EXPORT, OnCmdExport)
	END_MSG_MAP()

	CRegWizCompletionPage(HWND hWndParent, PREG_WIZARD_DATA pData);

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	BOOL OnSetActive();
	void OnCmdExport(UINT, int, HWND);

	void SetCompletionMessage(UINT nID);
};

///////////////////////////////////////////////////////////////////////
//
// Registration Wizard Property Sheet
//
///////////////////////////////////////////////////////////////////////

class CRegWizard :
	public CPropertySheetImpl<CRegWizard>
{
	BOOL m_fCentered;
	REG_WIZARD_DATA m_wizData;
public:

	BEGIN_MSG_MAP(CRegWizard)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<CRegWizard>)
	END_MSG_MAP()

	// Property pages
	CRegWizIntroPage m_pgIntro;
	CRegWizCompletionPage m_pgComplete;
	CRegWizDeviceIdPage m_pgDeviceId;
	CRegWizDeviceNamePage m_pgDeviceName;
	CRegWizDeviceMountPage m_pgMount;

	CRegWizard(HWND hWndParent = NULL);
	void OnShowWindow(BOOL fShow, UINT nStatus);
};

