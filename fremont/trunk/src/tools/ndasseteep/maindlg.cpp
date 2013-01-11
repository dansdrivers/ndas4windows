#include "stdatl.hpp"
#include "maindlg.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasid.h>



inline
VOID
StringMacAddress(ATL::CString& AddressStr, const BYTE MacAddress[]) {
	AddressStr.Format(_T("%02X:%02X:%02X:%02X:%02X:%02X"),
		MacAddress[0],
		MacAddress[1],
		MacAddress[2],
		MacAddress[3],
		MacAddress[4],
		MacAddress[5]);
}


VOID
NDASHEARAPI_CALLBACK
NdasHeartbeatCallback(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	CMainDlg *mainDlg = (CMainDlg *)lpContext;
	ATL::CString address;

	if(pHeartbeat->Version < 2) {
		return;
	}
	StringMacAddress(address, pHeartbeat->DeviceAddress.Node);
	if(mainDlg->m_listBox.FindStringExact(-1, address) == LB_ERR) {
		mainDlg->m_listBox.AddString(address);
	}
}

LRESULT
CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	
	time_t t;
	struct tm *today;

	::time(&t);
	today = ::localtime(&t);
	if(today == NULL) {
		return FALSE;
	}

	// Initialize the NDAS hear
	BOOL fSuccess = NdasHeartbeatInitialize();
	if (!fSuccess) 
	{
		_tprintf(_T("Failed to init listener : %d\n"), ::GetLastError());
		return 1;
	}
	m_NdasHearHandle = NdasHeartbeatRegisterNotification(NdasHeartbeatCallback, this);
	if(NULL == m_NdasHearHandle) {
		_tprintf(_T("Failed to register handler : %d\n"), ::GetLastError());
		return 1;
	}

	// Initialize controls
	m_listBox.Attach(GetDlgItem(IDC_LIST_BOX));
	m_autoDetectCheck.Attach(GetDlgItem(IDC_CHECK_USE_AUTO_DETECTED));
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

	m_pnPrefix.Attach(GetDlgItem(IDC_PN_PREFIX));
	m_pnBegin.Attach(GetDlgItem(IDC_PN_BEGIN));
	m_pnAutoIncrease.Attach(GetDlgItem(IDC_CHECK_PN_AUTO_INCREASE));
	m_pnPostFix.Attach(GetDlgItem(IDC_PN_POSTFIX));
	
	// Set initial values
	m_currentMacText[0].SetWindowText(_T("00"));
	m_currentMacText[1].SetWindowText(_T("0B"));
	m_currentMacText[2].SetWindowText(_T("D0"));
	m_currentMacText[3].SetWindowText(_T("FF"));
	m_currentMacText[4].SetWindowText(_T("FF"));
	m_currentMacText[5].SetWindowText(_T("FF"));

	/*	m_currentMacText[3].SetWindowText(_T("FF"));
	m_currentMacText[4].SetWindowText(_T("FF"));
	m_currentMacText[5].SetWindowText(_T("FF"));
*/
	m_newMacText[0].SetWindowText(_T("00"));
	m_newMacText[1].SetWindowText(_T("0B"));
	m_newMacText[2].SetWindowText(_T("D0"));
	m_newMacText[3].SetWindowText(_T(""));	// Empty to prevent mistake.
	m_newMacText[4].SetWindowText(_T(""));
	m_newMacText[5].SetWindowText(_T(""));

	m_macIncButton.SetCheck(BST_CHECKED);
	m_pnAutoIncrease.SetCheck(BST_CHECKED);
	
	ZeroMemory(m_eepromImage, sizeof(m_eepromImage));
	m_imageLoaded = FALSE;

	COMVERIFY(StringCchPrintf(
		m_logFileName, RTL_NUMBER_OF(m_logFileName), 
		_T("ndasseteep_%4d-%02d-%02d.%02d%02d.txt"), 
		today->tm_year + 1900, today->tm_mon + 1, today->tm_mday, today->tm_hour, today->tm_min
		));
	m_logFile = CreateFile(m_logFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == m_logFile) {
		AtlMessageBox(m_hWnd, _T("Failed to create log file."));
	} else {
		ATL::CString logMsg;
		logMsg.Format(_T("Using %s as log file\n"), m_logFileName);
		WriteLogText(static_cast<LPCTSTR>(logMsg));
	}
	m_logWritten = FALSE;

	// read usedmacs.dat and fill m_usedMacs
	m_usedMacsFile = CreateFile(_T("usedmacs.dat"), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE != m_usedMacsFile) {
		UINT64 oneMac;
		DWORD bytesRead;

		while (0 != ReadFile(m_usedMacsFile, &oneMac, sizeof(UINT64), &bytesRead, NULL) && bytesRead == sizeof(UINT64)) {
			if (0 == m_usedMacs.count(oneMac)) {
				m_usedMacs.insert(oneMac);
			}
		}

		CloseHandle(m_usedMacsFile);
		m_usedMacsFile = NULL;
	}

	// open usedmacs.dat to append new mac addresses
	m_usedMacsFile = CreateFile(_T("usedmacs.dat"), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == m_usedMacsFile) {
		AtlMessageBox(m_hWnd, _T("Failed to create usedmacs.dat"));
		m_usedMacsFile = NULL;
	}

	SetFilePointer(m_usedMacsFile, 0, NULL, FILE_END);

	return TRUE;
}

