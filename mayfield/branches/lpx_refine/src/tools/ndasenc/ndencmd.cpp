#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <shlwapi.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <stdio.h>

#include "scrc32.h"
#include "ndas/ndastypeex.h"
#include "ndas/ndassyscfg.h"
#include "ndas/ndasdib.h"
#include "ndas/ndascntenc.h"
#include "ndas/ndascomm.h"
#include "ndas/ndashixnotify.h"
#include "xs/xautores.h"
#include "xs/xstrhelp.h"
#include <winsock2.h>
#include "socketlpx.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"

//
// get the option parameters
//
typedef struct _OPTION_VALUE {
	LPCTSTR option;
	BOOL is_set;
	LPTSTR pval;
} OPTION_VALUE, *POPTION_VALUE;

//
// OPTION_VALUE is NULL terminated
//
// e.g. OPTION_VALUE optValues[] = {{ _T("id"), NULL }, {NULL, NULL}}
//
static const int GETOPT_ERROR_SUCCESS = 0;
static const int GETOPT_UNKNOWN_OPTION = 1;

void header()
{
	_tprintf(
		_T("NDAS Device Content Encrypt Manager Version 1.00.1225\n")
		_T("Copyright (C) 2003-2004 XIMETA, Inc.\n\n"));
}

int ndascomm_api_version_check()
{
	DWORD apiVer = NdasCommGetAPIVersion();
	if (NDASCOMM_API_VERSION != apiVer)
	{
		_tprintf(_T("Incompatible NDASCOMM API %d.%d is loaded.")
			_T("\nThis application is linked against NDASCOMM API %d.%d."),
			NDASCOMM_API_VERSION_MAJOR,
			NDASCOMM_API_VERSION_MINOR,
			LOWORD(apiVer),
			HIWORD(apiVer));
		return 1;
	}
	return 0;
}

POPTION_VALUE
findoptptr(TCHAR* szOption, POPTION_VALUE pOptValues)
{
	for (POPTION_VALUE p = pOptValues; NULL != p->option; ++p) {
		if (0 == ::lstrcmpi(p->option, szOption)) {
			return p;
		}
	}
	return NULL;
}

int 
getopts(int* pArgc, TCHAR*** ppArgs, OPTION_VALUE* pOptValues)
{
	POPTION_VALUE pCurOpt = NULL;
	while (*pArgc > 0) {
		if (_T('-') == (**ppArgs)[0] || _T('/') == (**ppArgs)[0]) {
			POPTION_VALUE popt = findoptptr(&(**ppArgs)[1], pOptValues);
			if (NULL == popt) {
				return GETOPT_UNKNOWN_OPTION;
			}
			popt->is_set = TRUE;
			popt->pval = NULL;
			pCurOpt = popt;
		} else if (NULL != pCurOpt) {
			pCurOpt->pval = **ppArgs;
			pCurOpt = NULL;
		} else {
			return GETOPT_ERROR_SUCCESS;
		}
		--(*pArgc);
		++(*ppArgs);
	}
	return GETOPT_ERROR_SUCCESS;
}


class CWsaDll
{
	BOOL m_fInitalized;
	WORD m_wVersionRequired;
	WSADATA m_wsaData;

public:


	CWsaDll(WORD wVersionRequired = MAKEWORD(2, 2)) :
		m_fInitalized(FALSE),
		m_wVersionRequired(wVersionRequired)
	{
		::ZeroMemory(&m_wsaData, sizeof(WSADATA));
	}

	BOOL Initialize()
	{
		if (m_fInitalized) {
			return TRUE;
		}

		INT iResult = ::WSAStartup(m_wVersionRequired, &m_wsaData);
		if (ERROR_SUCCESS != iResult) {
			_tprintf(_T("WSAInit failed\n"));
			return FALSE;
		}

		m_fInitalized = TRUE;

		_tprintf(_T("WSAInit\n"));

		return TRUE;
	}

	const WSADATA& GetWSAData()
	{
		return m_wsaData;
	}

	~CWsaDll()
	{
		if (m_fInitalized) {
			INT iResult = ::WSACleanup();
			_ASSERTE(ERROR_SUCCESS == iResult);
			_tprintf(_T("WSACleanup returned %d\n"), iResult);
		}
	}

};

// {C9463038-6917-4758-8AFE-3738305A86A6}
static CONST GUID NDAS_ENC_GUID = 
{ 0xc9463038, 0x6917, 0x4758, { 0x8a, 0xfe, 0x37, 0x38, 0x30, 0x5a, 0x86, 0xa6 } };

