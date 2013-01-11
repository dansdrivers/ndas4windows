#include "stdafx.h"
#include <ndas/ndasnif.h>
#include "resource.h"
#include "ndasmgmt.h"
#include "devregwiz.h"
#include "appconf.h"
#include "waitdlg.h"
#include "apperrdlg.h"
#include "exportdlg.h" // NdasExportGetSaveFileName

namespace
{
	HFONT pGetTitleFont();
}

//////////////////////////////////////////////////////////////////////////
//
// Wizard Property Sheet
//
//////////////////////////////////////////////////////////////////////////

static const INT WIZARD_COMPLETION_INDEX = 4;

CRegWizard::CRegWizard(HWND hWndParent) :
	m_fCentered(FALSE),
	m_pgIntro(hWndParent, &m_wizData),
	m_pgDeviceId(hWndParent, &m_wizData),
	m_pgDeviceName(hWndParent, &m_wizData),
	m_pgMount(hWndParent, &m_wizData),
	m_pgComplete(hWndParent, &m_wizData),
	CPropertySheetImpl<CRegWizard>(IDS_DRZ_TITLE, 0, hWndParent)
{
	SetWizardMode();
	m_psh.dwFlags |= PSH_WIZARD97 | PSH_USEPAGELANG;
	SetWatermark(MAKEINTRESOURCE(IDB_WATERMARK256));
	SetHeader(MAKEINTRESOURCE(IDB_BANNER256));

	//
	// We don't have to stretch our watermark bitmap as
	// we already have a larger watermark to cover the higher DPI monitor
	//
	// StretchWatermark(true);
	//
	AddPage(m_pgIntro);
	AddPage(m_pgDeviceName);
	AddPage(m_pgDeviceId);
	AddPage(m_pgMount);
	AddPage(m_pgComplete);

	::ZeroMemory(
		&m_wizData, 
		sizeof(REG_WIZARD_DATA));
	m_wizData.ppsh = this;
	m_wizData.ppspComplete = &m_pgComplete;
}

void 
CRegWizard::OnShowWindow(BOOL fShow, UINT nStatus)
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

CRegWizIntroPage::CRegWizIntroPage(
	HWND hWndParent, 
	PREG_WIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CRegWizIntroPage>(IDS_DRZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
}


LRESULT 
CRegWizIntroPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());
	return 0;
}


//
// Non-zero if the page was successfully set active;
// otherwise 0
//
BOOL 
CRegWizIntroPage::OnSetActive()
{
	CStatic stIntroCtl;
	stIntroCtl.Attach(GetDlgItem(IDC_INTRO_1));
	CString strIntro1;
	strIntro1.LoadString(IDS_DRZ_INTRO_1);
	stIntroCtl.SetWindowText(strIntro1);

	SetWizardButtons(PSWIZB_NEXT);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Device Name Page
//
//////////////////////////////////////////////////////////////////////////

CRegWizDeviceNamePage::CRegWizDeviceNamePage(
	HWND hWndParent, 
	PREG_WIZARD_DATA pData) :
	m_pWizData(pData),
	CPropertyPageImpl<CRegWizDeviceNamePage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_NAME_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_NAME_HEADER_SUBTITLE));
}

LRESULT 
CRegWizDeviceNamePage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndDevName.Attach(GetDlgItem(IDC_DEV_NAME));
	m_wndDevName.SetLimitText(30);
	m_bValidName = FALSE;

	DWORD nDeviceCount = ndas::GetDevices().size();

	CString strNewDeviceName;

	for (DWORD i = 1; ; ++i)
	{
		strNewDeviceName.FormatMessage(IDS_NEW_DEVICE_NAME_TEMPLATE, nDeviceCount + i);
		ndas::DevicePtr pExistingDevice;
		if (ndas::FindDeviceByName(pExistingDevice, strNewDeviceName))
		{
			continue;
		}
		break;
	}
	m_wndDevName.SetWindowText(strNewDeviceName);

	// Let the dialog manager set the initial focus
	return 1;
}

