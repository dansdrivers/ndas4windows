#pragma once
#include <ndas/ndasuser.h>

//
// Local functions
//
CString&
pCapacityString(
	IN OUT CString& strBuffer, 
	IN DWORD lowPart, 
	IN DWORD highPart);

CString& 
pCreateDelimitedDeviceId(
	IN OUT CString& strBuffer, 
	IN const CString& strDevId,
	IN TCHAR chPassword);

CString& 
pHWVersionString(
	IN OUT CString& strBuffer, 
	IN DWORD dwHWVersion);

CString& 
pUnitDeviceMediaTypeString(
	IN OUT CString& strBuffer, 
	IN DWORD dwMediaType);

CString&
pLogicalDeviceStatusString(
	IN OUT CString&,
	IN NDAS_LOGICALDEVICE_STATUS status,
	IN OPTIONAL ACCESS_MASK access);

CString&
pLogicalDeviceTypeString(
	IN OUT CString& strType, 
	IN NDAS_LOGICALDEVICE_TYPE type);

CString&
pDeviceStatusString(
	IN OUT CString& strStatus,
	IN NDAS_DEVICE_STATUS status,
	IN NDAS_DEVICE_ERROR lastError);

CString&
pUnitDeviceTypeString(
	IN OUT CString& strType,
	IN NDAS_UNITDEVICE_TYPE type,
	IN NDAS_UNITDEVICE_SUBTYPE subType);

CString&
pUnitDeviceStatusString(
	IN OUT CString& strStatus, 
	IN NDAS_UNITDEVICE_STATUS status);

CString&
pLPXAddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR* pAddr);

CString& 
pIPV4AddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_IPV4_ADDR* pAddr);

CString& pAddressString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR_ARRAY* pLPXAddrs,
	CONST NDAS_HOST_INFO_IPV4_ADDR_ARRAY* pIPV4Addrs);

