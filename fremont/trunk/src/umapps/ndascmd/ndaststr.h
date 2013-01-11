#pragma once

LPTSTR ndas_device_status_to_string(NDAS_DEVICE_STATUS status, LPTSTR buf, DWORD buflen);
LPTSTR ndas_logicaldevice_status_to_string(NDAS_LOGICALDEVICE_STATUS status, LPTSTR buf, DWORD buflen);
LPTSTR ndas_unitdevice_type_to_string(NDAS_UNITDEVICE_TYPE type, LPTSTR buf, DWORD buflen);
LPTSTR ndas_logicaldevice_type_to_string(NDAS_LOGICALDEVICE_TYPE type,	LPTSTR buf, DWORD buflen);
LPTSTR ndas_logicaldevice_error_to_string(NDAS_LOGICALDEVICE_ERROR err, LPTSTR buf, DWORD buflen);
LPTSTR ndas_device_error_to_string(NDAS_DEVICE_ERROR err, LPTSTR buf, DWORD buflen);

LPTSTR veto_type_string(PNP_VETO_TYPE type, LPTSTR buf, UINT buflen);

HRESULT blocksize_to_string(LPTSTR buf, DWORD buflen, UINT64 dwBlocks);
HRESULT guid_to_string(LPTSTR buf, DWORD buflen, LPCGUID lpGuid);
HRESULT ndas_device_id_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID);
