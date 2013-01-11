#pragma once

#include <windows.h>
#include <tchar.h>

int ndascmd_register_device(int argc, TCHAR** argv);
int ndascmd_unregister_device(int argc, TCHAR** argv);

int ndascmd_mount(int argc, TCHAR** argv);
int ndascmd_unmount(int argc, TCHAR** argv);
int ndascmd_recover(int argc, TCHAR** argv);
int ndascmd_enum_logicaldevices(int argc, TCHAR** argv);
int ndascmd_enum_devices(int argc, TCHAR** argv);
int ndascmd_enable(int argc, TCHAR** argv);
int ndascmd_disable(int argc, TCHAR** argv);
int ndascmd_trace_events(int argc, TCHAR** argv);
int ndascmd_query_unitdevice_hosts(int argc, TCHAR** argv);

/* misc.c */

void NC_PrintNdasErrMsg(DWORD dwError);
void NC_PrintSysErrMsg(DWORD dwError);
void NC_PrintErrMsg(DWORD dwError);
void NC_PrintLastErrMsg();

BOOL NC_BlockSizeToString   (LPTSTR lpBuffer, DWORD cchBuffer, DWORD dwBlocks);
BOOL NC_GuidToString        (LPTSTR lpBuffer, DWORD cchBuffer, LPCGUID lpGuid);
BOOL NC_NdasDeviceIDToString(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID);

BOOL NC_ParseSlotUnitNo(LPCTSTR arg, DWORD* pdwSlotNo, DWORD* pdwUnitNo);
BOOL NC_ResolveLDIDFromSlotUnit(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* pLDID);
LPCTSTR NC_UnitDeviceTypeString(NDAS_UNITDEVICE_TYPE udType);