BOOL
CRegWizDeviceNamePage::OnSetActive()
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
CRegWizDeviceNamePage::DevName_OnChange(
	UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	(void) ValidateDeviceName();
	return 0;
}

BOOL 
CRegWizDeviceNamePage::ValidateDeviceName()
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
CRegWizDeviceNamePage::OnWizardNext()
{
	BOOL fSuccess = m_wndDevName.GetWindowText(
		m_pWizData->szDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1);

	ATLASSERT(fSuccess);

	ndas::DevicePtr pExistingDevice;
	if (ndas::FindDeviceByName(pExistingDevice, m_pWizData->szDeviceName))
	{
		AtlMessageBox(
			m_hWnd, 
			IDS_ERROR_DUPLICATE_NAME, 
			IDS_MAIN_TITLE, 
			MB_OK | MB_ICONWARNING);

		m_wndDevName.SetFocus();
		m_wndDevName.SetSel(0, -1);
		return -1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Device ID Page
//
//////////////////////////////////////////////////////////////////////////

CRegWizDeviceIdPage::CRegWizDeviceIdPage(
	HWND hWndParent, 
	PREG_WIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CRegWizDeviceIdPage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_ID_HEADER_TITLE));
	
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_DEVICE_ID_HEADER_SUBTITLE));
}

LRESULT 
CRegWizDeviceIdPage::OnInitDialog(HWND hWnd, LPARAM lParam)
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
CRegWizDeviceIdPage::OnSetActive()
{
	ATLTRACE("OnSetActive\n");
	if (m_bNextEnabled) {
		SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	} else {
		SetWizardButtons(PSWIZB_BACK);
	}

	return TRUE;
}

void 
CRegWizDeviceIdPage::UpdateDevId()
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

	ATLTRACE("Device Id: %ws\n", m_szDevId);
}

void 
CRegWizDeviceIdPage::UpdateDevKey()
{
	::ZeroMemory(
		m_szDevKey, 
		sizeof(TCHAR) * (NDAS_DEVICE_WRITE_KEY_LEN + 1));

	m_DevKey.GetWindowText(
		m_szDevKey, 
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	ATLTRACE("Device Key: %ws\n", m_szDevKey);
}

LRESULT 
CRegWizDeviceIdPage::DevId_OnChange(
	UINT uCode, 
	int nCtrlID, 
	HWND hwndCtrl)
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

		if (fSuccess) 
		{
			bEnableNext = TRUE;
			bNextDlgCtrl = TRUE;
		}
		else
		{
			CString strText, strTitle;
			strTitle.LoadString(IDS_INVALID_DEVICE_ID_TOOLTIP_TITLE);
			strText.LoadString(IDS_INVALID_DEVICE_ID_TOOLTIP_TEXT);
			EDITBALLOONTIP tip;
			tip.cbStruct = sizeof(tip);
			tip.pszTitle = strTitle;
			tip.pszText = strText;
			tip.ttiIcon = TTI_WARNING;
			wndCurEdit.ShowBalloonTip(&tip);
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
CRegWizDeviceIdPage::OnWizardNext()
{
	UpdateDevId();
	UpdateDevKey();

	ATLTRACE("Device Id: %ws, Key: %ws\n", m_szDevId, m_szDevKey);

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
		m_pWizData->szDeviceName,
		NDAS_DEVICE_REG_FLAG_NONE);

	if (!fSuccess) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_REGISTER_DEVICE_FAILURE);
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

CRegWizDeviceMountPage::CRegWizDeviceMountPage(
	HWND hWndParent, 
	PREG_WIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	m_hPrepareMountThread(NULL),
	m_hDoMountThread(NULL),
	CPropertyPageImpl<CRegWizDeviceMountPage>(IDS_DRZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_DRZ_MOUNT_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_DRZ_MOUNT_HEADER_SUBTITLE));
	m_state = ST_INIT;
}