void CMainDlg::OnClose()
{
	if (NULL != m_usedMacsFile) {
		CloseHandle(m_usedMacsFile);
		m_usedMacsFile = NULL;
	}

	EndDialog(0);
}

void CMainDlg::OnCmdCheckUseAutoDetected(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	int idx;

	m_listBox.EnableWindow(BST_CHECKED == m_autoDetectCheck.GetCheck());
	for(idx = 0; idx < 6; idx++) {
		m_currentMacText[idx].EnableWindow(
			BST_CHECKED != m_autoDetectCheck.GetCheck());
	}

}

void CMainDlg::OnCmdClearAll(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	m_listBox.ResetContent();
}

void CMainDlg::OnCmdClearSelected(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	m_listBox.DeleteString(m_listBox.GetCurSel());
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
	ofn.lpstrFilter = _T("BIN\0*.BIN\0All\0*.*\0");
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
CMainDlg::GetHexFromControl(CEdit& Ctrl, BYTE* value)
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
		if (result>0x0ff) {
			return FALSE;
		}
		*value = (BYTE)result;
		return TRUE;
	}
}

BOOL 
CMainDlg::GetHexFromControl(CListBox& Ctrl, BYTE* value)
{
	int idx_cursel, idx_value;
	unsigned long result;
	TCHAR s[32] = {0};
	PTCHAR startptr, endptr;

	idx_cursel = Ctrl.GetCurSel();
	if(idx_cursel == LB_ERR) {
			AtlMessageBox(m_hWnd, _T("No address is selected."));
			return FALSE;
	}
	Ctrl.GetText(idx_cursel, s);
	startptr = s;
	for(idx_value = 0; idx_value < 6; idx_value++) {
		result = _tcstoul(startptr, &endptr, 16);
		if (endptr == startptr) {
			// No conversion occured
			return FALSE;
		} else {
			if (result>0x0ff) {
				return FALSE;
			}
			value[idx_value] = (BYTE)result;
		}
		startptr = endptr;
		if(*startptr == _T('\0') && idx_value <= 4) {
			AtlMessageBox(m_hWnd, _T("Invalid address is selected."));
			return FALSE;
		}
		// Skip delimiter such as ':'
		startptr++;

	}

	return TRUE;
}

BOOL 
CMainDlg::GetHexFromControl(CEdit& Ctrl, ULONG64* value)
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
		*value = result;
		return TRUE;
	}
}

BOOL 
pNdasCommDisconnectAfterReset(HNDAS NdasHandle)
{
	__try
	{
		ATLVERIFY( NdasCommDisconnectEx(NdasHandle, NDASCOMM_DF_DONT_LOGOUT) );
	}
	__except(0xC06D007E == GetExceptionCode() ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("NdasCommDisconnectEx is not available\n");
		ATLVERIFY( NdasCommDisconnect(NdasHandle) );
		// ATLVERIFY( m_ndasDeviceListWnd.ModifyStyle(0, CBS_DROPDOWN) );
	}

	return TRUE;
}

