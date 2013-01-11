#pragma once
#include "ndassvcdef.h"
#include "ndasdevcomm.h"

class CNdasUnitDevice;
class CNdasUnitDiskDevice;

//////////////////////////////////////////////////////////////////////////
//
// NDAS Unit Device Instance Creator
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDeviceCreator
{
	CNdasDevicePtr m_pDevice;
	DWORD m_dwUnitNo;
	CNdasDeviceComm m_devComm;
	NDAS_UNITDEVICE_HARDWARE_INFO m_udinfo;

public:

	CNdasUnitDeviceCreator(CNdasDevicePtr pDevice, DWORD dwUnitNo);
	CNdasUnitDevice* CreateUnitDevice();

protected:

	CNdasUnitDevice* CreateUnitDiskDevice();

	BOOL ReadDIB(NDAS_DIB_V2** ppDIB_V2);
	BOOL ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2);
	BOOL ReadContentEncryptBlock(PNDAS_CONTENT_ENCRYPT_BLOCK pCEB);

	BOOL ConvertDIBv1toDIBv2(
		CONST NDAS_DIB* pDIBv1, 
		NDAS_DIB_V2* pDIBv2,
		UINT64 nDiskSectorCount);

	BOOL IsConsistentDIB(CONST NDAS_DIB_V2* pDIBv2);

	static void InitializeDIBv2(
		PNDAS_DIB_V2 pDIB_V2, 
		UINT64 nDiskSectorCount);

	void InitializeDIBv2AsSingle(PNDAS_DIB_V2 pDIBv2);

	CNdasUnitDevice* _CreateUnknownUnitDevice();
	CNdasUnitDevice* _CreateUnknownUnitDiskDevice(NDAS_UNITDEVICE_ERROR Error);
};
