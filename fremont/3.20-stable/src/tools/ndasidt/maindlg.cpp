#include "precomp.hpp"
#include <ndas/ndasidenc.h>
#include "maindlg.hpp"

namespace
{
	bool pGetHexValue(const CEdit& wndEdit, int& value)
	{
		TCHAR buffer[5] = _T("0x");
		int n = wndEdit.GetWindowText(&buffer[2], 3);
		ATLTRACE("Got %d ", n);
		ATLTRACE("String %ls ", buffer);
		int parsedValue;
		if (!StrToIntEx(buffer, STIF_SUPPORT_HEX, &parsedValue))
		{
			return false;
		}
		value = parsedValue;
		return true;
	}

	bool pGetByteFromEdit(const CEdit& wndEdit, BYTE& byteValue)
	{
		int value;
		if (!pGetHexValue(wndEdit, value))
		{
			return false;
		}
		byteValue = static_cast<BYTE>(value);
		ATLTRACE("Parsed as %02X\n", byteValue);
		return true;
	}

	CString pNdasKeyCharsToString(const CHAR* part)
	{
		CString str;
		str.Format(_T("%C%C%C%C%C"), part[0], part[1], part[2], part[3], part[4]);
		return str;
	}

	CString pBytesToString(const BYTE* p, int len)
	{
		CString str, part;
		for (int i = 0; i < len; ++i)
		{
			part.Format(_T("%02X"), p[i]);
			str += part;
		}
		return str;
	}
}

LRESULT CMainDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	HICON hIcon = AtlLoadIcon(IDR_MAINFRAME);
	SetIcon(hIcon);

	HWND hWndPrevCtrl = NULL;
	for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	{
		m_wndAddresses[i].SubclassWindow(GetDlgItem(IDC_ADDR_1 + i));
		// AttachToDlgItem(m_hWnd, IDC_ADDR_1 + i);
		// m_wndAddresses[i].Attach(GetDlgItem(IDC_ADDR_1 + i));
		m_wndAddresses[i].LimitText(2);
		m_wndAddresses[i].SetPrevCtrl(hWndPrevCtrl);
		hWndPrevCtrl = m_wndAddresses[i];
	}

	m_wndVID.SubclassWindow(GetDlgItem(IDC_VID));
	m_wndVID.LimitText(2);

	for (int i = 0; i < RTL_NUMBER_OF(m_wndReserved); ++i)
	{
		m_wndReserved[i].SubclassWindow(GetDlgItem(IDC_RESERVED_1 + i));
		m_wndReserved[i].SetLimitText(2);
		m_wndReserved[i].SetPrevCtrl(hWndPrevCtrl);
		hWndPrevCtrl = m_wndReserved[i];
	}

	m_wndSeed.SubclassWindow(GetDlgItem(IDC_SEED));
	m_wndSeed.SetLimitText(2);
	m_wndSeed.SetPrevCtrl(hWndPrevCtrl);
	hWndPrevCtrl = m_wndSeed;

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys1); ++i)
	{
		m_wndKeys1[i].SubclassWindow(GetDlgItem(IDC_KEY_1_1 + i));
		m_wndKeys1[i].LimitText(2);
		m_wndKeys1[i].SetPrevCtrl(hWndPrevCtrl);
		hWndPrevCtrl = m_wndKeys1[i];
	}

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys2); ++i)
	{
		m_wndKeys2[i].SubclassWindow(GetDlgItem(IDC_KEY_2_1 + i));
		m_wndKeys2[i].LimitText(2);
		m_wndKeys2[i].SetPrevCtrl(hWndPrevCtrl);
		hWndPrevCtrl = m_wndKeys2[i];
	}

	for (int i = 0; i < RTL_NUMBER_OF(m_wndNdasId); ++i)
	{
		m_wndNdasId[i].SubclassWindow(GetDlgItem(IDC_NDASID_1 + i));
		m_wndNdasId[i].LimitText(5);
		m_wndNdasId[i].SetPrevCtrl(hWndPrevCtrl);
		hWndPrevCtrl = m_wndNdasId[i];
	}

	// Additional controls
	m_wndUseCustomKeys.Attach(GetDlgItem(IDC_USE_CUSTOMKEYS));
	m_wndUseDefaultKeys.Attach(GetDlgItem(IDC_SET_DEFAULT_KEYS));
	m_wndUseDefaultVendor.Attach(GetDlgItem(IDC_SET_DEFAULT_VENDOR));

	// subclassing
	{
		int ei = 0;
		for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
		{
			m_hexOnlyEdit[ei++].SubclassWindow(m_wndAddresses[i]);
		}
		m_hexOnlyEdit[ei++].SubclassWindow(m_wndVID);
		for (int i = 0; i < RTL_NUMBER_OF(m_wndReserved); ++i)
		{
			m_hexOnlyEdit[ei++].SubclassWindow(m_wndReserved[i]);
		}
		m_hexOnlyEdit[ei++].SubclassWindow(m_wndSeed);
		for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys1); ++i)
		{
			m_hexOnlyEdit[ei++].SubclassWindow(m_wndKeys1[i]);
		}
		for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys2); ++i)
		{
			m_hexOnlyEdit[ei++].SubclassWindow(m_wndKeys2[i]);
		}
	}

	m_wndWriteKey.SubclassWindow(GetDlgItem(IDC_WRITEKEY));
	m_wndWriteKey.SetLimitText(5);
	m_wndWriteKey.SetPrevCtrl(hWndPrevCtrl);
	hWndPrevCtrl = m_wndWriteKey;

	_EnableEditKeys(FALSE);
	_SetDefaultVendor();
	_SetDefaultKeys();

	return 1;
}

