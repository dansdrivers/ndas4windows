#include "stdafx.h"
//#include "ddk_inc.h"
#include <SocketLpx.h>
#include "devioctl.h"
#include "ntddvol.h"
#include "ntddstor.h"
#include "ntddscsi.h"
#include "lsminiportioctl.h"

#include "ndaserror.h"

#include "DriveMatch.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_DRVMATCH
#include "xdebug.h"

#define MAX_VOL_DISK_EXTS 256

BOOL GetDiskNumbersForDrvLtr(int drvno, int diskno[], int disknobuff_cnt, int *diskcnt) {
	BOOL	bret = FALSE ;
	HANDLE	volMgr ;
	ULONG	retbytes ;
	TCHAR	volName[] = _T("\\\\.\\A:") ;
	UINT	i ;
	PVOLUME_DISK_EXTENTS	pvolExts ;
	ULONG	buffSz ;

	//	init counter
	*diskcnt = 0 ;

	//
	//	fix volume name
	//
	volName[4] += (TCHAR) drvno ;
	DPInfo(_FT("DOS device name : %s\n"), volName) ;

	//
	//	open the volume manager
	//
	volMgr = CreateFile(volName,
						GENERIC_READ |  GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL) ;
	if(volMgr == INVALID_HANDLE_VALUE) {
		DPErrorEx(_FT("Couldn't open %s: "), volName) ;
		return FALSE ;
	}


	buffSz = sizeof(VOLUME_DISK_EXTENTS) + MAX_VOL_DISK_EXTS * sizeof(VOLUME_DISK_EXTENTS) ;
	pvolExts = (PVOLUME_DISK_EXTENTS)HeapAlloc(GetProcessHeap(), 0, buffSz) ;
	if(pvolExts == 0) {
		DPErrorEx(_FT("Memory allocation failure: "));
		return FALSE ;
	}
	//
	// get extend information via IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS
	//
	bret = DeviceIoControl(volMgr,
		IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		NULL,
		0,
		pvolExts,
		buffSz,
		&retbytes,
		NULL) ;
	if(bret == 0) {
		DPErrorEx(_FT("GetDiskNumbersForDrvLtr(), failed DeviceIoControl(): "));
		goto cleanup ;
	}

	//
	//	usually it returns one disk device
	//
	for(i = 0 ; i < pvolExts->NumberOfDiskExtents; i ++ ) {
		DPInfo(_FT("GetDiskNumbersForDrvLtr(), DiskNo:%ld Offset:%lld Length:%lld\n"), 
			pvolExts->Extents[i].DiskNumber, 
			pvolExts->Extents[i].StartingOffset, 
			pvolExts->Extents[i].ExtentLength);
		if(disknobuff_cnt > *diskcnt) {
			diskno[i] = pvolExts->Extents[i].DiskNumber ;
			(*diskcnt) ++ ;
		}
	}

cleanup:
	if(volMgr != 0 && volMgr != INVALID_HANDLE_VALUE) CloseHandle(volMgr) ;
	if(pvolExts) HeapFree(GetProcessHeap(), 0, pvolExts) ;

	return bret ;
}


BOOL GetDiskNumbersForHVol(HANDLE volMgr, int diskno[], int disknobuff_cnt, int *diskcnt) {
	BOOL	bret = FALSE ;
	ULONG	retbytes ;
	UINT	i ;
	PVOLUME_DISK_EXTENTS	pvolExts ;
	ULONG	buffSz ;
	
	//	init counter
	*diskcnt = 0 ;

	buffSz = sizeof(VOLUME_DISK_EXTENTS) + MAX_VOL_DISK_EXTS * sizeof(VOLUME_DISK_EXTENTS) ;
	pvolExts = (PVOLUME_DISK_EXTENTS)HeapAlloc(GetProcessHeap(), 0, buffSz) ;
	if(pvolExts == 0) {
		DPErrorEx(_FT("Memory allocation failure: "));
		return FALSE ;
	}
	//
	// get extend information via IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS
	//
	bret = DeviceIoControl(volMgr,
		IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		NULL,
		0,
		pvolExts,
		buffSz,
		&retbytes,
		NULL) ;
	if(bret == 0) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS) failed: "));
		goto cleanup ;
	}

	//
	//	usually it returns one disk device
	//
	for(i = 0 ; i < pvolExts->NumberOfDiskExtents; i ++ ) {
		DPInfo(_FT("DiskNo:%ld Offset:%lld Length:%lld.\n"),
			pvolExts->Extents[i].DiskNumber, 
			pvolExts->Extents[i].StartingOffset, 
			pvolExts->Extents[i].ExtentLength);
		if(disknobuff_cnt > *diskcnt) {
			diskno[i] = pvolExts->Extents[i].DiskNumber ;
			(*diskcnt) ++ ;
		}
	}

cleanup:
	if(pvolExts) HeapFree(GetProcessHeap(), 0, pvolExts) ;

	return bret ;
}


#include "ntddstor.h"