CRegWizDeviceMountPage::~CRegWizDeviceMountPage()
{
	if (NULL != m_hPrepareMountThread) {
		::CloseHandle(m_hPrepareMountThread);
	}
	if (NULL != m_hDoMountThread) {
		::CloseHandle(m_hDoMountThread);
	}
}

LRESULT 
CRegWizDeviceMountPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndMountRW.Attach(GetDlgItem(IDC_MOUNT_RW));
	m_wndMountRO.Attach(GetDlgItem(IDC_MOUNT_RO));
	m_wndDontMount.Attach(GetDlgItem(IDC_DONT_MOUNT));

	m_wndMountStatus.Attach(GetDlgItem(IDC_MOUNT_STATUS));
	m_wndMountQuestion.Attach(GetDlgItem(IDC_MOUNT_QUESTION));
	m_wndMountWarning.Attach(GetDlgItem(IDC_MOUNT_WARNING));

	m_aniWait.Attach(GetDlgItem(IDC_ANIMATE));
	m_aniWait.ModifyStyle(0,ACS_TRANSPARENT);

	CRect rect; 
	GetClientRect(&rect);

	Animate_OpenEx(
		m_aniWait.m_hWnd, 
		ATL::_AtlBaseModule.GetResourceInstance(),
		MAKEINTRESOURCE(IDA_FINDHOSTS));

	// Let the dialog manager set the initial focus
	return TRUE;
}

