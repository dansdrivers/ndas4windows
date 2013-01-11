#pragma once

class CNdasUnit;
class CNdasDiskUnit;

//////////////////////////////////////////////////////////////////////////
//
// NDAS Unit Device Instance Creator
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDeviceFactory
{
	CComPtr<INdasDevice> m_pNdasDevice;
	CNdasDeviceComm m_devComm;
	NDAS_UNITDEVICE_ID m_ndasUnitId;
	NDAS_UNITDEVICE_HARDWARE_INFO m_udinfo;

public:

	CNdasUnitDeviceFactory(INdasDevice* pNdasDevice, DWORD dwUnitNo);
	HRESULT CreateUnitDevice(__deref_out INdasUnit** ppNdasUnit);

protected:

	HRESULT ReadDIB(NDAS_DIB_V2** ppDIB_V2);
	HRESULT ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2);
	HRESULT ReadContentEncryptBlock(PNDAS_CONTENT_ENCRYPT_BLOCK pCEB);
	HRESULT ReadBlockAcl(BLOCK_ACCESS_CONTROL_LIST** ppBACL, UINT32 BACLSize);

	HRESULT ConvertDIBv1toDIBv2(
		CONST NDAS_DIB* pDIBv1, 
		NDAS_DIB_V2* pDIBv2,
		UINT64 nDiskSectorCount);

	BOOL IsConsistentDIB(CONST NDAS_DIB_V2* pDIBv2);

	static void InitializeDIBv2(
		PNDAS_DIB_V2 pDIB_V2, 
		UINT64 nDiskSectorCount);

	void InitializeDIBv2AsSingle(PNDAS_DIB_V2 pDIBv2);

	HRESULT pCreateUnitDiskDevice(__deref_out INdasUnit** ppNdasUnit);
	HRESULT pCreateUnknownUnitDevice(__deref_out INdasUnit** ppNdasUnit);
	HRESULT pCreateUnknownNdasDiskUnit(
		__in NDAS_UNITDEVICE_ERROR Error, 
		__deref_out INdasUnit** ppNdasUnit);
};
