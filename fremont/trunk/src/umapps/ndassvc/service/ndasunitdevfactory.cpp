#include "stdafx.h"
#include <ndas/ndastype.h>
#include <ndas/ndasctype.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndasop.h>
#include <scrc32.h>
#include <des.h>
#include <ndasscsi.h>
#include "eventlog.h"
#include "ndascomobjectsimpl.hpp"
#include "ndasunitdev.h"
#include "ndassvcdef.h"
#include "ndasdevcomm.h"

#include "ndasunitdevfactory.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasunitdevfactory.tmh"
#endif

LONG DbgLevelSvcUnitFact = DBG_LEVEL_SVC_UNIT_FACT;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelSvcUnitFact) {							\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

namespace
{
	NDAS_LOGICALDEVICE_TYPE
	pGetNdasLogicalUnitTypeFromDIBv2(CONST NDAS_DIB_V2* pDIBV2);

	NDAS_DISK_UNIT_TYPE
	pGetNdasDiskUnitTypeFromDIBv2(CONST NDAS_DIB_V2* pDIBV2);
}

//////////////////////////////////////////////////////////////////////////
//
// NDAS Unit Device Instance Creator
//
//////////////////////////////////////////////////////////////////////////

CNdasUnitDeviceFactory::CNdasUnitDeviceFactory (
	INdasDevice* pNdasDevice, 
	DWORD		 dwUnitNo) :
	m_pNdasDevice(pNdasDevice),
	m_devComm(pNdasDevice, dwUnitNo)
{
	ZeroMemory( &m_udinfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFO) );
	ZeroMemory( &m_ndasUnitId, sizeof(NDAS_UNITDEVICE_ID) );

	m_udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);

	COMVERIFY(pNdasDevice->get_NdasDeviceId(&m_ndasUnitId.DeviceId));
	
	m_ndasUnitId.UnitNo = dwUnitNo;
}

HRESULT
CNdasUnitDeviceFactory::CreateUnitDevice(__deref_out INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = NULL;

	NDASID_EXT_DATA ndasIdExtension;
	
	m_pNdasDevice->get_NdasIdExtension(&ndasIdExtension);

	HRESULT hr = m_devComm.Connect();

	if (FAILED(hr)) {

		NdasUiDbgCall( 1, "devComm.Connect failed, ndasDevice=%p, unit=%d, hr=0x%X\n",
						  m_pNdasDevice, m_ndasUnitId.UnitNo, hr );

		return hr;
	}

	// Discover unit device information

	ZeroMemory( &m_udinfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFO) );

	m_udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);

	hr = m_devComm.GetNdasUnitInfo(&m_udinfo);

	if (FAILED(hr)) {

		NdasUiDbgCall( 1, "GetUnitDeviceInformation failed, ndasDevice=%p, unit=%d, hr=0x%X\n",
						  m_pNdasDevice, m_ndasUnitId.UnitNo, hr );

		return pCreateUnknownUnitDevice(ppNdasUnit);
	}

	// Seagate Extension

	if (ndasIdExtension.Vid == NDAS_VID_SEAGATE) {

		if (m_udinfo.Model[0] != 'S' || m_udinfo.Model[1] != 'T') {

			XTLTRACE2( NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
					   "VID Restriction, ndasDevice=%p, ndasUnit=%d: Model=%ls\n",
					   m_pNdasDevice, m_ndasUnitId.UnitNo, m_udinfo.Model );

			return pCreateUnknownNdasDiskUnit( NDAS_UNITDEVICE_ERROR_SEAGATE_RESTRICTION, ppNdasUnit );
		}
	}

	switch (m_udinfo.MediaType) {

	case NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE:

		hr = pCreateUnitDiskDevice(ppNdasUnit);

		break;

	case NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE:
	case NDAS_UNIT_ATAPI_SEQUENTIAL_ACCESS_DEVICE:
	case NDAS_UNIT_ATAPI_PRINTER_DEVICE:
	case NDAS_UNIT_ATAPI_PROCESSOR_DEVICE:
	case NDAS_UNIT_ATAPI_WRITE_ONCE_DEVICE:
	case NDAS_UNIT_ATAPI_CDROM_DEVICE:
	case NDAS_UNIT_ATAPI_SCANNER_DEVICE:
	case NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE:
	case NDAS_UNIT_ATAPI_MEDIUM_CHANGER_DEVICE:
	case NDAS_UNIT_ATAPI_COMMUNICATIONS_DEVICE:
	case NDAS_UNIT_ATAPI_ARRAY_CONTROLLER_DEVICE:
	case NDAS_UNIT_ATAPI_ENCLOSURE_SERVICES_DEVICE:
	case NDAS_UNIT_ATAPI_REDUCED_BLOCK_COMMAND_DEVICE:
	case NDAS_UNIT_ATAPI_OPTICAL_CARD_READER_WRITER_DEVICE: {

		NDAS_LOGICALUNIT_DEFINITION ludef = {0};

		ludef.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);

		ludef.Type = NDAS_LOGICALDEVICE_TYPE_DVD;
		ludef.ConfigurationGuid = GUID_NULL;
		ludef.DiskCount = 1;
		ludef.SpareCount = 0;
		ludef.NdasChildDeviceId[0] = m_ndasUnitId.DeviceId;
		ludef.ActiveNdasUnits[0] = TRUE;

		CComObject<CNdasUnit>* pNdasUnitInstance;
		CComObject<CNdasUnit>::CreateInstance(&pNdasUnitInstance);

		pNdasUnitInstance->ImplInitialize( m_pNdasDevice,
									   m_ndasUnitId.UnitNo, 
									   NDAS_UNITDEVICE_TYPE_CDROM,
									   CNdasUnit::CreateSubType(NDAS_UNITDEVICE_CDROM_TYPE_DVD),
									   m_udinfo,
									   ludef,
									   0 );

		CComPtr<INdasUnit> pNdasUnit = pNdasUnitInstance;
		*ppNdasUnit = pNdasUnit.Detach();

		break;
	}

	default:

		XTLASSERT(FALSE);
		hr = E_FAIL;
	}

	return hr;
}

