#include "stdafx.h"
#include "ndashixcli.h"
#include "ndashixutil.h"
#include "ndashostinfocache.h"
#include "autores.h"
#include "xdebug.h"

static
VOID
pConvertToHostInfoVerInfoFromNHIXVerInfo(
	NDAS_HOST_INFO_VERINFO* pVerInfo, 
	CONST NDAS_HIX_HOST_INFO_VER_INFO* pNHIXVerInfo)
{
	pVerInfo->VersionMajor = ::ntohs(pNHIXVerInfo->VersionMajor);
	pVerInfo->VersionMinor = ::ntohs(pNHIXVerInfo->VersionMinor);
	pVerInfo->VersionBuild = ::ntohs(pNHIXVerInfo->VersionBuild);
	pVerInfo->VersionPrivate = ::ntohs(pNHIXVerInfo->VersionPrivate);
}

static
VOID
pConvertToHostInfo(
	PNDAS_HOST_INFO pHostInfo, 
	ULONG ulInfoClass,
	DWORD cbData,
	CONST BYTE* pData)
{
	switch (ulInfoClass) {
	case NHIX_HIC_OS_FAMILY:
		if (cbData < sizeof(UCHAR)) return;
		pHostInfo->OSType = (UCHAR)pData[0]; 
		return;
	case NHIX_HIC_NDFS_VER_INFO: 
		if (cbData < sizeof(NDAS_HIX_HOST_INFO_VER_INFO)) return;
		pConvertToHostInfoVerInfoFromNHIXVerInfo(
			&pHostInfo->ReservedVerInfo,
			(CONST NDAS_HIX_HOST_INFO_VER_INFO*) pData);
		return;
	case NHIX_HIC_NDAS_SW_VER_INFO: 
		if (cbData < sizeof(NDAS_HIX_HOST_INFO_VER_INFO)) return;
		pConvertToHostInfoVerInfoFromNHIXVerInfo(
			&pHostInfo->NDASSWVerInfo,
			(CONST NDAS_HIX_HOST_INFO_VER_INFO*) pData);
		return;
	case NHIX_HIC_OS_VER_INFO:
		if (cbData < sizeof(NDAS_HIX_HOST_INFO_VER_INFO)) return;
		pConvertToHostInfoVerInfoFromNHIXVerInfo(
			&pHostInfo->OSVerInfo,
			(CONST NDAS_HIX_HOST_INFO_VER_INFO*) pData);
		return;
	case NHIX_HIC_HOSTNAME: 
	case NHIX_HIC_NETBIOSNAME:
		if (_T('\0') == pHostInfo->szHostname[0]) {
			HRESULT hr = ::StringCchPrintf(
				pHostInfo->szHostname,
				48,
				_T("%S"),
				(LPTSTR)pData);
			_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);
		}
		return;
	case NHIX_HIC_FQDN: 
		if (_T('\0') == pHostInfo->szFQDN[0]) {
			HRESULT hr = ::StringCchPrintf(
				pHostInfo->szFQDN,
				48,
				_T("%S"),
				(LPTSTR)pData);
			_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);
		}
		return;
	case NHIX_HIC_UNICODE_NETBIOSNAME:
	case NHIX_HIC_UNICODE_HOSTNAME:
		if (_T('\0') == pHostInfo->szHostname[0]) {
			SIZE_T cbToCopy = min(cbData,48 * sizeof(WCHAR));
			if (0 != (cbToCopy % 2)) --cbToCopy;
			::CopyMemory(
				pHostInfo->szHostname,
				pData,
				cbToCopy);
			pUnicodeStringFromNetwork(cbToCopy, (LPBYTE)pHostInfo->szHostname);
			pHostInfo->szHostname[47] = L'\0';
		}
		return;
	case NHIX_HIC_UNICODE_FQDN: 
		if (_T('\0') == pHostInfo->szFQDN[0]) {
			SIZE_T cbToCopy = min(cbData,48 * sizeof(WCHAR));
			if (0 != (cbToCopy % 2)) --cbToCopy;
			::CopyMemory(
				pHostInfo->szFQDN,
				pData,
				cbToCopy);
			pUnicodeStringFromNetwork(cbToCopy, (LPBYTE)pHostInfo->szFQDN);
			pHostInfo->szHostname[47] = L'\0';
		}
		return;
	case NHIX_HIC_ADDR_LPX: 
		if (0 == pHostInfo->LPXAddrs.AddressCount) {
			if (cbData < sizeof(UCHAR) * (2 + 6)) return;
			UCHAR ucAddrCount = *pData;
			UCHAR ucAddrLen = *(pData+1);
			if (ucAddrLen != 6) return;
			UCHAR i = 0;
			for (; i < ucAddrCount && 
				(2+ucAddrLen*(i+1)) < cbData && 
				i < 4; ++i)  // Max Address Count to be stored = 4
			{
				::CopyMemory(
					pHostInfo->LPXAddrs.Addresses[i].Bytes,
					(pData + 2 + ucAddrLen * i),
					6);
			}
			pHostInfo->LPXAddrs.AddressCount = i;
		}
		return;
	case NHIX_HIC_ADDR_IPV4:
		if (0 == pHostInfo->IPV4Addrs.AddressCount) {
			if (cbData < sizeof(UCHAR) * (2 + 4)) return;
			UCHAR ucAddrCount = *pData;
			UCHAR ucAddrLen = *(pData+1);
			if (ucAddrLen != 4) return;
			UCHAR i = 0;
			for (; i < ucAddrCount && 
				(2+ucAddrLen*(i+1)) < cbData && 
				i < 4; ++i)  // Max Address Count to be stored = 4
			{
				::CopyMemory(
					pHostInfo->IPV4Addrs.Addresses[i].Bytes,
					(pData + 2 + ucAddrLen * i),
					ucAddrLen);
			}
			pHostInfo->IPV4Addrs.AddressCount = i;
		}
		return;
	case NHIX_HIC_ADDR_IPV6:
		if (0 == pHostInfo->LPXAddrs.AddressCount) {
			if (cbData < sizeof(UCHAR) * (2 + 16)) return;
			UCHAR ucAddrCount = *pData;
			UCHAR ucAddrLen = *(pData+1);
			if (ucAddrLen != 6) return;
			UCHAR i = 0;
			for (; i < ucAddrCount && 
				(2+ucAddrLen*(i+1)) < cbData&& 
				i < 2; ++i)  // Max Address Count to be stored = 2
			{
				::CopyMemory(
					pHostInfo->IPV6Addrs.Addresses[i].Bytes,
					(pData + 2 + ucAddrLen * i),
					ucAddrLen);
			}
			pHostInfo->IPV6Addrs.AddressCount = i;
		}
		return;
	case NHIX_HIC_HOST_FEATURE: 
		if (cbData < sizeof(ULONG)) return;
		pHostInfo->FeatureFlags = ::ntohl(*((ULONG*)pData));
		return;
	case NHIX_HIC_TRANSPORT: 
		if (cbData < sizeof(UCHAR)) return;
		pHostInfo->TransportFlags = *pData;
		return;
	case NHIX_HIC_AD_HOC:
		// ignores ad-hoc
	default:
		return;
	}
}