BOOL GetScsiportsForDisks(
		int diskno[],
		int diskno_cnt,
		int scsiportno[],
		int scsiportno_sz,
		int *scsiportno_cnt
) {
	BOOL			bret = FALSE ;
	HANDLE			phyDrv ;
	ULONG			retbytes ;
	TCHAR			phyDrvName[256]  ;
	int				i ;

	// to query storage property
	STORAGE_PROPERTY_QUERY	strgQuery ;
	ULONG			querybuffSz ;
	PSTORAGE_ADAPTER_DESCRIPTOR	pstrgAdtDesc = NULL ;
	int				buffSz ;

	// to query a SCSI Address.
	SCSI_ADDRESS	ScsiAddress ;
	int				outbuff_sz ;

	//	init counter
	*scsiportno_cnt = 0 ;

	// input mem
	querybuffSz = sizeof(STORAGE_PROPERTY_QUERY) ;
	memset(&strgQuery, 0, sizeof(STORAGE_PROPERTY_QUERY)) ;
	strgQuery.PropertyId =  StorageAdapterProperty ;
	strgQuery.QueryType = PropertyStandardQuery  ;

	// output mem
	buffSz = sizeof(STORAGE_ADAPTER_DESCRIPTOR) ;
	pstrgAdtDesc = (PSTORAGE_ADAPTER_DESCRIPTOR)HeapAlloc(GetProcessHeap(), 0, buffSz) ;
	if(pstrgAdtDesc == 0) {
		DPErrorEx(_FT("Memory allocation failed: "));
		return FALSE ;
	}

	for(i = 0 ; i < diskno_cnt ; i ++ ) {
		//
		//	make up Physical Drive Name
		//
		_stprintf(phyDrvName, _T("\\\\.\\PHYSICALDRIVE%d") , diskno[i] ) ;
		DPInfo(_FT("Physical drive name :%s\n"), phyDrvName);

		//
		//	open the disk device
		//
		phyDrv = CreateFile(phyDrvName,
						GENERIC_READ |  GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL) ;
		if(phyDrv == INVALID_HANDLE_VALUE) {
			DPErrorEx(_FT("Opening %s failed: "), phyDrvName);
			goto cleanup ;
		}

		//
		//	ask storage property
		//
		bret = DeviceIoControl(phyDrv,
			IOCTL_STORAGE_QUERY_PROPERTY,
			&strgQuery,
			querybuffSz,
			pstrgAdtDesc,
			buffSz,
			&retbytes,
			NULL) ;
		if(bret == 0) {
			DPErrorEx(_FT("DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY) failed: "));
			goto cleanup ;
		}
		//
		//	if it's scsi bus, get a scsiport number.
		//
		if(pstrgAdtDesc->BusType == BusTypeScsi) {
			outbuff_sz = sizeof(SCSI_ADDRESS) ;
	
			bret = DeviceIoControl(phyDrv,
						IOCTL_SCSI_GET_ADDRESS,
						NULL,
						0,
						&ScsiAddress,
						outbuff_sz,
						&retbytes,
						NULL) ;

			if(bret == 0 /* || retbytes */ ) {
				DPErrorEx(_FT("DeviceIoControl(IOCTL_SCSI_GET_ADDRESS) failed: "));
				bret = FALSE ;
				goto cleanup ;
			}
			if(scsiportno_sz > *scsiportno_cnt)  {
				scsiportno[*scsiportno_cnt] = ScsiAddress.PortNumber ;
				(*scsiportno_cnt) ++ ;
			}

			DPInfo( _FT("ScsiAddress=Len:%d PortNumber:%d PathId:%d TargetId:%d Lun:%d\n"),
					(LONG)ScsiAddress.Length,
					(LONG)ScsiAddress.PortNumber,
					(LONG)ScsiAddress.PathId,
					(LONG)ScsiAddress.TargetId,
					(LONG)ScsiAddress.Lun);

		}

		//
		//	close a handle for a disk.
		//
		CloseHandle(phyDrv) ;
		phyDrv = NULL ;
	}

cleanup:
	if(phyDrv != 0 && phyDrv != INVALID_HANDLE_VALUE) CloseHandle(phyDrv) ;
	if(pstrgAdtDesc) HeapFree(GetProcessHeap(), 0, pstrgAdtDesc) ;

	return bret ;

}

BOOL GetNetDisksForScsiports(
		int scsiportno[],						// IN
		int scsiportno_cnt,						// IN
		DRIVEMATCH_UNITDISK_INFO unitdisks[],	// OUT
		int slotno_sz,							// OUT
		int *slotno_cnt							// OUT
) {
	BOOL	bret = FALSE ;
	HANDLE	scsiPort ;
	ULONG	retbytes ;
	TCHAR	scsiPortName[256]  ;
	int		i ;
	// to send SRB IOCTL
	PSRB_IO_CONTROL psrbioctl ;
	int	outbuff_sz ;
//	ULONG	*pslotno ;

	PLSMPIOCTL_PRIMUNITDISKINFO	PrimUnitDisk ;
	PLSMPIOCTL_QUERYINFO		QueryInfo ;

	//	init counter
	*slotno_cnt = 0 ;
	psrbioctl = NULL ;

	for(i = 0 ; i < scsiportno_cnt ; i ++ ) {
		//
		//	make up SCSI Port Name
		//
		_stprintf(scsiPortName, _T("\\\\.\\Scsi%d:") , scsiportno[i] ) ;
		DPInfo(_FT("SCSI Port name :%s\n"), scsiPortName) ;

		//
		//	open the scsiport device
		//
		scsiPort = CreateFile(scsiPortName,
						GENERIC_READ |  GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL) ;
		if(scsiPort == INVALID_HANDLE_VALUE) {
			DPErrorEx(_FT("CreateFile(%s) failed: "), scsiPortName);
			goto cleanup ;
		}

		outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO) ;
		psrbioctl = (PSRB_IO_CONTROL)HeapAlloc(GetProcessHeap(), 0, outbuff_sz) ;
		if(psrbioctl == NULL) {
			DPErrorEx(_FT("HeapAlloc(%d) failed: "), outbuff_sz);
			goto cleanup ;
		}

		memset(psrbioctl, 0, sizeof(*psrbioctl)) ;
		psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL) ;
		memcpy(psrbioctl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8) ;
		psrbioctl->Timeout = 10 ;
		psrbioctl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERYINFO_EX ;
		psrbioctl->Length = sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO) ;

		QueryInfo = (PLSMPIOCTL_QUERYINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL)) ;
		PrimUnitDisk = (PLSMPIOCTL_PRIMUNITDISKINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL)) ;

		QueryInfo->Length = sizeof(LSMPIOCTL_QUERYINFO) ;
		QueryInfo->InfoClass = LsmpPrimaryUnitDiskInformation ;
		QueryInfo->QueryDataLength = 0 ;

		bret = DeviceIoControl(scsiPort,
					IOCTL_SCSI_MINIPORT,
					psrbioctl,
					outbuff_sz,
					psrbioctl,
					outbuff_sz,
					&retbytes,
					NULL) ;

		if(bret == 0 || psrbioctl->ReturnCode != 0) {
			HeapFree(GetProcessHeap(), 0, psrbioctl) ;
			DPErrorEx(_FT("DeviceIoControl(IOCTL_SCSI_MINIPORT/LANSCSIMINIPORT_IOCTL_QUERYINFO_EX) failed: "));
			bret = FALSE ;
			goto cleanup ;
		}
