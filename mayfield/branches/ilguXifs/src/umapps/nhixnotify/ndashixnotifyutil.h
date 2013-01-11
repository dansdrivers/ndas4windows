#pragma once
#include "ndastypeex.h"

LPGUID pGuidToNetwork(LPGUID pGuid);
LPGUID pGuidFromNetwork(LPGUID pGuid);
VOID pNHIXHeaderToNetwork(NDAS_HIX::PHEADER pHeader);
VOID pNHIXHeaderFromNetwork(NDAS_HIX::PHEADER pHeader);

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