//
// ndasenc setautoreg <range_begin> <range_end> <ro|rw>
//  
// ndasenc viewautoreg
//
// ndasenc setsyskey <options>
//   -psys <system key passphrase>
//
// ndasenc setdiskencrypt <options>
//   -id <NDAS Device ID without dash>
//   -un <NDAS Device Unit No> (default: 0)
//   -writekey <NDAS Device Write Key>
//   -psys <system key passphrase>
//   -pdisk <disk key passphrase>
//   -enc <SIMPLE|AES>
//   -len <key length>
//        SIMPLE: 32 bit only
//        AES:    128/192/256 bit
// 
// SetSysKet sets the system key to 
// Key: HKLM\Software\NDAS\Keys
// Value: (default)
// Data: 16 bytes (REG_BINARY)
//
// ks : md5hash(sys_key_passphrase)
// kd : md5hash(disk_key_passphrase)
//
// simple: key = (ks ^ kd) & (0xFFFF)
// aes   : key = (kd ^ ks) . (~kd ^ ks) & (keylen)
// 

static LPCTSTR pNdasContentEncryptMethodString(unsigned _int16 method);
static VOID pShowErrorMessage(DWORD dwError = ::GetLastError());
static BOOL pHexCharToByte(TCHAR ch, BYTE& bval);
static BOOL pParseNdasDeviceID(NDAS_DEVICE_ID& deviceID, LPCTSTR lpsz);

int usage()
{
	_tprintf(
		_T("usage: ndasenc <command> [options]\n")
		_T("\n")
		_T("Available commands:\n")
		_T("\n")
		_T("  createsyskeyfile <keyfilename> <syskeyphrase>\n")
		_T("\n")
		_T("    Create a system key file\n")
		_T("\n")
		_T("  importsyskeyfile <keyfilename>\n")
		_T("\n")
		_T("    Import a system key file to the system\n")
		_T("\n")
		_T("  setsyskey <passphrase>\n")
		_T("\n")
		_T("    Set the system key\n")
		_T("\n")
		_T("  verifysyskey <passphrase>\n")
		_T("\n")
		_T("    Verify the passphrase with the existing system key\n")
		_T("\n")
#ifdef _DEBUG
		_T("  getsyskey\n")
		_T("\n")
		_T("    Display the system key\n")
		_T("\n")
#endif
		_T("  clearsyskey\n")
		_T("\n")
		_T("    Clear the system key\n")
		_T("\n")
		_T("  setdiskencrypt <options>\n")
		_T("\n")
		_T("    Set the disk content encryption\n")
		_T("\n")
		_T("    -id    <NDAS Device ID without dash> \n")
		_T("    -wk    <NDAS Device Write Key>       \n")
		_T("    -addr  <MAC Address of the device)   (id/wk or addr is required)\n")
		_T("           (MAC Address is a format of AABBCCDDEEFF or AA:BB:CC:DD:EE:FF)\n")
		_T("    -un    <NDAS Device Unit No>         (optional, default: 0)\n")
		_T("    -psys  <System Key Passphrase>       (required)\n")
		_T("    -pdisk <Disk Key Passphrase>         (required)\n")
		_T("    -enc   <NONE, SIMPLE or AES>         (required)\n")
		_T("    -len   <Key Length>                  (optional)\n")
		_T("           SIMPLE: 32 bits only,  default 32\n")
		_T("           AES: 128/192/256 bits, default 128\n")
		_T("\n")
		_T("  getdiskencrypt <options>\n")
		_T("\n")
		_T("    Get the disk content encryption information\n")
		_T("\n")
		_T("    -id     <NDAS Device ID without dash> \n")
		_T("    -addr   <MAC Address of the device)   (id or addr is required)\n")
		_T("    -un     <NDAS Device Unit No>         (optional, default: 0)\n")
		_T("    -syskey <System Key Passphrase>      (optional)\n")
		_T("            Checks the validity of the system key\n")
		_T("            against the disk content encryption information\n")
		_T("            If the passphrase is '*', system key will be retrieved\n")
		_T("            from the system key store.\n")
		_T("\n")
		);

	return 255;
}

static BOOL pNotifySysKeyChange();

int setsyskey(int argc, TCHAR** argv)
{
	BOOL fSuccess = FALSE;

	//
	// Check Arguments 
	//

	if (argc != 1) {
		return usage();
	}

	LPCTSTR lpSysPass = argv[0];

	UINT lResult = ::NdasEncSetSysKeyPhrase(lpSysPass);
	if (ERROR_SUCCESS != lResult) {
		DWORD dwError = ::GetLastError();
		_tprintf(_T("Setting the system key failed")
			_T(" with error %08X (Code: %08X)\n"), lResult, dwError);
		return lResult;
	}

	fSuccess = pNotifySysKeyChange();
	if (!fSuccess)
	{
		_tprintf(
			_T("WARNING!\n")
			_T("Resetting a system key in the NDAS service (Error: %08X)\n")
			_T("You may have to restart the NDAS service to apply the new system key\n")
			_T("\n"),
			::GetLastError());
	}

	_tprintf(_T("NDAS System Key is set successfully.\n"));

	return 0;
}

