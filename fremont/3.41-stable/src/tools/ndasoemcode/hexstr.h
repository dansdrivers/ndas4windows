#pragma once
#include <windows.h>

/* Parse the Hex String into Byte Array.
 * Returns number of bytes stored in Value.
 */

DWORD
string_to_hex(
	LPCTSTR HexString,
	BYTE* Value,
	DWORD ValueLength);

