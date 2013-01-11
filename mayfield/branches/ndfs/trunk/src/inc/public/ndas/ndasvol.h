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
BOOL
NDASVOL_CALL
NdasIsNdasPathW(
	IN LPCWSTR VolumeMountPoint);

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasIsNdasPathA(
	IN LPCSTR VolumeMountPoint);

#ifdef UNICODE
#define NdasIsNdasPath NdasIsNdasPathW
#else
#define NdasIsNdasPath NdasIsNdasPathA
#endif

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasIsNdasVolume(
	IN HANDLE hVolume);

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasGetNdasScsiLocationForVolume(
	IN HANDLE hVolume,
	OUT PNDAS_SCSI_LOCATION NdasScsiLocation);

typedef BOOL (CALLBACK *NDASSCSILOCATIONENUMPROC)(
	const NDAS_SCSI_LOCATION* NdasScsiLocation,
	LPVOID Context);

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasEnumNdasScsiLocationsForVolume(
	IN HANDLE hVolume,
	NDASSCSILOCATIONENUMPROC EnumProc,
	LPVOID Context);

typedef struct _NDAS_REQUEST_DEVICE_EJECT_DATAW
{
	DWORD Size;
	DWORD ConfigRet; /* CONFIGRET */
	DWORD VetoType;  /* PNP_VETO_TYPE */
	WCHAR VetoName[MAX_PATH];
} NDAS_REQUEST_DEVICE_EJECT_DATAW, *PNDAS_REQUEST_DEVICE_EJECT_DATAW;

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasRequestDeviceEjectW(
	IN CONST NDAS_SCSI_LOCATION* NdasScsiLocation,
	IN OUT PNDAS_REQUEST_DEVICE_EJECT_DATAW EjectData);

#ifdef UNICODE
#define NdasRequestDeviceEject NdasRequestDeviceEjectW
#else
/* Ansi is not supported */
/* #define NdasRequestDeviceEject NdasRequestDeviceEjectA */
#endif

#ifdef __cplusplus
}
#endif

#endif /* NDASVOL_H_INCLUDED */