#ifdef _DEBUG
int getsyskey(int argc, TCHAR** argv)
{
	BYTE SysKey[16] = {0};
	DWORD cbSysKey = sizeof(SysKey);

	UINT lResult = ::NdasEncGetSysKey(cbSysKey, SysKey, &cbSysKey);
	if (ERROR_SUCCESS != lResult) {
		DWORD dwIntErr = ::GetLastError();
		_tprintf(_T("Retrieving the system key failed")
			_T(" with error %08X (Code: %08X).\n"), lResult, dwIntErr);
		return lResult;
	}

	_tprintf(_T("System Key: %s\n"), 
		xs::CXSByteString(cbSysKey, SysKey, _T(' ')).ToString());

	return 0;
}
#endif

int verifysyskey(int argc, TCHAR** argv)
{
	BOOL fSuccess = FALSE;

	//
	// Check Arguments 
	//

	if (argc != 1) {
		return usage();
	}

	LPCTSTR lpSysPass = argv[0];

	BOOL fVerified = FALSE;
	UINT lResult = ::NdasEncVerifySysKey(lpSysPass, &fVerified);

	if (ERROR_SUCCESS != lResult) {
		DWORD dwIntErr = ::GetLastError();
		_tprintf(_T("Verifying the system key failed")
			_T(" with error %08X (Code: %08X).\n"), lResult, dwIntErr);
		return lResult;
	}

	if (fVerified) {
		_tprintf(_T("System key matches the passphrase.\n"));
		return 0;
	} else {
		_tprintf(_T("System key does not match the passphrase.\n"));
		return 1;
	}
}

int clearsyskey(int argc, TCHAR** argv)
{
	if (0 != argc && 1 != argc) {
		return usage();
	}

	BOOL fConfirm = TRUE;
	if (1 == argc) {
		if (0 == ::lstrcmpi(_T("/q"), argv[0]) ||
			0 == ::lstrcmpi(_T("-q"), argv[0]))
		{
			fConfirm = FALSE;
		} else {
			return usage();
		}
	}

	if (fConfirm) {

		_tprintf(_T("Are you sure to delete the system key? (Y/N) "));

#if 0 
		//
		// MSVCRT.DLL (6.1 in Windows 2000) does not have _getwch().
		// But MSVCRT.DLL (7.1 in Windows XP) does.
		// Use _gettchar() instead.
		//

		TCHAR ch = _gettch();
		_tprintf(_T("%c\n"), ch);

		if (_T('Y') != ch && _T('y') != ch) {
			return 1;
		}
#else
		TCHAR ch = _gettchar();

		if (_T('Y') != ch && _T('y') != ch) 
		{
			return 1;
		}
#endif
	}

	UINT lResult = ::NdasEncRemoveSysKey();

	if (ERROR_SUCCESS != lResult) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Removing the system key failed with error %08X (Code: %08X).\n"), lResult, dwStatus);
		return lResult;
	}

	_tprintf(_T("NDAS System Key is deleted successfully.\n"));

	return 0;
}

