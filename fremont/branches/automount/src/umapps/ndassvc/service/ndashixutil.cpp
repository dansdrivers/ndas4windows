#include "stdafx.h"
#include <lfsfiltctl.h>
#include <ndas/ndashix.h>
#include <xtl/xtlautores.h>

#include "lpxtrans.h"
#include "ndashixutil.h"
#include "ndas.ver"

#include "trace.h"
#ifdef RUN_WPP
#include "ndashixutil.tmh"
#endif

#ifndef VER_FILEMAJORVERSION
#define VER_FILEMAJORVERSION VER_PRODUCTMAJORVERSION
#endif
#ifndef VER_FILEMINORVERSION
#define VER_FILEMINORVERSION VER_PRODUCTMINORVERSION
#endif
#ifndef VER_FILEBUILD
#define VER_FILEBUILD VER_PRODUCTBUILD
#endif
#ifndef VER_FILEBUILD_QFE
#define VER_FILEBUILD_QFE VER_PRODUCTBUILD_QFE
#endif


#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

typedef DWORD (*GET_HOST_INFO_PROC)(DWORD cbBuffer, LPBYTE lpBuffer);

LPGUID pGuidToNetwork(LPGUID pGuid)
{
	XTLASSERT(!IsBadWritePtr(pGuid, sizeof(GUID)));
	pGuid->Data1 = htonl(pGuid->Data1);
	pGuid->Data2 = htons(pGuid->Data2);
	pGuid->Data3 = htons(pGuid->Data3);
	return pGuid;
}

LPGUID pGuidFromNetwork(LPGUID pGuid)
{
	XTLASSERT(!IsBadWritePtr(pGuid, sizeof(GUID)));
	pGuid->Data1 = ntohl(pGuid->Data1);
	pGuid->Data2 = ntohs(pGuid->Data2);
	pGuid->Data3 = ntohs(pGuid->Data3);
	return pGuid;
}

VOID pNHIXHeaderToNetwork(NDAS_HIX::PHEADER pHeader)
{
	XTLASSERT(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));
	pHeader->Signature = ::htonl(pHeader->Signature);
	pHeader->Length = ::htons(pHeader->Length);
	pGuidToNetwork(&pHeader->HostGuid);
}

VOID pNHIXHeaderFromNetwork(NDAS_HIX::PHEADER pHeader)
{
	XTLASSERT(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));
	pHeader->Signature = ::ntohl(pHeader->Signature);
	pHeader->Length = ::ntohs(pHeader->Length);
	pGuidFromNetwork(&pHeader->HostGuid);
}

DWORD pGetUnicodeComputerName(
	COMPUTER_NAME_FORMAT NameType, 
	DWORD cbBuffer, 
	LPBYTE lpbBuffer)
{
	DWORD cchSize = cbBuffer / sizeof(WCHAR);
	BOOL fSuccess = ::GetComputerNameExW(
		NameType,
		(WCHAR*) lpbBuffer,
		&cchSize);

	if (!fSuccess) {
		return 0;
	}

	return (cchSize + 1) * sizeof(WCHAR); // plus null character
}

DWORD pGetAnsiComputerName(
	COMPUTER_NAME_FORMAT NameType,
	DWORD cbBuffer, PBYTE lpbBuffer)
{
	DWORD cchSize = cbBuffer * sizeof(CHAR);
	BOOL fSuccess = ::GetComputerNameExA(
		NameType,
		(CHAR*) lpbBuffer,
		&cchSize);

	if (!fSuccess) {
		return 0;
	}

	return (cchSize + 1) * sizeof(CHAR); // plus null character
}

VOID pUnicodeStringToNetwork(DWORD cbData, LPBYTE lpbData)
{
	static const DWORD MAX_CCB = 1024;
	if (0 == cbData) cbData = MAX_CCB;

	USHORT* lpwc = (USHORT*) lpbData;
	for (DWORD i = 0; 
		i < cbData && 0 != *lpwc; 
		i += sizeof(USHORT),
		++lpwc) 
	{
		// 00 00 same for either endian
		*lpwc = htons(*lpwc);
	}
}