HRESULT
CNdasUnitDeviceFactory::pCreateUnknownUnitDevice (
	__deref_out INdasUnit** ppNdasUnit
	)
{
	*ppNdasUnit = NULL;

	CComObject<CNdasNullUnit>* pNdasNullUnitInstance;
	HRESULT hr = CComObject<CNdasNullUnit>::CreateInstance(&pNdasNullUnitInstance);
	
	if (FAILED(hr)) {

		ATLASSERT(FALSE);
		XTLTRACE2( NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				   "CNdasNullUnit::CreateInstance failed, hr=0x%X\n", hr );
		return hr;
	}

	hr = pNdasNullUnitInstance->Initialize(m_pNdasDevice, m_ndasUnitId.UnitNo);
	
	if (FAILED(hr)) {

		ATLASSERT(FALSE);
		
		XTLTRACE2( NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				   "CNdasNullUnit::Initialize failed, hr=0x%X\n", hr );
		return hr;
	}

	CComPtr<INdasUnit> pNdasUnit = pNdasNullUnitInstance;

	*ppNdasUnit = pNdasUnit.Detach();

	return S_OK;
}

HRESULT
CNdasUnitDeviceFactory::pCreateUnknownNdasDiskUnit(
	__in NDAS_UNITDEVICE_ERROR Error,
	__deref_out INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = NULL;

	CComObject<CNdasNullDiskUnit>* pNdasNullDiskUnitInstance;
	HRESULT hr = CComObject<CNdasNullDiskUnit>::CreateInstance(&pNdasNullDiskUnitInstance);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"CNdasNullDiskUnit::CreateInstance failed, hr=0x%X\n", hr);
		return hr;
	}

	hr = pNdasNullDiskUnitInstance->Initialize(
		m_pNdasDevice, 
		m_ndasUnitId.UnitNo,
		m_udinfo,
		Error);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"CNdasNullDiskUnit::Initialize failed, hr=0x%X\n", hr);
		return hr;
	}

	CComPtr<INdasUnit> pNdasUnit = pNdasNullDiskUnitInstance;
	*ppNdasUnit = pNdasUnit.Detach();
	return S_OK;
}

BOOL
CNdasUnitDeviceFactory::IsConsistentDIB (
	CONST NDAS_DIB_V2* DibV2
	)
{
	XTLASSERT( !IsBadReadPtr(DibV2, sizeof(NDAS_DIB_V2)) );

	BOOL consistent = FALSE;

	for (DWORD nidx = 0; nidx < DibV2->nDiskCount + DibV2->nSpareCount; nidx++) {

		CHAR	ndasSimpleSerialNo[20+4] = {0};
		UCHAR	zeroUnitSimpleSerialNo[NDAS_DIB_SERIAL_LEN] = {0};

		if (nidx == NDAS_MAX_UNITS_IN_V2_1) {

			ATLASSERT(FALSE);
			break;
		}

		C_ASSERT( sizeof(m_ndasUnitId.DeviceId.Node) == sizeof(DibV2->UnitLocation[nidx].MACAddr) );

		if (memcmp(m_ndasUnitId.DeviceId.Node, DibV2->UnitLocation[nidx].MACAddr, sizeof(m_ndasUnitId.DeviceId.Node)) != 0) {
			
			continue;
		}

		if (FAILED(GetNdasSimpleSerialNo(&m_udinfo, ndasSimpleSerialNo))) {

			ATLASSERT(FALSE);
			continue;
		}

		if (memcmp(ndasSimpleSerialNo, DibV2->UnitSimpleSerialNo[nidx], sizeof(NDAS_DIB_SERIAL_LEN)) == 0 ||
			memcmp(zeroUnitSimpleSerialNo, DibV2->UnitSimpleSerialNo[nidx], sizeof(NDAS_DIB_SERIAL_LEN)) == 0) {

			consistent = TRUE;
			break;
		}
	}

	ATLASSERT( consistent == TRUE );

	return consistent;
}

