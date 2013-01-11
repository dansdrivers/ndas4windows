#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "stdafx.h"
#include "ndasdevid.h"
#include "ndashixcli.h"
#include "ndashixutil.h"
#include "lpxtrans.h"
#include "ndashostinfocache.h"
#include "xguid.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"

static 
LPCTSTR pUDAString(NHIX_UDA uda)
{
	switch (uda) {
	case NHIX_UDA_READ_ACCESS:	return _T("RO");
	case NHIX_UDA_WRITE_ACCESS: return _T("WO");
	case NHIX_UDA_READ_WRITE_ACCESS: return _T("RW");
	case NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS: return _T("SHRW_PRIM");
	case NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS: return _T("SHRW_SEC");
	case NHIX_UDA_NO_ACCESS: return _T("NONE");
	default: return _T("INVALID_UDA");
	}
}

static VOID PrintVerInfo(CONST NDAS_HIX_HOST_INFO_VER_INFO* pVerInfo)
{
	_tprintf(_T("%d.%d.%d.%d"), 
		pVerInfo->VersionMajor,
		pVerInfo->VersionMinor,
		pVerInfo->VersionBuild,
		pVerInfo->VersionPrivate);
}

static VOID PrintIPV4Addr(DWORD cbData, CONST BYTE* pAddrData)
{
	DWORD nAddr = pAddrData[0];
	DWORD cbAddrLen = pAddrData[1];

	_tprintf(_T("Count: %d, Address Len: %d "), nAddr, cbAddrLen);
	CONST BYTE* pAddr = pAddrData + 2;
	for (DWORD i = 0; i < nAddr; ++i) {
		_tprintf(_T("%d.%d.%d.%d "), 
			pAddr[0],
			pAddr[1],
			pAddr[2],
			pAddr[3]);
		pAddr += cbAddrLen;
	}

}

static VOID PrintIPV6Addr(DWORD cbData, CONST BYTE* pAddrData)
{
	DWORD nAddr = pAddrData[0];
	DWORD cbAddrLen = pAddrData[1];

	_tprintf(_T("Count: %d, Address Len: %d "), nAddr, cbAddrLen);
	CONST BYTE* pAddr = pAddrData + 2;
	for (DWORD i = 0; i < nAddr; ++i) {
		_tprintf(_T("%X:%X:%X:%X:%X:%X:%X:%X"),
			ntohs(*(USHORT*)(&pAddr[0])),
			ntohs(*(USHORT*)(&pAddr[2])),
			ntohs(*(USHORT*)(&pAddr[4])),
			ntohs(*(USHORT*)(&pAddr[6])),
			ntohs(*(USHORT*)(&pAddr[8])),
			ntohs(*(USHORT*)(&pAddr[10])),
			ntohs(*(USHORT*)(&pAddr[12])),
			ntohs(*(USHORT*)(&pAddr[14])));
		pAddr += cbAddrLen;
	}

	for (DWORD i = 2; i < cbData + 16; i += 16) {
	}
}

static VOID PrintLPXAddr(DWORD cbData, CONST BYTE* pLpxAddr)
{
	DWORD nAddr = pLpxAddr[0];
	DWORD cbAddrLen = pLpxAddr[1];

	_tprintf(_T("Count: %d, Address Len: %d "), nAddr, cbAddrLen);
	CONST BYTE* pAddr = pLpxAddr + 2;
	for (DWORD i = 0; i < nAddr; ++i) {
		_tprintf(_T("%02X:%02X:%02X:%02X:%02X:%02X "), 
			pAddr[0],
			pAddr[1],
			pAddr[2],
			pAddr[3],
			pAddr[4],
			pAddr[5]);
		pAddr += cbAddrLen;
	}
}