static 
PNDAS_HOST_INFO
pCreateHostInfo(CONST CNdasHIXQueryHostInfo::HOST_ENTRY* pHostEntry)
{
	DWORD cbAvail = pHostEntry->cbData; // to guard against invalid packets
	PNDAS_HIX_HOST_INFO_DATA pNHIXHostInfoData = pHostEntry->pData;

	LPVOID pBuffer = ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		sizeof(NDAS_HOST_INFO));

	if (NULL == pBuffer) {
		return NULL;
	}

	PNDAS_HOST_INFO pHostInfo = reinterpret_cast<PNDAS_HOST_INFO>(pBuffer);

	if (cbAvail < sizeof(NDAS_HIX_HOST_INFO_DATA)) {
		// invalid host info data
		return NULL;
	}

	cbAvail -= sizeof(NDAS_HIX_HOST_INFO_DATA) + 
		sizeof(NDAS_HIX_HOST_INFO_ENTRY); // excluding actual entry

	PCNDAS_HIX_HOST_INFO_ENTRY pNHIXHostInfoEntry = pNHIXHostInfoData->Entry;

	for (UCHAR i = 0; 
		cbAvail > sizeof(NDAS_HIX_HOST_INFO_ENTRY) && 
		i < pNHIXHostInfoData->Count; 
		++i) 
	{
		UCHAR ucInfoLen = pNHIXHostInfoEntry->Length;
		if (cbAvail < ucInfoLen) { // adding Data[1]
			break;
		}

		ULONG ulInfoClass = ::ntohl(pNHIXHostInfoEntry->Class);

		pConvertToHostInfo(
			pHostInfo, 
			ulInfoClass, 
			ucInfoLen, 
			pNHIXHostInfoEntry->Data);

		pNHIXHostInfoEntry = reinterpret_cast<PCNDAS_HIX_HOST_INFO_ENTRY>(
			PBYTE(pNHIXHostInfoEntry) + ucInfoLen);

		cbAvail -= ucInfoLen;
	}

	return pHostInfo;
}

