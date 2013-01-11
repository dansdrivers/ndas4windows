#include "stdafx.h"
#include "resource.h"
#include "ndasmgmt.h"
#include "devregwiz.h"
#include "appconf.h"
#include "waitdlg.h"

class CDeviceIdEdit :
	public CWindowImpl<CDeviceIdEdit >
{
public:
	DECLARE_WND_SUPERCLASS(0, _T("EDIT"))

	BEGIN_MSG_MAP_EX(CDeviceIdEdit)
		MSG_WM_CHAR(OnChar)
	END_MSG_MAP()

	VOID OnChar(TCHAR nChar, UINT nRepCnt, UINT nFlags)
	{
//		SetMsgHandled(TRUE);
	}
};

//
// got this from mainframe.cpp
//
BOOL 
pCheckNoPendingInstall(LPVOID lpContext);

namespace ndrwiz {

static HFONT pGetTitleFont();

//////////////////////////////////////////////////////////////////////////
//
// Wizard Property Sheet
//
//////////////////////////////////////////////////////////////////////////

static const INT WIZARD_COMPLETION_INDEX = 4;

CWizard::CWizard(HWND hWndParent) :
	m_fCentered(FALSE),
	m_pgIntro(hWndParent, &m_wizData),
	m_pgDeviceId(hWndParent, &m_wizData),
	m_pgDeviceName(hWndParent, &m_wizData),
	m_pgMount(hWndParent, &m_wizData),
	m_pgComplete(hWndParent, &m_wizData),
	CPropertySheetImpl<CWizard>(IDS_DRZ_TITLE, 0, hWndParent)
{
	SetWizardMode();
	m_psh.dwFlags |= PSH_WIZARD97 | PSH_USEPAGELANG;
	SetWatermark(MAKEINTRESOURCE(IDB_WATERMARK256));
	SetHeader(MAKEINTRESOURCE(IDB_BANNER256));

	// StretchWatermark(true);
	AddPage(m_pgIntro);
	AddPage(m_pgDeviceName);
	AddPage(m_pgDeviceId);
	AddPage(m_pgMount);
	AddPage(m_pgComplete);

	::ZeroMemory(
		&m_wizData, 
		sizeof(WIZARD_DATA));
	m_wizData.ppsh = this;
	m_wizData.ppspComplete = &m_pgComplete;
}

VOID 
CWizard::OnShowWindow(BOOL fShow, UINT nStatus)
{
	if (fShow && !m_fCentered) {
		// Center Windows only once!
		m_fCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

//////////////////////////////////////////////////////////////////////////
//
// Intro Page
//
//////////////////////////////////////////////////////////////////////////

CIntroPage::CIntroPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CIntroPage>(IDS_DRZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
}


LRESULT 
CIntroPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());

	m_wndDontShow.Attach(GetDlgItem(IDC_DONT_SHOW_REGWIZ));
	return 0;
}


//
// Non-zero if the page was successfully set active;
// otherwise 0
//
BOOL 
CIntroPage::OnSetActive()
{
	CStatic stIntroCtl;
	stIntroCtl.Attach(GetDlgItem(IDC_INTRO_1));
	CString strIntro1;
	strIntro1.LoadString(IDS_DRZ_INTRO_1);
	stIntroCtl.SetWindowText(strIntro1);

	SetWizardButtons(PSWIZB_NEXT);
	return TRUE;
}

VOID 
CIntroPage::OnCheckDontShow(UINT uCode, INT nCtrlID, HWND hwndCtrl)
{
	INT iCheck = m_wndDontShow.GetCheck();
	if (BST_CHECKED == iCheck) {
		pSetAppConfigValue(_T("UseRegWizard"), FALSE);
	} else if (BST_UNCHECKED == iCheck) {
		pSetAppConfigValue(_T("UseRegWizard"), TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Device Name Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceNamePage::CDeviceNamePage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	CPropertyPageImpl<CDeviceNamePage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_NAME_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_NAME_HEADER_SUBTITLE));
}

LRESULT 
CDeviceNamePage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndDevName.Attach(GetDlgItem(IDC_DEV_NAME));
	m_wndDevName.SetLimitText(30);
	m_bValidName = FALSE;

