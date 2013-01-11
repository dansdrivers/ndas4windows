#pragma once
#include <ndas/ndasuser.h>

//
// Local functions
//
CString& 
pCapacityString(
	IN OUT CString& str, 
	IN UINT64 ByteSize);

CString&
pCapacityString(
	IN OUT CString& strBuffer, 
	IN DWORD lowPart, 
	IN DWORD highPart);

CString& 
pDelimitedDeviceIdString(
	IN OUT CString& strBuffer, 
	IN const CString& strDevId,
	IN TCHAR chPassword);


CString& 
pDelimitedDeviceIdString2(
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
	IN NDAS_UNITDEVICE_STATUS status,
	IN NDAS_UNITDEVICE_ERROR error);

CString&
pLPXAddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR* pAddr);

CString& 
pIPV4AddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_IPV4_ADDR* pAddr);

CString& 
pAddressString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR_ARRAY* pLPXAddrs,
	CONST NDAS_HOST_INFO_IPV4_ADDR_ARRAY* pIPV4Addrs);


inline CString
pCapacityString(
	IN UINT64 ByteSize)
{
	CString s; return pCapacityString(s, ByteSize);
}

inline CString 
pCapacityString(DWORD lowPart, DWORD highPart)
{
	CString s; return pCapacityString(s, lowPart, highPart);
}

inline CString&
pBlockCapacityString(CString& strBuffer, DWORD lowPart, DWORD highPart)
{
	UINT64 cb = (static_cast<UINT64>(lowPart) | (static_cast<UINT64>(highPart) << 32)) << 9;
	return pCapacityString(strBuffer, cb);
}

inline CString
pBlockCapacityString(DWORD lowPart, DWORD highPart)
{
	CString s; return pBlockCapacityString(s, lowPart, highPart);
}

inline CString&
pBlockCapacityString(CString& strBuffer, UINT64 blocks)
{
	return pCapacityString(strBuffer, blocks << 9);
}

inline CString
pBlockCapacityString(UINT64 blocks)
{
	CString s; return pBlockCapacityString(s, blocks);
}

inline CString 
pDelimitedDeviceIdString(const CString& strDeviceId, TCHAR chPassword)
{
	CString s; return pDelimitedDeviceIdString(s, strDeviceId, chPassword);
}

inline CString 
pDelimitedDeviceIdString2(const CString& strDeviceId, TCHAR chPassword)
{
	CString s; return pDelimitedDeviceIdString2(s, strDeviceId, chPassword);
}

inline CString
pHWVersionString(DWORD dwHWVersion)
{
	CString s; return pHWVersionString(s, dwHWVersion);
}

inline CString
pUnitDeviceMediaTypeString(DWORD MediaType)
{
	CString s; return pUnitDeviceMediaTypeString(s, MediaType);
}

inline CString
pLogicalDeviceStatusString(
	NDAS_LOGICALDEVICE_STATUS status,
	OPTIONAL ACCESS_MASK access)
{
	CString s; return pLogicalDeviceStatusString(s, status, access);
}

inline CString
pLogicalDeviceTypeString(NDAS_LOGICALDEVICE_TYPE Type)
{
	CString s; return pLogicalDeviceTypeString(s, Type);
}

inline CString
pDeviceStatusString(NDAS_DEVICE_STATUS status, NDAS_DEVICE_ERROR lastError)
{
	CString s; return pDeviceStatusString(s, status, lastError);
}

inline CString
pUnitDeviceTypeString(NDAS_UNITDEVICE_TYPE type, NDAS_UNITDEVICE_SUBTYPE subType)
{
	CString s; return pUnitDeviceTypeString(s, type, subType);
}

inline CString
pUnitDeviceStatusString(NDAS_UNITDEVICE_STATUS Status, NDAS_UNITDEVICE_ERROR Error)
{
	CString s; return pUnitDeviceStatusString(s, Status, Error);
}

CString&
pNdasLogicalDiskEncryptString(
	CString& str,
	const NDASUSER_LOGICALDEVICE_INFORMATION* pLogDeviceInfo);

inline CString
pNdasLogicalDiskEncryptString(
	const NDASUSER_LOGICALDEVICE_INFORMATION* pLogDeviceInfo)
{
	CString s; return pNdasLogicalDiskEncryptString(s, pLogDeviceInfo);
}
