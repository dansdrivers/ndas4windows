#pragma once

BOOL ValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId, LPCWSTR lpszDeviceStringKey = NULL, PBOOL pbWritable = NULL);

BOOL ValidateStringIdKeyA(
	LPCSTR lpszDeviceStringId, LPCSTR lpszDeviceStringKey = NULL, PBOOL pbWritable = NULL);

BOOL ConvertStringIdToRealIdW(
	LPCWSTR lpszDeviceStringId, PNDAS_DEVICE_ID pDeviceId);

BOOL ConvertStringIdToRealIdA(
	LPCSTR lpszDeviceStringId, PNDAS_DEVICE_ID pDeviceId);

BOOL ConvertRealIdToStringIdW(
	const PNDAS_DEVICE_ID pDeviceId, LPWSTR lpszDeviceStringId);

BOOL ConvertRealIdToStringIdA(
	const PNDAS_DEVICE_ID pDeviceId, LPSTR lpszDeviceStringId);