	DWORD nDeviceCount = _pDeviceColl->GetDeviceCount();
	CString strNewDeviceName;

	strNewDeviceName.FormatMessage(IDS_NEW_DEVICE_NAME_TEMPLATE, nDeviceCount + 1);
	m_wndDevName.SetWindowText(strNewDeviceName);

	// Let the dialog manager set the initial focus
	return 1;
}

BOOL
CDeviceNamePage::OnSetActive()
{
	INT iNameLen = m_wndDevName.GetWindowTextLength();

	if (iNameLen > 0) {
		m_bValidName = TRUE;
		SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	} else {
		SetWizardButtons(PSWIZB_BACK);
		m_bValidName = FALSE;
	}
	return TRUE;
}

LRESULT 
CDeviceNamePage::DevName_OnChange(
	UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	(VOID) ValidateDeviceName();
	return 0;
}

BOOL 
CDeviceNamePage::ValidateDeviceName()
{
	INT iNameLen = m_wndDevName.GetWindowTextLength();

	if (iNameLen > 0 && !m_bValidName) {
		m_bValidName = TRUE;
		SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	} else if (iNameLen == 0 && m_bValidName) {
		SetWizardButtons(PSWIZB_BACK);
		m_bValidName = FALSE;
	}

	return m_bValidName;
}

// 0 to automatically advance to the next page
// -1 to prevent the page from changing
INT 
CDeviceNamePage::OnWizardNext()
{
	BOOL fSuccess = m_wndDevName.GetWindowText(
		m_pWizData->szDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1);

	ATLASSERT(fSuccess);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Device ID Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceIdPage::CDeviceIdPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CDeviceIdPage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_ID_HEADER_TITLE));
	
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_ID_HEADER_SUBTITLE));
}

LRESULT 
CDeviceIdPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	ATLTRACE("OnInitDialog\n");

	m_DevId1.Attach(GetDlgItem(IDC_DEV_ID_1));
	m_DevId2.Attach(GetDlgItem(IDC_DEV_ID_2));
	m_DevId3.Attach(GetDlgItem(IDC_DEV_ID_3));
	m_DevId4.Attach(GetDlgItem(IDC_DEV_ID_4));
	m_DevKey.Attach(GetDlgItem(IDC_DEV_KEY));

	//
	// chaining paste support
	//
	m_wndPasteChains[0].Attach(m_hWnd,m_DevId1,m_DevId2);
	m_wndPasteChains[1].Attach(m_hWnd,m_DevId2,m_DevId3);
	m_wndPasteChains[2].Attach(m_hWnd,m_DevId3,m_DevId4);
	m_wndPasteChains[3].Attach(m_hWnd,m_DevId4,m_DevKey);
	m_wndPasteChains[4].Attach(m_hWnd,m_DevKey,NULL);

	m_DevId1.SetLimitText(5);
	m_DevId2.SetLimitText(5);
	m_DevId3.SetLimitText(5);
	m_DevId4.SetLimitText(5);
	m_DevKey.SetLimitText(5);

	m_bNextEnabled = FALSE;

	// Let the dialog manager set the initial focus
	return 1;
}

BOOL 
CDeviceIdPage::OnSetActive()
{
	ATLTRACE("OnSetActive\n");
	if (m_bNextEnabled) {
		SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	} else {
		SetWizardButtons(PSWIZB_BACK);
	}

	return TRUE;
}

