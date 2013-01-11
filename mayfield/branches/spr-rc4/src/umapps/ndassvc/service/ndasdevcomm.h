#pragma once
#include "lanscsiop.h"
#include "ndas/ndastypeex.h"
#include "ndas/ndasdib.h"

//
// forward declarations for structures and classes in the header
//
class CNdasDevice;

/*++

A utility class for processing communications 
between the host and NDAS devices.

--*/

class CNdasDeviceComm
{
protected:

	BOOL m_bInitialized;
	BOOL m_bWriteAccess;
	CNdasDevice* m_pDevice;
	CONST DWORD m_dwUnitNo;

	LANSCSI_PATH m_lspath;

	INT32 GetUserId();
	VOID InitializeLANSCSIPath();

public:

	explicit CNdasDeviceComm(CNdasDevice& device, DWORD dwUnitNo);
	virtual ~CNdasDeviceComm();

	BOOL Initialize(BOOL bWriteAccess = FALSE);
	BOOL Cleanup();
	BOOL GetUnitDeviceInformation(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);
	BOOL GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	BOOL ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock, INT32 i32BlockSize = 1);
	BOOL WriteDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	UINT64 GetDiskSectorCount();

};