static LPCTSTR HostOSTypeString(UCHAR Type)
{
	switch (Type) {
	case NHIX_HIC_OS_WIN9X: return _T("Windows 9x");
	case NHIX_HIC_OS_WINNT: return _T("Windows NT");
	case NHIX_HIC_OS_LINUX: return _T("Linux");
	case NHIX_HIC_OS_WINCE: return _T("Windows CE");
	case NHIX_HIC_OS_PS2: return _T("PlayStation2");
	case NHIX_HIC_OS_MAC: return _T("Macintosh");
	case NHIX_HIC_OS_EMBEDDED_OS: return _T("Embedded OS");
	case NHIX_HIC_OS_OTHER: return _T("Other");
	case NHIX_HIC_OS_UNKNOWN: 
	default:
		return _T("Unknown Type");
	}
}
static LPCTSTR HostInfoClassString(DWORD type)
{
	switch (type) {
	case NHIX_HIC_OS_FAMILY: return _T("OSTYPE");
	case NHIX_HIC_OS_VER_INFO: return _T("OSVERINFO");
	case NHIX_HIC_HOSTNAME: return _T("HOSTNAME");
	case NHIX_HIC_FQDN: return _T("FQDN");
	case NHIX_HIC_NETBIOSNAME: return _T("NETBIOSNAME");
	case NHIX_HIC_UNICODE_HOSTNAME: return _T("WHOSTNAME");
	case NHIX_HIC_UNICODE_FQDN: return _T("WFQDN");
	case NHIX_HIC_UNICODE_NETBIOSNAME: return _T("WNETBIOSNAME");
	case NHIX_HIC_ADDR_LPX: return _T("LPX");
	case NHIX_HIC_ADDR_IPV4: return _T("IPV4");
	case NHIX_HIC_ADDR_IPV6: return _T("IPV6");
	case NHIX_HIC_NDAS_SW_VER_INFO: return _T("SWVERINFO");
	case NHIX_HIC_NDFS_VER_INFO: return _T("NDFSVERINFO");
	case NHIX_HIC_HOST_FEATURE: return _T("HOSTFEATURE");
	case NHIX_HIC_TRANSPORT: return _T("TRANSPORT");
	case NHIX_HIC_AD_HOC: return _T("ADHOC");
	default: return _T("???");
	}
}

VOID
pVerInfoFromNetwork(NDAS_HIX_HOST_INFO_VER_INFO* pVerInfo)
{
	pVerInfo->VersionMajor = ::ntohs(pVerInfo->VersionMajor);
	pVerInfo->VersionMinor = ::ntohs(pVerInfo->VersionMinor);
	pVerInfo->VersionBuild = ::ntohs(pVerInfo->VersionBuild);
	pVerInfo->VersionPrivate = ::ntohs(pVerInfo->VersionPrivate);
}

static
VOID PrintHostInfoEntry(const NDAS_HIX_HOST_INFO_ENTRY* pEntry)
{
	DWORD infoClass = ntohl(pEntry->Class);
	DWORD cbData = pEntry->Length 
		- sizeof(NDAS_HIX_HOST_INFO_ENTRY)
		+ sizeof(UCHAR) * 1; // adding non-zero sized array

	_tprintf(_T("%s:"), HostInfoClassString(infoClass));
	switch (infoClass) {
	case NHIX_HIC_OS_FAMILY:
		_tprintf(_T("%s"), HostOSTypeString(*pEntry->Data));
		break;
	case NHIX_HIC_NDFS_VER_INFO: 
	case NHIX_HIC_NDAS_SW_VER_INFO: 
	case NHIX_HIC_OS_VER_INFO:
		{
			PNDAS_HIX_HOST_INFO_VER_INFO pVerInfo = 
				reinterpret_cast<PNDAS_HIX_HOST_INFO_VER_INFO>(
				(UCHAR*)&pEntry->Data[0]);
			pVerInfoFromNetwork(pVerInfo);
			PrintVerInfo(pVerInfo);
		}
		break;
	case NHIX_HIC_HOSTNAME: 
	case NHIX_HIC_FQDN: 
	case NHIX_HIC_NETBIOSNAME: 
		_tprintf(_T("%S"),pEntry->Data);
		break;
	case NHIX_HIC_UNICODE_HOSTNAME:
	case NHIX_HIC_UNICODE_FQDN: 
	case NHIX_HIC_UNICODE_NETBIOSNAME:
		pUnicodeStringFromNetwork(cbData,(PBYTE)pEntry->Data);
		_tprintf(_T("%s"),pEntry->Data);
		break;
	case NHIX_HIC_ADDR_LPX: 
		{
			PrintLPXAddr(cbData, pEntry->Data);
		}
		break;
	case NHIX_HIC_ADDR_IPV4:
		{
			PrintIPV4Addr(cbData, pEntry->Data);
		}
		break;
	case NHIX_HIC_ADDR_IPV6:
		{
			PrintIPV6Addr(cbData, pEntry->Data);
		}
	case NHIX_HIC_HOST_FEATURE: 
		{
			_tprintf(_T("%d"), ntohl(pEntry->Data[0]));
		}
		break;
	case NHIX_HIC_TRANSPORT: 
		{
			DWORD flags = pEntry->Data[0];
			_tprintf(_T("%d"), flags);
		}
		break;
	case NHIX_HIC_AD_HOC:
		{
			_tprintf(_T("%p"), pEntry->Data);
		}
		break;
	}
	_tprintf(_T("\n"));
}

