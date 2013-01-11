#ifndef NETDISK_LDSERV_DRVMATCH_H
#define NETDISK_LDSERV_DRVMATCH_H
#pragma once

//////////////////////////////////////////////////////////////////////////
//
//	structures
//

typedef struct _DRIVEMATCH_UNITDISK_INFO {

	ULONG	SlotNo ;
	UCHAR	UnitDiskNo ;
	UCHAR	MacAddress[6] ;

} DRIVEMATCH_UNITDISK_INFO, *PDRIVEMATCH_UNITDISK_INFO ;

//////////////////////////////////////////////////////////////////////////
//
//	exported functions
//

//
//	translate a logical drive number to slot numbers of NetDisks
//
BOOL 
GetNetDiskFromDrvLtr(
	int drvno, 
	DRIVEMATCH_UNITDISK_INFO unitdisks[], 
	int unitdisks_sz, 
	int *unitdisks_cnt);

//
//	translate a volume handler to slot numbers of NetDisks
//
BOOL 
GetNetDiskFromHVol(
	HANDLE hVol, 
	DRIVEMATCH_UNITDISK_INFO unitdisks[], 
	int unitdisks_sz, 
	int *unitdisks_cnt);

//
//	translate a slot number of NetDisk to logical drive numbers
//
BOOL 
GetDrvLtrsFromNDSlotNo(
	ULONG slotno, 
	ULONG *drvmask);

//
//	translate a volume handler to a logical drive number
//
BOOL Handle2LogDrvNo(
	HANDLE	hFile, 
	int *drvno);

//////////////////////////////////////////////////////////////////////////

//
// find the NDAS logical device slot no of a given volume
//

BOOL
WINAPI
NdasDmGetNdasLogDevSlotNoOfDisk(
	HANDLE hDisk, LPDWORD lpdwSlotNo);

//
// find the NDAS logical device slot no of a given volume
//

BOOL 
WINAPI
NdasDmGetNdasLogDevSlotNoOfVolume(
	HANDLE hVolume, 
	LPDWORD lpdwSlotNo, DWORD nBuffer, LPDWORD lpdwBufferUsed);

//
// find the NDAS logical device slot no of a given scsi port
//
// GetLastError returns NDASDM_ERROR_SCSI_IO_SIGNATURE_MISMATCH if not matching
//

BOOL 
WINAPI
NdasDmGetNdasLogDevSlotNoOfScsiPort(
	HANDLE hScsiPort,
	LPDWORD lpdwSlotNo);

//
// find by dbcc_name
//
// OnArrival SCSI command may not be available
//
BOOL 
WINAPI
NdasDmGetNdasLogDevSlotNoOfScsiPort(
	LPCWSTR wszDbccName, LPDWORD lpdwSlotNo);

BOOL 
WINAPI
NdasDmIsLANSCSIPortInterface(LPCWSTR wszDbccName);

//
// find the drive letters of a given volume
// returns the first associated drive letter if found
//

BOOL 
WINAPI
NdasDmGetDriveNumberOfVolume(
	HANDLE hVolume,
	LPDWORD lpdwFirstDriverLetter);

#endif // NETDISK_LDSERV_DRVMATCH_H