VOID pUnicodeStringFromNetwork(DWORD cbData, LPBYTE lpbData)
{
	static const DWORD MAX_CCB = 1024;
	if (0 == cbData) cbData = MAX_CCB;

	USHORT* lpwc = (USHORT*) lpbData;
	for (DWORD i = 0; 
		i < cbData && 0 != *lpwc; 
		i += sizeof(USHORT),
		++lpwc) 
	{ 
		// 00 00 same for either endian
		*lpwc = ntohs(*lpwc);
	}
}

VOID pPrintHostInfoDataEntry(NDAS_HIX::HOST_INFO::DATA::PENTRY pEntry)
{
	switch (pEntry->Class) {
	case NHIX_HIC_OS_FAMILY:
		_tprintf(_T("OS_FAMILY: %d\n"), (BYTE)pEntry->Data);
		break;
	case NHIX_HIC_OS_VER_INFO:
		_tprintf(_T("OS_VER_INFO: %d.%d.%d.%d\n"), 
			ntohs(*((WORD*)(&pEntry->Data)+0)),
			ntohs(*((WORD*)(&pEntry->Data)+1)),
			ntohs(*((WORD*)(&pEntry->Data)+2)),
			ntohs(*((WORD*)(&pEntry->Data)+3)));
		break;
	case NHIX_HIC_HOSTNAME:
		_tprintf(_T("HOSTNAME: %S\n"), (CHAR*)pEntry->Data);
		break;
	case NHIX_HIC_FQDN:
		_tprintf(_T("FQDN: %S\n"), (CHAR*)pEntry->Data);
		break;
	case NHIX_HIC_NETBIOSNAME:
		_tprintf(_T("NETBIOSNAME: %S\n"), (CHAR*)pEntry->Data);
		break;
	case NHIX_HIC_UNICODE_HOSTNAME:
		pUnicodeStringFromNetwork(0, pEntry->Data);
		_tprintf(_T("Unicode Hostname: %s\n"), (WCHAR*) pEntry->Data);
		// pUnicodeStringToNetwork(pEntry->Length, pEntry->Data);
		break;
	case NHIX_HIC_UNICODE_FQDN:
		pUnicodeStringFromNetwork(0, pEntry->Data);
		_tprintf(_T("Unicode FQDN: %s\n"), (WCHAR*) pEntry->Data);
		// pUnicodeStringToNetwork(pEntry->Length, pEntry->Data);
		break;
	case NHIX_HIC_UNICODE_NETBIOSNAME:
		pUnicodeStringFromNetwork(0, pEntry->Data);
		_tprintf(_T("Unicode NetBIOS Name: %s\n"), (WCHAR*) pEntry->Data);
		// pUnicodeStringToNetwork(pEntry->Length, pEntry->Data);
		break;
	case NHIX_HIC_ADDR_LPX:
	case NHIX_HIC_ADDR_IPV4:
	case NHIX_HIC_ADDR_IPV6:
	case NHIX_HIC_NDAS_SW_VER_INFO:
	case NHIX_HIC_NDFS_VER_INFO:
	case NHIX_HIC_TRANSPORT:
	case NHIX_HIC_AD_HOC:
		break;
	}
}

VOID pPrintContains(NDAS_HIX::HOST_INFO::PDATA pData)
{
	DWORD dwFlags = ntohl(pData->Contains);
	if (NHIX_HIC_OS_FAMILY & dwFlags)
		_tprintf(_T("OS_FAMILY "));
	if (NHIX_HIC_OS_VER_INFO & dwFlags)
		_tprintf(_T("OS_VER_INFO "));
	if (NHIX_HIC_HOSTNAME & dwFlags)
		_tprintf(_T("HOSTNAME "));
	if (NHIX_HIC_FQDN & dwFlags)
		_tprintf(_T("FQDN "));
	if (NHIX_HIC_NETBIOSNAME & dwFlags)
		_tprintf(_T("NETBIOSNAME "));
	if (NHIX_HIC_UNICODE_HOSTNAME & dwFlags)
		_tprintf(_T("UNICODE_HOSTNAME "));
	if (NHIX_HIC_UNICODE_FQDN & dwFlags)
		_tprintf(_T("UNICODE_FQDN "));
	if (NHIX_HIC_UNICODE_NETBIOSNAME & dwFlags)
		_tprintf(_T("UNICODE_NETBIOSNAME "));
	if (NHIX_HIC_ADDR_LPX & dwFlags)
		_tprintf(_T("ADDR_LPX "));
	if (NHIX_HIC_ADDR_IPV4 & dwFlags)
		_tprintf(_T("ADDR_IPV4 "));
	if (NHIX_HIC_ADDR_IPV6 & dwFlags)
		_tprintf(_T("ADDR_IPV6 "));
	if (NHIX_HIC_NDAS_SW_VER_INFO & dwFlags)
		_tprintf(_T("NDAS_SW_VER_INFO "));
	if (NHIX_HIC_NDFS_VER_INFO & dwFlags)
		_tprintf(_T("NDFS_VER_INFO "));
	if (NHIX_HIC_TRANSPORT & dwFlags)
		_tprintf(_T("TRANSPORT_TYPE "));
	if (NHIX_HIC_AD_HOC & dwFlags)
		_tprintf(_T("AD_HOC "));
}

