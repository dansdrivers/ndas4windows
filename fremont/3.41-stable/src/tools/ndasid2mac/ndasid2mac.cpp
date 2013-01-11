#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <ndas/ndasid.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasmsg.h>


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

	deviceID = id;

	return TRUE;
}


int usage() {

	_tprintf( _T("usage: ndasoemcode <ndas-id>\n")
			  _T("\n")
			  _T(" <ndas-id>   : e.g. AAAA-BBBB-CCCC-DDDD\n") );

	return 1;
}


int __cdecl _tmain(int argc, TCHAR** argv)
{
	if (argc != 2) {

		return usage();
	}

	NDAS_DEVICE_ID deviceId = {0};

	if (ParseNdasDeviceId(deviceId, argv[1]) == FALSE) {

		_tprintf( _T("Device ID format is invalid.\n") );
		return usage();
	}

	_tprintf( _T("%02X:%02X:%02X:%02X:%02X:%02X\n"), 
				deviceId.Node[0], deviceId.Node[1], deviceId.Node[2], deviceId.Node[3], deviceId.Node[4], deviceId.Node[5] );

	return 0;
}
