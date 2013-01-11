#include "stdatl.hpp"
#include "deviceselectiondlg.h"

namespace
{

void
StripNonAlphanum(
	__in LPCTSTR Source, 
	__out_ecount(DestinationLength + 1) LPTSTR Destination,
	__in DWORD DestinationLength,
	__out_opt LPDWORD OutputLength,
	__out_opt LPCTSTR* NextPosition)
{
	LPCTSTR src = Source;
	LPTSTR dst = Destination;
	DWORD i = 0;
	for (; NULL != *src && i < DestinationLength; ++src)
	{
		if (_istalnum(*src))
		{
			*(dst++) = *src;
			++i;
		}
	}
	*dst = _T('\0');
	if (OutputLength) *OutputLength = i;
	if (NextPosition) *NextPosition = src;
}

} // end anonymous namespace

#if (_WIN32_WINNT < 0x501)
#define ECM_FIRST               0x1500      // Edit control messages
#define	EM_SETCUEBANNER	    (ECM_FIRST + 1)		// Set the cue banner with the lParm = LPCWSTR
#define Edit_SetCueBannerText(hwnd, lpcwText) \
	(BOOL)SNDMSG((hwnd), EM_SETCUEBANNER, 0, (LPARAM)(lpcwText))
#define	EM_GETCUEBANNER	    (ECM_FIRST + 2)		// Set the cue banner with the lParm = LPCWSTR
#define Edit_GetCueBannerText(hwnd, lpwText, cchText) \
	(BOOL)SNDMSG((hwnd), EM_GETCUEBANNER, (WPARAM)(lpwText), (LPARAM)(cchText))
#endif

LRESULT 
CDeviceSelectionDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_dlgParam = reinterpret_cast<PDLG_PARAM>(lParam);
	ATLASSERT(NULL != m_dlgParam && sizeof(DLG_PARAM) == m_dlgParam->Size);

	m_deviceListComboBox.Attach(GetDlgItem(IDC_DEVICE_LIST));
	m_deviceIdEdit.Attach(GetDlgItem(IDC_DEVICE_ID));
	m_deviceListButton.Attach(GetDlgItem(IDC_FROM_DEVICE_LIST));
	m_deviceIdButton.Attach(GetDlgItem(IDC_FROM_DEVICE_ID));

	m_deviceIdEdit.ModifyStyle(0, ES_UPPERCASE);
	m_deviceIdEdit.SetLimitText(
		NDAS_DEVICE_STRING_ID_LEN + 3 + 
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	Edit_SetCueBannerText(
		m_deviceIdEdit, 
		_T("Enter NDAS ID. Append Write Key to change settings."));
	
	m_deviceListButton.EnableWindow(FALSE);
	m_deviceListComboBox.EnableWindow(FALSE);
	m_fromDeviceList = false;

	HMODULE ndasUserModuleHandle = LoadLibrary(_T("ndasuser.dll"));
	if (NULL != ndasUserModuleHandle)
	{
		BOOL success = NdasEnumDevices(OnNdasDeviceEnum, this);
		if (!success)
		{
			ATLTRACE("NdasEnumDevice failed, error=0x%X\n", GetLastError());
			m_deviceIdButton.Click();
		}
		else
		{
			// m_deviceListComboBox.SetCurSel(0);
			m_deviceListButton.EnableWindow(TRUE);
			m_deviceListComboBox.EnableWindow(TRUE);
			m_deviceListButton.Click();
			m_fromDeviceList = true;
		}
		FreeLibrary(ndasUserModuleHandle);
	}

	//
	// Change the font of the edit box to fixed/bold font
	//
	CFontHandle font = m_deviceIdEdit.GetFont();
	LOGFONT logFont;
	font.GetLogFont(logFont);
	logFont.lfWeight = FW_BOLD;
	logFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	logFont.lfFaceName[0] = _T('\0');
	m_fixedBoldFont.CreateFontIndirect(&logFont);
	m_deviceIdEdit.SetFont(m_fixedBoldFont);

	//
	// Clear the last edit length
	//
	m_lastEditLength = 0;

	//
	// Set the selected device
	// 
	switch (m_dlgParam->Type)
	{
	case DLG_PARAM_MANUAL:
		{
			TCHAR idText[48];
			LPTSTR p = idText;
			SIZE_T rem = RTL_NUMBER_OF(idText);
			int i = 0;
			for (; i < 3; ++i)
			{
				COMVERIFY( StringCchCopyN(p, rem, m_dlgParam->NdasDeviceId + i * 5, 5) );
				p += 5;
				rem -= 5;
				COMVERIFY( StringCchCat(p, rem, _T("-")) );
			}
			COMVERIFY( StringCchCopyN(p, rem, m_dlgParam->NdasDeviceId + i * 5, 5) );
			m_deviceIdEdit.SetWindowText(idText);
		}
		break;
	case DLG_PARAM_REGISTERED:
		{
			int count = m_deviceListComboBox.GetCount();
			m_deviceListComboBox.SetCurSel(0);
			for (int i = 0; i < count; ++i)
			{
				PLIST_ENTRY_CONTEXT entryContext = static_cast<PLIST_ENTRY_CONTEXT>(
					m_deviceListComboBox.GetItemDataPtr(i));
				if (0 == lstrcmpi(
					entryContext->NdasDeviceId,
					m_dlgParam->NdasDeviceId))
				{
					m_deviceListComboBox.SetCurSel(i);
					break;
				}
			}
		}
		break;
	case DLG_PARAM_UNSPECIFIED:

	default:
		;
	}

	return TRUE;
}

