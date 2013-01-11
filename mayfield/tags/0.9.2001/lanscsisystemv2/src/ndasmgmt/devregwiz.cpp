#include "stdafx.h"
#include "resource.h"
#include "ndasmgmt.h"
#include "devregwiz.h"

class CDeviceIdEdit :
	public CWindowImpl<CDeviceIdEdit >
{
public:
	DECLARE_WND_SUPERCLASS(0, _T("EDIT"))

	BEGIN_MSG_MAP_EX(CDeviceIdEdit)
		MSG_WM_CHAR(OnChar)
	END_MSG_MAP()

	void OnChar(TCHAR nChar, UINT nRepCnt, UINT nFlags)
	{
//		SetMsgHandled(TRUE);
	}
};

namespace ndrwiz {

	static HFONT GetTitleFont();

	CWizard::CWizard(HWND hWndParent) :
		m_pgIntro(hWndParent, &m_wizData),
		m_pgDeviceId(hWndParent, &m_wizData),
		m_pgDeviceName(hWndParent, &m_wizData),
		m_pgMount(hWndParent, &m_wizData),
		m_pgComplete(hWndParent, &m_wizData),
		CPropertySheetImpl<CWizard>(IDS_DRZ_TITLE, 0, hWndParent)
	{
		SetWizardMode();
		m_psh.dwFlags |= PSH_WIZARD97;
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
	}

	CIntroPage::CIntroPage(HWND hWndParent, PWIZARD_DATA pData) :
		m_pWizData(pData),
		m_hWndParent(hWndParent),
		CPropertyPageImpl<CIntroPage>(IDS_DRZ_TITLE)
	{
		m_psp.dwFlags |= PSP_HIDEHEADER;
	}


	LRESULT CIntroPage::OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		CContainedWindow wndTitle;
		wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
		wndTitle.SetFont(GetTitleFont());

		return 0;
	}

	int CIntroPage::OnSetActive()
	{
		CStatic stIntroCtl;
		stIntroCtl.Attach(GetDlgItem(IDC_INTRO_1));
		WTL::CString strIntro1;
		strIntro1.LoadString(IDS_DRZ_INTRO_1);
		stIntroCtl.SetWindowText(strIntro1);

		SetWizardButtons(PSWIZB_NEXT);
		return 0;
	}

	CCompletionPage::CCompletionPage(HWND hWndParent, PWIZARD_DATA pData) :
		m_pWizData(pData),
		m_hWndParent(hWndParent),
		CPropertyPageImpl<CCompletionPage>(IDS_DRZ_TITLE)
	{
		m_psp.dwFlags |= PSP_HIDEHEADER;
	}

	LRESULT CCompletionPage::OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		CContainedWindow wndTitle;
		wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
		wndTitle.SetFont(GetTitleFont());

		return 0;
	}

	int CCompletionPage::OnSetActive()
	{
		SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
		return 0;
	}

	CDeviceNamePage::CDeviceNamePage(HWND hWndParent, PWIZARD_DATA pData) :
		CPropertyPageImpl<CDeviceNamePage>(IDS_DRZ_TITLE)
	{
		SetHeaderTitle(MAKEINTRESOURCE(
			IDS_DRZ_DEVICE_NAME_HEADER_TITLE));
		SetHeaderSubTitle(MAKEINTRESOURCE(
			IDS_DRZ_DEVICE_NAME_HEADER_SUBTITLE));
	}

	LRESULT CDeviceNamePage::OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		m_DevName.Attach(GetDlgItem(IDC_DEV_NAME));
		m_DevName.SetLimitText(31);

		// Let the dialog manager set the initial focus
		return 1;
	}

	int CDeviceNamePage::OnSetActive()
	{
		SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
		return 0;
	}

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

	LRESULT CDeviceIdPage::OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		ATLTRACE("OnInitDialog\n");

		m_DevId1.Attach(GetDlgItem(IDC_DEV_ID_1));
		m_DevId2.Attach(GetDlgItem(IDC_DEV_ID_2));
		m_DevId3.Attach(GetDlgItem(IDC_DEV_ID_3));
		m_DevId4.Attach(GetDlgItem(IDC_DEV_ID_4));
		m_DevKey.Attach(GetDlgItem(IDC_DEV_KEY));

		m_DevId1.SetLimitText(5);
		m_DevId2.SetLimitText(5);
		m_DevId3.SetLimitText(5);
		m_DevId4.SetLimitText(5);
		m_DevKey.SetLimitText(5);

		m_bNextEnabled = FALSE;

		// Let the dialog manager set the initial focus
		return 1;
	}

	int CDeviceIdPage::OnSetActive()
	{
		ATLTRACE("OnSetActive\n");
		if (m_bNextEnabled) {
			SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
		} else {
			SetWizardButtons(PSWIZB_BACK);
		}

		return 0;
	}

	int CDeviceIdPage::OnWizardNext()
	{
		ATLTRACE(_T("Device Id: %s, Key: %s\n"), m_szDevId, m_szDevKey);

		::CopyMemory(
			m_pWizData->szDeviceId,
			m_szDevId,
			sizeof(TCHAR) * (NDAS_DEVICE_STRING_ID_LEN + 1));

		::CopyMemory(
			m_pWizData->szDeviceKey,
			m_szDevKey,
			sizeof(TCHAR) * (NDAS_DEVICE_WRITE_KEY_LEN + 1));

		return 0;
	}

	void CDeviceIdPage::UpdateDevId()
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

	void CDeviceIdPage::UpdateDevKey()
	{
		::ZeroMemory(
			m_szDevKey, 
			sizeof(TCHAR) * (NDAS_DEVICE_WRITE_KEY_LEN + 1));

		m_DevKey.GetWindowText(
			m_szDevKey, 
			NDAS_DEVICE_WRITE_KEY_LEN + 1);

		ATLTRACE(_T("Device Key: %s\n"), m_szDevKey);
	}

	LRESULT CDeviceIdPage::DevId_OnChange(
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

	CDeviceMountPage::CDeviceMountPage(HWND hWndParent, PWIZARD_DATA pData) :
		m_pWizData(pData),
		m_hWndParent(hWndParent),
		CPropertyPageImpl<CDeviceMountPage>(IDS_DRZ_TITLE)
	{
		SetHeaderTitle(MAKEINTRESOURCE(
			IDS_DRZ_MOUNT_HEADER_TITLE));
		SetHeaderSubTitle(MAKEINTRESOURCE(
			IDS_DRZ_MOUNT_HEADER_SUBTITLE));

	}

	LRESULT CDeviceMountPage::OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		// Let the dialog manager set the initial focus
		return 1;
	}

	int CDeviceMountPage::OnSetActive()
	{
		SetWizardButtons(PSWIZB_NEXT);

		CButton but;
		but.Attach(GetDlgItem(IDC_DONT_MOUNT));
		but.SetCheck(BST_CHECKED);
		return 0;
	}

	static HFONT GetTitleFont()
	{
		BOOL fSuccess = FALSE;
		static HFONT hTitleFont = NULL;
		if (NULL != hTitleFont) {
			return hTitleFont;
		}

		WTL::CString strFontName;
		WTL::CString strFontSize;
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

}