HRESULT
CNdasUnitDeviceFactory::pCreateUnitDiskDevice (__deref_out INdasUnit** ppNdasUnit)
{
	// Read DIB
	// Read DIB should success, even if the unit disk does not contain
	// the DIB. If it fails, there should be some communication error.

	NdasUiDbgCall( 2, "in\n" );

	HRESULT hr;

	*ppNdasUnit = NULL;

	NDAS_LOGICALUNIT_DEFINITION ndasLogicalUnitDefinition;
	ndasLogicalUnitDefinition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);

	hr = NdasVsmReadLogicalUnitDefinition( m_devComm.GetNdasHandle(), &ndasLogicalUnitDefinition );

	if (FAILED(hr)) {

		ATLASSERT(FALSE);
		NdasUiDbgCall( 1, "NdasVsmReadLogicalUnitDefinition failed, hr=0x%X\n", hr );
		return hr;
	}

	if (ndasLogicalUnitDefinition.Type == NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE) {

		ATLASSERT( memcmp(&m_ndasUnitId.DeviceId, &ndasLogicalUnitDefinition.NdasChildDeviceId[0], sizeof(m_ndasUnitId.DeviceId)) == 0 );
	}

	//
	// ReadDIB is to allocate pDIBv2
	//

	CHeapPtr<NDAS_DIB_V2> pDIBv2;

	hr = ReadDIB(&pDIBv2);

	if (FAILED(hr)) {

		ATLASSERT(FALSE);
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadDIB failed, hr=0x%X\n", hr);

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Creating unit disk device instance failed.\n");

		return pCreateUnknownNdasDiskUnit(
			NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE,
			ppNdasUnit);
	}

	//
	// Read Block Access Control List
	//

	CHeapPtr<BLOCK_ACCESS_CONTROL_LIST> pBlockAcl;

	if (0 != pDIBv2->BACLSize)
	{
		hr = ReadBlockAcl(&pBlockAcl, pDIBv2->BACLSize);
		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				"ReadBACL failed, hr=0x%X\n", hr);

			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				"Creating unit disk device instance failed.\n");

			return pCreateUnknownNdasDiskUnit(
				NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE,
				ppNdasUnit);
		}
	}

	NDAS_DISK_UNIT_TYPE diskType = pGetNdasDiskUnitTypeFromDIBv2(pDIBv2);
	
	if (NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN == diskType) 
	{
		//
		// Error! Invalid media type
		//
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Media type in DIBv2 is invalid, mediaType=0x%X\n", pDIBv2->iMediaType);

		//
		// we should create generic unknown unit disk device
		//
		return pCreateUnknownNdasDiskUnit(
			NDAS_UNITDEVICE_ERROR_HDD_UNKNOWN_LDTYPE,
			ppNdasUnit);
	}

	CHeapPtr<void> pNdasLogicalUnitRaidData;
	NDAS_RAID_META_DATA raidMetaData = {0};

	if (NMT_RAID1 == pDIBv2->iMediaType ||
		NMT_RAID4 == pDIBv2->iMediaType)
	{
		//
		// These types are not used anymore, however, 
		// for compatibility reasons, we retain these codes
		//
		if (!pNdasLogicalUnitRaidData.AllocateBytes(sizeof(NDAS_LURN_RAID_INFO_V1)))
		{
			return E_OUTOFMEMORY;
		}

		PNDAS_LURN_RAID_INFO_V1 raidInfo = 
			static_cast<PNDAS_LURN_RAID_INFO_V1>(static_cast<PVOID>(pNdasLogicalUnitRaidData));
		raidInfo->SectorsPerBit = pDIBv2->iSectorsPerBit;
		raidInfo->SectorBitmapStart = m_udinfo.SectorCount.QuadPart - 0x0f00;
		raidInfo->SectorInfo = m_udinfo.SectorCount.QuadPart - 0x0002;
		raidInfo->SectorLastWrittenSector = m_udinfo.SectorCount.QuadPart - 0x1000;

	}
	else if (NMT_RAID1R2 == pDIBv2->iMediaType ||
		NMT_RAID4R2 == pDIBv2->iMediaType ||
		NMT_RAID1R3 == pDIBv2->iMediaType ||
		NMT_RAID4R3 == pDIBv2->iMediaType ||
		NMT_RAID5 == pDIBv2->iMediaType) 
	{	
		//
		// do not allocate with "new INFO_RAID"
		// destructor will delete with "HeapFree"
		//

		if (!pNdasLogicalUnitRaidData.AllocateBytes(sizeof(NDAS_RAID_INFO)))
		{
			return E_OUTOFMEMORY;
		}

		PNDAS_RAID_INFO pIR = static_cast<PNDAS_RAID_INFO>(
			static_cast<PVOID>(pNdasLogicalUnitRaidData));

		hr = m_devComm.ReadDiskBlock(&raidMetaData, NDAS_BLOCK_LOCATION_RMD, 1);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, "Reading RMD failed: hr=0x%X\n", hr);
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, "Creating unit disk device instance failed.\n");
			return pCreateUnknownNdasDiskUnit(
				NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE,
				ppNdasUnit);
		}

		if (raidMetaData.Signature != NDAS_RAID_META_DATA_SIGNATURE) {
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, "RMD signature mismatch.");
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR, "Creating unit disk device instance failed.\n");
			return pCreateUnknownNdasDiskUnit(
				NDAS_UNITDEVICE_ERROR_HDD_UNKNOWN_LDTYPE,
				ppNdasUnit);
		}
		
		pIR->BlocksPerBit = pDIBv2->iSectorsPerBit;
		pIR->SpareDiskCount = (UCHAR)(pDIBv2->nSpareCount);
		// To fix: Use RaidSetId and ConfigSetId from NdasOpGetRaidInfo
		::CopyMemory(&pIR->NdasRaidId, &raidMetaData.RaidSetId, sizeof(pIR->NdasRaidId));	
		::CopyMemory(&pIR->ConfigSetId, &raidMetaData.ConfigSetId, sizeof(pIR->ConfigSetId));
	}

	UINT64 ulUserBlocks = static_cast<UINT64>(pDIBv2->sizeUserSpace);
	DWORD luseq = pDIBv2->iSequence;
	
	NdasUiDbgCall( 2, "pDIBv2->iSequence = %d pDIBv2->iMediaType = %d\n", pDIBv2->iSequence, pDIBv2->iMediaType );

	//
	// Read CONTENT_ENCRYPT
	//
	// Read CONTENT_ENCRYPT should success, even if the unit disk does not contain
	// the CONTENT_ENCRYPT. If it fails, there should be some communication error.
	//

	NDAS_CONTENT_ENCRYPT_BLOCK ceb = {0};
	NDAS_CONTENT_ENCRYPT ce = {0};

	hr = ReadContentEncryptBlock(&ceb);

	if (FAILED(hr)) 
	{
		ATLASSERT(FALSE);

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadContentEncryptBlock failed, error=0x%X\n", GetLastError());

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Creating unit disk device instance failed.\n");

		(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_CEB_READ_FAILURE);

		return pCreateUnknownNdasDiskUnit(
			NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE, 
			ppNdasUnit);
	}

	UINT uiRet = ::NdasEncVerifyContentEncryptBlock(&ceb);

	if (NDASENC_ERROR_CEB_INVALID_SIGNATURE == uiRet) 
	{
		// No Content Encryption
		// Safe to ignore
		ce.Method = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
	}
	else if (NDASENC_ERROR_CEB_INVALID_CRC == uiRet) 
	{
		// No Content Encryption
		// Safe to ignore
		ce.Method = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
	}
	else if (NDASENC_ERROR_CEB_UNSUPPORTED_REVISION == uiRet) 
	{
		// Error !
		(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_CEB_UNSUPPORTED_REVISION, uiRet);
		// return NULL;
		return pCreateUnknownNdasDiskUnit(
			NDAS_UNITDEVICE_ERROR_HDD_ECKEY_FAILURE,
			ppNdasUnit);
	}
	else if (NDASENC_ERROR_CEB_INVALID_KEY_LENGTH == uiRet) 
	{
		// No Content Encryption
		(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_CEB_INVALID_KEY_LENGTH, uiRet);
		ce.Method = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
	}
	else if (ERROR_SUCCESS != uiRet) 
	{
		// No Content Encryption
		ce.Method = NDAS_CONTENT_ENCRYPT_METHOD_NONE;
	}
	else 
	{
		//
		// We consider the case that ENCRYPTION is not NONE.
		//

		if (NDAS_CONTENT_ENCRYPT_METHOD_NONE != ceb.Method) 
		{
			BYTE SysKey[16] = {0};
			DWORD cbSysKey = sizeof(SysKey);

			uiRet = ::NdasEncGetSysKey(cbSysKey, SysKey, &cbSysKey);

			if (ERROR_SUCCESS != uiRet) 
			{
				(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_GET_SYS_KEY, uiRet);
				// return NULL;
				return pCreateUnknownNdasDiskUnit(
					NDAS_UNITDEVICE_ERROR_HDD_ECKEY_FAILURE,
					ppNdasUnit);
			}

			uiRet = ::NdasEncVerifyFingerprintCEB(SysKey, cbSysKey, &ceb);

			if (ERROR_SUCCESS != uiRet) 
			{
				(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_SYS_KEY_MISMATCH, uiRet);
				// return NULL;
				return pCreateUnknownNdasDiskUnit(
					NDAS_UNITDEVICE_ERROR_HDD_ECKEY_FAILURE,
					ppNdasUnit);
			}

			//
			// Create Content Encrypt
			//
			ce.Method = ceb.Method;
			ce.KeyLength = ceb.KeyLength;
			ce.Key;

			uiRet = ::NdasEncCreateContentEncryptKey(
				SysKey, 
				cbSysKey, 
				ceb.Key, 
				ceb.KeyLength, 
				ce.Key,
				sizeof(ce.Key));

			if (ERROR_SUCCESS != uiRet) 
			{
				(VOID) ::NdasLogEventError2(EVT_NDASSVC_ERROR_KEY_GENERATION_FAILURE, uiRet);
				// return NULL;
				return pCreateUnknownNdasDiskUnit(
					NDAS_UNITDEVICE_ERROR_HDD_ECKEY_FAILURE,
					ppNdasUnit);
			}

		}

	}

	CComObject<CNdasDiskUnit>* pNdasDiskUnitInstance;
	hr = CComObject<CNdasDiskUnit>::CreateInstance(&pNdasDiskUnitInstance);
	if (FAILED(hr))
	{
		ATLASSERT(FALSE);

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"CNdasDiskUnit::CreateInstance failed, hr=0x%X\n", hr);
		return hr;
	}

	hr = pNdasDiskUnitInstance->UnitInitialize(
		m_pNdasDevice,
		m_ndasUnitId.UnitNo,
		diskType,
		m_udinfo,
		ndasLogicalUnitDefinition,
		luseq,
		ulUserBlocks,
		pNdasLogicalUnitRaidData,
		ce,
		pDIBv2,
		pBlockAcl);

	if (FAILED(hr))
	{
		ATLASSERT(FALSE);
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"CNdasDiskUnit::Initialize failed, hr=0x%X\n", hr);
		return hr;
	}

	// We should detach pDIBv2 from being freeed here
	// pUnitDiskDevice will take care of it at dtor
	pDIBv2.Detach();
	pBlockAcl.Detach();
	pNdasLogicalUnitRaidData.Detach();

	CComPtr<INdasUnit> pNdasUnit = pNdasDiskUnitInstance;
	*ppNdasUnit = pNdasUnit.Detach();

	return S_OK;
}