//		pslotno = (ULONG *)(((BYTE *)(psrbioctl)) + psrbioctl->HeaderLength) ;
		if(slotno_sz > (LONG)PrimUnitDisk->UnitDisk.SlotNo)  {
			unitdisks[*slotno_cnt].SlotNo = PrimUnitDisk->UnitDisk.SlotNo / 10 ;
			unitdisks[*slotno_cnt].UnitDiskNo = PrimUnitDisk->UnitDisk.UnitDiskId ;
			_ASSERT(PrimUnitDisk->UnitDisk.NetDiskAddress.TAAddressCount == 1);
			CopyMemory(unitdisks[*slotno_cnt].MacAddress, &PrimUnitDisk->UnitDisk.NetDiskAddress.Address[0].Address, LPXADDR_NODE_LENGTH) ;
			(*slotno_cnt) ++ ;
		}


		DPInfo(_FT("Size of SRB_IOCONTROL:%d size of SCSI_ADRESS:%d retbytes:%d\n"),
						sizeof(SRB_IO_CONTROL), sizeof(ULONG), retbytes) ;
		DPInfo(_FT("Error Code:%d\n"),psrbioctl->ReturnCode);
		DPInfo(_FT("Slot No:%d\n"),PrimUnitDisk->UnitDisk.SlotNo / 10 );

		HeapFree(GetProcessHeap(), 0, psrbioctl) ;
		psrbioctl = NULL ;

		CloseHandle(scsiPort) ;
		scsiPort = NULL ;
	}


cleanup:
	if(scsiPort != 0 && scsiPort != INVALID_HANDLE_VALUE) CloseHandle(scsiPort) ;

	return bret ;
}

//
//	translate a logical drive number to slot numbers of NetDisks
//
BOOL GetNetDiskFromDrvLtr(int drvno, DRIVEMATCH_UNITDISK_INFO unitdisks[], int unitdisks_sz, int *unitdisks_cnt) {
	BOOL	bret = FALSE ;
	int		diskno[256] ;
	int		diskcnt = 0 ;
	int		scsiportno[256] ;
	int		scsiportno_cnt = 0 ;

	//
	//	get PhysicalDisk numbers of the drive
	//
	bret = GetDiskNumbersForDrvLtr(drvno, diskno, 256, &diskcnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetDiskNumbersForDrvLtr() failed.\n"));
		return FALSE ;
	}

	//
	//	get ScsiPort number for each disks
	//
	bret = GetScsiportsForDisks(diskno, diskcnt, scsiportno, 256, &scsiportno_cnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetSCSIPortsForDisks() failed.\n"));
		return FALSE ;
	}

	//
	//	get slot number for each scsiport
	//
	bret = GetNetDisksForScsiports(scsiportno, scsiportno_cnt, unitdisks, unitdisks_sz, unitdisks_cnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetNetDisksForScsiports() failed.\n"));
		return FALSE ;
	}

	return TRUE ;
}


//
//	translate a volume handler to slot numbers of NetDisks
//
BOOL GetNetDiskFromHVol(HANDLE hVol, DRIVEMATCH_UNITDISK_INFO unitdisks[], int unitdisks_sz, int *unitdisks_cnt) {
	BOOL	bret = FALSE ;
	int		diskno[256] ;
	int		diskcnt = 0 ;
	int		scsiportno[256] ;
	int		scsiportno_cnt = 0 ;

	//
	//	get PhysicalDisk numbers of the drive
	//
	bret = GetDiskNumbersForHVol(hVol, diskno, 256, &diskcnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetDiskNumbersForDrvLtr() failed.\n"));
		return FALSE ;
	}

	//
	//	get ScsiPort number for each disks
	//
	bret = GetScsiportsForDisks(diskno, diskcnt, scsiportno, 256, &scsiportno_cnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetScsiportsForDisks() failed.\n"));
		return FALSE ;
	}

	//
	//	get slot number for each scsiport
	//
	bret = GetNetDisksForScsiports(scsiportno, scsiportno_cnt, unitdisks, unitdisks_sz, unitdisks_cnt) ;
	if(bret == FALSE) {
		DPError(_FT("GetSCSIPortsForDisks() failed.\n"));
		return FALSE ;
	}

	return TRUE ;
}

