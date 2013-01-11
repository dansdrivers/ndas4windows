#pragma once
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
	CNdasDevice* m_pDevice;
	DWORD m_dwUnitNo;
	CNdasDeviceComm m_devComm;
	NDAS_UNITDEVICE_INFORMATION m_unitDevInfo;

public:

	CNdasUnitDeviceCreator(CNdasDevice& device, DWORD dwUnitNo);
	CNdasUnitDevice* CreateUnitDevice();

protected:

	CNdasUnitDiskDevice* CreateUnitDiskDevice();

	BOOL ReadDIB(PNDAS_DIB_V2* ppDIB_V2);
	BOOL ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2);
	BOOL ReadContentEncryptBlock(PNDAS_CONTENT_ENCRYPT_BLOCK pCEB);

	BOOL ConvertDIBv1toDIBv2(
		CONST NDAS_DIB* pDIBv1, 
		NDAS_DIB_V2* pDIBv2,
		UINT64 nDiskSectorCount);

	static VOID InitializeDIBv2(
		NDAS_DIB_V2* pDIB_V2, 
		UINT64 nDiskSectorCount);

};