VOID pPrintHostInfoData(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	NDAS_HIX::HOST_INFO::PDATA pData = 
		reinterpret_cast<NDAS_HIX::HOST_INFO::PDATA>(lpbBuffer);

	_tprintf(_T("Length: %03d, Count: %03d\n"), 
		pData->Length, pData->Count);
	pPrintContains(pData);
	_tprintf(_T("\n"));

	LPBYTE lpb = (LPBYTE) pData->Entry;
	for (DWORD i = 0; i < pData->Count; ++i) {
		NDAS_HIX::HOST_INFO::DATA::PENTRY pEntry = 
			reinterpret_cast<NDAS_HIX::HOST_INFO::DATA::PENTRY>(lpb);
		_tprintf(_T("Entry[%02d] Length: %03d: "), i, pEntry->Length);
		lpb += pEntry->Length;
		pPrintHostInfoDataEntry(pEntry);
		pEntry = (NDAS_HIX::HOST_INFO::DATA::PENTRY)
			(((BYTE*)pEntry) + pEntry->Length);
	}
}

DWORD pGetUnicodeHostname(DWORD cbBuffer, LPBYTE lpBuffer)
{
	DWORD cbUsed = pGetUnicodeComputerName(
		ComputerNameDnsHostname, cbBuffer, lpBuffer);
	pUnicodeStringToNetwork(cbUsed, lpBuffer);
	return cbUsed;
}

DWORD pGetUnicodeFQDN(DWORD cbBuffer, LPBYTE lpBuffer)
{
	DWORD cbUsed = pGetUnicodeComputerName(
		ComputerNameDnsFullyQualified, cbBuffer, lpBuffer);
	pUnicodeStringToNetwork(cbUsed, lpBuffer);
	return cbUsed;
}

DWORD pGetUnicodeNetBIOSName(DWORD cbBuffer, LPBYTE lpBuffer)
{
	DWORD cbUsed = pGetUnicodeComputerName(
		ComputerNameNetBIOS, cbBuffer, lpBuffer);
	pUnicodeStringToNetwork(cbUsed, lpBuffer);
	return cbUsed;
}

DWORD pGetAnsiHostname(DWORD cbBuffer, LPBYTE lpBuffer)
{
	return pGetAnsiComputerName(ComputerNameDnsHostname, cbBuffer, lpBuffer);
}

DWORD pGetAnsiFQDN(DWORD cbBuffer, LPBYTE lpBuffer)
{
	return pGetAnsiComputerName(ComputerNameDnsFullyQualified, cbBuffer, lpBuffer);
}

DWORD pGetAnsiNetBIOSName(DWORD cbBuffer, LPBYTE lpBuffer)
{
	return pGetAnsiComputerName(ComputerNameNetBIOS, cbBuffer, lpBuffer);
}

DWORD pGetOSFamily(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD cbRequired = sizeof(UCHAR);
	if (cbBuffer < cbRequired) return 0;

	UCHAR* puc = (UCHAR*) lpbBuffer;
	*puc = NHIX_HIC_OS_WINNT;

	return cbRequired;
}