//
//	translate a slot number of NetDisk to logical drive numbers
//
BOOL
GetDrvLtrsFromNDSlotNo(ULONG slotno, ULONG *drvmask) {
	int		i, j ;
	DRIVEMATCH_UNITDISK_INFO	unitdisks[256] ;
	int		slotno_cnt ;
	BOOL	bret ;
	ULONG	tmp_mask = 0 ;
	ULONG	MaxDriveSet ;

	// Get available drives we can monitor
	MaxDriveSet = GetLogicalDrives() ;
	if(MaxDriveSet == 0) return FALSE ;

	//
	//	go throuh every drive letter
	//
	for(i = 0 ; i < 32 ; i ++) {

		if( (MaxDriveSet & ( 1 << i )) == 0 ) continue ;

		bret = GetNetDiskFromDrvLtr(i, unitdisks, 256, &slotno_cnt) ;
		if(bret == FALSE) 
			continue ;

		for(j = 0 ; j < slotno_cnt ; j++) {
			//
			//	if a slot number is what we are looking for, check drive bit mask.
			//
			if(slotno == unitdisks[j].SlotNo) {
				tmp_mask |= 1 << j ;
			}
		}
		
	}

	if( tmp_mask == 0 ) return FALSE ;

	(*drvmask) = tmp_mask ;
	return TRUE ;
}

#define IOCTL_VOLUME_QUERY_VOLUME_NUMBER        CTL_CODE(IOCTL_VOLUME_BASE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)

BOOL GetVolNumForDrvNum(int drvno, ULONG *vol) {
	BOOL	bret = FALSE ;
	HANDLE	volMgr ;
	TCHAR	volName[] = _T("\\\\.\\A:") ;
	VOLUME_NUMBER VN ;
	ULONG	retsz ;

	//
	//	fix volume name
	//
	volName[4] += (TCHAR)drvno ;
	DPInfo(_FT("DOS device name: %s\n"), volName);

	//
	//	open the volume manager
	//
	volMgr = CreateFile(volName,
						GENERIC_READ |  GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL) ;
	if(volMgr == INVALID_HANDLE_VALUE) {
		DPErrorEx(_FT("CreateFile(%s) failed: "), volName);
		return FALSE ;
	}


	bret = DeviceIoControl(
					volMgr, 
					IOCTL_VOLUME_QUERY_VOLUME_NUMBER, 
					NULL,                           // lpInBuffer
					0,								// size of input buffer
					(LPVOID) &VN,                   // output buffer
					(DWORD) sizeof(VN),             // size of output buffer
					&retsz,							// number of bytes returned
					NULL							// OVERLAPPED structure
			) ;
	if(bret == FALSE) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_VOLUME_QUERY_VOLUME_NUMBER) failed: "));
		goto cleanup ;
	}

	DPInfo(_FT("Volume Number:%d\n"), VN.VolumeNumber );
	DPInfo(_FT("Volume Manager Name:%ws\n"), VN.VolumeManagerName );
	*vol = VN.VolumeNumber ;

cleanup:
	if(volMgr != 0 && volMgr != INVALID_HANDLE_VALUE) CloseHandle(volMgr) ;

	return bret ;
}

//
//	translate a volume handler to a logical drive number
//
BOOL Handle2LogDrvNo(HANDLE	hFile, int *drvno) {
	BOOL	bret ;
	ULONG	MaxDriveSet ;
	int		i ;
	ULONG volno ;
	VOLUME_NUMBER VN ;
	ULONG	retsz ;

	//
	// find a volume number for hFile
	//
	bret = DeviceIoControl(hFile, IOCTL_VOLUME_QUERY_VOLUME_NUMBER, 
					NULL,                           // lpInBuffer
					0,								// size of input buffer
					(LPVOID) &VN,                   // output buffer
					(DWORD) sizeof(VN),             // size of output buffer
					&retsz,							// number of bytes returned
					NULL							// OVERLAPPED structure
			) ;
	if(bret == FALSE) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_VOLUME_QUERY_VOLUME_NUMBER) failed: "));
		goto cleanup ;
	}
	DPInfo(_FT("Volume Number:%d\n"), VN.VolumeNumber);
	DPInfo(_FT("Volume Manager Name:%ws\n"), VN.VolumeManagerName);

	MaxDriveSet = GetLogicalDrives() ;
	if(MaxDriveSet == 0) return FALSE ;


	//
	//	go throuh every drive letter
	//	TODO: from 1 to skip drive A:
	//
	for(i = 1 ; i < 32 ; i ++) {

		if( (MaxDriveSet & ( 1 << i )) == 0 ) continue ;

		bret = GetVolNumForDrvNum(i, &volno) ;
		if(bret == FALSE) 
			continue ;

		if(VN.VolumeNumber == volno) {
			*drvno = i ;
			return TRUE ;
		}
	}
cleanup:
	return FALSE ;
}

//-------------------------------------------------------------------------
//
// NdasDmGetNdasLogDevSlotNoOfScsiport
//
// Find the NDAS logical device slot number of a given scsi port number
//
//-------------------------------------------------------------------------

