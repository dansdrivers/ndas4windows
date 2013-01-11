#include "stdafx.h"
#include "lpxtrans.h"
#include "ndashix.h"
#include "ndashixnotifyutil.h"

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

typedef DWORD (*GET_HOST_INFO_PROC)(DWORD cbBuffer, LPBYTE lpBuffer);

LPGUID pGuidToNetwork(LPGUID pGuid)
{
	_ASSERTE(!IsBadWritePtr(pGuid, sizeof(GUID)));
	pGuid->Data1 = htonl(pGuid->Data1);
	pGuid->Data2 = htons(pGuid->Data2);
	pGuid->Data3 = htons(pGuid->Data3);
	return pGuid;
}

LPGUID pGuidFromNetwork(LPGUID pGuid)
{
	_ASSERTE(!IsBadWritePtr(pGuid, sizeof(GUID)));
	pGuid->Data1 = ntohl(pGuid->Data1);
	pGuid->Data2 = ntohs(pGuid->Data2);
	pGuid->Data3 = ntohs(pGuid->Data3);
	return pGuid;
}

VOID pNHIXHeaderToNetwork(NDAS_HIX::PHEADER pHeader)
{
	_ASSERTE(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));
	pHeader->Signature = ::htonl(pHeader->Signature);
	pHeader->Length = ::htons(pHeader->Length);
	pGuidToNetwork(&pHeader->HostGuid);
}

VOID pNHIXHeaderFromNetwork(NDAS_HIX::PHEADER pHeader)
{
	_ASSERTE(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));
	pHeader->Signature = ::ntohl(pHeader->Signature);
	pHeader->Length = ::ntohs(pHeader->Length);
	pGuidFromNetwork(&pHeader->HostGuid);
}


VOID 
pBuildNHIXHeader(
	NDAS_HIX::PHEADER pHeader,
	LPCGUID pHostGuid,
	BOOL ReplyFlag, 
	UCHAR Type, 
	USHORT Length)
{
	_ASSERTE(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));

	pHeader->Signature = NDAS_HIX_SIGNATURE;
	::CopyMemory(pHeader->HostId, pHostGuid, sizeof(pHeader->HostId));
	pHeader->ReplyFlag = (ReplyFlag) ? 1 : 0;
	pHeader->Type = Type;
	pHeader->Revision = NHIX_CURRENT_REVISION;
	pHeader->Length = Length;
}

