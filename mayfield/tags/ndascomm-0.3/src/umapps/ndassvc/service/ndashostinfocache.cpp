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

class CDataStringizer
{
	size_t m_cchBuffer;
	TCHAR* m_lpCur;
	TCHAR* m_szBuffer;

public:
	CDataStringizer(size_t MaxBuffer = 512) :
		m_cchBuffer(MaxBuffer),
		m_lpCur(m_szBuffer)
	{
		m_szBuffer = new TCHAR[MaxBuffer];
		m_szBuffer[0] = _T('\0');
	}

	template <typename T>
	CDataStringizer(const T* data, size_t MaxBuffer = 512) :
		m_cchBuffer(MaxBuffer),
		m_lpCur(m_szBuffer)
	{
		m_szBuffer = new TCHAR[MaxBuffer];
		m_szBuffer[0] = _T('\0');
		(VOID) Append(data);
	}

	~CDataStringizer()
	{
		if (m_szBuffer) delete m_szBuffer;
	}

	BOOL AppendF(LPCTSTR szFormat, ...);
	BOOL VAppendF(LPCTSTR szFormat, va_list ap);

	template <typename T> BOOL Append(const T*);

	template <typename T> BOOL AppendEx(LPCTSTR szPrefix, const T* lpData)
	{
		return Append<TCHAR>(szPrefix) && Append<T>(lpData);
	}

	LPCTSTR ToString()
	{ 
		return m_szBuffer; 
	}
};

BOOL CDataStringizer::AppendF(LPCTSTR szFormat, ...)
{
	va_list ap; va_start(ap, szFormat);
	BOOL fSuccess = VAppendF(szFormat, ap);
	va_end(ap);
	return fSuccess;
}

BOOL CDataStringizer::VAppendF(LPCTSTR szFormat, va_list ap)
{
	HRESULT hr = ::StringCchVPrintfEx(
		m_lpCur, m_cchBuffer, &m_lpCur, &m_cchBuffer, STRSAFE_IGNORE_NULLS,
		szFormat, ap);
	return SUCCEEDED(hr);
}

template<>
BOOL
CDataStringizer::Append<TCHAR>(
	const TCHAR* szStr)
{
	HRESULT hr = ::StringCchCopyEx(
		m_lpCur, m_cchBuffer, szStr, 
		&m_lpCur, &m_cchBuffer, STRSAFE_IGNORE_NULLS);
	return SUCCEEDED(hr);
}


template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO_VERINFO>(
	const NDAS_HOST_INFO_VERINFO* lpVerInfo)
{
	return AppendF(
		_T("%d.%d.%d.%d"),
		lpVerInfo->VersionMajor,
		lpVerInfo->VersionMinor,
		lpVerInfo->VersionBuild,
		lpVerInfo->VersionPrivate);
}

template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO_IPV4_ADDR>(
	const NDAS_HOST_INFO_IPV4_ADDR* lpAddr)
{
	return AppendF(
		_T("%d.%d.%d.%d"),
		lpAddr->Bytes[0], lpAddr->Bytes[1], 
		lpAddr->Bytes[2], lpAddr->Bytes[3]);
}

template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO_LPX_ADDR>(
	const NDAS_HOST_INFO_LPX_ADDR* lpAddr)
{
	return AppendF(
		_T("%02X%02X%02X:%02X%02X%02X"),
		lpAddr->Bytes[0], lpAddr->Bytes[1], lpAddr->Bytes[2],
		lpAddr->Bytes[3], lpAddr->Bytes[4], lpAddr->Bytes[5]);
}

template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO_LPX_ADDR_ARRAY>(
	const NDAS_HOST_INFO_LPX_ADDR_ARRAY* lpAddrs)
{
	for (DWORD i = 0; i < lpAddrs->AddressCount && i < 4; ++i) 
	{
		if (!AppendEx(_T(" "), lpAddrs->Addresses)) {
			return FALSE;
		}
	}
	return TRUE;
}