DWORD pGetOSVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD cbRequired = sizeof(NDAS_HIX::HOST_INFO::VER_INFO);
	if (cbBuffer < cbRequired) return 0;

	OSVERSIONINFOEX osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	BOOL fSuccess = ::GetVersionEx((LPOSVERSIONINFO)&osvi);
	XTLASSERT(fSuccess);

	NDAS_HIX::HOST_INFO::PVER_INFO pVerInfo = 
		(NDAS_HIX::HOST_INFO::PVER_INFO) lpbBuffer;

	pVerInfo->VersionMajor = htons(static_cast<USHORT>(osvi.dwMajorVersion));
	pVerInfo->VersionMinor = htons(static_cast<USHORT>(osvi.dwMinorVersion));
	pVerInfo->VersionBuild = htons(static_cast<USHORT>(osvi.dwBuildNumber));
	pVerInfo->VersionPrivate = htons(static_cast<USHORT>(osvi.wServicePackMajor));

	return cbRequired;
}

DWORD pGetLpxAddr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	static const DWORD LPX_ADDRESS_LEN = 6 * sizeof(UCHAR);
	static const DWORD MAX_ADDRESS_COUNT = 4; // maximum 4 addresses are reported

	XTL::AutoSocket s = ::WSASocket(AF_LPX, SOCK_DGRAM, LPXPROTO_DGRAM, NULL, 0, 0);
	if (s.IsInvalid()) 
	{
		return 0;
	}

	LPSOCKET_ADDRESS_LIST pAddrList = pCreateSocketAddressList(s);
	if (NULL == pAddrList) 
	{
		return 0;
	}
	XTL::AutoLocalHandle hLocal = (HLOCAL) pAddrList;

	DWORD cbUsed = 2;
	if (cbBuffer < cbUsed) 
	{
		return 0;
	}

	PUCHAR pucCount = lpbBuffer;
	++lpbBuffer;
	PUCHAR pucAddressLen = lpbBuffer;
	++lpbBuffer;

	*pucAddressLen = LPX_ADDRESS_LEN;
	*pucCount = 0;

	for (int i = 0; 
		i < pAddrList->iAddressCount && 
		cbUsed + LPX_ADDRESS_LEN <= cbBuffer &&
		i < MAX_ADDRESS_COUNT; 
		++i) 
	{
		PSOCKADDR_LPX pSockAddrLpx = (PSOCKADDR_LPX)
			pAddrList->Address[i].lpSockaddr;
		::CopyMemory(
			lpbBuffer, 
			&pSockAddrLpx->LpxAddress.Node[0], 
			LPX_ADDRESS_LEN);
		++(*pucCount);
		cbUsed += LPX_ADDRESS_LEN;
		lpbBuffer += LPX_ADDRESS_LEN;
	}

	return cbUsed;
}

DWORD pGetIPv4Addr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	static const DWORD INET_ADDRESS_LEN = 4 * sizeof(UCHAR);
	static const DWORD MAX_ADDRESS_COUNT = 4; // maximum 4 ip addresses are reported

	XTL::AutoSocket s = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
	if (s.IsInvalid()) 
	{
		return 0;
	}

	LPSOCKET_ADDRESS_LIST pAddrList = pCreateSocketAddressList(s);
	if (NULL == pAddrList) 
	{
		return 0;
	}
	XTL::AutoLocalHandle hLocal = (HLOCAL) pAddrList;

	DWORD cbUsed = 2;
	if (cbBuffer < cbUsed) 
	{
		return 0;
	}

	PUCHAR pucCount = lpbBuffer;
	++lpbBuffer;
	PUCHAR pucAddressLen = lpbBuffer;
	++lpbBuffer;
	*pucAddressLen = INET_ADDRESS_LEN;
	*pucCount = 0;

	for (int i = 0; 
		i < pAddrList->iAddressCount && 
		cbUsed + INET_ADDRESS_LEN <= cbBuffer &&
		i < MAX_ADDRESS_COUNT; 
		++i) 
	{
		PSOCKADDR_IN pSockAddr = (PSOCKADDR_IN)
			pAddrList->Address[i].lpSockaddr;
		::CopyMemory(
			lpbBuffer, 
			&pSockAddr->sin_addr,
			INET_ADDRESS_LEN);
		++(*pucCount);
		cbUsed += INET_ADDRESS_LEN;
		lpbBuffer += INET_ADDRESS_LEN;
	}

	return cbUsed;
}