VOID 
CDeviceIdPage::UpdateDevId()
{
	::ZeroMemory(
		m_szDevId, 
		sizeof(TCHAR) * (NDAS_DEVICE_STRING_ID_LEN + 1));

	m_DevId1.GetWindowText(
		&m_szDevId[0 * NDAS_DEVICE_STRING_ID_PART_LEN], 
		NDAS_DEVICE_STRING_ID_PART_LEN + 1);

	m_DevId2.GetWindowText(
		&m_szDevId[1 * NDAS_DEVICE_STRING_ID_PART_LEN], 
		NDAS_DEVICE_STRING_ID_PART_LEN + 1);

	m_DevId3.GetWindowText(
		&m_szDevId[2 * NDAS_DEVICE_STRING_ID_PART_LEN], 
		NDAS_DEVICE_STRING_ID_PART_LEN + 1);

	m_DevId4.GetWindowText(
		&m_szDevId[3 * NDAS_DEVICE_STRING_ID_PART_LEN], 
		NDAS_DEVICE_STRING_ID_PART_LEN + 1);

	ATLTRACE(_T("Device Id: %s\n"), m_szDevId);
}

VOID 
CDeviceIdPage::UpdateDevKey()
{
	::ZeroMemory(
		m_szDevKey, 
		sizeof(TCHAR) * (NDAS_DEVICE_WRITE_KEY_LEN + 1));

	m_DevKey.GetWindowText(
		m_szDevKey, 
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	ATLTRACE(_T("Device Key: %s\n"), m_szDevKey);
}

LRESULT 
CDeviceIdPage::DevId_OnChange(
	UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	CEdit wndCurEdit;
	wndCurEdit.Attach(hwndCtrl);

//		TCHAR s[NDAS_DEVICE_STRING_ID_PART_LEN + 2] = {0};
//		wndCurEdit.GetWindowText(s, NDAS_DEVICE_STRING_ID_PART_LEN + 1);
//		::CharUpper(s);
//		wndCurEdit.SetWindowText(s);

	BOOL bEnableNext = FALSE;
	BOOL bNextDlgCtrl = FALSE;

	if (m_DevId1.GetWindowTextLength() == 5 &&
		m_DevId2.GetWindowTextLength() == 5 &&
		m_DevId3.GetWindowTextLength() == 5 &&
		m_DevId4.GetWindowTextLength() == 5 &&
		(m_DevKey.GetWindowTextLength() == 0 || 
		m_DevKey.GetWindowTextLength() == 5))
	{
		UpdateDevId();
		UpdateDevKey();

		LPTSTR lpWriteKey = NULL;
		if (m_DevKey.GetWindowTextLength() != 0) {
			lpWriteKey = m_szDevKey;
		}

		BOOL fSuccess = ::NdasValidateStringIdKeyW(
			m_szDevId, 
			lpWriteKey);

		if (fSuccess) {
			bEnableNext = TRUE;
			bNextDlgCtrl = TRUE;
		}

	} else {

		if (wndCurEdit.GetWindowTextLength() >= 5) {
			bNextDlgCtrl = TRUE;
		}
	}

	if (bEnableNext) {
		if ( m_bNextEnabled ) {
		} else {
			SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
			m_bNextEnabled = TRUE;
		}
	} else {
		if ( m_bNextEnabled ) {
			SetWizardButtons(PSWIZB_BACK);
			m_bNextEnabled = FALSE;
		} else {
		}
	}

	if (bNextDlgCtrl) {
		NextDlgCtrl();
	}

	return 1;
}

// 0 to automatically advance to the next page
// -1 to prevent the page from changing
INT 
CDeviceIdPage::OnWizardNext()
{
	UpdateDevId();
	UpdateDevKey();

	ATLTRACE(_T("Device Id: %s, Key: %s\n"), m_szDevId, m_szDevKey);

	BOOL fSuccess = ::NdasValidateStringIdKeyW(
		m_szDevId, 
		(_T('\0') == m_szDevKey[0]) ? NULL : m_szDevKey);

	if (!fSuccess) {
		return -1;
	}

	::CopyMemory(
		m_pWizData->szDeviceId,
		m_szDevId,
		sizeof(TCHAR) * (NDAS_DEVICE_STRING_ID_LEN + 1));

	::CopyMemory(
		m_pWizData->szDeviceKey,
		m_szDevKey,
		sizeof(TCHAR) * (NDAS_DEVICE_WRITE_KEY_LEN + 1));

	//
	// Registering
	//

	HCURSOR hCursor = ::LoadCursor(NULL, IDC_WAIT);
	HCURSOR hExitingCursor = ::SetCursor(hCursor);

	fSuccess = ::NdasRegisterDevice(
		m_pWizData->szDeviceId,
		(m_pWizData->szDeviceKey[0] == _T('\0')) ? 
			NULL : m_pWizData->szDeviceKey,
		m_pWizData->szDeviceName);

	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_REGISTER_DEVICE_FAILURE);
		::SetCursor(hExitingCursor);
		return -1;
	}

	::SetCursor(hExitingCursor);
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Device Mount Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceMountPage::CDeviceMountPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	m_hPrepareMountThread(NULL),
	m_hDoMountThread(NULL),
	m_pLogDevice(NULL),
	CPropertyPageImpl<CDeviceMountPage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_MOUNT_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_MOUNT_HEADER_SUBTITLE));
	m_state = ST_INIT;
}