int setdiskencrypt(int argc, TCHAR** argv)
{

	//
	// setdiskencrypt -id <deviceid> 
	//
	OPTION_VALUE opts[] = {
		{ _T("id"), 0, FALSE }, 
		{ _T("addr"), 0, FALSE},
		{ _T("un"), 0, FALSE },
		{ _T("wk"), 0, FALSE},
		{ _T("psys"), 0, FALSE},
		{ _T("pdisk"), 0, FALSE},
		{ _T("enc"), 0, FALSE},
		{ _T("len"), 0, FALSE},
		{ NULL }
	};

	POPTION_VALUE 
		pOptID				= &opts[0],
		pOptAddr			= &opts[1],
		pOptUnitNo			= &opts[2],
		pOptWriteKey		= &opts[3],
		pOptSysPassphrase	= &opts[4],
		pOptDiskPassphrase	= &opts[5],
		pOptEncMethod		= &opts[6],
		pOptEncLen			= &opts[7];

	int iResult = getopts(&argc, &argv, opts);

	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID;
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.WriteAccess = TRUE;
	// ci.PrivilegedOEMCode.UI64Value = 0;
	// ci.OEMCode.UI64Value = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;

	if (GETOPT_ERROR_SUCCESS != iResult) {
		_tprintf(_T("Error: Invalid parameter.\n"));
		return 2;
	}

	if (argc > 0) {
		_tprintf(_T("Error: Too many parameters.\n"));
		return 2;
	}

	if (pOptID->is_set && pOptAddr->is_set) {
		_tprintf(_T("Error: Both address and ID is specified.\n"));
		return 2;
	}

	if (pOptID->is_set) {

		if (::IsBadReadPtr(pOptID->pval, sizeof(ci.NdasId.Id))) {
			_tprintf(_T("Error: Device String ID is too short.\n"));
			return 2;
		}

		::CopyMemory(
			ci.NdasId.Id,
			pOptID->pval,
			sizeof(ci.NdasId.Id));

		if (!pOptWriteKey->is_set) {
			_tprintf(_T("Error: Write key is not specified.\n"));
			return 2;
		}

		if (::IsBadReadPtr(pOptWriteKey->pval, sizeof(ci.NdasId.Key))) {
			_tprintf(_T("Error: Device String ID is too short.\n"));
			return 2;
		}

		::CopyMemory(
			ci.NdasId.Key,
			pOptWriteKey->pval,
			sizeof(ci.NdasId.Key));

	} else if (pOptAddr->is_set) {
		if (::IsBadReadPtr(pOptAddr->pval, sizeof(ci.DeviceId.Node))) {
			_tprintf(_T("Error: Device Address is too short.\n"));
			return 2;
		}

		ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
		NDAS_DEVICE_ID id;
		BOOL fParsed = pParseNdasDeviceID(id, pOptAddr->pval);
		if (!fParsed) {
			_tprintf(_T("Invalid MAC Address.\n"));
			return 2;
		}

		::CopyMemory(
			ci.DeviceId.Node,
			id.Node,
			sizeof(ci.DeviceId.Node));

	} else {
		_tprintf(_T("Error: Connection String is not specified.\n"));
		return 2;
	}

	if (pOptUnitNo->is_set) {
		int unitNo = 0;
		BOOL fSuccess = ::StrToIntEx(pOptUnitNo->pval,STIF_DEFAULT, &unitNo);
		if (!fSuccess) {
			_tprintf(_T("Error: Invalid unit number.\n"));
			return 2;
		}
		ci.UnitNo = unitNo;
	}

	if (!pOptEncMethod->is_set) {
		_tprintf(_T("Error: Encryption is not specified.\n"));
		return 2;
	}

	DWORD ceEncryption = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
	DWORD ceKeyLength = 0;

	if (0 == ::lstrcmpi(_T("NONE"), pOptEncMethod->pval)) {

		ceEncryption = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
		ceKeyLength = 0;
		
		BOOL fSuccess = ::NdasCommInitialize();
		if (!fSuccess)
		{
			_tprintf(_T("Initializing NDAS Communication module failed.\n"));
			pShowErrorMessage();
			return 255;
		}

		HNDAS hNdasConn = ::NdasCommConnect(&ci, 0, NULL);
		if (NULL == hNdasConn) {
			_tprintf(_T("Initializing NDAS connection failed.\n"));
			pShowErrorMessage();
			return 255;
		}

		NDAS_CONTENT_ENCRYPT_BLOCK ceb = {0};

		fSuccess = ::NdasCommBlockDeviceWriteSafeBuffer(
			hNdasConn, 
			NDAS_BLOCK_LOCATION_ENCRYPT, 
			1, 
			(const BYTE*)&ceb);

		if (!fSuccess) {
			fSuccess = ::NdasCommDisconnect(hNdasConn);
			_ASSERTE(fSuccess);

			_tprintf(_T("Clearing content encrypt information failed.\n"));
			pShowErrorMessage();
			return 255;
		}

		fSuccess = ::NdasCommDisconnect(hNdasConn);
		_ASSERTE(fSuccess);

		_tprintf(_T("Content Encrypt Information is cleared from the disk.\n"));

		goto ndas_hix_notify;

	} else if (0 == ::lstrcmpi(_T("SIMPLE"), pOptEncMethod->pval)) {
		ceEncryption = NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE;
		ceKeyLength = 4; // 32bit
	} else if (0 == ::lstrcmpi(_T("AES"), pOptEncMethod->pval)) {
		ceEncryption = NDAS_CONTENT_ENCRYPT_METHOD_AES;
		ceKeyLength = 16; // 128bit
	} else {
		_tprintf(_T("Error: Invalid Encryption.\n"));
		return 2;
	}

	if (pOptEncLen->is_set) {
		DWORD keylen;
		BOOL fConv = ::StrToIntEx(pOptEncLen->pval, STIF_DEFAULT, (int*)&keylen);
		ceKeyLength = keylen / 8;
		if (!fConv || !::NdasEncIsValidKeyLength(ceEncryption, ceKeyLength)) {
			_tprintf(_T("Error: Invalid Key Length.\n"));
			return 2;
		}
	}

	if (!pOptSysPassphrase->is_set) {
		_tprintf(_T("Error: System Passphrase is not specified.\n"));
		return 2;
	}

	if (!pOptDiskPassphrase->is_set) {
		_tprintf(_T("Error: Disk Passphrase is not specified.\n"));
		return 2;
	}

	{
		BYTE syskey[16] = {0};
		BYTE diskkey[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH] = {0};
		NDAS_CONTENT_ENCRYPT_BLOCK ceb = {0};

		UINT uiRet = ::NdasEncCreateSysKey(pOptSysPassphrase->pval, syskey);

		if (ERROR_SUCCESS != uiRet) {
			_tprintf(_T("Key generation failed  at phase 1 (Code: %08X)\n"), uiRet);
			pShowErrorMessage();
			return uiRet;
		}

		uiRet = ::NdasEncCreateKey(pOptDiskPassphrase->pval, ceKeyLength, diskkey);

		if (ERROR_SUCCESS != uiRet) {
			_tprintf(_T("Key generation failed at phase 2 (Code: %08X)\n"), uiRet);
			pShowErrorMessage();
			return uiRet;
		}

		uiRet = ::NdasEncCreateContentEncryptBlock(
			syskey,
			sizeof(syskey),
			diskkey,
			ceKeyLength,
			ceEncryption,
			&ceb);

		if (ERROR_SUCCESS != uiRet) {
			_tprintf(_T("Key generation failed at phase 3 (Code: %08X)"), uiRet);
			pShowErrorMessage();
			return uiRet;
		}

		BOOL fSuccess = ::NdasCommInitialize();
		if (!fSuccess)
		{
			_tprintf(_T("Initializing NDAS Communication module failed.\n"));
			pShowErrorMessage();
			return 255;
		}

		HNDAS hNdasConn = ::NdasCommConnect(&ci, 0, NULL);
		
		if (NULL == hNdasConn) {
			_tprintf(_T("Initializing a connection failed.\n"));
			pShowErrorMessage();
			return 255;
		}

		fSuccess = ::NdasCommBlockDeviceWriteSafeBuffer(
			hNdasConn,
			NDAS_BLOCK_LOCATION_ENCRYPT,
			1,
			(const BYTE*) &ceb);

		if (!fSuccess) {
			_tprintf(_T("Connection to the device failed.\n"));
			pShowErrorMessage();

			fSuccess = ::NdasCommDisconnect(hNdasConn);
			_ASSERTE(fSuccess);

			return 255;
		}

		fSuccess = ::NdasCommDisconnect(hNdasConn);
		_ASSERTE(fSuccess);

#ifdef _DEBUG
		_tprintf(_T("Generated Data\n%s\n"), 
			xs::CXSByteString(sizeof(ceb), (CONST BYTE*)&ceb, _T(' ')).ToString());
#endif

	}

ndas_hix_notify:

	if (NDASCOMM_CIT_DEVICE_ID == ci.AddressType)
	{
		NDAS_UNITDEVICE_ID unitDeviceID = {0};
		C_ASSERT(
			sizeof(unitDeviceID.DeviceId.Node) == 
			sizeof(ci.DeviceId.Node));
		::CopyMemory(
			unitDeviceID.DeviceId.Node, 
			ci.DeviceId.Node, 
			sizeof(unitDeviceID.DeviceId.Node));
		unitDeviceID.UnitNo = ci.UnitNo;

		_tprintf(_T("Notifying disk changes... ")); 
		BOOL fSuccess = ::NdasHixNotifyUnitDeviceChange(&unitDeviceID);
		if (!fSuccess)
		{
			_tprintf(_T("failed with error %08X\n"), ::GetLastError());
		}
		else
		{
			_tprintf(_T("Done.\n"));
		}
	}
	else
	{
		_tprintf(_T("WARNING: Sending a notification of the changes failed.\n")
			_T("You should reset the connection to the NDAS device\n")
			_T("from property page.\n")
			_T("If you want to notify the change, use MAC ADDRESS.\n")
			);
	}

	return 0;
}

