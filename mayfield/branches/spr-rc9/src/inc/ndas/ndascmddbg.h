#pragma once

#include "packetppt.h"
// #include "xdebug.h"
#include "ndascmd.h"

#define BEGIN_ENUM_STRING(x) switch(x) {
#define ENUM_STRING(x) case x: {return TEXT(#x);}
#define ENUM_STRING_DEFAULT(x) default: {return TEXT(x);}
#define END_ENUM_STRING() }

LPCTSTR NdasCmdTypeString(NDAS_CMD_TYPE cmd)
{
	BEGIN_ENUM_STRING(cmd)
		ENUM_STRING(NDAS_CMD_TYPE_NONE)

		ENUM_STRING(NDAS_CMD_TYPE_REGISTER_DEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_UNREGISTER_DEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_ENUMERATE_DEVICES)
		ENUM_STRING(NDAS_CMD_TYPE_SET_DEVICE_PARAM)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_DEVICE_STATUS)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION)

		ENUM_STRING(NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES)
		ENUM_STRING(NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT)
		ENUM_STRING(NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE)

		ENUM_STRING(NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES)
		ENUM_STRING(NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION)

		ENUM_STRING(NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_EJECT_LOGICALDEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE)

		ENUM_STRING(NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE)
		ENUM_STRING(NDAS_CMD_TYPE_QUERY_HOST_INFO)

		ENUM_STRING(NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS)
		ENUM_STRING(NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE)

		ENUM_STRING(NDAS_CMD_TYPE_SET_SERVICE_PARAM)
		ENUM_STRING(NDAS_CMD_TYPE_GET_SERVICE_PARAM)

		ENUM_STRING_DEFAULT("NDAS_CMD_TYPE_???")
	END_ENUM_STRING()
}

LPCTSTR NdasCmdStatusString(NDAS_CMD_STATUS status)
{
	BEGIN_ENUM_STRING(status)
		ENUM_STRING(NDAS_CMD_STATUS_REQUEST)
		ENUM_STRING(NDAS_CMD_STATUS_SUCCESS)
		ENUM_STRING(NDAS_CMD_STATUS_FAILED)
		ENUM_STRING(NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED)
		ENUM_STRING(NDAS_CMD_STATUS_INVALID_REQUEST)
		ENUM_STRING(NDAS_CMD_STATUS_TERMINATION)
		ENUM_STRING(NDAS_CMD_STATUS_UNSUPPORTED_VERSION)
		ENUM_STRING_DEFAULT("NDAS_CMD_STATUS_???")
	END_ENUM_STRING()
}

#undef BEGIN_ENUM_STRING
#undef ENUM_STRING
#undef ENUM_STRING_DEFAULT
#undef END_ENUM_STRING


template<>
VOID StructString(IStructFormatter* psf, NDAS_CMD_HEADER* pHeader)
{
	psf->AppendFormat(TEXT("%c%c%c%c %d.%d "), 
		pHeader->Protocol[0], pHeader->Protocol[1], 
		pHeader->Protocol[2], pHeader->Protocol[3],
		pHeader->VersionMajor, pHeader->VersionMinor);
	psf->AppendFormat(TEXT("OP(%s) "), NdasCmdTypeString((NDAS_CMD_TYPE)pHeader->Command));
	psf->AppendFormat(TEXT("Status(%s) "), NdasCmdStatusString((NDAS_CMD_STATUS)pHeader->Status));
	psf->AppendFormat(TEXT("Trx(%d) "), pHeader->TransactionId);
	psf->AppendFormat(TEXT("MsgSize(%d) "), pHeader->MessageSize);
}


#define DumpPacket __noop

#if 0
//
// Generic Packet Dumper
//
template <class T>
VOID DumpPacket(T* pT, IPacketPrinter* ppt);

template <class T>
VOID DumpPacket(T* pT, IPacketPrinter* ppt)
{
//	ppt->Append(TEXT("Undefined packet format!!\n"));
}

VOID DumpPacket(const BYTE* lpData, DWORD cbData, IPacketPrinter *ppt)
{
	// ignore 1 byte structure (dummy structure is 1 byte in its size!)
	LPBYTE lpb = const_cast<LPBYTE>(lpData);
	for (DWORD i = 0; i < cbData; ++i, ++lpb) {
		ppt->AppendFormat(TEXT("%02X "), *lpb);
		if (i % 20 && i > 0) {
			ppt->Append(TEXT("\n"));
		}
	}
	ppt->Flush();
}