template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO_IPV4_ADDR_ARRAY>(
	const NDAS_HOST_INFO_IPV4_ADDR_ARRAY* lpAddrs)
{
	for (DWORD i = 0; i < lpAddrs->AddressCount && i < 4; ++i) 
	{
		if (!AppendEx(_T(" "), lpAddrs->Addresses)) {
			return FALSE;
		}
	}
	return TRUE;
}

static
LPCTSTR
NdasHostOsTypeString(NDAS_HOST_OS_TYPE t)
{
	switch (t) {
	case NHIX_HIC_OS_WIN9X:	return _T("WIN9X");
	case NHIX_HIC_OS_WINNT:	return _T("WINNT");
	case NHIX_HIC_OS_LINUX:	return _T("LINUX");
	case NHIX_HIC_OS_WINCE:	return _T("WINCE");
	case NHIX_HIC_OS_PS2:	return _T("PS2");
	case NHIX_HIC_OS_MAC:	return _T("MAC");
	case NHIX_HIC_OS_EMBEDDED_OS: return _T("EMBEDDED");
	case NHIX_HIC_OS_OTHER:		return _T("OTHER");
	case NHIX_HIC_OS_UNKNOWN:	return _T("UNKNOWN");
	default:
		{
			static  __declspec(thread) TCHAR buf[8] = {0};
			HRESULT hr = ::StringCchPrintf(buf, 8, _T("(%02X)"), t);
			_ASSERTE(SUCCEEDED(hr));
			return buf;
		}
	}
}

static
LPCTSTR
NdasHostInfoTransportString(UCHAR flags)
{
	static  __declspec(thread) TCHAR buf[8] = {0};
	HRESULT hr = ::StringCchPrintf(buf, 8, _T("(%02X)"), flags);
	_ASSERTE(SUCCEEDED(hr));
	return buf;
}

static
LPCTSTR
NdasHostInfoFeatureString(ULONG flags)
{
	static  __declspec(thread) TCHAR buf[8] = {0};
	HRESULT hr = ::StringCchPrintf(buf, 8, _T("(%08X)"), flags);
	_ASSERTE(SUCCEEDED(hr));
	return buf;
}

//template <typename T> 
//BOOL 
//CDataStringizer::AppendEx(LPCTSTR szPrefix, const T*)
//{
//	return Append<TCHAR>(szPrefix) && Append<T>(lpData);
//}


template<>
BOOL
CDataStringizer::Append<NDAS_HOST_INFO>(
	const NDAS_HOST_INFO* lpData)
{
	return 
		AppendEx(_T(" OSType: "), NdasHostOsTypeString(lpData->OSType)) &&
		AppendEx(_T(" Hostname: "), lpData->szHostname) &&
		AppendEx(_T(" FQDN: "), lpData->szFQDN) &&
		AppendEx(_T(" OS: "), &lpData->OSVerInfo) &&
		AppendEx(_T(" NDFS: "), &lpData->ReservedVerInfo) &&
        AppendEx(_T(" NDAS: "), &lpData->NDASSWVerInfo) &&
		AppendEx(_T(" Transport: "), NdasHostInfoTransportString(lpData->TransportFlags)) &&
		AppendEx(_T(" Feature: "), NdasHostInfoFeatureString(lpData->FeatureFlags));
}

static VOID
pDumpHostInfo(CONST NDAS_HOST_INFO* pHostInfo)
{
	CDataStringizer(pHostInfo).ToString();
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

	cbAvail -= sizeof(NDAS_HIX_HOST_INFO_DATA) - 
		sizeof(NDAS_HIX_HOST_INFO_ENTRY); // excluding actual entry

	const NDAS_HIX_HOST_INFO_ENTRY* pNHIXHostInfoEntry = pNHIXHostInfoData->Entry;

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

		pNHIXHostInfoEntry = 
			reinterpret_cast<const NDAS_HIX_HOST_INFO_ENTRY*>(
				reinterpret_cast<const BYTE*>(pNHIXHostInfoEntry) + ucInfoLen);

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