int getdiskencrypt(int argc, TCHAR** argv)
{
	//
	// getdiskencrypt -id AAAAABBBBBCCCCCDDDDD [-un 0]
	// getdiskencrypt -addr 000BD0CCEEDD [-un 0]
	//

	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_NDAS_ID;
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.WriteAccess = FALSE;
	// ci.PrivilegedOEMCode.UI64Value = 0;
	// ci.OEMCode.UI64Value = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;

	OPTION_VALUE opts[] = {
		{ _T("id"), 0, FALSE }, 
		{ _T("addr"), 0, FALSE},
		{ _T("un"), 0, FALSE },
		{ _T("syskey"), 0, FALSE },
		{ NULL }
	};

	POPTION_VALUE 
		pOptID = &opts[0], 
		pOptAddr = &opts[1],
		pOptUnitNo = &opts[2],
		pOptSysKey = &opts[3];

	int iResult = getopts(&argc, &argv, opts);

	if (GETOPT_ERROR_SUCCESS != iResult) {
		_tprintf(_T("Error: Invalid parameter.\n"));
		return 2;
	}

	if (argc > 0) {
		_tprintf(_T("Error: Too many parameters.\n"));
		return 2;
	}

	if (!pOptID->is_set && !pOptAddr->is_set) {
		_tprintf(_T("Error: No connection information is specified.\n"));
		return 2;
	}

	if (pOptID->is_set && pOptAddr->is_set) {
		_tprintf(_T("Error: Both address and ID is specified.\n"));
		return 2;
	}

	if (pOptID->is_set) {

		if (::IsBadReadPtr(pOptID->pval, sizeof(ci.NdasId.Id))) {
			_tprintf(_T("Error: Device String ID is too short.\n"));
			return 2;
		}

		::CopyMemory(
			ci.NdasId.Id,
			pOptID->pval,
			sizeof(ci.NdasId.Id));

	} else if (pOptAddr->is_set) {
		if (::IsBadReadPtr(pOptAddr->pval, sizeof(TCHAR) * 6)) {
			_tprintf(_T("Error: Device Address is too short.\n"));
			return 2;
		}

		ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
		NDAS_DEVICE_ID id;
		BOOL fParsed = pParseNdasDeviceID(id, pOptAddr->pval);
		if (!fParsed) {
			_tprintf(_T("Invalid MAC Address.\n"));
			return 2;
		}

		::CopyMemory(
			ci.DeviceId.Node,
			id.Node,
			sizeof(ci.DeviceId.Node));
	}

	if (pOptUnitNo->is_set) {
		int unitNo = 0;
		BOOL fSuccess = ::StrToIntEx(pOptUnitNo->pval,STIF_DEFAULT, &unitNo);
		if (!fSuccess) {
			_tprintf(_T("Error: Invalid unit number.\n"));
			return 2;
		}
		ci.UnitNo = unitNo;
	}

	if (pOptSysKey->is_set && NULL == pOptSysKey->pval)
	{
		_tprintf(_T("Error: No system key passphrase is specified.\n"));
		return 2;
	}

	NDAS_CONTENT_ENCRYPT_BLOCK ceb = {0};

	BOOL fSuccess = ::NdasCommInitialize();
	if (!fSuccess)
	{
		_tprintf(_T("Initializing NDAS Communication module failed.\n"));
		pShowErrorMessage();
		return 255;
	}

	HNDAS hNdasConn = ::NdasCommConnect(&ci, 0, NULL);
	if (NULL == hNdasConn) {
		_tprintf(_T("Initializing device connection failed.\n"));
		pShowErrorMessage();
		return 255;
	}

	fSuccess = ::NdasCommBlockDeviceRead(
		hNdasConn, 
		NDAS_BLOCK_LOCATION_ENCRYPT, 
		1, 
		(BYTE*) &ceb);

	if (!fSuccess) {
		_tprintf(_T("Communication to the device failed.\n"));
		pShowErrorMessage();

		fSuccess = ::NdasCommDisconnect(hNdasConn);
		_ASSERTE(fSuccess);

		return 255;
	}

	UINT uiRet = ::NdasEncVerifyContentEncryptBlock(&ceb);
	
	if (ERROR_SUCCESS != uiRet) {
		_tprintf(_T("Disk does not contain valid encryption information (Code: %08X).\n"), uiRet);

		fSuccess = ::NdasCommDisconnect(hNdasConn);
		_ASSERTE(fSuccess);

		return 255;
	}

	fSuccess = ::NdasCommDisconnect(hNdasConn);
	_ASSERTE(fSuccess);

	_tprintf(_T("Revision  : %08X\n"), ceb.Revision);
	_tprintf(_T("Key Length: %u bits\n"), ceb.KeyLength * 8);
	_tprintf(_T("Encryption: %s\n"), pNdasContentEncryptMethodString(ceb.Method));
#ifdef _DEBUG
	_tprintf(_T("Key       : %s\n"), xs::CXSByteString(ceb.KeyLength, ceb.Key, _T(' ')).ToString());
#endif

	// If syskey is specified, verify it against CEB
	if (pOptSysKey->is_set)
	{
		BYTE pbSysKey[16] = {0};
		DWORD cbSysKey = sizeof(pbSysKey);
		UINT uiRes;

		if (_T('*') == *pOptSysKey->pval)
		{
			uiRes = ::NdasEncGetSysKey(cbSysKey, pbSysKey, &cbSysKey);
			if (ERROR_SUCCESS != uiRes)
			{
				_tprintf(_T("WARNING: Unable to retrieve a system key: Error %d\n"), uiRes);
				pShowErrorMessage(uiRes);
				return 1;
			}
		}
		else
		{
			uiRes = ::NdasEncCreateSysKey(
				pOptSysKey->pval,
				pbSysKey);
			if (ERROR_SUCCESS != uiRes) 
			{
				_tprintf(_T("WARNING: Unable to create a system key: Error %d\n"), uiRes);
				pShowErrorMessage(uiRes);
				return 1;
			}
		}

		uiRes = ::NdasEncVerifyFingerprintCEB(pbSysKey, cbSysKey, &ceb);
		if (ERROR_SUCCESS != uiRes)
		{
			if (NDASENC_ERROR_CEB_INVALID_FINGERPRINT == uiRes)
			{
				_tprintf(_T("System Key: NOT VALID\n"));
			}
			else
			{
				_tprintf(_T("WARNING: Unable to verify the system key : Error %d\n"), uiRes);
				pShowErrorMessage(uiRes);
			}
		}
		else
		{
			_tprintf(_T("System Key: VERIFIED\n"));
		}
	}
	else
	{
	}

	return 0;
}

