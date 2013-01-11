#pragma once
#include "ndastypeex.h"

DWORD pGetUnicodeComputerName(
	COMPUTER_NAME_FORMAT NameType, 
	DWORD cbBuffer, 
	LPBYTE lpbBuffer);

DWORD pGetAnsiComputerName(
	COMPUTER_NAME_FORMAT NameType,
	DWORD cbBuffer, PBYTE lpbBuffer);

VOID pUnicodeStringToNetwork(DWORD cbData, LPBYTE lpbData);
VOID pUnicodeStringFromNetwork(DWORD cbData, LPBYTE lpbData);

VOID pPrintContains(NDAS_HIX::HOST_INFO::PDATA pData);
VOID pPrintHostInfoDataEntry(NDAS_HIX::HOST_INFO::DATA::PENTRY pEntry);
VOID pPrintHostInfoData(DWORD cbBuffer, LPBYTE lpbBuffer);

DWORD pGetOSFamily(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetOSVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetUnicodeHostname(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetUnicodeFQDN(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetUnicodeNetBIOSName(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetAnsiHostname(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetAnsiFQDN(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetAnsiNetBIOSName(DWORD cbBuffer, LPBYTE lpBuffer);
DWORD pGetLpxAddr(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetIPv4Addr(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetIPv6Addr(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetNDASSWVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer);
DWORD pGetNDFSVerInfo(DWORD cbBuffer, LPBYTE lpbBuffer);

DWORD pGetHostInfo(DWORD cbBuffer, PNDAS_HIX_HOST_INFO_DATA pHostInfoData);

LPGUID pGuidToNetwork(LPGUID pGuid);
LPGUID pGuidFromNetwork(LPGUID pGuid);
VOID pNHIXHeaderToNetwork(NDAS_HIX::PHEADER pHeader);
VOID pNHIXHeaderFromNetwork(NDAS_HIX::PHEADER pHeader);

BOOL pIsValidNHIXHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader);
BOOL pIsValidNHIXRequestHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader);
BOOL pIsValidNHIXReplyHeader(DWORD cbData, const NDAS_HIX::HEADER* pHeader);

VOID pBuildNHIXHeader(
	NDAS_HIX::PHEADER pHeader, 
	LPCGUID pHostGuid, BOOL ReplyFlag, UCHAR Type, USHORT Length);

inline VOID pBuildNHIXReplyHeader(
	NDAS_HIX::PHEADER pHeader, 
	LPCGUID pHostGuid, UCHAR Type, USHORT Length)
{ 
	pBuildNHIXHeader(pHeader, pHostGuid, TRUE, Type, Length);
}

inline VOID pBuildNHIXRequestHeader(
	NDAS_HIX::PHEADER pHeader, 
	LPCGUID pHostGuid, UCHAR Type, USHORT Length)
{ 
	pBuildNHIXHeader(pHeader, pHostGuid, FALSE, Type, Length);
}

NDAS_UNITDEVICE_ID& 
pBuildUnitDeviceIdFromHIXUnitDeviceEntry(
	const NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry,
	NDAS_UNITDEVICE_ID& UnitDeviceId);

NDAS_HIX::UNITDEVICE_ENTRY_DATA*
pBuildHIXUnitDeviceEntryFromUnitDeviceId(
	const NDAS_UNITDEVICE_ID& UnitDeviceId,
	NDAS_HIX::UNITDEVICE_ENTRY_DATA* pEntry);