void 
CDeviceSelectionDlg::OnCmdOK(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	if (m_fromDeviceList)
	{
		m_dlgParam->Type = DLG_PARAM_REGISTERED;

		int n = m_deviceListComboBox.GetCurSel();
		if (CB_ERR != n)
		{
			PLIST_ENTRY_CONTEXT entryContext = reinterpret_cast<PLIST_ENTRY_CONTEXT>(
				m_deviceListComboBox.GetItemDataPtr(n));

			COMVERIFY( StringCchCopy(
				m_dlgParam->NdasDeviceId,
				NDAS_DEVICE_STRING_ID_LEN + 1,
				entryContext->NdasDeviceId) );

			m_deviceListComboBox.GetWindowText(
				m_dlgParam->NdasDeviceName, MAX_NDAS_DEVICE_NAME_LEN + 1);

			LPCTSTR writeKey = _T("");
			if (GENERIC_WRITE & entryContext->GrantedAccess)
			{
				writeKey = _T("*****");
			}

			COMVERIFY( StringCchCopy(
				m_dlgParam->NdasDeviceKey,
				NDAS_DEVICE_WRITE_KEY_LEN + 1,
				writeKey) );
		}
		else
		{
			EndDialog(IDCANCEL);
		}
	}
	else
	{
		m_dlgParam->Type = DLG_PARAM_MANUAL;

		m_deviceIdEdit.GetWindowText(
			m_dlgParam->NdasDeviceName, 
			RTL_NUMBER_OF(m_dlgParam->NdasDeviceName));
		
		DWORD outputLength;
		LPCTSTR writeKeyPosition = NULL;

		StripNonAlphanum(
			m_dlgParam->NdasDeviceName,
			m_dlgParam->NdasDeviceId,
			NDAS_DEVICE_STRING_ID_LEN,
			&outputLength,
			&writeKeyPosition);

		StripNonAlphanum(
			writeKeyPosition,
			m_dlgParam->NdasDeviceKey,
			NDAS_DEVICE_WRITE_KEY_LEN,
			&outputLength,
			NULL);

		ATLTRACE("DeviceName: %ls\n", m_dlgParam->NdasDeviceName);
		ATLTRACE("NDAS ID: %ls\n", m_dlgParam->NdasDeviceId);
		ATLTRACE("NDAS WK: %ls\n", m_dlgParam->NdasDeviceKey);
	}

	EndDialog(wID);
}

void 
CDeviceSelectionDlg::OnCmdCancel(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	EndDialog(wID);
}

BOOL 
CDeviceSelectionDlg::OnNdasDeviceEnum(PNDASUSER_DEVICE_ENUM_ENTRY EnumEntry)
{
	PLIST_ENTRY_CONTEXT context = new LIST_ENTRY_CONTEXT;
	if (NULL == context)
	{
		ATLTRACE("Out of memory\n");
		return TRUE;
	}

	COMVERIFY( StringCchCopy(
		context->NdasDeviceId,
		NDAS_DEVICE_STRING_ID_LEN + 1,
		EnumEntry->szDeviceStringId) );

	context->GrantedAccess = EnumEntry->GrantedAccess;

	int n = m_deviceListComboBox.AddString(EnumEntry->szDeviceName);
	ATLASSERT(n != CB_ERR);
	ATLVERIFY(CB_ERR != m_deviceListComboBox.SetItemDataPtr(n, context));

	return TRUE;
}

void
CDeviceSelectionDlg::OnDeviceListClicked(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	ATLTRACE("OnDeviceListClicked\n");
	m_deviceListComboBox.EnableWindow(TRUE);
	m_deviceIdEdit.EnableWindow(FALSE);
	m_fromDeviceList = TRUE;
	// m_deviceListComboBox.SetFocus();
}

void 
CDeviceSelectionDlg::OnDeviceIdClicked(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	ATLTRACE("OnDeviceIdClicked\n");
	m_deviceListComboBox.EnableWindow(FALSE);
	m_deviceIdEdit.EnableWindow(TRUE);
	m_fromDeviceList = FALSE;
	// m_deviceIdEdit.SetFocus();
}

void 
CDeviceSelectionDlg::OnDeviceIdChange(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	int len = m_deviceIdEdit.GetWindowTextLength();
	ATLTRACE("OnDeviceIdChange: %d -> %d\n", m_lastEditLength, len);
	if (len > m_lastEditLength && len >= 5 && 0 == (len - 5) % 6)
	{
		m_deviceIdEdit.AppendText(_T("-"));
	}
	m_lastEditLength = len;
}

void 
CDeviceSelectionDlg::OnDeviceIdSetFocus(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	ATLTRACE(__FUNCTION__ "\n");
	if (IDC_DEVICE_LIST == wID)
	{
		m_deviceListButton.SetCheck(BST_CHECKED);
		m_deviceIdButton.SetCheck(BST_UNCHECKED);
	}
	else
	{
		m_deviceListButton.SetCheck(BST_UNCHECKED);
		m_deviceIdButton.SetCheck(BST_CHECKED);
	}
}