BOOL 
CMainDlg::UpdateEeprom(BYTE* curMac, BOOL keepMac, BYTE* newMac, ULONG64 superpw, ULONG64 userpw, LPCTSTR szProductNumber)
{
	NDASCOMM_CONNECTION_INFO ci = {0};
	NDAS_DEVICE_ID devId;
	int address;

	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = 0;
	ci.WriteAccess = FALSE;
	ci.Flags = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.OEMCode.UI64Value = userpw;
	ci.PrivilegedOEMCode.UI64Value = superpw;
	RtlCopyMemory(ci.Address.DeviceId.Node, curMac, 6);
	ci.Address.DeviceId.Vid = 1;

	HNDAS ndasHandle = NdasCommConnect(&ci);

	if (NULL == ndasHandle)
	{
		// lpx does not return correct error code. Set error code.
		::SetLastError(NDASCOMM_WARNING_CONNECTION_FAIL);
		return FALSE;
	}
	DWORD dwErr;
	BOOL success;
	NDASCOMM_VCMD_PARAM vcmdparam = {0};

	if (keepMac) {
		m_eepromImage[0] = curMac[5];
		m_eepromImage[1] = curMac[4];
		m_eepromImage[2] = curMac[3];
		m_eepromImage[3] = curMac[2];
		m_eepromImage[4] = curMac[1];
		m_eepromImage[5] = curMac[0];
	} else {
		m_eepromImage[0] = newMac[5];
		m_eepromImage[1] = newMac[4];
		m_eepromImage[2] = newMac[3];
		m_eepromImage[3] = newMac[2];
		m_eepromImage[4] = newMac[1];
		m_eepromImage[5] = newMac[0];
	}

	if (szProductNumber) {
		size_t lenPN = _tcsclen(szProductNumber);
		if (sizeof(TCHAR) != 1) {
			char szPN[256];
			_wcstombsz(szPN, szProductNumber, lenPN);

			CopyMemory(m_eepromImage + 0xb0, szPN, lenPN);
		}
		else {
			CopyMemory(m_eepromImage + 0xb0, szProductNumber, lenPN);
		}
	}

#if 1

	for (address=m_eepromSize-16; address>=0; address-=16) {

		vcmdparam.GETSET_EEPROM.Address = address;
		
		RtlCopyMemory( vcmdparam.GETSET_EEPROM.Data, &m_eepromImage[address], 16 );
		
		success = NdasCommVendorCommand( ndasHandle,
										 ndascomm_vcmd_set_eep,
										 &vcmdparam,
										 NULL,
										 0,
										 NULL,
										 0 );

		if (!success) {

			break;
		}
	}

#else

	vcmdparam.GETSET_EEPROM.Address = 0x80;
	success = NdasCommVendorCommand(
		ndasHandle,
		ndascomm_vcmd_get_eep,
		&vcmdparam,
		NULL, 0,
		NULL, 0);

	if (success) {
		ATL::CString message;
		message.Format(_T("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x"),
			vcmdparam.GETSET_EEPROM.Data[0], 
			vcmdparam.GETSET_EEPROM.Data[1], 
			vcmdparam.GETSET_EEPROM.Data[2], 
			vcmdparam.GETSET_EEPROM.Data[3], 
			vcmdparam.GETSET_EEPROM.Data[4], 
			vcmdparam.GETSET_EEPROM.Data[5], 
			vcmdparam.GETSET_EEPROM.Data[6], 
			vcmdparam.GETSET_EEPROM.Data[7], 
			vcmdparam.GETSET_EEPROM.Data[8], 
			vcmdparam.GETSET_EEPROM.Data[9], 
			vcmdparam.GETSET_EEPROM.Data[10], 
			vcmdparam.GETSET_EEPROM.Data[11], 
			vcmdparam.GETSET_EEPROM.Data[12], 
			vcmdparam.GETSET_EEPROM.Data[13], 
			vcmdparam.GETSET_EEPROM.Data[14], 
			vcmdparam.GETSET_EEPROM.Data[15]);
		AtlMessageBox(m_hWnd, static_cast<LPCTSTR>(message));
	}

#endif

	dwErr = ::GetLastError();
	
	RtlZeroMemory(&vcmdparam, sizeof(vcmdparam));
	NdasCommVendorCommand(
		ndasHandle, 
		ndascomm_vcmd_reset, 
		&vcmdparam, 
		NULL, 0, 
		NULL, 0);

	pNdasCommDisconnectAfterReset(ndasHandle);

	::SetLastError(dwErr);



	return success;
}

VOID 
CMainDlg::WriteLogText(LPCTSTR str)
{
	int length = m_logText.GetWindowTextLength();
	m_logText.SetSel(length, length);
	m_logText.ReplaceSel(str);
}

VOID 
CMainDlg::WriteLogFile(LPCTSTR str)
{
	size_t len;
	DWORD written;
	char ansi_str[256];

	if (INVALID_HANDLE_VALUE != m_logFile) {
		StringCchPrintfA(ansi_str, 256, "%S", str);
		StringCchLengthA(ansi_str, 256, &len);
		WriteFile(m_logFile, ansi_str, (DWORD)len, &written, NULL);
		m_logWritten = TRUE;
	}
}

