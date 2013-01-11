#include "stdatl.hpp"
#include "maindlg.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasmsg.h>

LRESULT
CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	TCHAR logFileName[256];
	CTime t = CTime::GetCurrentTime();

	m_loadButton.Attach(GetDlgItem(IDC_LOAD));
	m_currentMacText[0].Attach(GetDlgItem(IDC_CURMAC1));
	m_currentMacText[1].Attach(GetDlgItem(IDC_CURMAC2));
	m_currentMacText[2].Attach(GetDlgItem(IDC_CURMAC3));
	m_currentMacText[3].Attach(GetDlgItem(IDC_CURMAC4));
	m_currentMacText[4].Attach(GetDlgItem(IDC_CURMAC5));
	m_currentMacText[5].Attach(GetDlgItem(IDC_CURMAC6));

	m_currentSuperPasswordText.Attach(GetDlgItem(IDC_SUPER_PW));
	m_currentUserPasswordText.Attach(GetDlgItem(IDC_USER_PW));

	m_eepromPathText.Attach(GetDlgItem(IDC_EEPROM_PATH));
	m_newMacText[0].Attach(GetDlgItem(IDC_NEWMAC1));
	m_newMacText[1].Attach(GetDlgItem(IDC_NEWMAC2));
	m_newMacText[2].Attach(GetDlgItem(IDC_NEWMAC3));
	m_newMacText[3].Attach(GetDlgItem(IDC_NEWMAC4));
	m_newMacText[4].Attach(GetDlgItem(IDC_NEWMAC5));
	m_newMacText[5].Attach(GetDlgItem(IDC_NEWMAC6));

	m_dontChangeMacButton.Attach(GetDlgItem(IDC_DONT_CHANGE_MAC));
	m_macIncButton.Attach(GetDlgItem(IDC_CHECK_INC_MAC));
	
	m_udpateButton.Attach(GetDlgItem(IDC_UPDATE));
	m_logText.Attach(GetDlgItem(IDC_LOG));
	
	// Set initial values
	m_currentMacText[0].SetWindowText(_T("00"));
	m_currentMacText[1].SetWindowText(_T("0D"));
	m_currentMacText[2].SetWindowText(_T("D0"));
	m_currentMacText[3].SetWindowText(_T("FF"));
	m_currentMacText[4].SetWindowText(_T("FF"));
	m_currentMacText[5].SetWindowText(_T("FF"));

	m_newMacText[0].SetWindowText(_T("00"));
	m_newMacText[1].SetWindowText(_T("0B"));
	m_newMacText[2].SetWindowText(_T("D0"));
	m_newMacText[3].SetWindowText(_T(""));	// Empty to prevent mistake.
	m_newMacText[4].SetWindowText(_T(""));
	m_newMacText[5].SetWindowText(_T(""));

	m_macIncButton.SetCheck(BST_CHECKED);
	
	ZeroMemory(m_eepromImage, sizeof(m_eepromImage));
	m_imageLoaded = FALSE;

	COMVERIFY(StringCchPrintf(
		logFileName, RTL_NUMBER_OF(logFileName), 
		_T("ndasseteep_%4d-%02d-%02d.%02d%02d.txt"), 
		t.GetYear(), t.GetMonth(), t.GetDay(), t.GetHour(), t.GetMinute()
		));
	m_logFile = CreateFile(logFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == m_logFile) {
		AtlMessageBox(m_hWnd, _T("Failed to create log file."));
	}

	return TRUE;
}

