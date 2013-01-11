#include "stdafx.h"
#include "apperrdlg.h"

CAppErrorDlg::CAppErrorDlg(
	LPCTSTR szMessage, 
	LPCTSTR szDescription,
	LPCTSTR szTitle) :
	m_strDescription(szDescription),
	m_strMessage(szMessage)
{
	if (NULL == szTitle) {
		m_strTitle.LoadString(IDS_ERROR_TITLE);	
	} else {
		m_strTitle = szTitle;
	}
}

LRESULT
CAppErrorDlg::OnInitDialog(HWND hWndFocus, LPARAM lParam)
{
	m_wndIcon.Attach(GetDlgItem(IDC_ERROR_ICON));
	m_wndMessage.Attach(GetDlgItem(IDC_ERROR_MESSAGE));
	m_wndDescription.Attach(GetDlgItem(IDC_ERROR_DESCRIPTION));

	HICON hErrorIcon = ::LoadIcon(NULL, IDI_ERROR);
	m_wndIcon.SetIcon(hErrorIcon);

	m_wndMessage.SetWindowText(m_strMessage);
	m_wndDescription.SetWindowText(m_strDescription);

	return 1;
};

VOID 
CAppErrorDlg::OnOK(UINT wNotifyCode, INT wID, HWND hWndCtl)
{
	UNREFERENCED_PARAMETER(wNotifyCode);
	UNREFERENCED_PARAMETER(hWndCtl);
	EndDialog(wID);
}


INT_PTR ShowErrorMessageBox(
	UINT uMessageID, UINT uTitleID, HWND hWnd, DWORD dwError)
{
	WTL::CString strMessage, strTitle;

	BOOL fSuccess = strMessage.LoadString(uMessageID);
	ATLASSERT(fSuccess);

	if (0 != uTitleID) {

		fSuccess = strTitle.LoadString(uTitleID);
		ATLASSERT(fSuccess);

		return ShowErrorMessageBox(strMessage,strTitle,hWnd,dwError);

	} else {

		return ShowErrorMessageBox(strMessage,NULL,hWnd,dwError);

	}
}


VOID
GetDescription(WTL::CString &strDescription, DWORD dwError)
{
	BOOL fSuccess = FALSE;
	if (dwError & APPLICATION_ERROR_MASK) {

		HMODULE hModule = ::LoadLibraryEx(
			_T("ndasmsg.dll"), 
			NULL, 
			LOAD_LIBRARY_AS_DATAFILE);

		LPTSTR lpszErrorMessage = NULL;

		if (NULL != hModule) {

			INT iChars = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}

		//
		// Facility: NDAS 0x%1!04X!\r\n
		// Error Code: %2!u! (0x%2!04X!)\r\n
		// %3!s!			
		//

		fSuccess = strDescription.FormatMessage(
			IDS_ERROR_NDAS_DESCRIPTION_FMT,
			(dwError & 0x0FFF000),
			(dwError & 0xFFFF),
			(lpszErrorMessage) ? lpszErrorMessage : _T(""));
		ATLASSERT(fSuccess);

		if (NULL != lpszErrorMessage) {
			::LocalFree(lpszErrorMessage);
		}

		if (NULL != hModule) {
			::FreeLibrary(hModule);
		}


	} else {

		LPTSTR lpszErrorMessage = NULL;
		INT iChars = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, 
			dwError, 
			0, 
			(LPTSTR) &lpszErrorMessage, 
			0, 
			NULL);

		//
		// Error Code: %1!u! (0x%1!04X!)
		// %2!s!
		//
		fSuccess = strDescription.FormatMessage(
			IDS_ERROR_SYSTEM_DESCRIPTION_FMT,
			dwError,
			(lpszErrorMessage) ? lpszErrorMessage : _T(""));
		ATLASSERT(fSuccess);

		if (NULL != lpszErrorMessage) {
			::LocalFree(lpszErrorMessage);
		}
	}
}

INT_PTR 
ShowErrorMessageBox(
	LPCTSTR szMessage, LPCTSTR szTitle, HWND hWnd, DWORD dwError)
{
	WTL::CString strDescription;

	GetDescription(strDescription, dwError);

	CAppErrorDlg dlg(szMessage,strDescription,szTitle);
	INT_PTR iDlgResult = dlg.DoModal(hWnd);
	return iDlgResult;
}