BOOL NdasDmGetNdasLogDevSlotNoOfScsiPort(
	HANDLE hScsiPort,
	LPDWORD lpdwSlotNo)
{
	//
	// Make up IOCTL In-parameter
	// LANSCSIMINIPORT_IOCTL_GET_SLOT_NO to get Logical Device Slot No
	//

	BOOL fSuccess(FALSE);

	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	const DWORD cbData = sizeof(ULONG);
	const DWORD cbInBuffer = cbHeader + cbData;
	DWORD cbReturned(0);

	BYTE lpInBuffer[cbInBuffer];
	::ZeroMemory(lpInBuffer, cbInBuffer);
	PSRB_IO_CONTROL pSrbIoControl = (PSRB_IO_CONTROL) lpInBuffer;

	pSrbIoControl->HeaderLength = cbHeader;
	::CopyMemory(pSrbIoControl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
	pSrbIoControl->Timeout = 10;
	pSrbIoControl->ControlCode = LANSCSIMINIPORT_IOCTL_GET_SLOT_NO;
	pSrbIoControl->Length = cbData;

	//
	// Get Logical Device Slot No
	//

	fSuccess = ::DeviceIoControl(
		hScsiPort,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_SCSI_MINIPORT,")
			_T("LANSCSIMINIPORT_IOCTL_GET_SLOT_NO) failed: "));
		return FALSE;
	}

	if (0 != pSrbIoControl->ReturnCode) {
		DPError(_FT("DeviceIoControl(IOCTL_SCSI_MINIPORT,")
			_T("LANSCSIMINIPORT_IOCTL_GET_SLOT_NO) returned SrbIoControl error %d.\n"),
			pSrbIoControl->ReturnCode);
		::SetLastError(NDASDM_ERROR_SRB_IO_CONTROL_ERROR);
		return FALSE;
	}

	//
	// Calculate the slot number offset and retrieve the value
	//

	ULONG* lpdwSlotNoOut = (ULONG*)(lpInBuffer + cbHeader);
	*lpdwSlotNo = *lpdwSlotNoOut;

	DPInfo(_FT("NDAS Logical Device Slot: %d\n"), *lpdwSlotNo);
	
	return TRUE;
}

BOOL NdasDmGetNdasLogDevSlotNoOfScsiPort(
	DWORD dwScsiPortNumber,
	LPDWORD lpdwSlotNo)
{
	BOOL fSuccess(FALSE);

	//
	// Make up SCSI Port Name
	//
	
	TCHAR szScsiPortName[_MAX_PATH + 1];
	HRESULT hr = ::StringCchPrintf(
		szScsiPortName, _MAX_PATH + 1,
		TEXT("\\\\.\\Scsi%d:"), dwScsiPortNumber);

	DPInfo(_FT("SCSI Port Name: %s\n"), szScsiPortName);

	//
	// Open SCSI Port Device
	//

	HANDLE hScsiPort = ::CreateFile(
		szScsiPortName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hScsiPort) {
		DPErrorEx(_FT("CreateFile(%s) failed: "), szScsiPortName);
		return FALSE;
	}

	//
	// Make up IOCTL In-parameter
	// LANSCSIMINIPORT_IOCTL_GET_SLOT_NO to get Logical Device Slot No
	//

	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	const DWORD cbData = sizeof(ULONG);
	const DWORD cbInBuffer = cbHeader + cbData;
	DWORD cbReturned(0);

	BYTE lpInBuffer[cbInBuffer];
	::ZeroMemory(lpInBuffer, cbInBuffer);
	PSRB_IO_CONTROL pSrbIoControl = (PSRB_IO_CONTROL) lpInBuffer;

	pSrbIoControl->HeaderLength = cbHeader;
	::CopyMemory(pSrbIoControl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
	pSrbIoControl->Timeout = 10;
	pSrbIoControl->ControlCode = LANSCSIMINIPORT_IOCTL_GET_SLOT_NO;
	pSrbIoControl->Length = cbData;

	//
	// Get Logical Device Slot No
	//

	fSuccess = ::DeviceIoControl(
		hScsiPort,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_SCSI_MINIPORT,")
			_T("LANSCSIMINIPORT_IOCTL_GET_SLOT_NO) failed: "));
		::CloseHandle(hScsiPort);
		return FALSE;
	}

	if (0 != pSrbIoControl->ReturnCode) {
		DPError(_FT("DeviceIoControl(IOCTL_SCSI_MINIPORT,")
			_T("LANSCSIMINIPORT_IOCTL_GET_SLOT_NO) returned error %d.\n"),
			pSrbIoControl->ReturnCode);
		::CloseHandle(hScsiPort);
		return FALSE;
	}

	//
	// Calculate the slot number offset and retrieve the value
	//

	ULONG* lpdwSlotNoOut = (ULONG*)(lpInBuffer + cbHeader);
	*lpdwSlotNo = *lpdwSlotNoOut;

	DPInfo(_FT("NDAS Logical Device Slot: %d\n"), *lpdwSlotNo);
	
	::CloseHandle(hScsiPort);
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetDiskNumbersOfVolume
//
// Find the physical disk numbers of a volume handle
//
//-------------------------------------------------------------------------

BOOL NdasDmGetDiskNumbersOfVolume(
	HANDLE hVolume, 
	LPDWORD pDiskNumbers, 
	DWORD dwDiskNumbers,
	LPDWORD lpdwDiskNumbersUsed)
{
	BOOL fSuccess(FALSE);
	DWORD cbReturned(0);

	//
	// Prepare Volume Disk Extent Buffer
	//
	DWORD cbVolumeDiskExtents = sizeof(VOLUME_DISK_EXTENTS) + 
		MAX_VOL_DISK_EXTS * sizeof(VOLUME_DISK_EXTENTS);

	PVOLUME_DISK_EXTENTS pVolumeDiskExtents = 
		(PVOLUME_DISK_EXTENTS) ::HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbVolumeDiskExtents);

	if (NULL == pVolumeDiskExtents) {
		DPErrorEx(_FT("Memory allication (%d bytes) failed: "), cbVolumeDiskExtents);
		return FALSE;
	}

	//
	// Get Volume Disk Extents
	//

	DPTrace(_FT("DeviceIoControl(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS)\n"));

	fSuccess = ::DeviceIoControl(
		hVolume,
		IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
		NULL,
		0,
		pVolumeDiskExtents,
		cbVolumeDiskExtents,
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS) failed: "));
		::HeapFree(::GetProcessHeap(), 0, pVolumeDiskExtents);
		return FALSE;
	}

	//
	//	usually it returns one disk device
	//
	*lpdwDiskNumbersUsed = 0;
	for (DWORD i = 0; i < pVolumeDiskExtents->NumberOfDiskExtents; ++i) {
		DPInfo(_FT("Extents[%d]:: Disk Number: %d, Starting Offset: %d, Extent Length: %d.\n"),
			i,
			pVolumeDiskExtents->Extents[i].DiskNumber,
			pVolumeDiskExtents->Extents[i].StartingOffset,
			pVolumeDiskExtents->Extents[i].ExtentLength);

		if (i < dwDiskNumbers) {
			pDiskNumbers[i] = pVolumeDiskExtents->Extents[i].DiskNumber;
			++(*lpdwDiskNumbersUsed);
		}
	}

	::HeapFree(::GetProcessHeap(), 0, pVolumeDiskExtents);
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetScsiPortNumberOfDisk
//
// Find the SCSI port number of a disk device file handle
//
// may return NDASDM_ERROR_NON_SCSI_TYPE_DEVICE for non-scsi devices
//
//-------------------------------------------------------------------------