DWORD pGetIPv6Addr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	//
	// BUGBUG:
	// Does not seem to be working. Should be fixed later
	// After fix this function, add to the procTable in pGetHostInfo()
	//

	static const DWORD INET6_ADDRESS_LEN = 16 * sizeof(UCHAR);

	XTL::AutoSocket s = ::WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
	if (s.IsInvalid()) {
		return 0;
	}

	LPSOCKET_ADDRESS_LIST pAddrList = pCreateSocketAddressList(s);
	if (NULL == pAddrList) {
		return 0;
	}
	XTL::AutoLocalHandle hLocal = (HLOCAL) pAddrList;

	DWORD cbUsed = 2;
	if (cbBuffer < cbUsed) {
		return 0;
	}

	PUCHAR pucCount = lpbBuffer;
	++lpbBuffer;
	PUCHAR pucAddressLen = lpbBuffer;
	++lpbBuffer;
	*pucAddressLen = INET6_ADDRESS_LEN;
	*pucCount = 0;

	for (int i = 0; 
		i < pAddrList->iAddressCount && 
		cbUsed + INET6_ADDRESS_LEN <= cbBuffer; 
		++i) 
	{
		PSOCKADDR_IN pSockAddr = (PSOCKADDR_IN)
			pAddrList->Address[i].lpSockaddr;
		::CopyMemory(
			lpbBuffer, 
			&pSockAddr->sin_addr,
			INET6_ADDRESS_LEN);
		++(*pucCount);
		cbUsed += INET6_ADDRESS_LEN;
		lpbBuffer += INET6_ADDRESS_LEN;
	}

	return cbUsed;
}

DWORD pGetNDASSWVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD cbRequired = sizeof(NDAS_HIX::HOST_INFO::VER_INFO);
	if (cbBuffer < cbRequired) return 0;

	NDAS_HIX::HOST_INFO::PVER_INFO pVerInfo = 
		(NDAS_HIX::HOST_INFO::PVER_INFO) lpbBuffer;

	pVerInfo->VersionMajor = htons(VER_FILEMAJORVERSION);
	pVerInfo->VersionMinor = htons(VER_FILEMINORVERSION);
	pVerInfo->VersionBuild = htons(VER_FILEBUILD);
	pVerInfo->VersionPrivate = htons(VER_FILEBUILD_QFE);

	return cbRequired;
}

DWORD pGetNDFSVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD cbRequired = sizeof(NDAS_HIX::HOST_INFO::VER_INFO);
	if (cbBuffer < cbRequired) return 0;

	NDAS_HIX::HOST_INFO::PVER_INFO pVerInfo = 
		(NDAS_HIX::HOST_INFO::PVER_INFO) lpbBuffer;

	WORD wvMajor, wvMinor;
	
	BOOL fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL, 
		&wvMajor, &wvMinor);

	if (!fSuccess) {
		return 0;
	}

	pVerInfo->VersionMajor = htons(wvMajor);
	pVerInfo->VersionMinor = htons(wvMinor);
	pVerInfo->VersionBuild = 0;
	pVerInfo->VersionPrivate = 0;

	return cbRequired;
}

DWORD pGetNDASTransport(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD FIELD_SIZE = sizeof(NHIX_HIC_TRANSPORT_TYPE);
	if (cbBuffer < FIELD_SIZE) {
		return 0;
	}

	*lpbBuffer = NHIX_TF_LPX;
	// *lpbBuffer = NHIX_TF_IP;
	return FIELD_SIZE;
}

DWORD pGetNDASHostFeature(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	const DWORD FIELD_SIZE = sizeof(NHIX_HIC_HOST_FEATURE_TYPE);
	if (cbBuffer < FIELD_SIZE) 
	{
		return 0;
	}

	NHIX_HIC_HOST_FEATURE_TYPE Type = htonl(NHIX_HFF_DEFAULT);
	::CopyMemory(lpbBuffer, &Type, sizeof(FIELD_SIZE));
	return FIELD_SIZE;
}