HRESULT
CNdasUnitDeviceFactory::ReadContentEncryptBlock(
	PNDAS_CONTENT_ENCRYPT_BLOCK pCEB)
{
	HRESULT hr = m_devComm.ReadDiskBlock(pCEB, NDAS_BLOCK_LOCATION_ENCRYPT);

	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadDiskBlock failed, block=%I64d, error=0x%X\n", 
			NDAS_BLOCK_LOCATION_ENCRYPT, hr);
		return hr;
	}

	return S_OK;
}

HRESULT
CNdasUnitDeviceFactory::ReadBlockAcl(BLOCK_ACCESS_CONTROL_LIST** ppBACL, UINT32 BACLSize)
{
	HRESULT hr;

	//
	// ppBACL will be set only if this function succeed.
	//

	*ppBACL = 0;

	UINT32 ElementCount = 
		(BACLSize - (sizeof(BLOCK_ACCESS_CONTROL_LIST) - sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT))) / 
		sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);

	CHeapPtr<BLOCK_ACCESS_CONTROL_LIST> pBlockAcl;
	// allocate pBACL to fit sector align
	if (!pBlockAcl.AllocateBytes(512 * BACL_SECTOR_SIZE(ElementCount)))
	{
		// Out of memory!
		return E_OUTOFMEMORY;
	}

	hr = m_devComm.ReadDiskBlock(
		pBlockAcl,
		NDAS_BLOCK_LOCATION_BACL,
		BACL_SECTOR_SIZE(ElementCount));

	//
	// Regardless of the existence,
	// Disk Block should be read.
	// Failure means communication error or disk error
	//
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadDiskBlock failed, block=%I64d, hr=0x%X\n", 
			NDAS_BLOCK_LOCATION_BACL, hr);

		return hr;
	}

	//
	// check structure
	//
	if (BACL_SIGNATURE != pBlockAcl->Signature ||
		BACL_VERSION < pBlockAcl->Version ||
		crc32_calc((unsigned char *)&pBlockAcl->Elements[0],
		sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) * (pBlockAcl->ElementCount)) != 
		pBlockAcl->crc)
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"On-disk BACL information is invalid!\n");

		return NDAS_ERROR_INVALID_BACL_ON_DISK;
	}

	*ppBACL = pBlockAcl.Detach();

	return S_OK;
}