static
VOID PrintHostInfo(const CNdasHIXQueryHostInfo::HOST_ENTRY* pEntry)
{
	_tprintf(_T("host id: %s\n"), 
		ximeta::CGuid(pGuidFromNetwork((LPGUID)&pEntry->HostGuid)).ToString());
	UCHAR cbLength = pEntry->pData->Length;

	PBYTE pb = (PBYTE)&(*pEntry->pData);
	for (DWORD i = 0; i < cbLength; ++i) {
		if (i % 40 == 0) _tprintf(_T("\n"));
		UCHAR ucData = *pb;
		_tprintf(_T("%02X "), ucData);
		++pb;
	}
	_tprintf(_T("\n"));

	PNDAS_HIX_HOST_INFO_DATA pInfoData = pEntry->pData;
	_tprintf(_T("HOST_INFO_DATA\n"));
	_tprintf(_T("Length      (UCHAR): %d\n"), pInfoData->Length);
	_tprintf(_T("Contains    (ULONG): %08X\n"), ::ntohl(pInfoData->Contains));
	_tprintf(_T("Entry Count (UCHAR): %d\n"), pInfoData->Count);

	PNDAS_HIX_HOST_INFO_ENTRY pInfoEntry = pInfoData->Entry;
	for (DWORD i = 0; i < pEntry->pData->Count; ++i) {
		//_tprintf(_T("Entry [%2d]: "), i);
		//_tprintf(_T("Length (UCHAR): %3d "), pInfoEntry->Length);
		//_tprintf(_T("Class  (ULONG): %08X"), ::ntohl(pInfoEntry->Class));
		//_tprintf(_T("\n"));
		PrintHostInfoEntry(pInfoEntry);
		pInfoEntry = PNDAS_HIX_HOST_INFO_ENTRY(
			PBYTE(pInfoEntry) + pInfoEntry->Length);
	}
	return;
}

int gethostinfo()
{
	CNdasHIXQueryHostInfo query;
	BOOL fSuccess = query.Initialize();
	if (!fSuccess) {
		printf("query init failed.\n");
		return 1;
	}

	fSuccess = query.BroadcastQuery(500);
	if (!fSuccess) {
		printf("query broadcast failed.\n");
		return 1;
	}

	DWORD nHosts = query.GetHostInfoCount();
	printf("found: %d hosts\n", nHosts);

	for (DWORD i = 0; i < nHosts; ++i) {
		CONST CNdasHIXQueryHostInfo::HOST_ENTRY* pEntry = 
			query.GetHostInfo(i);
		PrintHostInfo(pEntry);
	}

	return 0;
}