DWORD pGetHostInfo(DWORD cbBuffer, PNDAS_HIX_HOST_INFO_DATA pHostInfoData)
{
	const DWORD CB_DATA_HEADER = 
		sizeof(NDAS_HIX::HOST_INFO::DATA) -
		sizeof(NDAS_HIX::HOST_INFO::DATA::ENTRY);

	const DWORD CB_ENTRY_HEADER = 
		sizeof(NDAS_HIX::HOST_INFO::DATA::ENTRY) -
		sizeof(UCHAR);

	static struct _PROC_TABLE {
		ULONG Class;
		GET_HOST_INFO_PROC Proc;
	} procTable[] = {
		{NHIX_HIC_OS_FAMILY, pGetOSFamily},
		{NHIX_HIC_OS_VER_INFO, pGetOSVerInfo},
		{NHIX_HIC_NDFS_VER_INFO, pGetNDFSVerInfo},
		{NHIX_HIC_NDAS_SW_VER_INFO, pGetNDASSWVerInfo},
		{NHIX_HIC_UNICODE_HOSTNAME, pGetUnicodeHostname},
//		{NHIX_HIC_UNICODE_FQDN,		pGetUnicodeFQDN},
//		{NHIX_HIC_UNICODE_NETBIOSNAME, pGetUnicodeNetBIOSName},
//		{NHIX_HIC_HOSTNAME,	pGetAnsiHostname},
//		{NHIX_HIC_FQDN, pGetAnsiFQDN},
//		{NHIX_HIC_NETBIOSNAME, pGetAnsiNetBIOSName},
		{NHIX_HIC_TRANSPORT, pGetNDASTransport},
		{NHIX_HIC_HOST_FEATURE, pGetNDASHostFeature},
		{NHIX_HIC_ADDR_LPX, pGetLpxAddr},
		{NHIX_HIC_ADDR_IPV4, pGetIPv4Addr}
//		{NHIX_HIC_ADDR_IPV6, pGetIPv6Addr},
	};

	LPBYTE lpb = (LPBYTE) pHostInfoData;
	DWORD cbBufferUsed = 0;
	DWORD cbBufferRemaining = cbBuffer;

	PNDAS_HIX_HOST_INFO_DATA pData = pHostInfoData;

	XTLASSERT(cbBufferRemaining >= CB_DATA_HEADER);

	pData->Count = 0;
	pData->Contains = 0;

	cbBufferUsed += CB_DATA_HEADER;
	cbBufferRemaining -= CB_DATA_HEADER;

	if (cbBufferRemaining == 0) 
	{
		pData->Length = static_cast<UCHAR>(cbBufferUsed);
		return cbBufferUsed;
	}

	lpb += CB_DATA_HEADER;

	//
	// Get Host Information
	//
	for (DWORD i = 0; i < RTL_NUMBER_OF(procTable); ++i) 
	{

		if (cbBufferRemaining < (CB_ENTRY_HEADER + 1)) 
		{
			// not enough buffer
			break;
		}

		NDAS_HIX::HOST_INFO::DATA::PENTRY pEntry = 
			reinterpret_cast<NDAS_HIX::HOST_INFO::DATA::PENTRY>(lpb);

		pEntry->Class = ::htonl(procTable[i].Class);
		pEntry->Length = (UCHAR) CB_ENTRY_HEADER;
		// pEntry->Data;

		cbBufferRemaining -= CB_ENTRY_HEADER;
		cbBufferUsed += CB_ENTRY_HEADER;

		DWORD cbUsed = procTable[i].Proc(
			cbBufferRemaining, 
			pEntry->Data);

		XTLASSERT(cbUsed <= cbBufferRemaining);

		if (cbUsed > 0) 
		{

			pData->Contains |= pEntry->Class;
			pEntry->Length += (UCHAR)cbUsed;

			lpb += pEntry->Length;
			cbBufferUsed += cbUsed;
			cbBufferRemaining -= cbUsed;

			++(pData->Count);

		}
		else 
		{

			// rollback
			cbBufferRemaining += CB_ENTRY_HEADER;
			cbBufferUsed -= CB_ENTRY_HEADER;
		}

		//_tprintf(_T("%d bytes total, %d bytes used, %d byte remaining.\n"), 
		//	cbBuffer, 
		//	cbBufferUsed,
		//	cbBufferRemaining); 

//		printBytes(cbBufferUsed, lpbBuffer);
	}
	
//	printBytes(cbBufferUsed, lpbBuffer);
//	_tprintf(_T("%d bytes total, %d byte used.\n"), cbBuffer, cbBufferUsed); 

	//
	// Maximum length is 256 bytes
	pData->Length = static_cast<UCHAR>(min(0xFF, cbBufferUsed));
	return cbBufferUsed;
}

