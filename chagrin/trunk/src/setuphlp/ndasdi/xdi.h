#ifndef _NDASDI_H_
#define _NDASDI_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

HRESULT
WINAPI
xDiInstallFromInfW(
	__in_opt HWND Owner,
	__in LPCWSTR InfFile,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService);

HRESULT
WINAPI
xDiInstallFromInfHandleW(
	__in_opt HWND Owner,
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService);

#ifdef __cplusplus
}
#endif

#endif
