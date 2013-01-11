#include <windows.h>
#include <tchar.h>

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
	LPCTSTR lpString);
