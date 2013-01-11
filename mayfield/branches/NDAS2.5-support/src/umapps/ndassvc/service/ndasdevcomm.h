#pragma once
#include <ndas/ndascomm.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasdib.h>
#include "ndassvcdef.h"

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

	HNDAS m_hNdas;

	BOOL m_bWriteAccess;
	CNdasDevicePtr m_pDevice;
	const DWORD m_dwUnitNo;

public:

	explicit CNdasDeviceComm(CNdasDevicePtr pDevice, DWORD dwUnitNo);
	virtual ~CNdasDeviceComm();

	BOOL Connect(BOOL bWriteAccess = FALSE);
	BOOL Disconnect();
	BOOL GetUnitDeviceInformation(PNDAS_UNITDEVICE_HARDWARE_INFO pUnitDevInfo);
	BOOL GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	BOOL ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock, INT32 i32BlockSize = 1);
};

