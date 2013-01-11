#include "devstr.h"

/* Normalize device string ID by removing delimiters, up to
 * cchDeviceStringId characters. 
 *
 * This function returns the number of Device String ID characters put
 * in lpDeviceStringId.
 */

DWORD
normalize_device_string_id(
	LPTSTR lpDeviceStringId,
	DWORD cchDeviceStringId,
	LPCTSTR lpString)
{
	DWORD i = 0;
	LPCTSTR lp = lpString;

	/* we need 'cchDeviceStringId' characters excluding delimiters */
	for (i = 0; *lp != _T('\0') && i < cchDeviceStringId; ++i, ++lp)
	{
		if (i > 0 && i % 5 == 0)
		{
			/* '-' or ' ' are accepted as a delimiter */
			if (_T('-') == *lp ||
				_T(' ') == *lp)
			{
				++lp;
			}
		}

		if ((*lp >= _T('0') && *lp <= _T('9')) ||
			(*lp >= _T('a') && *lp <= _T('z')) ||
			(*lp >= _T('A') && *lp <= _T('Z')))
		{
			lpDeviceStringId[i] = *lp;
		}
		else
		{
			/* return as error */
			return i;
		}
	}

	return i;
}
