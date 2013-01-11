#pragma once

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

int ndascmd_purge_volume(int argc, TCHAR** argv);

/* misc.c */

#include "ndaststr.h"

void print_ndas_error_messageprint_ndas_error_message(DWORD errcode);
void print_system_error_message(DWORD errcode);
void print_error_message(DWORD errcode);
void print_last_error_message();

BOOL parse_slot_unit_number(LPCTSTR arg, DWORD* pdwSlotNo, DWORD* pdwUnitNo);
BOOL resolve_logicaldevice_id_from_slot_unit_number(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* pLDID);

DWORD string_to_hex(LPCTSTR HexString, BYTE* Value, DWORD ValueLength);

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