void CMainDialog::OnDestroy()
{
	//for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	//{
	//	m_wndAddresses[i].UnsubclassWindow();
	//}
}

void CMainDialog::CloseDialog(int retValue)
{
	//if (m_bModal)
	//{
		EndDialog(retValue);
	//}
	//else
	//{
	//	DestroyWindow();
	//	::PostQuitMessage(retValue);
	//}
}

void CMainDialog::OnClose()
{
	CloseDialog(IDCLOSE);
}

void CMainDialog::OnCmdOK(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	NDAS_ID_KEY_INFO info = {0};

	for (int i = 0; i < RTL_NUMBER_OF(m_wndNdasId); ++i)
	{
		m_wndNdasId[i].SetWindowText(_T(""));
	}

	{
		m_wndWriteKey.SetWindowText(_T(""));
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndAddresses) == RTL_NUMBER_OF(info.address));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	{
		if (!pGetByteFromEdit(m_wndAddresses[i], info.address[i]))
		{
			return;
		}
	}

	if (!pGetByteFromEdit(m_wndVID, info.vid))
	{
		return;
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndReserved) == RTL_NUMBER_OF(info.reserved));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndReserved); ++i)
	{
		if (!pGetByteFromEdit(m_wndReserved[i], info.reserved[i]))
		{
			return;
		}
	}

	if (!pGetByteFromEdit(m_wndSeed, info.random))
	{
		return;
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndKeys1) == RTL_NUMBER_OF(info.key1));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys1); ++i)
	{
		if (!pGetByteFromEdit(m_wndKeys1[i], info.key1[i]))
		{
			return;
		}
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndKeys2) == RTL_NUMBER_OF(info.key2));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys2); ++i)
	{
		if (!pGetByteFromEdit(m_wndKeys2[i], info.key2[i]))
		{
			return;
		}
	}

	if (!NdasIdKey_Encrypt(&info))
	{
		return;
	}

	for (int i = 0; i < 4; ++i)
	{
		m_wndNdasId[i].SetWindowText(pNdasKeyCharsToString(info.serialNo[i]));
	}

	m_wndWriteKey.SetWindowText(pNdasKeyCharsToString(info.writeKey));

}

void CMainDialog::OnCmdParse(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	NDAS_ID_KEY_INFO info = {0};

	for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	{
		m_wndAddresses[i].SetWindowText(_T(""));
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndKeys1) == RTL_NUMBER_OF(info.key1));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys1); ++i)
	{
		if (!pGetByteFromEdit(m_wndKeys1[i], info.key1[i]))
		{
			return;
		}
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndKeys2) == RTL_NUMBER_OF(info.key2));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys2); ++i)
	{
		if (!pGetByteFromEdit(m_wndKeys2[i], info.key2[i]))
		{
			return;
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		TCHAR part[6] = {0};
		m_wndNdasId[i].GetWindowText(part, 6);
		for (int j = 0; j < 5; ++j)
		{
			info.serialNo[i][j] = static_cast<char>(part[j]);
		}
	}

	if (!NdasIdKey_Decrypt(&info))
	{
		m_wndNdasId[0].SetFocus();
		m_wndNdasId[0].SetSel(0, -1);
		return;
	}

	for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	{
		CString s = pBytesToString(&info.address[i], 1);
		m_wndAddresses[i].SetWindowText(s);
	}

	{
		CString s = pBytesToString(&info.vid, 1);
		m_wndVID.SetWindowText(s);
	}

	C_ASSERT(RTL_NUMBER_OF(m_wndReserved) == RTL_NUMBER_OF(info.reserved));

	for (int i = 0; i < RTL_NUMBER_OF(m_wndReserved); ++i)
	{
		CString s = pBytesToString(&info.reserved[i], 1);
		m_wndReserved[i].SetWindowText(s);
	}

	{
		CString s = pBytesToString(&info.random, 1);
		m_wndSeed.SetWindowText(s);
	}

}

void CMainDialog::OnCmdClose(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	CloseDialog(wID);
}

void CMainDialog::OnCmdSetDefaultKeys(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	_SetDefaultKeys();
}

void CMainDialog::OnCmdSetDefaultVendor(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	_SetDefaultVendor();
}