int createsyskeyfile(int argc, TCHAR** argv)
{
	if (argc != 2) {
		_tprintf(_T("Error: Invalid parameter.\n"));
		return 2;
	}

	UINT uiRes = ::NdasEncCreateSysKeyFile(argv[0], argv[1]);
	if (ERROR_SUCCESS != uiRes) {
		_tprintf(_T("Creating a system key file failed ")
			_T("with error %08X (Code: %08X)\n"), uiRes, ::GetLastError());
		return uiRes;
	}

	_tprintf(_T("NDAS System Key file is created as %s.\n"), argv[0]);
	return 0;
}

int importsyskeyfile(int argc, TCHAR** argv)
{
	if (argc != 1) {
		_tprintf(_T("Error: Invalid parameter.\n"));
		return 2;
	}

	LPCTSTR lpFileName = argv[0];

	UINT uiRes = ::NdasEncImportSysKeyFromFile(lpFileName);

	if (NDASENC_ERROR_INVALID_SYSKEY_FILE == uiRes) {
		_tprintf(_T("Invalid system key file.\n"));
		return uiRes;
	}
	if (ERROR_SUCCESS != uiRes) {
		_tprintf(_T("Importing a system key file (%s) failed ")
			_T("with error %08X (Code: %08X)\n"), lpFileName, uiRes, ::GetLastError());
		return uiRes;
	}

	BOOL fSuccess = pNotifySysKeyChange();
	if (!fSuccess)
	{
		_tprintf(
			_T("WARNING!\n")
			_T("Resetting a system key in the NDAS service (Error: %08X)\n")
			_T("You may have to restart the NDAS service to apply the new system key\n")
			_T("\n"),
			::GetLastError());
	}

	_tprintf(_T("NDAS System Key is imported successfully from %s.\n"), lpFileName);

	return 0;
}