template <>
VOID DumpPacket(NDAS_CMD_HEADER* pHeader, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("Protocol: %c%c%c%c %d.%d\n"), 
		pHeader->Protocol[0], pHeader->Protocol[1], 
		pHeader->Protocol[2], pHeader->Protocol[3],
		pHeader->VersionMajor, pHeader->VersionMinor);
	ppt->AppendFormat(TEXT("Oper    : %s\n"), NdasCmdTypeString((NDAS_CMD_TYPE)pHeader->Command));
	ppt->AppendFormat(TEXT("Status  : %s\n"), NdasCmdStatusString((NDAS_CMD_STATUS)pHeader->Status));
	ppt->AppendFormat(TEXT("Trx Id  : %d\n"), pHeader->TransactionId);
	ppt->AppendFormat(TEXT("Msg Size: %d\n"), pHeader->MessageSize);
	ppt->Flush();
}

template <>
VOID DumpPacket(NDAS_CMD_ERROR::REPLY* pErrorReply, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("Error   : %d (0x%08x)\n"), pErrorReply->dwErrorCode, pErrorReply->dwErrorCode);
	ppt->AppendFormat(TEXT("Data Len: %d\n"), pErrorReply->dwDataLength);
	if (pErrorReply->dwDataLength > 0) {
		ppt->AppendFormat(TEXT("Data:\n"));
		LPBYTE lpData = reinterpret_cast<LPBYTE>(pErrorReply->lpData);
		for (DWORD i = 0; i < pErrorReply->dwDataLength; ++i) {
			ppt->AppendFormat(TEXT("%02X "), lpData[i]);
			if (i > 0 && i % 20 == 0) {
				ppt->Append(TEXT("\n"));
			}
		}
	}
	ppt->Flush();
}

template <>
VOID DumpPacket(NDAS_DEVICE_ID* pDeviceId, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("%02X:%02X:%02X:%02X:%02X:%02X"), 
		pDeviceId->Node[0], pDeviceId->Node[1], pDeviceId->Node[2],
		pDeviceId->Node[3], pDeviceId->Node[4], pDeviceId->Node[5]);
}

template <>
VOID DumpPacket(NDAS_DEVICE_ID_OR_SLOT* pDeviceIdOrSlot, IPacketPrinter *ppt)
{
	if (pDeviceIdOrSlot->bUseSlotNo) {
		ppt->AppendFormat(TEXT("%d"), pDeviceIdOrSlot->SlotNo);
	} else {
		DumpPacket(&pDeviceIdOrSlot->DeviceId, ppt);
	}
}

/*
template <> 
VOID DumpPacket(PNDAS_DEVICE_ENTRY pEntry, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("Device Id:"));
	DumpPacket(&pEntry->NdasDeviceId, ppt);
	ppt->AppendFormat(TEXT("\n"));

	ppt->AppendFormat(TEXT("Device Name: %s\n"), pEntry->wszNdasDeviceName);
	ppt->AppendFormat(TEXT("Allowed Acc: %08X\n"), pEntry->AllowedAccess);
}
*/

/*
template <>
VOID DumpPacket(NDAS_CMD_REGISTER_DEVICE::REQUEST* pRequest, IPacketPrinter* ppt)
{
	PNDAS_DEVICE_ENTRY pEntry = &pRequest->DeviceEntry[0];
	DumpPacket(pEntry, ppt);
	ppt->Flush();
}
*/

template <>
VOID DumpPacket(NDAS_CMD_UNREGISTER_DEVICE::REQUEST* pRequest, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("DeviceID: "));
	DumpPacket(&pRequest->DeviceIdOrSlot, ppt);
	ppt->AppendFormat(TEXT("\n"));
	ppt->Flush();
}

template <>
VOID DumpPacket(NDAS_CMD_ENUMERATE_DEVICES::REPLY* pRequest, IPacketPrinter *ppt)
{
	ppt->AppendFormat(TEXT("Device Entries: %d\n"), pRequest->nDeviceEntries);
	for (DWORD i = 0; i < pRequest->nDeviceEntries; ++i) {
		NDAS_CMD_ENUMERATE_DEVICES::ENUM_ENTRY* pEntry = &pRequest->DeviceEntry[i];
		ppt->AppendFormat(TEXT("Device %d\n"), i);
		DumpPacket(pEntry, ppt);
	}
	ppt->Flush();
}


template <>
VOID DumpPacket(NDAS_CMD_QUERY_DEVICE_INFORMATION::REQUEST* pRequest, IPacketPrinter *ppt)
{
	ppt->Append(TEXT("Device Id: "));
	DumpPacket(&pRequest->DeviceIdOrSlot, ppt);
	ppt->Append(TEXT("\n"));
	ppt->Flush();
}

//template <>
//VOID DumpPacket(NDAS_DEVICE_QUERY_INFORMATION::REPLY* pRequest, IPacketPrinter *ppt)
//{
//}

#endif
