#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <ndas/ndasid.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>

#include "addrstr.h"

#define NDASOC_USE_DISCONNECT
//
// CAUTION: 
// CUSTOM_OEM_CODE is disabled by default.
// If encryption is enabled (1.0 by default), and if you forget the OEM code
// there is no way to reset the OEM code. 
// 
// #define NDASOC_ALLOW_CUSTOM_OEM_CODE
//

BOOL
hex_char_to_byte(TCHAR ch, BYTE& bval)
{
	if (ch >= _T('0') && ch <= '9') 
	{ 
		bval = ch - _T('0'); 
		return TRUE; 
	}
	else if (ch >= _T('A') && ch <= 'F') 
	{ 
		bval = ch - _T('A') + 0xA; 
		return TRUE; 
	}
	else if (ch >= _T('a') && ch <= 'f') 
	{ 
		bval = ch - _T('a') + 0xA; 
		return TRUE; 
	}
	else return FALSE;
}

#if 0

BOOL 
parse_ndas_device_id(NDAS_DEVICE_ID& deviceID, LPCTSTR lpszHexString)
{
	NDAS_DEVICE_ID id = {0};
	LPCTSTR p = lpszHexString;
	for (DWORD i = 0; i < 6; ++i) 
	{
		if (_T('\0') == *p )
		{
			return FALSE;
		}

		BYTE low = 0, high = 0;
		BOOL fSuccess = 
			hex_char_to_byte(*p++, high) && 
			hex_char_to_byte(*p++, low);
		if (!fSuccess) 
		{
			return FALSE;
		}

		// delimiter
		if (_T('-') == *p || _T(':') == *p)
		{
			++p;
		}

		id.Node[i] = (high << 4) + low;
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

#endif

DWORD
normalize_device_string_id (
	LPTSTR	lpDeviceStringId,
	DWORD	cchDeviceStringId,
	LPCTSTR lpString
	)
{
	DWORD	i = 0;
	LPCTSTR lp = lpString;

	/* we need 'cchDeviceStringId' characters excluding delimiters */
	
	for (i = 0; *lp != _T('\0') && i < cchDeviceStringId; ++i, ++lp) {

		if (i > 0 && i % 5 == 0) {

			/* '-' or ' ' are accepted as a delimiter */
			
			if (_T('-') == *lp || _T(' ') == *lp) {

				++lp;
			}
		}

		if ((*lp >= _T('0') && *lp <= _T('9')) ||
			(*lp >= _T('a') && *lp <= _T('z')) ||
			(*lp >= _T('A') && *lp <= _T('Z'))) {

			lpDeviceStringId[i] = *lp;
		
		} else {

			/* return as error */
			
			return i;
		}
	}

	return i;
}

BOOL
ParseNdasDeviceId (
	NDAS_DEVICE_ID	&deviceID, 
	LPCTSTR			lpszHexString
	)
{
	BOOL			result;

	NDAS_DEVICE_ID	id = {0};
	TCHAR			szDeviceID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};


	if (normalize_device_string_id(szDeviceID, NDAS_DEVICE_STRING_ID_LEN, lpszHexString) != NDAS_DEVICE_STRING_ID_LEN) {

		return FALSE;
	}

	result = NdasIdStringToDevice( szDeviceID, &id );

#ifdef _DEBUG

	if (result == TRUE) {

		_tprintf( _T("%02X:%02X:%02X:%02X:%02X:%02X\n"), 
				  id.Node[0], id.Node[1], id.Node[2], id.Node[3], id.Node[4], id.Node[5] );
	}

#endif

	deviceID = id;

	return TRUE;
}

int check_ndas_api_version()
{
	DWORD ncApiVersion = NdasCommGetAPIVersion();
	if (NDASCOMM_API_VERSION != ncApiVersion)
	{
		_tprintf(_T("Incompatible NDASCOMM API %d.%d is loaded.\n")
			_T("This application is linked against API %d.%d."),
			LOWORD(ncApiVersion), HIWORD(ncApiVersion),
			NDASCOMM_API_VERSION_MAJOR, NDASCOMM_API_VERSION_MINOR);
		return 1;
	}
	return 0;
}

int reset_device(
	HNDAS hNdasComm)
{
	NDASCOMM_VCMD_PARAM vp = {0};

	_tprintf(_T("VC_RESET..."));

	BOOL fSuccess = NdasCommVendorCommand(
		hNdasComm,
		ndascomm_vcmd_reset,
		&vp,
		NULL, 0,
		NULL, 0);

	if(!fSuccess)
	{
		_tprintf(_T("Error\nCommand failed with error %08X (%u)\n"), GetLastError(), GetLastError());
		return 1;
	}

	_tprintf(_T("Done\n"));
	return 0;
}

int set_oem_code(
	HNDAS hNdasComm, 
	const NDAS_OEM_CODE* newOemCode)
{
	NDASCOMM_VCMD_PARAM vp = {0};

	C_ASSERT(
		sizeof(newOemCode->Bytes) == 
		sizeof(vp.SET_USER_PW.UserPassword));

	CopyMemory(
		vp.SET_USER_PW.UserPassword,
		newOemCode->Bytes,
		sizeof(vp.SET_USER_PW.UserPassword));

	_tprintf(_T("VC_SET_USER_PW..."));

	BOOL fSuccess = NdasCommVendorCommand(
		hNdasComm, 
		ndascomm_vcmd_set_user_pw, 
		&vp, 
		NULL, 0, 
		NULL, 0);

	if(!fSuccess)
	{
		if(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED != GetLastError())
		{
			_tprintf(_T("Error\nCommand failed with error %08X (%u)\n"), GetLastError(), GetLastError());
		}
		else
		{
			_tprintf(_T("Error\nUnsupported hardware version\n"));
		}
		return 1;
	}

	_tprintf(_T("Done\n"));
	return 0;
}

const NDAS_OEM_CODE NDAS_PRIVILEGED_OEM_CODE_DEFAULT = { 0X1E, 0X13, 0X50, 0X47, 0X1A, 0X32, 0X2B, 0X3E };

const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE			 = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT			 = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE			 = { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };
const NDAS_OEM_CODE NDAS_OEM_CODE_WINDOWS_RO		 = { 0xBF, 0x57, 0x53, 0x48, 0x1F, 0x33, 0x7B, 0x3F };
const NDAS_OEM_CODE NDAS_OEM_CODE_AUTO_REGISTRY		 = { 0xCE, 0xD0, 0x6E, 0x3C, 0x2F, 0x6A, 0x3C, 0x5E };

const struct {

	LPCTSTR				Name;
	NDAS_VID			NdasVid;
	const NDAS_OEM_CODE	&OemCode; 

} OemCodeTable[] = {

	{ _T("default"),	NDAS_VID_DEFAULT,		NDAS_OEM_CODE_DEFAULT },
	{ _T("sample"),		NDAS_VID_DEFAULT,		NDAS_OEM_CODE_SAMPLE },
	{ _T("pccam"),		NDAS_VID_WINDWOS_RO,	NDAS_OEM_CODE_WINDOWS_RO },
	{ _T("seagate"),	NDAS_VID_SEAGATE,		NDAS_OEM_CODE_SEAGATE },
	{ _T("autoreg"),	NDAS_VID_AUTO_REGISTRY,	NDAS_OEM_CODE_AUTO_REGISTRY },
	{ NULL,				NDAS_VID_PNP,			NDAS_OEM_CODE_DEFAULT }
};

int usage() {

	_tprintf( _T("usage: ndaschangevid <ndas-id> <new-oem-code>\n")
			  _T("\n")
			  _T(" <ndas-id>   : e.g. AAAA-BBBB-CCCC-DDDD\n")
			  _T(" <oem-code>  : ") );

	_tprintf(_T("%s"), OemCodeTable[0].Name);

	for (int i = 1; i < RTL_NUMBER_OF(OemCodeTable); ++i) {

		if (OemCodeTable[i].NdasVid == NDAS_VID_PNP) {

			continue;
		}

		_tprintf( _T(", %s"), OemCodeTable[i].Name );
	}

#ifdef NDASOC_ALLOW_CUSTOM_OEM_CODE
	_tprintf( _T(", or 00:00:00:00:00:00:00:00") );
#endif
	_tprintf( _T("\n") );

	return 1;
}


int __cdecl _tmain(int argc, TCHAR** argv)
{
	// API Version Check

	if (check_ndas_api_version() != 0) {

		return 1;
	}

    BOOL success = NdasCommInitialize();

    if (!success) {

        _tprintf( _T("NdasCommInitialization failed\n") );
        return 1;
    }

	if (argc != 3) {

		return usage();
	}

	NDAS_DEVICE_ID deviceId = {0};

	if (ParseNdasDeviceId(deviceId, argv[1]) == FALSE) {

		_tprintf( _T("Device ID format is invalid.\n") );
		return usage();
	}

	int i;

#if 0

	NDAS_OEM_CODE currentOemCode;

	for (i = 0; i < RTL_NUMBER_OF(OemCodeTable); ++i) {

		if (lstrcmpi(argv[2], OemCodeTable[i].Name) == 0) {

			currentOemCode = OemCodeTable[i].OemCode;
			break;
		}
	}

	if (i >= RTL_NUMBER_OF(OemCodeTable)) {

		_tprintf( _T("Current OEM Code Name is invalid.\n") );
		return usage();
	}

#endif

	NDAS_OEM_CODE newOemCode;

	for (i = 0; i < RTL_NUMBER_OF(OemCodeTable); ++i) {

		if (0 == lstrcmpi(argv[2], OemCodeTable[i].Name)) {

			newOemCode = OemCodeTable[i].OemCode;

			_tprintf( _T("New OEM Code is %X\n"), newOemCode.Bytes[0] );
			break;
		}
	}

	if (i >= RTL_NUMBER_OF(OemCodeTable)) {

		_tprintf( _T("New OEM Code Name is invalid.\n") );
		return usage();
	}

	NDASCOMM_CONNECTION_INFO nci;
	HNDAS ndasCommHandle;

	i = 0;

	while (OemCodeTable[i].NdasVid != NDAS_VID_PNP) {

		RtlZeroMemory( &nci, sizeof(NDASCOMM_CONNECTION_INFO) );

		nci.Size		= sizeof(NDASCOMM_CONNECTION_INFO);
		nci.Flags		= NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;
		nci.LoginType	= NDASCOMM_LOGIN_TYPE_NORMAL;
		nci.UnitNo		= 0;
		nci.WriteAccess = TRUE;
		nci.Protocol	= NDASCOMM_TRANSPORT_LPX;

		nci.PrivilegedOEMCode	= NDAS_PRIVILEGED_OEM_CODE_DEFAULT;		// privileged mode so it always success
		nci.OEMCode				= OemCodeTable[i].OemCode;
		nci.AddressType			= NDASCOMM_CIT_DEVICE_ID;
		nci.Address.DeviceId	= deviceId;
		nci.ReceiveTimeout		= 1000; /* we do not access hard disk, so we can use the shorter timeout */

		_tprintf( _T("Connecting...\n") );

		ndasCommHandle = NdasCommConnect(&nci);

		_tprintf( _T("\r              \r") );

		if (ndasCommHandle != NULL) {

			_tprintf( _T("Connected!\n") );
			break;
		}

		_tprintf(_T("NdasCommConnect failed, error=0x%X\n"), GetLastError());
		(void) NdasCommUninitialize();

		i ++;
		continue;
	}

	if (OemCodeTable[i].NdasVid == NDAS_VID_PNP) {

		return 1;
	}

	_tprintf( _T("WARNING: If the current OEM code is invalid,")
			  _T(" the current process will be stalled and")
			  _T(" it will take several minutes to stop it!\n") );

	int retcode = set_oem_code(ndasCommHandle, &newOemCode);

	if (retcode == 0) {

		reset_device(ndasCommHandle);
	}

	/* Disconnection may fail! */

	_tprintf(_T("Disconnecting (It takes a while.)..."));

	success = NdasCommDisconnect(ndasCommHandle);

	_tprintf(_T("\r                                      \r"));

	if (success == FALSE) {

		_tprintf( _T("Disconnect failed, error=0x%X\n"), GetLastError() );
	
	} else {

		_tprintf( _T("Disconnected\n") );
	}

	_tprintf( _T("*** Power-cycle the NDAS device to ensure proper operation! ***\n") );

    (void) NdasCommUninitialize();

	return retcode;
}