CNdasHostInfoCache::CNdasHostInfoCache(DWORD dwExpireInterval) :
	m_dwExpireInterval(dwExpireInterval),
	m_dwLastUpdate(0)
{

}

CNdasHostInfoCache::~CNdasHostInfoCache()
{
	ClearHostInfoMap();
}

BOOL
CNdasHostInfoCache::Update(DWORD dwTimeout)
{
	ClearHostInfoMap();

	CNdasHIXQueryHostInfo query;
	BOOL fSuccess = query.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = query.BroadcastQuery(dwTimeout);
	if (!fSuccess) {
		return FALSE;
	}

	DWORD nHosts = query.GetHostInfoCount();

	for (DWORD i = 0; i < nHosts; ++i) {
		CONST CNdasHIXQueryHostInfo::HOST_ENTRY* pHostEntry = 
			query.GetHostInfo(i);
		GUID hostGuid = pHostEntry->HostGuid;
		if (m_hostInfoMap.end() == m_hostInfoMap.find(hostGuid)) {
			HOST_INFO_CACHE_DATA cacheData;
			PNDAS_HOST_INFO pHostInfo = pCreateHostInfo(pHostEntry);
			if (NULL == pHostInfo) {
				continue;
			}
			cacheData.boundAddr = pHostEntry->boundAddr;
			cacheData.remoteAddr = pHostEntry->remoteAddr;
			cacheData.pHostInfo = pHostInfo;
			m_hostInfoMap.insert(
				HostInfoMap::value_type(hostGuid,cacheData));
		}
	}

	m_dwLastUpdate = ::GetTickCount();

	return TRUE;
}

CONST NDAS_HOST_INFO*
CNdasHostInfoCache::GetHostInfo(LPCGUID lpHostGuid)
{
	if (0 == m_dwLastUpdate || 
		::GetTickCount() - m_dwLastUpdate > m_dwExpireInterval) 
	{
		Update(800);
	}

	HostInfoMap::const_iterator itr = m_hostInfoMap.find(*lpHostGuid);
	if (m_hostInfoMap.end() == itr) {
		Update(800);
		itr = m_hostInfoMap.find(*lpHostGuid);
		if (m_hostInfoMap.end() == itr) {
			return NULL;
		}
	}

	CONST HOST_INFO_CACHE_DATA* pCacheData = &itr->second;
	return pCacheData->pHostInfo;
}

BOOL 
CNdasHostInfoCache::GetHostNetworkInfo(
	LPCGUID lpHostGuid, 
	PSOCKADDR_LPX lpBoundAddr,
	PSOCKADDR_LPX lpRemoteAddr)
{
	_ASSERTE(!IsBadWritePtr(lpBoundAddr,sizeof(SOCKADDR_LPX)));
	_ASSERTE(!IsBadWritePtr(lpRemoteAddr,sizeof(SOCKADDR_LPX)));

	HostInfoMap::const_iterator itr = m_hostInfoMap.find(*lpHostGuid);
	if (m_hostInfoMap.end() == itr) {
		return FALSE;
	}

	CONST HOST_INFO_CACHE_DATA* pCacheData = &itr->second;
	*lpBoundAddr = pCacheData->boundAddr;
	*lpRemoteAddr = pCacheData->remoteAddr;

	return TRUE;
}

VOID
CNdasHostInfoCache::ClearHostInfoMap()
{
	HostInfoMap::const_iterator itr = m_hostInfoMap.begin();
	for (; itr != m_hostInfoMap.end(); ++itr) {
		CONST HOST_INFO_CACHE_DATA* pCacheData = &itr->second;
		PNDAS_HOST_INFO pHostInfo = pCacheData->pHostInfo;
		::HeapFree(::GetProcessHeap(),0,pHostInfo);
	}
	m_hostInfoMap.clear();
}