int surrender()
{
	CNdasHIXSurrenderAccessRequest sar;
	BOOL fSuccess = sar.Initialize();
	if (!fSuccess) {
		printf("sar init failed.\n");
		return 1;
	}

	CNdasUnitDeviceId udi
		(CNdasDeviceId(0x00,0x0b,0xd0,0x01,0x6b,0xb2),
		0);

	NHIX_UDA uda = NHIX_UDA_READ_WRITE_ACCESS;

	BYTE pbLocalAddr[] = {0x02,0x0B,0xDB,0x5C,0xAC,0xB8};
	BYTE pbRemoteAddr[] = {0x00,0x0c,0x29,0x06,0x69,0xB0};
	CSockLpxAddr localAddr(pbLocalAddr,0);
	CSockLpxAddr remoteAddr(pbRemoteAddr,NDAS_HIX_LISTEN_PORT);
	sar.Request(
		&(SOCKADDR_LPX)localAddr,
		&(SOCKADDR_LPX)remoteAddr, 
		udi,
		NHIX_UDA_READ_WRITE_ACCESS);
	return 0;
}

int gethostinfocached()
{
	CNdasHostInfoCache cache;
	GUID hostGuid;
	hostGuid.Data1 = 0x73ba3bba;
	hostGuid.Data2 = 0xead6;
	hostGuid.Data3 = 0x7d4d;
	hostGuid.Data4[0] = 0x8b;
	hostGuid.Data4[1] = 0x62;
	hostGuid.Data4[2] = 0xa0;
	hostGuid.Data4[3] = 0x5a;
	hostGuid.Data4[4] = 0x0c;
	hostGuid.Data4[5] = 0x6c;
	hostGuid.Data4[6] = 0x71;
	hostGuid.Data4[7] = 0x9b;

	PCNDAS_HOST_INFO phi = cache.GetHostInfo(&hostGuid);

	if(NULL != phi) {
		UCHAR osType = phi->OSType;
		wprintf(L"Hostname: %s\n", phi->szHostname);
		wprintf(L"FQDN: %s\n", phi->szFQDN);
	}

	

	return 0;
}

int discover()
{
  CNdasHIXDiscover discover;
  BOOL fSuccess = discover.Initialize();
  if (!fSuccess) {
	printf("discover init failed.\n");
  }

  // NDAS_UNITDEVICE_ID udi;
  CNdasUnitDeviceId udi
	(CNdasDeviceId(0x00,0x0b,0xd0,0x01,0x6b,0xb2),
	 0);
	// CNdasDeviceId(0x00,0x0b,0xd0,0x00,0xc6,0x4c),

  fSuccess = discover.Discover(udi,0x00, 2);
  if (!fSuccess) {
	printf("discover failed.\n");
	WSACleanup();
	return 1;
  }

  DWORD count = discover.GetHostCount(udi);
  for (DWORD i = 0; i < count; ++i) {
	NHIX_UDA uda;
	CSockLpxAddr remoteAddr;
	GUID hostGuid;
	BOOL fSuccess = discover.GetHostData(udi, i, &uda, &hostGuid,&remoteAddr.m_sockLpxAddr, NULL);
	if (!fSuccess) {
	  _tprintf(_T("Error?\n"));
	}
	_tprintf(_T("remoteAddr %s, uda %s\n"), remoteAddr.ToString(), pUDAString(uda));
  }

  return 0;
}

int __cdecl wmain(int argc, TCHAR** argv)
{
  WSADATA wsaData;
  INT iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (0 != iResult) {
	printf("Socket init failed.\n");
	return 1;
  }

  XDbgConsoleOutput co;
  XDbgAttach(&co);
  XDbgInit(_T("nhixcli"));
  XDbgEnableSystemDebugOutput(TRUE);
  XDbgSetOutputLevel(0x40);
  XDbgSetModuleFlags(0xFFFFFFFF);

  if (argc > 1) {
	  if (lstrcmpi(_T("discover"), argv[1]) == 0) {
		  iResult = discover();
	  } else if (lstrcmpi(_T("surrender"), argv[1]) == 0) {
		  iResult = surrender();
	  } else if (lstrcmpi(_T("hostinfo"), argv[1]) == 0) {
		  iResult = gethostinfo();
	  } else if (lstrcmpi(_T("cached"), argv[1]) == 0) {
		  iResult = gethostinfocached();
	  }
  }

  WSACleanup();
  return iResult;
  //discover.Discover(
  //  printf("%s\n", ({1; 2; "aaa";}));
}