HRESULT
CNdasUnitDeviceFactory::ReadDIB(NDAS_DIB_V2** ppDIBv2)
{
	//
	// ppDIBv2 will be set only if this function succeed.
	//

	HRESULT hr;

	CHeapPtr<NDAS_DIB_V2> pDIBv2;

	if (!pDIBv2.Allocate()) {

		ATLASSERT(FALSE);

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Memory allocation failed\n");
		return E_OUTOFMEMORY;
	}

	hr = m_devComm.ReadDiskBlock(pDIBv2, NDAS_BLOCK_LOCATION_DIB_V2);

	//
	// Regardless of the existence,
	// Disk Block should be read.
	// Failure means communication error or disk error
	//
	if (FAILED(hr)) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadDiskBlock failed, hr=0x%X\n", hr);

		return hr;
	}

	//
	// check signature
	//

	UINT32 dibCrc = crc32_calc(
		static_cast<UCHAR*>(static_cast<void*>(pDIBv2)),
		sizeof(pDIBv2->bytes_248));

	UINT32 dibUnitCrc = DIB_UNIT_CRC_FUNC( crc32_calc, *pDIBv2 );

	if (NDAS_DIB_V2_SIGNATURE != pDIBv2->Signature ||
		pDIBv2->crc32 != dibCrc ||
		pDIBv2->crc32_unitdisks != dibUnitCrc) 
	{
		//
		// Read DIBv1
		//
		
		hr = ReadDIBv1AndConvert(pDIBv2);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				"ReadDIBv1AndConvert failed, hr=0x%X\n", hr);
			return hr;
		}

		if (!IsConsistentDIB(pDIBv2)) {

			ATLASSERT(FALSE);

			// Inconsistent DIB will be reported as single
			InitializeDIBv2AsSingle(pDIBv2);
			pDIBv2->iMediaType = NMT_CONFLICT;
		}

		*ppDIBv2 = pDIBv2.Detach();
		return S_OK;
	}

	//
	// check version
	//
	if (IS_HIGHER_VERSION_V2(*pDIBv2))
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"Unsupported version V2.\n");

		return NDAS_ERROR_NEWER_DIB_VERSION;
	}

	//
	// TODO: Lower version process (future code) ???
	//
	if (0)
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"lower version V2 detected\n");
	}

	//
	// read additional locations if needed
	//
	if (pDIBv2->nDiskCount + pDIBv2->nSpareCount > NDAS_MAX_UNITS_IN_V2) 
	{
		UINT32 nTrailSectorCount = 
			GET_TRAIL_SECTOR_COUNT_V2(pDIBv2->nDiskCount + pDIBv2->nSpareCount);

		SIZE_T dwBytes = sizeof(NDAS_DIB_V2) + 512 * nTrailSectorCount;

		if (!pDIBv2.ReallocateBytes(dwBytes))
		{
			XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
				"HeapReAlloc failed, bytes=%d\n", dwBytes);
			return E_OUTOFMEMORY;
		}

		for (DWORD i = 0; i < nTrailSectorCount; i++) 
		{
			hr = m_devComm.ReadDiskBlock(
				reinterpret_cast<PBYTE>(static_cast<PVOID>(pDIBv2)) + 
				sizeof(NDAS_DIB_V2) + 512 * i,
				NDAS_BLOCK_LOCATION_ADD_BIND + i);

			if (FAILED(hr)) 
			{
				XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
					"Reading additional block failed, block=%d, hr=0x%X\n", 
					NDAS_BLOCK_LOCATION_ADD_BIND + i, hr);

				return hr;
			}
		}
	}

	// Virtual DVD check. Not supported ATM.

	//
	// DIB Consistency Check
	//
	if ( ! IsConsistentDIB(pDIBv2) )
	{
		// Inconsistent DIB will be reported as single
		InitializeDIBv2AsSingle(pDIBv2);
		pDIBv2->iMediaType = NMT_CONFLICT;
	}

	*ppDIBv2 = pDIBv2.Detach();

	return S_OK;
}

