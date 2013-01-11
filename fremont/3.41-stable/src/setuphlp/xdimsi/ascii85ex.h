#pragma once

HRESULT
Ascii85EncodeW(
	__in PVOID Data,
	__in DWORD DataLength,
	__out LPWSTR* StringData,
	__out_opt LPDWORD StringDataLength);

HRESULT
Ascii85DecodeW(
	__in LPCWSTR StringData,
	__out PVOID* BinaryData,
	__out_opt LPDWORD BinaryDataLength);
