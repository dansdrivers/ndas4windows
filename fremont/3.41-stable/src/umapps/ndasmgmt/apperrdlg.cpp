#include "stdafx.h"
#include "apperrdlg.h"

LONG DbgLevelMagmErr = DBG_LEVEL_MAGM_ERR;

#define NdasUiDbgCall(l,x,...) do {						\
	if (l <= DbgLevelMagmErr) {						\
	ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
	ATLTRACE (x,__VA_ARGS__);							\
	} 													\
} while(0)

CAppErrorDlg::CAppErrorDlg(
	ATL::_U_STRINGorID szMessage, 
	ATL::_U_STRINGorID szDescription,
	ATL::_U_STRINGorID szTitle) :
	m_strMessage(szMessage.m_lpstr),
	m_strDescription(szDescription.m_lpstr),
	m_strTitle(szTitle.m_lpstr)
{
}

CAppErrorDlg::CAppErrorDlg(
	const CString& strMessage, 
	const CString& strDescription,
	const CString& strTitle) :
	m_strMessage(strMessage),
	m_strDescription(strDescription),
	m_strTitle(strTitle)
{
}

LRESULT
CAppErrorDlg::OnInitDialog(HWND hWndFocus, LPARAM lParam)
{
	if (m_strTitle.IsEmpty()) 
	{
		m_strTitle.LoadString(IDS_ERROR_TITLE);	
	}

	m_wndIcon.Attach(GetDlgItem(IDC_ERROR_ICON));
	m_wndMessage.Attach(GetDlgItem(IDC_ERROR_MESSAGE));
	m_wndDescription.Attach(GetDlgItem(IDC_ERROR_DESCRIPTION));

	HICON hErrorIcon = ::AtlLoadSysIcon(IDI_ERROR);
	m_wndIcon.SetIcon(hErrorIcon);

	m_wndMessage.SetWindowText(m_strMessage);
	m_wndDescription.SetWindowText(m_strDescription);

	CenterWindow(GetParent());

	return TRUE;
};

namespace
{

bool
pLoadMessageString(
	CString& str, 
	HMODULE hModule, 
	DWORD MessageId, 
	LANGID LanguageId = 0)
{
	LPTSTR lpMessage = NULL;
	
	DWORD flags = 
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS | 
		FORMAT_MESSAGE_MAX_WIDTH_MASK |
		FORMAT_MESSAGE_FROM_HMODULE |
		FORMAT_MESSAGE_FROM_SYSTEM;

	int cch = ::FormatMessage(
		flags,
		hModule,
		MessageId,
		LanguageId,
		reinterpret_cast<LPTSTR>(&lpMessage),
		0,
		NULL);

	if (0 == cch &&  
		0 != LanguageId &&
		::GetLastError() == ERROR_RESOURCE_LANG_NOT_FOUND)
	{
		// Language-specific resource is not found.
		// Try again with other available languages.
		cch = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | 
			FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
			hModule,
			MessageId,
			0,
			reinterpret_cast<LPTSTR>(&lpMessage),
			0,
			NULL);		
	}

	if (0 == cch)
	{
		ATLTRACE("FormatMessage %d failed with error %d\n", MessageId, ::GetLastError());
		return false;
	}

	str = lpMessage;

	if (lpMessage)
	{
		::LocalFree(lpMessage);
	}

	return true;
}

bool
pLoadMessageString(
	CString& str, 
	LPCTSTR ModuleName, 
	DWORD MessageId, 
	LANGID Language = 0)
{
	HMODULE hModule = ::LoadLibraryEx(ModuleName, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (NULL == hModule)
	{
		ATLTRACE("Loading message module %ws failed with error %d\n", 
			ModuleName, ::GetLastError());
		return false;
	}

	bool ret = pLoadMessageString(str, hModule, MessageId, Language);
	::FreeLibrary(hModule);
	return ret;
}

} // namespace

INT_PTR 
ErrorMessageBox(
	HWND hWndOwner,
	ATL::_U_STRINGorID Message,
	ATL::_U_STRINGorID Title,
	LANGID LanguageId,
	DWORD ErrorCode)
{
	ATLASSERT(hWndOwner == NULL || ::IsWindow(hWndOwner));

	const LPCTSTR NDASMSG_MODULE = _T("ndasmsg.dll");

	// use implicit load string
	
	CString strMessage = Message.m_lpstr; 
	CString strTitle = Title.m_lpstr; 

	CString strErrorDescription;
	CString strDescription;

	pLoadMessageString(
		strErrorDescription, 
		NDASMSG_MODULE, 
		ErrorCode, 
		LanguageId);

	if (ErrorCode & APPLICATION_ERROR_MASK)
	{
		//
		// Facility: NDAS 0x%1!04X!\r\n
		// Error Code: %2!u! (0x%2!04X!)\r\n
		// %3!s!			
		strDescription.FormatMessage(
			IDS_ERROR_NDAS_DESCRIPTION_FMT,
			(ErrorCode & 0x0FFF0000) >> 16,
			(ErrorCode & 0x0000FFFF),
			strErrorDescription);
	}
	else
	{
		// Error Code: %1!u! (0x%1!04X!)\r\n
		// %2!s!
		strDescription.FormatMessage(
			IDS_ERROR_SYSTEM_DESCRIPTION_FMT,
			ErrorCode,
			strErrorDescription);
	}


	CAppErrorDlg dlg(strMessage, strDescription, strTitle);
	INT_PTR response = dlg.DoModal(hWndOwner);
	return response;
}

CString
pFormatErrorString (
	__in DWORD  ErrorCode,
	__in LANGID LangId
	)
{
	const LPCTSTR NDASMSG_MODULE = _T("ndasmsg.dll");

	CString errorDescription;
	CString formatted;

	NdasUiDbgCall( 1, "ErrorCode = %x\n", ErrorCode );

	pLoadMessageString( errorDescription, NDASMSG_MODULE, ErrorCode, LangId );

	if (ErrorCode & APPLICATION_ERROR_MASK) {

		// Facility: NDAS 0x%1!04X!\r\n
		// Error Code: %2!u! (0x%2!04X!)\r\n
		// %3!s!			

		formatted.FormatMessage( IDS_ERROR_NDAS_DESCRIPTION_FMT, (ErrorCode & 0x0FFF0000), (ErrorCode & 0xFFFF), errorDescription );
	
	} else {

		// Error Code: %1!u! (0x%1!04X!)\r\n
		// %2!s!
		
		formatted.FormatMessage( IDS_ERROR_SYSTEM_DESCRIPTION_FMT, (ErrorCode & 0x0FFFFFFF), errorDescription );
	}

	return formatted;
}