void CMainDialog::OnEditUpdate(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	ATLTRACE("OnEditUpdate %d,%d,%d\n", wNotifyCode, wID, hWndCtl);
}

void CMainDialog::OnEditChange(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	ATLTRACE("OnEditChange %d,%d,%d\n", wNotifyCode, wID, hWndCtl);
	CEdit wndEdit(hWndCtl);
	int endPos = wndEdit.GetLimitText();
	if (wndEdit.GetWindowTextLength() == endPos)
	{
		int startChar = 0, endChar = 0;
		wndEdit.GetSel(startChar, endChar);
		if (startChar == endPos)
		{
			ATLTRACE("Goto next\n", wNotifyCode, wID, hWndCtl);
			CWindow wndNext = GetNextDlgTabItem(hWndCtl);
			wndNext.SetFocus();
			CEdit wndNextEdit = wndNext;
			wndNextEdit.SetSel(0, -1);
		}
	}
}

void CMainDialog::OnCustomKeyClick(UINT, int, HWND)
{
	_EnableEditKeys(BST_CHECKED == m_wndUseCustomKeys.GetCheck());
}

void CMainDialog::OnCmdCopyNdasId(UINT, int, HWND)
{
	CString s;
	for (int i = 0; i < RTL_NUMBER_OF(m_wndNdasId); ++i)
	{
		TCHAR part[6] = {0};
		m_wndNdasId[i].GetWindowText(part, 6);
		s += part;
		if (i + 1 < RTL_NUMBER_OF(m_wndNdasId))
		{
			s += _T("-");
		}
	}

	int len = s.GetLength();
	HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(TCHAR));
	if (NULL == hData)
	{
		return;
	}

	LPVOID lpData = GlobalLock(hData);
	CopyMemory(lpData, static_cast<LPCTSTR>(s), (len + 1) * sizeof(TCHAR));
	GlobalUnlock(hData);

	OpenClipboard();
	EmptyClipboard();

#ifdef UNICODE
	SetClipboardData(CF_UNICODETEXT, hData);
#else
	SetClipboardData(CF_TEXT, hData);
#endif

	CloseClipboard();
}

void CMainDialog::OnCmdCopyMacAddress(UINT, int, HWND)
{
	CString s;
	for (int i = 0; i < RTL_NUMBER_OF(m_wndAddresses); ++i)
	{
		TCHAR part[3] = {0};
		m_wndAddresses[i].GetWindowText(part, 3);
		s += part;
		if (i + 1 < RTL_NUMBER_OF(m_wndAddresses))
		{
			s += _T(":");
		}
	}

	int len = s.GetLength();
	HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(TCHAR));
	if (NULL == hData)
	{
		// TODO: Error message
		return;
	}

	LPVOID lpData = GlobalLock(hData);
	CopyMemory(lpData, static_cast<LPCTSTR>(s), (len + 1) * sizeof(TCHAR));
	GlobalUnlock(hData);

	OpenClipboard();
	EmptyClipboard();

#ifdef UNICODE
	SetClipboardData(CF_UNICODETEXT, hData);
#else
	SetClipboardData(CF_TEXT, hData);
#endif

	CloseClipboard();
}

void CMainDialog::_EnableEditKeys(BOOL fEnable)
{
	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys1); ++i)
	{
		m_wndKeys1[i].EnableWindow(fEnable);
	}

	for (int i = 0; i < RTL_NUMBER_OF(m_wndKeys2); ++i)
	{
		m_wndKeys2[i].EnableWindow(fEnable);
	}

	m_wndUseDefaultKeys.EnableWindow(fEnable);
}

void CMainDialog::_SetDefaultVendor()
{
	m_wndVID.SetWindowText(_T("01"));
	m_wndReserved[0].SetWindowText(_T("FF"));
	m_wndReserved[1].SetWindowText(_T("FF"));
	m_wndSeed.SetWindowText(_T("CD"));
}

void CMainDialog::_SetDefaultKeys()
{
	m_wndKeys1[0].SetWindowText(_T("45"));
	m_wndKeys1[1].SetWindowText(_T("32"));
	m_wndKeys1[2].SetWindowText(_T("56"));
	m_wndKeys1[3].SetWindowText(_T("2F"));
	m_wndKeys1[4].SetWindowText(_T("EC"));
	m_wndKeys1[5].SetWindowText(_T("4A"));
	m_wndKeys1[6].SetWindowText(_T("38"));
	m_wndKeys1[7].SetWindowText(_T("53"));

	m_wndKeys2[0].SetWindowText(_T("1E"));
	m_wndKeys2[1].SetWindowText(_T("4E"));
	m_wndKeys2[2].SetWindowText(_T("0F"));
	m_wndKeys2[3].SetWindowText(_T("EB"));
	m_wndKeys2[4].SetWindowText(_T("33"));
	m_wndKeys2[5].SetWindowText(_T("27"));
	m_wndKeys2[6].SetWindowText(_T("50"));
	m_wndKeys2[7].SetWindowText(_T("C1"));
}