CDeviceMountPage::~CDeviceMountPage()
{
	if (NULL != m_pLogDevice) {
		m_pLogDevice->Release();
	}
	if (NULL != m_hPrepareMountThread) {
		::CloseHandle(m_hPrepareMountThread);
	}
	if (NULL != m_hDoMountThread) {
		::CloseHandle(m_hDoMountThread);
	}
}

LRESULT 
CDeviceMountPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndMountRW.Attach(GetDlgItem(IDC_MOUNT_RW));
	m_wndMountRO.Attach(GetDlgItem(IDC_MOUNT_RO));
	m_wndDontMount.Attach(GetDlgItem(IDC_DONT_MOUNT));

	m_wndMountStatus.Attach(GetDlgItem(IDC_MOUNT_STATUS));
	m_wndMountQuestion.Attach(GetDlgItem(IDC_MOUNT_QUESTION));
	m_wndMountWarning.Attach(GetDlgItem(IDC_MOUNT_WARNING));
	// Let the dialog manager set the initial focus
	return 1;
}

UINT
CDeviceMountPage::PrepareMount()
{
	ndas::Device* pDevice = NULL;

	BOOL fSuccess = _pDeviceColl->Update();
	if (!fSuccess) {
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	::Sleep(300);

	pDevice = _pDeviceColl->FindDevice(m_pWizData->szDeviceId);
	if (NULL == pDevice) {
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	::Sleep(300);

	fSuccess = pDevice->Enable();
	if (!fSuccess) {
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	fSuccess = pDevice->UpdateStatus();
	if (!fSuccess) {
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	// wait for connected status in 15 seconds.
	DWORD dwMaxTick = ::GetTickCount() + 15000;
	while (NDAS_DEVICE_STATUS_CONNECTED != pDevice->GetStatus() &&
		::GetTickCount() < dwMaxTick)
	{
		::Sleep(2000);
		fSuccess = pDevice->UpdateStatus();
		if (!fSuccess) {
			pDevice->Release();
			return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
		}
	}

	if (NDAS_DEVICE_STATUS_CONNECTED != pDevice->GetStatus()) {
		// still failed, do not wait more
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	//
	// Connected, the error will be 
	// IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE from here
	//

	fSuccess = pDevice->UpdateInfo();
	if (!fSuccess) {
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	ndas::UnitDevice* pUnitDevice = pDevice->FindUnitDevice(0);
	if (NULL == pUnitDevice) {
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();

	fSuccess = _pLogDevColl->Update();
	if (!fSuccess) {
		pUnitDevice->Release();
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		pUnitDevice->Release();
		pDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	fSuccess = pLogDevice->UpdateStatus();
	if (!fSuccess) {
		pUnitDevice->Release();
		pDevice->Release();
		pLogDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	fSuccess = pLogDevice->UpdateInfo();
	if (!fSuccess) {
		pUnitDevice->Release();
		pDevice->Release();
		pLogDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED != pLogDevice->GetStatus()) {
		pUnitDevice->Release();
		pDevice->Release();
		pLogDevice->Release();
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (GENERIC_WRITE & pLogDevice->GetGrantedAccess()) {
		m_fMountRW = TRUE;
	} else {
		m_fMountRW = FALSE;
	}

	// preparing done

	pUnitDevice->Release();
	pDevice->Release();

	// pLogDevice is retained for next step
	m_pLogDevice = pLogDevice;
	ReportWorkDone(ST_USERCONFIRM);

	return 0;
}

UINT
CDeviceMountPage::DoMount()
{
	return 0;
}

DWORD
CDeviceMountPage::pPrepareMountProc()
{
	//
	// Do as much as possible and if all succeeded, go to mount option
	//

	UINT nIDCompletion = PrepareMount(); // returns 0 on success
	if (0 != nIDCompletion) {
		m_pWizData->ppspComplete->SetCompletionMessage(nIDCompletion);
		m_pWizData->ppsh->PressButton(PSBTN_NEXT);
	}

	return 0;
}

DWORD
CDeviceMountPage::pDoMountProc()
{
	return 0;
}

DWORD WINAPI 
CDeviceMountPage::spPrepareMountProc(LPVOID lpParam)
{
	CDeviceMountPage* pThis = reinterpret_cast<CDeviceMountPage*>(lpParam);
	DWORD ret = pThis->pPrepareMountProc();
	::ExitThread(ret);
	return ret;
}

DWORD WINAPI 
CDeviceMountPage::spDoMountProc(LPVOID lpParam)
{
	CDeviceMountPage* pThis = reinterpret_cast<CDeviceMountPage*>(lpParam);
	DWORD ret = pThis->pDoMountProc();
	::ExitThread(ret);
	return ret;
}

VOID
CDeviceMountPage::ReportWorkDone(State newState)
{
	m_state = newState;
	m_pWizData->ppsh->SetActivePage(3);
}

VOID
CDeviceMountPage::pShowMountOptions(BOOL fShow)
{
	INT iShow = fShow ? SW_SHOW : SW_HIDE;
	m_wndMountRW.ShowWindow(fShow);
	m_wndMountRO.ShowWindow(fShow);
	m_wndDontMount.ShowWindow(fShow);
	m_wndMountQuestion.ShowWindow(fShow);
	m_wndMountWarning.ShowWindow(fShow);		
}

BOOL 
CDeviceMountPage::OnSetActive()
{
	BOOL fSuccess = FALSE;

	if (ST_INIT == m_state) {

		SetWizardButtons(0);

		HCURSOR hWaitCursor = ::LoadCursor(NULL,IDC_WAIT);
		m_hOldCursor = ::SetCursor(hWaitCursor);

		pShowMountOptions(FALSE);

		CString strStatus;
		fSuccess = strStatus.LoadString(IDS_DEVWIZ_MOUNT_STATUS_WAIT);
		ATLASSERT(fSuccess);
		m_wndMountStatus.SetWindowText(strStatus);

		m_hPrepareMountThread = ::CreateThread(
			NULL, 
			0, 
			spPrepareMountProc,
			this,
			0,
			NULL);

		m_state = ST_PREPARING;

	} else if (ST_MOUNTING == m_state) {

	} else if (ST_PREPARING == m_state) {

	} else if (ST_USERCONFIRM == m_state) {

		::SetCursor(m_hOldCursor);

		CString strStatus;
		fSuccess = strStatus.LoadString(IDS_DEVWIZ_MOUNT_STATUS_NORMAL);
		ATLASSERT(fSuccess);
		m_wndMountStatus.SetWindowText(strStatus);

		pShowMountOptions(TRUE);

//		m_wndMountRW.EnableWindow(m_fMountRW);
		m_wndMountRW.EnableWindow(m_pLogDevice->GetGrantedAccess()
			& (GENERIC_WRITE | GENERIC_READ) && (NULL != m_pWizData->szDeviceKey[0]));
		m_wndMountRO.EnableWindow(m_pLogDevice->GetGrantedAccess()
			& GENERIC_READ);
		m_wndDontMount.SetCheck(BST_CHECKED);

		SetWizardButtons(PSWIZB_NEXT);

	} else { 

		ATLASSERT(FALSE); 
	}

	return 0;
}

BOOL
CDeviceMountPage::OnQueryCancel()
{
	// Returns TRUE to prevent the cancel operation, 
	// or FALSE to allow it.
	if (ST_USERCONFIRM == m_state) {
		return FALSE;
	}
	return TRUE;
}

INT
CDeviceMountPage::OnWizardNext()
{
	if (ST_USERCONFIRM == m_state) {

		if (BST_CHECKED == m_wndMountRW.GetCheck() ||
			BST_CHECKED == m_wndMountRO.GetCheck())
		{
			ATLASSERT(NULL != m_pLogDevice);

			m_fMountRW = 
				(BST_CHECKED == m_wndMountRW.GetCheck()) ? TRUE : FALSE;

			BOOL fSuccess = m_pLogDevice->PlugIn(m_fMountRW);
			if (!fSuccess) {
				ShowErrorMessageBox( m_fMountRW ? 
					IDS_ERROR_MOUNT_DEVICE_RW :
					IDS_ERROR_MOUNT_DEVICE_RO);
			} else {
				if (!pCheckNoPendingInstall(NULL)) {
 					CWaitDialog(
						IDS_WAIT_MOUNT, 
						IDS_MAIN_TITLE, 
						1000, 
						pCheckNoPendingInstall, 
						NULL).DoModal();
				}
			}
		}

	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Completion Page
//
//////////////////////////////////////////////////////////////////////////

CCompletionPage::CCompletionPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CCompletionPage>(IDS_DRZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
	m_uIDCompletionMsg = IDS_REGWIZ_COMPLETE_NORMAL;
}

LRESULT 
CCompletionPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());

	m_wndCompletionMsg.Attach(GetDlgItem(IDC_COMPLETE_MESSAGE));

	return 1;
}

BOOL 
CCompletionPage::OnSetActive()
{
	CString strCompletionMsg;
	BOOL fSuccess = strCompletionMsg.LoadString(m_uIDCompletionMsg);
	ATLASSERT(fSuccess);
	m_wndCompletionMsg.SetWindowText(strCompletionMsg);

	SetWizardButtons(PSWIZB_FINISH);
	return TRUE;
}

VOID
CCompletionPage::SetCompletionMessage(UINT nID)
{
	m_uIDCompletionMsg = nID;
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
//////////////////////////////////////////////////////////////////////////

static 
HFONT 
pGetTitleFont()
{
	BOOL fSuccess = FALSE;
	static HFONT hTitleFont = NULL;
	if (NULL != hTitleFont) {
		return hTitleFont;
	}

	CString strFontName;
	CString strFontSize;
	fSuccess = strFontName.LoadString(IDS_BIG_BOLD_FONT_NAME);
	ATLASSERT(fSuccess);
	fSuccess = strFontSize.LoadString(IDS_BIG_BOLD_FONT_SIZE);
	ATLASSERT(fSuccess);

	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	fSuccess = ::SystemParametersInfo(
		SPI_GETNONCLIENTMETRICS, 
		sizeof(NONCLIENTMETRICS), 
		&ncm, 
		0);
	ATLASSERT(fSuccess);

	LOGFONT TitleLogFont = ncm.lfMessageFont;
	TitleLogFont.lfWeight = FW_BOLD;

	HRESULT hr = ::StringCchCopy(TitleLogFont.lfFaceName,
		(sizeof(TitleLogFont.lfFaceName)/sizeof(TitleLogFont.lfFaceName[0])),
		strFontName);

	ATLASSERT(SUCCEEDED(hr));

	INT TitleFontSize = ::StrToInt(strFontSize);
	if (TitleFontSize == 0) {
		TitleFontSize = 12;
	}

	HDC hdc = ::GetDC(NULL);
	TitleLogFont.lfHeight = 0 - 
		::GetDeviceCaps(hdc,LOGPIXELSY) * TitleFontSize / 72;

	hTitleFont = ::CreateFontIndirect(&TitleLogFont);
	::ReleaseDC(NULL, hdc);

	return hTitleFont;
}

} // namespace ndrwiz