int exportsyskeyfile(int argc, TCHAR** argv)
{
	if (argc != 1) {
		_tprintf(_T("Error: Invalid parameter.\n"));
		return 2;
	}

	UINT uiRes = ::NdasEncExportSysKeyToFile(argv[0]);
	if (ERROR_SUCCESS != uiRes) {
		_tprintf(_T("Creating a system key file failed ")
			_T("with error %08X (Code: %08X)\n"), uiRes, ::GetLastError());
		return uiRes;
	}

	_tprintf(_T("NDAS System Key file is exported as %s.\n"), argv[0]);

	return 0;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	typedef int (*SUBCMDPROC)(int argc, TCHAR** argv);

	typedef struct _SUBCMDSPEC {
		LPCTSTR szCommandString;
		SUBCMDPROC pfn;
	} SUBCMDSPEC, *PSUBCMDSPEC;

	SUBCMDSPEC SubCmds[] = {
		{ _T("createsyskeyfile"), createsyskeyfile },
		{ _T("importsyskeyfile"), importsyskeyfile },
		{ _T("setsyskey"), setsyskey },
		{ _T("verifysyskey"), verifysyskey },
#ifdef _DEBUG
		{ _T("exportsyskeyfilefile"), exportsyskeyfile },
		{ _T("getsyskey"), getsyskey },
#endif
		{ _T("getdiskencrypt"), getdiskencrypt },
		{ _T("setdiskencrypt"), setdiskencrypt },
		{ _T("clearsyskey"), clearsyskey }
	};

	static const size_t nSubCmds = sizeof(SubCmds) / sizeof(SubCmds[0]);

	header();
	
	if (0 != ndascomm_api_version_check())
	{
		return 250;
	}

	BOOL fSuccess = FALSE;

	if (argc < 2) {
		return usage();
	}

	for (size_t i = 0; i < nSubCmds; ++i) {
		if (0 == ::lstrcmpi(SubCmds[i].szCommandString, argv[1])) {
			int iRet = SubCmds[i].pfn(argc - 2, argv + 2);
#ifdef _DEBUG
			_tprintf(_T("\nDEBUG: command returned: %d\n"), iRet);
#endif
			return iRet;
		}
	}

	return usage();
}