VOID
CNdasUnitDeviceFactory::InitializeDIBv2AsSingle(PNDAS_DIB_V2 DibV2)
{
	XTLASSERT( !::IsBadWritePtr(DibV2, sizeof(NDAS_DIB_V2)) );

	// Create a pseudo DIBv2

	InitializeDIBv2(DibV2, m_udinfo.SectorCount.QuadPart);

	C_ASSERT( sizeof(DibV2->UnitLocation[0].MACAddr) == sizeof(m_ndasUnitId.DeviceId.Node) );

	CopyMemory( DibV2->UnitLocation[0].MACAddr, m_ndasUnitId.DeviceId.Node, sizeof(DibV2->UnitLocation[0].MACAddr) );

	if (FAILED(GetNdasSimpleSerialNo(&m_udinfo, DibV2->UnitSimpleSerialNo[0]))) {

		ATLASSERT(FALSE);
	}
}

HRESULT
CNdasUnitDeviceFactory::ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2)
{
	HRESULT hr;
	NDAS_DIB DIBv1 = {0};
	PNDAS_DIB pDIBv1 = &DIBv1;

	hr = m_devComm.ReadDiskBlock(pDIBv1, NDAS_BLOCK_LOCATION_DIB_V1);

	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_ERROR,
			"ReadDiskBlock(DIBv1) failed, hr=0x%X\n", hr);

		return hr;
	}

	//
	// If there is no DIB in the disk,
	// create a pseudo DIBv2
	//
	if (NDAS_DIB_SIGNATURE != pDIBv1->Signature ||
		IS_NDAS_DIBV1_WRONG_VERSION(*pDIBv1)) 
	{
		//
		// Create a pseudo DIBv2
		//
		InitializeDIBv2AsSingle(pDIBv2);		
		return S_OK;
	}

	//
	// Convert V1 to V2
	//
	hr = ConvertDIBv1toDIBv2(
		pDIBv1, 
		pDIBv2, 
		m_udinfo.SectorCount.QuadPart);

	if (FAILED(hr)) 
	{
		//
		// Create a pseudo DIBv2 again!
		//
		InitializeDIBv2AsSingle(pDIBv2);		
		return S_OK;
	}

	return S_OK;
}