void
CMainDlg::OnCmdLoad(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{	
	OPENFILENAME ofn;       // common dialog box structure
	TCHAR szFile[260];       // buffer for file name
	HWND hwnd;              // owner window
	HANDLE hf;              // file handle
	BOOL ret;

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner =  m_hWnd;
	ofn.lpstrFile = szFile;
	//
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	//
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = _T("All\0*.*\0BIN\0*.BIN\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn)) {
		hf = CreateFile(ofn.lpstrFile, GENERIC_READ,
			0, (LPSECURITY_ATTRIBUTES) NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			(HANDLE) NULL);
		if (INVALID_HANDLE_VALUE == hf) {
			AtlMessageBox(m_hWnd, _T("Failed to open file"));
			m_eepromPathText.SetWindowText(_T(""));
			m_imageLoaded = FALSE;
		} else {
			DWORD Readbytes;
			ret = ReadFile(hf, m_eepromImage, m_eepromSize, &Readbytes, NULL);
			if (ret == FALSE) {
				AtlMessageBox(m_hWnd, _T("Failed to read file"));
				m_eepromPathText.SetWindowText(_T(""));
				m_imageLoaded = FALSE;
			} else {
				m_eepromPathText.SetWindowText(ofn.lpstrFile);
				m_imageLoaded = TRUE;
			}
			CloseHandle(hf);
		}
	} else {
		m_eepromPathText.SetWindowText(_T(""));
		m_imageLoaded = FALSE;
	}
}

BOOL 
CMainDlg::GetHexFromControl(CWnd& Ctrl, BYTE* value)
{
	unsigned long result;
	TCHAR s[32] = {0};
	TCHAR* endptr;
	Ctrl.GetWindowText(s, 32);
	result = _tcstoul(s, &endptr, 16);
	if (endptr == s) {
		// No conversion occured
		return FALSE;
	} else {
		if (result <0 || result>0x0ff) {
			return FALSE;
		}
		*value = result;
		return TRUE;
	}
}

BOOL 
CMainDlg::GetHexFromControl(CWnd& Ctrl, ULONG64* value)
{
	DWORDLONG result;
	TCHAR s[256] = {0};
	TCHAR* endptr;
	Ctrl.GetWindowText(s, 256);
	result = _tcstoui64(s, &endptr, 16);
	if (endptr == s) {
		// No conversion occured
		return FALSE;
	} else {
		value = *result;
		return TRUE;
	}
}

BOOL 
CMainDlg::UpdateEeprom(BYTE* curMac, BOOL keepMac, BYTE* newMac, ULONG64 superpw, ULONG64 userpw)
{
	// to do..
	


}

void 
CMainDlg::OnCmdUpdate(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	BYTE current_mac[6];
	BYTE new_mac[6];
	BOOL increase_mac;
	BOOL keep_mac;
	BOOL ret;
	unsigned _int64	user_password;
	unsigned _int64 super_password;
	TCHAR strBuf[256] = {0};

	//
	// 1. Get value from dialog and check current parameters' validity
	//
	if (m_imageLoaded == FALSE) {
		AtlMessageBox(m_hWnd, _T("EEPROM Image file is not selected."));
		return;
	}

	for(i=0;i<6;i++) {
		ret = GetHexFromControl(m_currentMacText[i], &current_mac[i]);
		if (!ret) {
			AtlMessageBox(m_hWnd, _T("Invalid current MAC address."));
			return;
		}
	}

	keep_mac = (BST_CHECKED == m_dontChangeMacButton.GetCheck())?TRUE:FALSE;
	increase_mac = 	(BST_CHECKED == m_macIncButton.GetCheck())?TRUE:FALSE;

	if (!keep_mac) {
		for(i=0;i<6;i++) {
			ret = GetHexFromControl(m_newMacText[i], &new_mac[i]);
			if (!ret) {
				AtlMessageBox(m_hWnd, _T("Invalid current MAC address."));
				return;
			}
		}
	}

	m_currentSuperPasswordText.GetWindowText(strBuf, 256);
	if (strBuf[0] == 0) {
		super_password = m_defaultSuperPassword;
	} else {
		ret = GetHexFromControl(m_currentSuperPasswordText, &super_password);
		if (!ret) {
			AtlMessageBox(m_hWnd, _T("Invalid superuser password."));
			return;
		}
	}

	m_currentUserPasswordText.GetWindowText(strBuf, 256);
	if (strBuf[0] == 0) {
		user_password = m_defaultUserPassword;
	} else {
		ret = GetHexFromControl(m_currentUserPasswordText, &user_password);
		if (!ret) {
			AtlMessageBox(m_hWnd, _T("Invalid user password."));
			return;
		}
	}

	//
	// 2. Update image
	//
	ret = UpdateEeprom(current_mac, keep_mac, new_mac, super_password, user_password);

	if (ret) {
		//
		// Succeed to update.
		// Write log 
			// to do
			
		if (increase_mac) {
			// Increase mac
		}
	} else {
		// Failed to update

	}
}

void
CMainDlg::OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	CSimpleDialog<IDD_ABOUT>().DoModal();
}