static VOID 
pShowErrorMessage(DWORD dwError)
{
	AutoHModule hModule = ::LoadLibraryEx(
		_T("ndasmsg.dll"),
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	LPTSTR lpszErrorMessage = NULL;

	if (dwError & APPLICATION_ERROR_MASK) {
		if (NULL != (HMODULE) hModule) {
			INT iChars = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				(HMODULE) hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}
	} else {
		INT iChars = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwError,
			0,
			(LPTSTR) &lpszErrorMessage,
			0,
			NULL);
	}

	if (NULL != lpszErrorMessage) {
		_tprintf(_T("Error : %u(%08X) : %s\n"), dwError, dwError, lpszErrorMessage);
		::LocalFree(lpszErrorMessage);
	} else {
		_tprintf(_T("Unknown error: %u(%08X)\n"), dwError, dwError);
	}
}

static BOOL 
pParseNdasDeviceID(NDAS_DEVICE_ID& deviceID, LPCTSTR lpszHexString)
{
	NDAS_DEVICE_ID id = {0};

	LPCTSTR p = lpszHexString;
	for (DWORD i = 0; i < 6; ++i) 
	{
		if (_T('\0') == *p ) return FALSE;

		BYTE b_low = 0, b_high = 0;
		BOOL fSuccess = pHexCharToByte(*p++, b_high) && pHexCharToByte(*p++, b_low);
		if (!fSuccess) return FALSE;

		// delimiter
		if (_T('-') == *p || _T(':') == *p) ++p;

		id.Node[i] = (b_high << 4) + b_low;
	}

	if (_T('\0') != *p) return FALSE;

#ifdef _DEBUG
	_tprintf(_T("%02X:%02X:%02X:%02X:%02X:%02X\n"), 
		id.Node[0], id.Node[1], id.Node[2], 
		id.Node[3], id.Node[4], id.Node[5]);
#endif

	deviceID = id;
	return TRUE;
}

static BOOL
pHexCharToByte(TCHAR ch, BYTE& bval)
{
	if (ch >= _T('0') && ch <= '9') { 
		bval = ch - _T('0'); return TRUE; 
	}
	else if (ch >= _T('A') && ch <= 'F') { 
		bval = ch - _T('A') + 0xA; return TRUE; 
	}
	else if (ch >= _T('a') && ch <= 'f') { 
		bval = ch - _T('a') + 0xA; return TRUE; 
	}
	else return FALSE;
}

static LPCTSTR 
pNdasContentEncryptMethodString(unsigned _int16 method)
{
	static TCHAR buf[30] = {0};

	switch (method) {
	case NDAS_CONTENT_ENCRYPT_METHOD_NONE:		return _T("None");
	case NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE:	return _T("Simple");
	case NDAS_CONTENT_ENCRYPT_METHOD_AES:		return _T("AES");
	default: 
		{
			HRESULT hr = ::StringCchPrintf(
				buf, 
				RTL_NUMBER_OF(buf), 
				_T("Unknown (%08X)"), 
				method);
			return buf;
		}
	}
}

//
// Notify to NDAS service to reset the system key
//
#include "ndas/ndasuser.h"

static BOOL pNotifySysKeyChange()
{
	NDAS_SERVICE_PARAM param = {0};
	param.ParamCode = NDASSVC_PARAM_RESET_SYSKEY;
	return ::NdasSetServiceParam(&param);
}