BOOL NdasDmGetScsiPortNumberOfDisk(
	IN HANDLE hDisk,
	OUT LPDWORD lpdwScsiPortNumber)
{
	_ASSERTE(!IsBadWritePtr(lpdwScsiPortNumber, sizeof(DWORD)));

	BOOL fSuccess(FALSE);
	DWORD cbReturned(0);

	//
	// Query Storage Property
	//

	STORAGE_PROPERTY_QUERY	storPropQuery;
	STORAGE_ADAPTER_DESCRIPTOR	storAdptDesc;

	DWORD cbStorPropQuery = sizeof(STORAGE_PROPERTY_QUERY);
	DWORD cbStorAdptDesc = sizeof(STORAGE_ADAPTER_DESCRIPTOR);

	DPTrace(_FT("DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)\n"));

	::ZeroMemory(&storPropQuery, cbStorPropQuery);
	storPropQuery.PropertyId = StorageAdapterProperty;
	storPropQuery.QueryType = PropertyStandardQuery;

	fSuccess = ::DeviceIoControl(
		hDisk,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &storPropQuery,
		cbStorPropQuery,
		&storAdptDesc,
		cbStorAdptDesc,
        &cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY) failed: "));
		return FALSE;
	}

	//
	// Ignore non-SCSI device
	//

	if (BusTypeScsi != storAdptDesc.BusType) {
		DPInfo(_FT("Ignoring non-scsi bus\n"));
		::SetLastError(NDASDM_ERROR_NON_SCSI_TYPE_DEVICE);
		return FALSE;
	}

	//
	// Query SCSI Address, given that the physical drive is a SCSI device
	//

	SCSI_ADDRESS scsiAddress;
	DWORD cbScsiAddress = sizeof(SCSI_ADDRESS);

	DPTrace(_FT("DeviceIoControl(IOCTL_SCSI_GET_ADDRESS)\n"));

	fSuccess = ::DeviceIoControl(
		hDisk,
		IOCTL_SCSI_GET_ADDRESS,
		NULL,
		0,
		&scsiAddress,
		cbScsiAddress,
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_SCSI_GET_ADDRESS) failed: "));
		return FALSE;
	}

	DPInfo(_FT("SCSIAddress: Len: %d, PortNumber: %d, PathId: %d, TargetId: %d, Lun: %d\n"),
		(DWORD) scsiAddress.Length,
		(DWORD) scsiAddress.PortNumber,
		(DWORD) scsiAddress.PathId,
		(DWORD) scsiAddress.TargetId,
		(DWORD) scsiAddress.Lun);

	//
	// Return the result
	//
	*lpdwScsiPortNumber = scsiAddress.PortNumber;

	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetScsiPortNumberOfDisk
//
// Find the SCSI port number of a physical disk number
//
//-------------------------------------------------------------------------

