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
	CComPtr<INdasDevice> m_pNdasDevice;
	const DWORD m_dwUnitNo;

public:

	explicit CNdasDeviceComm(INdasDevice* pNdasDevice, DWORD dwUnitNo);
	~CNdasDeviceComm();

	HRESULT Connect(BOOL bWriteAccess = FALSE);
	HRESULT Disconnect();
	HRESULT GetNdasUnitInfo(PNDAS_UNITDEVICE_HARDWARE_INFO pUnitDevInfo);
	HRESULT GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	HRESULT ReadDiskBlock(PVOID Buffer, INT64 LogicalBlockAddress, DWORD TransferBlocks = 1);
};

