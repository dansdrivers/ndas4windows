#ifndef NDASVOL_H_INCLUDED
#define NDASVOL_H_INCLUDED
#pragma once

#if defined(NDASVOL_IMP) || defined(NDASVOL_SLIB)
#define NDASVOL_LINKAGE
#else
#define NDASVOL_LINKAGE __declspec(dllimport)
#endif

#define NDASVOL_CALL __stdcall
#include "ndastype.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
	NDAS Volume Detection API

	Functions in the API requires Administrative privileges to perform
	operations. Otherwise, ERROR_ACCESS_DENIED will be set as a LastError.

*/

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasPathW(
	__in LPCWSTR VolumeMountPoint);

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasPathA(
	__in LPCSTR VolumeMountPoint);

#ifdef UNICODE
#define NdasIsNdasPath NdasIsNdasPathW
#else
#define NdasIsNdasPath NdasIsNdasPathA
#endif

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasVolume(
	__in HANDLE hVolume);

//
// returns S_OK if the volume spans any NDAS storage
// S_FALSE if no volume 
NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasGetNdasLocationForVolume(
	__in HANDLE hVolume,
	__out PNDAS_LOCATION NdasScsiLocation);

typedef HRESULT (CALLBACK *NDASLOCATIONENUMPROC)(
	__in NDAS_LOCATION NdasScsiLocation,
	__in LPVOID Context);

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasEnumNdasLocationsForVolume(
	__in HANDLE hVolume,
	__in NDASLOCATIONENUMPROC EnumProc,
	__in LPVOID Context);

typedef struct _NDAS_REQUEST_DEVICE_EJECT_DATAW
{
	DWORD Size;
	DWORD ConfigRet; /* CONFIGRET */
	DWORD VetoType;  /* PNP_VETO_TYPE */
	WCHAR VetoName[MAX_PATH];
} NDAS_REQUEST_DEVICE_EJECT_DATAW, *PNDAS_REQUEST_DEVICE_EJECT_DATAW;

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasRequestDeviceEjectW(
	__in NDAS_LOCATION NdasScsiLocation,
	__inout PNDAS_REQUEST_DEVICE_EJECT_DATAW EjectData);

#ifdef UNICODE
#define NdasRequestDeviceEject NdasRequestDeviceEjectW
#else
/* Ansi is not supported */
/* #define NdasRequestDeviceEject NdasRequestDeviceEjectA */
#endif

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasStorage(
	__in HANDLE DiskHandle);

/* Obsolete functions, do not use this function anymore */

#ifdef MSC_DEPRECATE_SUPPORTED
#pragma warning(disable: 4995)
#endif

typedef BOOL (CALLBACK *NDASSCSILOCATIONENUMPROC)(
	const NDAS_SCSI_LOCATION* NdasScsiLocation,
	LPVOID Context);

#ifdef MSC_DEPRECATE_SUPPORTED
#pragma warning(default: 4995)
#endif

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasEnumNdasScsiLocationsForVolume(
	__in HANDLE hVolume,
	__in NDASSCSILOCATIONENUMPROC EnumProc,
	__in LPVOID Context);

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasGetNdasScsiLocationForVolume(
	__in HANDLE hVolume, 
	__out PNDAS_SCSI_LOCATION NdasScsiLocation);

#ifdef MSC_DEPRECATE_SUPPORTED
#pragma deprecated(NdasEnumNdasScsiLocationsForVolume)
#pragma deprecated(NDASSCSILOCATIONENUMPROC)
#endif

#ifdef __cplusplus
}
#endif

#endif /* NDASVOL_H_INCLUDED */
