#ifndef NDASFSCTL_H_INCLUDED
#define NDASFSCTL_H_INCLUDED
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern const LPCWSTR NdasFatControlDeviceName;
extern const LPCWSTR NdasNtfsControlDeviceName;
extern const LPCWSTR NdasRofsControlDeviceName;

HRESULT
NdasFsCtlOpenControlDevice(
	__in LPCWSTR ControlDeviceName,
	__in DWORD DesiredAccess,
	__deref_out HANDLE* ControlDeviceHandle);

HRESULT
WINAPI
NdasFsCtlShutdownEx(
	__in HANDLE ControlDeviceHandle);

HRESULT
WINAPI
NdasFsCtlShutdown(
	__in LPCWSTR ControlDeviceName);

#ifdef __cplusplus
}
#endif

#endif /* NDASFSCTL_H_INCLUDED */