BOOL NdasDmGetScsiPortNumberOfDisk(
	IN DWORD dwPhysicalDiskNumber, 
	OUT LPDWORD lpdwScsiPortNumber)
{
	_ASSERTE(!IsBadWritePtr(lpdwScsiPortNumber, sizeof(DWORD)));

	//
	//	make up Physical Drive Name
	//

	TCHAR szDiskDevicePath[_MAX_PATH + 1];

	HRESULT hr = ::StringCchPrintf(
		szDiskDevicePath, 
		_MAX_PATH + 1, 
		_T("\\\\.\\PHYSICALDRIVE%d"),
		dwPhysicalDiskNumber);

	_ASSERT(SUCCEEDED(hr));

	DPInfo(_FT("Disk Device Path:%s\n"), szDiskDevicePath);

	//
	//	open the disk device
	//

	DPTrace(_FT("CreateFile(%s)\n"), szDiskDevicePath);

	HANDLE hDisk = ::CreateFile(
		szDiskDevicePath,
        GENERIC_READ |  GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hDisk) {
		DPErrorEx(_FT("CreateFile(%s) failed: "), szDiskDevicePath);
		return FALSE;
	}

	//
	// Get SCSI Port Number of the Disk
	//

	BOOL fSuccess = NdasDmGetScsiPortNumberOfDisk(hDisk, lpdwScsiPortNumber);
	if (!fSuccess) {
		DPError(_FT("NdasDmGetScsiPortNumberOfDisk(%s) failed.\n"),
			szDiskDevicePath);
		(VOID) ::CloseHandle(hDisk);
		return FALSE;
	}

	(VOID) ::CloseHandle(hDisk);
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetLogicalDeviceSlotNoOfVolume
//
// Find the NDAS Logical Device Slot Number of a volume handle
//
//-------------------------------------------------------------------------

BOOL NdasDmGetNdasLogDevSlotNoOfVolume(
	HANDLE hVolume, 
	LPDWORD lpdwSlotNo, 
	DWORD nBuffer, 
	LPDWORD lpdwBufferUsed)
{
	_ASSERTE(INVALID_HANDLE_VALUE != hVolume);
	_ASSERTE(NULL != hVolume);
	_ASSERTE(!IsBadWritePtr(lpdwSlotNo, sizeof(DWORD) * nBuffer));
	_ASSERTE(!IsBadWritePtr(lpdwBufferUsed, sizeof(DWORD)));

	BOOL fSuccess(FALSE);
	//
	// Get the physical disk numbers of the volume
	//
	DWORD pDiskNumbers[256];
	DWORD nDiskNumbers;

	fSuccess = NdasDmGetDiskNumbersOfVolume(hVolume, pDiskNumbers, 256, &nDiskNumbers);
	if (!fSuccess) {
		DPError(_FT("NdasDmGetDiskNumbersOfVolume failed.\n"));
		return FALSE;
	}

	//
	// Get SCSI port number for each disks
	//
	// TODO: Resolve this problem!
	// What if the volume is spanned through NDAS Logical Disks and 
	// OS's regular Disks?
	//
	
	*lpdwBufferUsed = 0;
	for (DWORD i = 0; i < nDiskNumbers; ++i) {

		if (*lpdwBufferUsed >= nBuffer) {
			DPError(_FT("Insufficient Slot Buffer.\n"));
			::SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return FALSE;
		}
		
		DWORD dwDiskNumber = pDiskNumbers[i];
		DWORD dwScsiPortNumber;

		fSuccess = NdasDmGetScsiPortNumberOfDisk(dwDiskNumber, &dwScsiPortNumber);
		if (!fSuccess) {
			DPErrorEx(_FT("NdasDmGetScsiPortNumberOfDisk(%d) failed: "), dwDiskNumber);
			return FALSE;
		}

		DWORD dwSlotNo;
		fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(dwScsiPortNumber, &dwSlotNo);
		if (!fSuccess) {
			DPErrorEx(_FT("NdasDmGetNdasLogDevSlotNoOfScsiPort(%d) failed: "), dwScsiPortNumber);
			return FALSE;
		}

		BOOL bDuplicate(FALSE);
		for (DWORD i = 0; i < *lpdwBufferUsed; ++i) {
			if (lpdwSlotNo[i] == dwSlotNo) {
				bDuplicate = TRUE;
				break;
			}
		}

		if (!bDuplicate) {
			lpdwSlotNo[*lpdwBufferUsed] = dwSlotNo;
			(*lpdwBufferUsed)++;
		}

	}

	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetNdasLogDevSlotNoOfDisk
//
// Find the NDAS Logical Device Slot Number of a disk device file handle
//
//-------------------------------------------------------------------------

BOOL NdasDmGetNdasLogDevSlotNoOfDisk(HANDLE hDisk, LPDWORD lpdwSlotNo)
{
	_ASSERTE(INVALID_HANDLE_VALUE != hDisk);
	_ASSERTE(NULL != hDisk);
	_ASSERTE(!IsBadWritePtr(lpdwSlotNo, sizeof(DWORD)));

	BOOL fSuccess(FALSE);
	DWORD dwScsiPortNumber;
	fSuccess = NdasDmGetScsiPortNumberOfDisk(hDisk, &dwScsiPortNumber);
	if (!fSuccess) {
		DPError(_FT("NdasDmGetScsiPortNumberOfDisk(%p) failed.\n"),
			hDisk);
		return FALSE;
	}
	
	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(dwScsiPortNumber, lpdwSlotNo);
	if (!fSuccess) {
		DPError(_FT("NdasDmGetNdasLogDevSlotNoOfScsiPort(%d) failed.\n"), 
			dwScsiPortNumber);
		return FALSE;
	}

	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasDmGetDriveNumberOfVolume
//
// Find the drive letters of the volume
//
//-------------------------------------------------------------------------

BOOL NdasDmGetDriveNumberOfVolume(
	HANDLE hVolume,
	LPDWORD lpdwFirstDriverLetter)
{
	_ASSERTE(INVALID_HANDLE_VALUE != hVolume);
	_ASSERTE(NULL != hVolume);
	_ASSERTE(!IsBadWritePtr(lpdwFirstDriverLetter, sizeof(DWORD)));

	BOOL fSuccess(FALSE);

	VOLUME_NUMBER volumeNumber ;

	//
	// find a volume number for hFile
	//
	DWORD cbReturned;
	fSuccess = ::DeviceIoControl(
		hVolume,
		IOCTL_VOLUME_QUERY_VOLUME_NUMBER, 
		NULL,
		0,							
		(LPVOID) &volumeNumber,                   
		(DWORD) sizeof(VOLUME_NUMBER),             
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_VOLUME_QUERY_VOLUME_NUMBER) failed: "));
		return FALSE;
	}

	DPInfo(_FT("Volume Number: %d\n"), volumeNumber.VolumeNumber);

	// VolumeManagerName is 8 wide-chars without null
	DPInfo(_FT("Volume Manager Name: %c%c%c%c%c%c%c%c\n"), 
		(TCHAR)volumeNumber.VolumeManagerName[0], (TCHAR)volumeNumber.VolumeManagerName[1],
		(TCHAR)volumeNumber.VolumeManagerName[2], (TCHAR)volumeNumber.VolumeManagerName[3],
		(TCHAR)volumeNumber.VolumeManagerName[4], (TCHAR)volumeNumber.VolumeManagerName[5],
		(TCHAR)volumeNumber.VolumeManagerName[6], (TCHAR)volumeNumber.VolumeManagerName[7]);

	DWORD dwLogicalDriveSet = ::GetLogicalDrives() ;
	if(dwLogicalDriveSet == 0) {
		DPErrorEx(_FT("GetLogicalDrivers() failed: "));
		return FALSE ;
	}

	//
	//	go through every drive letter from 1 (to skip drive A:)
	//
	for(DWORD i = 1 ; i < 26 ; ++i) {

		if( (dwLogicalDriveSet & ( 1 << i )) == 0 ) 
			continue ;

		ULONG vn;
		fSuccess = GetVolNumForDrvNum(i, &vn) ;
		if(!fSuccess) 
			continue ;

		if(volumeNumber.VolumeNumber == vn) {
			*lpdwFirstDriverLetter = i ;
			return TRUE ;
		}
	}

	return FALSE ;
	
}

const WCHAR LANSCSI_DEV_IFIDW[] = L"\\\\?\\LanscsiBus#NetDisk_V";
const DWORD LANSCSI_DEV_IFIDW_LEN = sizeof(LANSCSI_DEV_IFIDW)/sizeof(LANSCSI_DEV_IFIDW[0]) - 1;

BOOL NdasDmIsLANSCSIPortInterface(LPCWSTR wszDbccName)
{
	LPCWSTR pwch = wszDbccName;
	for (DWORD i = 0; i < LANSCSI_DEV_IFIDW_LEN; ++i, ++pwch) {
		if (*pwch != LANSCSI_DEV_IFIDW[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL NdasDmGetNdasLogDevSlotNoOfScsiPort(
	LPCWSTR wszDbccName, LPDWORD lpdwSlotNo)
{
	_ASSERTE(!IsBadWritePtr(lpdwSlotNo, sizeof(DWORD)));
	_ASSERTE(!IsBadStringPtr(wszDbccName, _MAX_PATH));

	DPInfo(_FT("NdasDmGetNdasLogDevSlotNoOfScsiPort got %s.\n"), wszDbccName);

	//
	// extract the last string between '&' and '#', which has slot number and unitdisk numer
	//
	// example of dbcc_name at logical device slot no 10
	//
	// \\?\lanscsibus#netdisk_v0#1&1a590e2c&5&10#{2accfe60-c130-11d2-b082-00a0c91efb8b}
	//                                       ^^^^

	if (!NdasDmIsLANSCSIPortInterface(wszDbccName)) {
		DPInfo(_FT("Non-LANSCSI Port Interface Name.\n"));
		return FALSE;
	}

	// skip LANSCSIIDDEV_IFDW
	LPCWSTR pwszLeftBound = &wszDbccName[LANSCSI_DEV_IFIDW_LEN];

	// pwszLeftBound now points to 
	// \\?\lanscsibus#netdisk_v0#1&1a590e2c&5&10#{2accfe60-c130-11d2-b082-00a0c91efb8b}
	//                         ^
	//                     left bound

	while (*pwszLeftBound != L'#') {
		if (*pwszLeftBound == L'\0') {
			DPError(_FT("Slot Number Parse Error at left bound!\n"));
			return FALSE; // invalid!
		}
		++pwszLeftBound;
	}

	// pwszpwszLeftBound now points to 
	// \\?\lanscsibus#netdisk_v0#1&1a590e2c&5&10#{2accfe60-c130-11d2-b082-00a0c91efb8b}
	//                          ^          
	//                      left bound     

	LPCWSTR pwszRightBound = pwszLeftBound + 1;
	while (*pwszRightBound != L'#') {
		if (*pwszRightBound == L'\0') {
			DPError(_FT("Slot Number Parse Error at right bound!\n"));
			return FALSE; // invalid!
		}
		++pwszRightBound;
	}

	// pwszRightBound now points to 
	// \\?\lanscsibus#netdisk_v0#1&1a590e2c&5&10#{2accfe60-c130-11d2-b082-00a0c91efb8b}
	//                          ^               ^
	//                      left bound     right bound
	
	LPCWSTR pwszSlotStart = pwszRightBound - 1;
	while (*pwszSlotStart != L'&') {
		if (pwszSlotStart <= pwszLeftBound) {
			DPError(_FT("Slot Number Parse Error at slot start bound!\n"));
			return FALSE;
		}
		--pwszSlotStart;
	}

	// pwszRightBound now points to 
	// \\?\lanscsibus#netdisk_v0#1&1a590e2c&5&10#{2accfe60-c130-11d2-b082-00a0c91efb8b}
	//                          ^            ^  ^
	//                      left bound       |  right bound
    //                                      slot start

	if (pwszSlotStart + 1 == pwszRightBound) {
		// &# -> no slot number
		DPError(_FT("Slot Number Parse Error at slot start bound!\n"));
		return FALSE;
	}

	WCHAR szSlotNo[10];
	LPCWSTR pch = pwszSlotStart + 1;
	DWORD i(0);
	for (; pch < pwszRightBound && i < 9; ++i, ++pch) {
		szSlotNo[i] = *pch;
	}
	szSlotNo[i] = L'\0';

	DWORD dwSlotNo = _wtoi(szSlotNo);
	if (dwSlotNo == 0) {
		DPError(_FT("Invalid slot number (%s) -> (%d)\n"), szSlotNo, dwSlotNo);
		return FALSE;
	}

	*lpdwSlotNo = dwSlotNo;

	DPInfo(_FT("Slot no is %d.\n"), dwSlotNo);
	return TRUE;

}