UINT
CRegWizDeviceMountPage::PrepareMount()
{
	if (!ndas::UpdateDeviceList()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	::Sleep(300);

	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceByNdasId(pDevice, m_pWizData->szDeviceId))
	{
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	::Sleep(300);

	if (!pDevice->Enable()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	if (!pDevice->UpdateStatus()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	m_aniWait.ShowWindow(SW_SHOW);
	m_aniWait.Play(0,-1,-1);

	// wait for connected status in 15 seconds.
	DWORD dwMaxTick = ::GetTickCount() + 15000;
	while (NDAS_DEVICE_STATUS_CONNECTED != pDevice->GetStatus() &&
		::GetTickCount() < dwMaxTick)
	{
		::Sleep(500);
		if (!pDevice->UpdateStatus()) 
		{
			return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
		}
	}

	m_aniWait.Stop();
	m_aniWait.ShowWindow(SW_HIDE);

	if (NDAS_DEVICE_STATUS_CONNECTED != pDevice->GetStatus()) 
	{
		// still failed, do not wait more
		return IDS_REGWIZ_COMPLETE_NOT_CONNECTED;
	}

	//
	// Connected, the error will be 
	// IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE from here
	//

	if (!pDevice->UpdateInfo()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	ndas::UnitDevicePtr pUnitDevice;
	if (!pDevice->FindUnitDevice(pUnitDevice, 0)) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	NDAS_LOGICALDEVICE_ID logDeviceId = pUnitDevice->GetLogicalDeviceId();

	if (!ndas::UpdateLogicalDeviceList()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId)) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (!pLogDevice->UpdateStatus()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (!pLogDevice->UpdateInfo()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED != pLogDevice->GetStatus()) 
	{
		return IDS_REGWIZ_COMPLETE_NOT_MOUNTABLE;
	}

	if (GENERIC_WRITE & pLogDevice->GetGrantedAccess()) 
	{
		m_fMountRW = TRUE;
	}
	else 
	{
		m_fMountRW = FALSE;
	}

	// preparing done

	// pLogDevice is retained for next step
	m_pLogDevice = pLogDevice;
	ReportWorkDone(ST_USERCONFIRM);

	return 0;
}

DWORD
CRegWizDeviceMountPage::PrepareMountThreadStart()
{
	//
	// Do as much as possible and if all succeeded, go to mount option
	//

	UINT nIDCompletion = PrepareMount(); // returns 0 on success
	if (0 != nIDCompletion) 
	{
		m_pWizData->ppspComplete->SetCompletionMessage(nIDCompletion);
		m_pWizData->ppsh->PressButton(PSBTN_NEXT);
	}

	return 0;
}

void
CRegWizDeviceMountPage::ReportWorkDone(State newState)
{
	m_state = newState;
	m_pWizData->ppsh->SetActivePage(3);
}

void
CRegWizDeviceMountPage::pShowMountOptions(BOOL fShow)
{
	INT iShow = fShow ? SW_SHOW : SW_HIDE;
	m_wndMountRW.ShowWindow(fShow);
	m_wndMountRO.ShowWindow(fShow);
	m_wndDontMount.ShowWindow(fShow);
	m_wndMountQuestion.ShowWindow(fShow);
	m_wndMountWarning.ShowWindow(fShow);		
}

BOOL 
CRegWizDeviceMountPage::OnSetActive()
{
	BOOL fSuccess = FALSE;

	if (ST_INIT == m_state) 
	{

		SetWizardButtons(0);

		m_hOldCursor = ::SetCursor(AtlLoadSysCursor(IDC_WAIT));

		pShowMountOptions(FALSE);

		CString strStatus = MAKEINTRESOURCE(IDS_DEVWIZ_MOUNT_STATUS_WAIT);
		m_wndMountStatus.SetWindowText(strStatus);

		m_hPrepareMountThread = ::CreateThread(
			NULL, 
			0, 
			PrepareMountThreadProc,
			this,
			0,
			NULL);

		m_state = ST_PREPARING;

	}
	else if (ST_MOUNTING == m_state) 
	{

	}
	else if (ST_PREPARING == m_state) 
	{

	}
	else if (ST_USERCONFIRM == m_state) 
	{
		::SetCursor(m_hOldCursor);

		CString strStatus = MAKEINTRESOURCE(IDS_DEVWIZ_MOUNT_STATUS_NORMAL);

		m_wndMountStatus.SetWindowText(strStatus);

		pShowMountOptions(TRUE);

		ACCESS_MASK granted = m_pLogDevice->GetGrantedAccess();

		BOOL fAllowRW = 
			(granted & (GENERIC_WRITE | GENERIC_READ)) && 
			(NULL != m_pWizData->szDeviceKey[0]);

		BOOL fAllowRO = granted & GENERIC_READ;

		m_wndMountRW.EnableWindow(fAllowRW);
		m_wndMountRO.EnableWindow(fAllowRO);
		m_wndDontMount.SetCheck(BST_CHECKED);

		SetWizardButtons(PSWIZB_NEXT);

	}
	else 
	{ 

		ATLASSERT(FALSE); 
	}

	return 0;
}

BOOL
CRegWizDeviceMountPage::OnQueryCancel()
{
	// Returns TRUE to prevent the cancel operation, 
	// or FALSE to allow it.
	if (ST_USERCONFIRM == m_state) 
	{
		return FALSE;
	}
	return TRUE;
}

INT
CRegWizDeviceMountPage::OnWizardNext()
{
	if (ST_USERCONFIRM == m_state) 
	{
		if (BST_CHECKED == m_wndMountRW.GetCheck() ||
			BST_CHECKED == m_wndMountRO.GetCheck())
		{
			ATLASSERT(NULL != m_pLogDevice.get());

			m_fMountRW = 
				(BST_CHECKED == m_wndMountRW.GetCheck()) ? TRUE : FALSE;

			BOOL fSuccess = m_pLogDevice->PlugIn(m_fMountRW);
			if (!fSuccess) 
			{
				ErrorMessageBox(m_hWnd, 
					m_fMountRW ? 
					IDS_ERROR_MOUNT_DEVICE_RW : 
					IDS_ERROR_MOUNT_DEVICE_RO);
			}
			else 
			{
				// wait for the completion of the mount process
				CWaitMountDialog().DoModal(m_hWnd, m_pLogDevice);
			}
		}

	}
	return 0;
}

void
CRegWizDeviceMountPage::OnReset()
{
	if (m_aniWait.m_hWnd)
	{
		m_aniWait.Close();
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Completion Page
//
//////////////////////////////////////////////////////////////////////////

CRegWizCompletionPage::CRegWizCompletionPage(
	HWND hWndParent, 
	PREG_WIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CRegWizCompletionPage>(IDS_DRZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
	m_uIDCompletionMsg = IDS_REGWIZ_COMPLETE_NORMAL;
}

LRESULT 
CRegWizCompletionPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());

	m_wndExport.SetHyperLinkExtendedStyle(HLINK_COMMANDBUTTON);
	m_wndExport.SubclassWindow(GetDlgItem(IDC_EXPORT));

	m_wndCompletionMsg.Attach(GetDlgItem(IDC_COMPLETE_MESSAGE));

	return 1;
}

BOOL 
CRegWizCompletionPage::OnSetActive()
{
	CString strCompletionMsg;
	BOOL fSuccess = strCompletionMsg.LoadString(m_uIDCompletionMsg);
	ATLASSERT(fSuccess);
	m_wndCompletionMsg.SetWindowText(strCompletionMsg);

	SetWizardButtons(PSWIZB_FINISH);
	return TRUE;
}

void 
CRegWizCompletionPage::OnCmdExport(UINT, int, HWND)
{
	TCHAR FilePath[MAX_PATH];
	ATLVERIFY(SUCCEEDED(
		::StringCchCopy(FilePath, MAX_PATH, m_pWizData->szDeviceName)));
	pNormalizePath(FilePath, MAX_PATH);
	BOOL fSuccess = pExportGetSaveFileName(m_hWnd, FilePath, MAX_PATH);
	if (!fSuccess)
	{
		if (ERROR_SUCCESS != ::GetLastError())
		{
			ErrorMessageBox(m_hWnd, IDS_ERROR_EXPORT);
		}
		return;
	}

	NDAS_NIF_V1_ENTRY entry = {0};
	entry.Name = m_pWizData->szDeviceName;
	entry.DeviceId = m_pWizData->szDeviceId;
	entry.WriteKey = m_pWizData->szDeviceKey;

	HRESULT hr = ::NdasNifExport(FilePath, 1, &entry);
	if (FAILED(hr))
	{
		ErrorMessageBox(
			m_hWnd,
			IDS_ERROR_EXPORT,
			IDS_ERROR_TITLE,
			ndasmgmt::CurrentUILangID,
			hr);
	}
}

void
CRegWizCompletionPage::SetCompletionMessage(UINT nID)
{
	m_uIDCompletionMsg = nID;
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
//////////////////////////////////////////////////////////////////////////

namespace {

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
} // namespace (local)

//int 
//pMessageLoop( 
//    HANDLE* lphObjects,  // handles that need to be waited on 
//    int     cObjects     // number of handles to wait on 
//  )
//{ 
//    // The message loop lasts until we get a WM_QUIT message,
//    // upon which we shall return from the function.
//    while (TRUE)
//    {
//        DWORD result ; 
//        MSG msg ; 
//
//        // Read all of the messages in this next loop, 
//        // removing each message as we read it.
//		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
//        { 
//            // If it is a quit message, exit.
//            if (msg.message == WM_QUIT)
//                return 1; 
//            // Otherwise, dispatch the message.
//            DispatchMessage(&msg); 
//        }
//
//        // Wait for any message sent or posted to this queue 
//        // or for one of the passed handles be set to signaled.
//        result = MsgWaitForMultipleObjects(cObjects, lphObjects, 
//                 FALSE, INFINITE, QS_ALLINPUT); 
//
//        // The result tells us the type of event we have.
//        if (result == (WAIT_OBJECT_0 + cObjects))
//        {
//            // New messages have arrived. 
//            // Continue to the top of the always while loop to 
//            // dispatch them and resume waiting.
//            continue;
//        } 
//        else 
//        { 
//            // One of the handles became signaled. 
//            DoStuff(result - WAIT_OBJECT_0) ; 
//        }
//    }
//}
//
