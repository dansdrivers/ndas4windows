#pragma once

#include <windows.h>
#include <tchar.h>

int ndascmd_register_device(int argc, TCHAR** argv);
int ndascmd_register_device_ex(int argc, TCHAR** argv);
int ndascmd_unregister_device(int argc, TCHAR** argv);

int ndascmd_mount(int argc, TCHAR** argv);
int ndascmd_mount_ex(int argc, TCHAR** argv);
int ndascmd_unmount(int argc, TCHAR** argv);
int ndascmd_unmount_ex(int argc, TCHAR** argv);
int ndascmd_recover(int argc, TCHAR** argv);
int ndascmd_enum_logicaldevices(int argc, TCHAR** argv);
int ndascmd_enum_devices(int argc, TCHAR** argv);
int ndascmd_enable(int argc, TCHAR** argv);
int ndascmd_disable(int argc, TCHAR** argv);
int ndascmd_trace_events(int argc, TCHAR** argv);
int ndascmd_query_unitdevice_hosts(int argc, TCHAR** argv);

/* misc.c */

void print_ndas_error_messageprint_ndas_error_message(DWORD dwError);
void print_system_error_message(DWORD dwError);
void print_error_message(DWORD dwError);
void print_last_error_message();

BOOL convert_blocksize_to_string(LPTSTR lpBuffer, DWORD cchBuffer, UINT64 dwBlocks);
BOOL convert_guid_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCGUID lpGuid);
BOOL conver_ndas_device_id_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID);

BOOL parse_slot_unit_number(LPCTSTR arg, DWORD* pdwSlotNo, DWORD* pdwUnitNo);
BOOL resolve_logicaldevice_id_from_slot_unit_number(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* pLDID);

LPCTSTR get_unitdevice_type_string(NDAS_UNITDEVICE_TYPE type);
LPCTSTR get_veto_type_string(PNP_VETO_TYPE type);