VOID 
CNdasUnitDeviceFactory::InitializeDIBv2(
	PNDAS_DIB_V2 pDIBv2, 
	UINT64 nDiskSectorCount)
{
	XTLASSERT(!IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	ZeroMemory(pDIBv2, sizeof(NDAS_DIB_V2));

	pDIBv2->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIBv2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIBv2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIBv2->sizeXArea = NDAS_BLOCK_SIZE_XAREA;
	pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
	pDIBv2->iSectorsPerBit = NDAS_USER_SPACE_ALIGN;
	pDIBv2->sizeUserSpace -= pDIBv2->sizeUserSpace % pDIBv2->iSectorsPerBit;
	pDIBv2->iMediaType = NMT_SINGLE;
	pDIBv2->iSequence = 0;
	pDIBv2->nDiskCount = 1;
	pDIBv2->nSpareCount = 0;
//	pDIBv2->FlagDirty = 0;

}

HRESULT
CNdasUnitDeviceFactory::ConvertDIBv1toDIBv2(
	CONST NDAS_DIB* pDIBv1, 
	NDAS_DIB_V2* pDIBv2, 
	UINT64 nDiskSectorCount)
{
	XTLASSERT(!IsBadReadPtr(pDIBv1, sizeof(NDAS_DIB)));
	XTLASSERT(!IsBadWritePtr(pDIBv2, sizeof(NDAS_DIB_V2)));

	InitializeDIBv2(pDIBv2, nDiskSectorCount);
	// fit to old system
	pDIBv2->sizeUserSpace = nDiskSectorCount - pDIBv2->sizeXArea;
	pDIBv2->iSectorsPerBit = 0; // no backup information

	// single disk
	if (IS_NDAS_DIBV1_WRONG_VERSION(*pDIBv1) || // no DIB information
		NDAS_DIB_DISK_TYPE_SINGLE == pDIBv1->DiskType)
	{
		InitializeDIBv2AsSingle(pDIBv2);
	}
	else
	{
		// pair(2) disks (mirror, aggregation)
		UNIT_DISK_LOCATION *pUnitDiskLocation0, *pUnitDiskLocation1;
		if (NDAS_DIB_DISK_TYPE_MIRROR_MASTER == pDIBv1->DiskType ||
			NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST == pDIBv1->DiskType)
		{
			pUnitDiskLocation0 = &pDIBv2->UnitLocation[0];
			pUnitDiskLocation1 = &pDIBv2->UnitLocation[1];
		}
		else
		{
			pUnitDiskLocation0 = &pDIBv2->UnitLocation[1];
			pUnitDiskLocation1 = &pDIBv2->UnitLocation[0];
		}

		//
		// EtherAddress Conversion
		//
		if (
			0x00 == pDIBv1->EtherAddress[0] &&
			0x00 == pDIBv1->EtherAddress[1] &&
			0x00 == pDIBv1->EtherAddress[2] &&
			0x00 == pDIBv1->EtherAddress[3] &&
			0x00 == pDIBv1->EtherAddress[4] &&
			0x00 == pDIBv1->EtherAddress[5]) 
		{
			// usually, there is no ether address information
			C_ASSERT(
				sizeof(pUnitDiskLocation0->MACAddr) ==
				sizeof(m_ndasUnitId.DeviceId.Node));

			CopyMemory(
				pUnitDiskLocation0->MACAddr, 
				m_ndasUnitId.DeviceId.Node, 
				sizeof(pUnitDiskLocation0->MACAddr));

			pUnitDiskLocation0->UnitNoObsolete = static_cast<UCHAR>(m_ndasUnitId.UnitNo);
			pUnitDiskLocation0->VID = m_ndasUnitId.DeviceId.Vid;
		}
		else
		{
			C_ASSERT(
				sizeof(pUnitDiskLocation0->MACAddr) ==
				sizeof(pDIBv1->EtherAddress));

			CopyMemory(
				pUnitDiskLocation0->MACAddr, 
				pDIBv1->EtherAddress, 
				sizeof(pUnitDiskLocation0->MACAddr));

			pUnitDiskLocation0->UnitNoObsolete = pDIBv1->UnitNumber;
			pUnitDiskLocation0->VID = NDAS_VID_DEFAULT;
		}

		//
		// Peer Address Conversion
		//
		{
			C_ASSERT(
				sizeof(pUnitDiskLocation1->MACAddr) ==
				sizeof(pDIBv1->PeerAddress));

			CopyMemory(
				pUnitDiskLocation1->MACAddr, 
				pDIBv1->PeerAddress, 
				sizeof(pUnitDiskLocation1->MACAddr));

			pUnitDiskLocation1->UnitNoObsolete = pDIBv1->PeerUnitNumber;
			pUnitDiskLocation1->VID = NDAS_VID_DEFAULT;
		}

		ATLASSERT( pUnitDiskLocation0->VID == NDAS_VID_DEFAULT && pUnitDiskLocation1->VID == NDAS_VID_DEFAULT );

		pDIBv2->nDiskCount = 2;
		pDIBv2->nSpareCount = 0;

		switch(pDIBv1->DiskType)
		{
		case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
			pDIBv2->iMediaType = NMT_MIRROR;
			pDIBv2->iSequence = 0;
			break;
		case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
			pDIBv2->iMediaType = NMT_MIRROR;
			pDIBv2->iSequence = 1;
			break;
		case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
			pDIBv2->iMediaType = NMT_AGGREGATE;
			pDIBv2->iSequence = 0;
			break;
		case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
			pDIBv2->iMediaType = NMT_AGGREGATE;
			pDIBv2->iSequence = 1;
			break;
		default:
			return E_FAIL;
		}
	}

	// write crc
	pDIBv2->crc32 = crc32_calc(
		(unsigned char *)pDIBv2,
		sizeof(pDIBv2->bytes_248));

	pDIBv2->crc32_unitdisks = DIB_UNIT_CRC_FUNC( crc32_calc, *pDIBv2 );

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// Local Utility Functions
//
//////////////////////////////////////////////////////////////////////////

namespace
{

NDAS_LOGICALDEVICE_TYPE
pGetNdasLogicalUnitTypeFromDIBv2(CONST NDAS_DIB_V2* pDIBV2)
{
	XTLASSERT(NULL != pDIBV2);
	switch (pDIBV2->iMediaType) 
	{
	case NMT_SINGLE:	return NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
	case NMT_MIRROR:	return NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED;
	case NMT_AGGREGATE: return NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED;
	case NMT_RAID0:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID0;
	case NMT_RAID1:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1;
	case NMT_RAID4:		return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4;
	case NMT_RAID1R2:    return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2;
	case NMT_RAID4R2:    return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2;
	case NMT_RAID1R3:    return NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3;
	case NMT_RAID4R3:    return NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3;
	case NMT_RAID5:    return NDAS_LOGICALDEVICE_TYPE_DISK_RAID5;
	case NMT_VDVD:		return NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD;
	case NMT_CONFLICT:	return NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB;
	default:			return NDAS_LOGICALDEVICE_TYPE_UNKNOWN;
	}
}

NDAS_DISK_UNIT_TYPE
pGetNdasDiskUnitTypeFromDIBv2(CONST NDAS_DIB_V2* pDIBV2)
{
	XTLASSERT(NULL != pDIBV2);
	switch (pDIBV2->iMediaType) {
	case NMT_SINGLE:	return NDAS_UNITDEVICE_DISK_TYPE_SINGLE;
	case NMT_MIRROR: 
		return (pDIBV2->iSequence == 0) ?
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER :
			NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE;
	case NMT_AGGREGATE:	return NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED;
	case NMT_RAID0:		return NDAS_UNITDEVICE_DISK_TYPE_RAID0;
	case NMT_RAID1:		return NDAS_UNITDEVICE_DISK_TYPE_RAID1;
	case NMT_RAID4:		return NDAS_UNITDEVICE_DISK_TYPE_RAID4;
	case NMT_RAID1R2:    return NDAS_UNITDEVICE_DISK_TYPE_RAID1_R2;
	case NMT_RAID4R2:    return NDAS_UNITDEVICE_DISK_TYPE_RAID4_R2;
	case NMT_RAID1R3:    return NDAS_UNITDEVICE_DISK_TYPE_RAID1_R3;
	case NMT_RAID4R3:    return NDAS_UNITDEVICE_DISK_TYPE_RAID4_R3;
	case NMT_RAID5:    return NDAS_UNITDEVICE_DISK_TYPE_RAID5;
	case NMT_VDVD:		return NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD;
	case NMT_CONFLICT:	return NDAS_UNITDEVICE_DISK_TYPE_CONFLICT;	
	default:			return NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN;
	}
}

} // namespace