BOOL
pIsValidNHIXHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (cbData < sizeof(NDAS_HIX::HEADER)) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Header: " 
			"NHIX Data Buffer too small: "
			"Buffer %d bytes, Required at least %d bytes.\n", 
			cbData, sizeof(NDAS_HIX::HEADER));
		return FALSE;
	}

	if (cbData < pHeader->Length) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Header: " 
			"NHIX Data buffer too small: "
			"Buffer %d bytes, Len in header %d bytes.\n",
			cbData, pHeader->Length);
		return FALSE;
	}

	if (NDAS_HIX_SIGNATURE != pHeader->Signature) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Header: " 
			"NHIX Header Signature Mismatch [%c%c%c%c]\n",
			pHeader->SignatureChars[0],
			pHeader->SignatureChars[1],
			pHeader->SignatureChars[2],
			pHeader->SignatureChars[3]);
		return FALSE;
	}

	if (NHIX_CURRENT_REVISION != pHeader->Revision) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Header: " 
			"Unsupported NHIX Revision %d, Required Rev. %d\n",
			pHeader->Revision, NHIX_CURRENT_REVISION);
		return FALSE;
	}

	return TRUE;
}


BOOL
pIsValidNHIXRequestHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (!pIsValidNHIXHeader(cbData, pHeader)) 
	{
		return FALSE;
	}

	if (pHeader->ReplyFlag) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Request Header: ReplyFlag is set at the request header.\n");
		return FALSE;
	}

	return TRUE;
}

BOOL 
pIsValidNHIXReplyHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (!pIsValidNHIXHeader(cbData, pHeader)) 
	{
		return FALSE;
	}

	if (!pHeader->ReplyFlag) 
	{
		XTLTRACE2(NDASSVC_HIXUTIL, TRACE_LEVEL_ERROR, 
			"Invalid HIX Reply Header: ReplyFlag is not set at the reply header.\n");
		return FALSE;
	}

	return TRUE;
}

VOID 
pBuildNHIXHeader(
	NDAS_HIX::PHEADER pHeader,
	LPCGUID pHostGuid,
	BOOL ReplyFlag, 
	UCHAR Type, 
	USHORT Length)
{
	XTLASSERT(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));

	pHeader->Signature = NDAS_HIX_SIGNATURE;
	::CopyMemory(pHeader->HostId, pHostGuid, sizeof(pHeader->HostId));
	pHeader->ReplyFlag = (ReplyFlag) ? 1 : 0;
	pHeader->Type = Type;
	pHeader->Revision = NHIX_CURRENT_REVISION;
	pHeader->Length = Length;
}

NDAS_UNITDEVICE_ID&
pBuildUnitDeviceIdFromHIXUnitDeviceEntry(
	const NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry,
	NDAS_UNITDEVICE_ID& UnitDeviceId)
{
	XTLASSERT(sizeof(pEntry->DeviceId) == 
		sizeof(UnitDeviceId.DeviceId.Node));
	::CopyMemory(
		UnitDeviceId.DeviceId.Node, 
		pEntry->DeviceId,
		sizeof(UnitDeviceId.DeviceId.Node));
	UnitDeviceId.UnitNo = (DWORD) pEntry->UnitNo;
	return UnitDeviceId;
}

NDAS_HIX::UNITDEVICE_ENTRY_DATA*
pBuildHIXUnitDeviceEntryFromUnitDeviceId(
	const NDAS_UNITDEVICE_ID& UnitDeviceId,
	NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry)
{
	XTLASSERT(sizeof(pEntry->DeviceId) == 
		sizeof(UnitDeviceId.DeviceId.Node));
	::CopyMemory(
		pEntry->DeviceId,
		UnitDeviceId.DeviceId.Node,
		sizeof(UnitDeviceId.DeviceId.Node));
	pEntry->UnitNo = (UCHAR) UnitDeviceId.UnitNo;
	return pEntry;
}