VOID
pGetSerialFromMac(BYTE* mac,LPTSTR Serial)
{
    int sn1, sn2, sn3, sn4;
    sn1 = (((unsigned int)(mac[4]) & 0x00000080)>>7)
                              + (((unsigned int)(mac[3]) & 0x000000FF)<<1);
    sn2 = (((unsigned int)(mac[4]) & 0x0000007F)<<8)
                               + ((unsigned int)(mac[5]) & 0x000000FF);
    sn3 = (((unsigned int)(mac[1]) & 0x00000080)>>7)
                              + (((unsigned int)(mac[0]) & 0x000000FF)<<1);
    sn4 = (((unsigned int)(mac[1]) & 0x0000007F)<<8)
                               + ((unsigned int)(mac[2]) & 0x000000FF);
	if (  mac[0] == 0 && mac[1] == 0xb && mac[2] == 0xd0 )
	{
		StringCchPrintf(Serial, 30, _T("%03d-%05d"), sn1, sn2);
	} else {
		StringCchPrintf(Serial, 30, _T("%03d-%05d:%03d-%05d"), sn3, sn4, sn1, sn2);
	}
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
	TCHAR strBuf[256] = {0}, strBuf1[256] = {0}, strBuf2[256] = {0};
	int i;
	UINT64 ui64Mac;

	BOOL bUsePN = FALSE;
	BOOL bPNAutoIncrease;
	TCHAR szPNPrefix[256] = {0};
	TCHAR szProductNumber[256] = {0};
	int pnSerialNo;
	ATL::CString strProductNumber;

	//
	// 1. Get value from dialog and check current parameters' validity
	//
	if (m_imageLoaded == FALSE) {
		AtlMessageBox(m_hWnd, _T("EEPROM Image file is not selected."));
		return;
	}

	if(BST_CHECKED == m_autoDetectCheck.GetCheck()) {
		ret = GetHexFromControl(m_listBox, current_mac);
		if(!ret) {
			return;
		}
	} else {
		for(i=0;i<6;i++) {
			ret = GetHexFromControl(m_currentMacText[i], &current_mac[i]);
			if (!ret) {
				AtlMessageBox(m_hWnd, _T("Invalid current MAC address."));
				return;
			}
		}
	}

	if (0 != m_pnPrefix.GetWindowTextLength()) {
		if (10 != m_pnPrefix.GetWindowTextLength() ||
			5 != m_pnBegin.GetWindowTextLength() ||
			(0 != m_pnPostFix.GetWindowTextLength() &&
			2 != m_pnPostFix.GetWindowTextLength()))
		{
			AtlMessageBox(m_hWnd, _T("Invalid Product format."));
			return;
		}

		bUsePN = TRUE;

		m_pnPrefix.GetWindowText(strBuf, 256);
		m_pnBegin.GetWindowText(strBuf1, 256);
		m_pnPostFix.GetWindowText(strBuf2, 256);

		pnSerialNo = _tstoi(strBuf1);

		strProductNumber.Format(_T("%s%05d%s"), strBuf, pnSerialNo, strBuf2);
		bPNAutoIncrease = (BST_CHECKED == m_pnAutoIncrease.GetCheck()) ? TRUE:FALSE;
	}

	keep_mac = (BST_CHECKED == m_dontChangeMacButton.GetCheck()) ? TRUE : FALSE;
	increase_mac = 	(BST_CHECKED == m_macIncButton.GetCheck()) ? TRUE : FALSE;

	if (!keep_mac) {
		for(i=0;i<6;i++) {
			ret = GetHexFromControl(m_newMacText[i], &new_mac[i]);
			if (!ret) {
				AtlMessageBox(m_hWnd, _T("Invalid new MAC address."));
				return;
			}
		}

		ui64Mac = 
			(((UINT64)new_mac[0]) << (5 * 8)) +
			(((UINT64)new_mac[1]) << (4 * 8)) +
			(((UINT64)new_mac[2]) << (3 * 8)) +
			(((UINT64)new_mac[3]) << (2 * 8)) +
			(((UINT64)new_mac[4]) << (1 * 8)) +
			(((UINT64)new_mac[5]) << (0 * 8));

		if (m_usedMacs.count(ui64Mac)) {
			if (IDYES != AtlMessageBox(m_hWnd, _T("Used Mac address, proceed anyway?"), _T("Warning"), MB_YESNO | MB_DEFBUTTON2 | MB_ICONEXCLAMATION))
				return;
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
	ret = UpdateEeprom(current_mac, keep_mac, new_mac, super_password, user_password, (bUsePN) ? (LPCTSTR)strProductNumber : NULL);


	if (ret) {
		ATL::CString logMessage;

		if (!keep_mac) {
			TCHAR Serial[30];
			NDAS_DEVICE_ID devId = {0};
			TCHAR Id[30];
			TCHAR Key[30];

			RtlCopyMemory(devId.Node, new_mac, 6);
			devId.Vid = 1;

			pGetSerialFromMac(new_mac, Serial);

			NdasIdDeviceToStringEx(&devId, Id, Key, NULL, NULL);

			logMessage.Format(_T("Updated %02x:%02x:%02x:%02x:%02x:%02x to %02x:%02x:%02x:%02x:%02x:%02x\n"),
				current_mac[0],current_mac[1],current_mac[2],
				current_mac[3],current_mac[4],current_mac[5],
				new_mac[0],new_mac[1],new_mac[2],
				new_mac[3],new_mac[4],new_mac[5]
				);
			WriteLogText(logMessage);
			logMessage.Format(_T("     SN=%s, ID=%s, Key=%s\n"), Serial, Id, Key);
			WriteLogText(logMessage);
			if (bUsePN) {
				logMessage.Format(_T("     PN=%s\n"), strProductNumber);
				WriteLogText(logMessage);
			}
			logMessage.Format(_T("%02x:%02x:%02x:%02x:%02x:%02x %s %s-%s %s\r\n"),
				new_mac[0],new_mac[1],new_mac[2],
				new_mac[3],new_mac[4],new_mac[5],				
				Serial, Id, Key, strProductNumber
				);
			WriteLogFile(logMessage);
		} else {
			logMessage.Format(_T("Updated %02x:%02x:%02x:%02x:%02x:%02x\n"),
				current_mac[0],current_mac[1],current_mac[2],
				current_mac[3],current_mac[4],current_mac[5]
				);
			WriteLogText(logMessage);
		}

		if (!keep_mac) {
			m_usedMacs.insert(ui64Mac);
			if (m_usedMacsFile) {
				DWORD byteWritten;
				WriteFile(m_usedMacsFile, &ui64Mac, sizeof(UINT64), &byteWritten, NULL);
			}
		}
		
		if (!keep_mac && increase_mac) {
			DWORD current = new_mac[3] * 256 * 256 + new_mac[4] * 256 + new_mac[5];
			current++;
			if (current >= 1<<24) {
				AtlMessageBox(m_hWnd, _T("MAC address limit reached"));
			} else {
				BYTE hexdigit;
				ATL::CString macstr;

				hexdigit = (BYTE)((current & 0x00ff0000)>>16);
				macstr.Format(_T("%02X"), hexdigit);
				m_newMacText[3].SetWindowText(static_cast<LPCTSTR>(macstr));
				hexdigit = (BYTE)((current & 0x0000ff00)>>8);
				macstr.Format(_T("%02X"), hexdigit);
				m_newMacText[4].SetWindowText(static_cast<LPCTSTR>(macstr));
				hexdigit = (BYTE)(current & 0x000000ff);
				macstr.Format(_T("%02X"), hexdigit);
				m_newMacText[5].SetWindowText(static_cast<LPCTSTR>(macstr));
			}
		}

		if (bPNAutoIncrease) {
			pnSerialNo++;
			ATL::CString strPNSerialNo;
			strPNSerialNo.Format(_T("%05d"), pnSerialNo);
			m_pnBegin.SetWindowText(strPNSerialNo);
		}
	} else {
		// Failed to update. Show possible error messages
		DWORD errCode = GetLastError();
		ATL::CString message;
		
		if (NDASCOMM_ERROR_HARDWARE_UNSUPPORTED == errCode) {
			message = _T("Hardware does not support EEPROM writing");
		} else if (NDASCOMM_ERROR_LOGIN_COMMUNICATION == errCode) {
			message = _T("Failed to login. Check password is right or another host is using it.");
		} else if (NDASCOMM_WARNING_CONNECTION_FAIL == errCode) {
			message = _T("Failed to connect. Check the device is connected and try to power-cycle the device.");
		} else {
			message.Format(_T("Error 0x%X"), errCode);
		}
		AtlMessageBox(m_hWnd, static_cast<LPCTSTR>(message));
	}
}

void
CMainDlg::OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	CSimpleDialog<IDD_ABOUT>().DoModal();
}

