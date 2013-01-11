#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSTransport.h"
#include "LSProtoIde.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSProto"

UINT32						LsprotoCnt  = NR_LSPROTO_PROTOCOL;
PLANSCSIPROTOCOL_INTERFACE	LsprotoList[NR_LSPROTO_MAX_PROTOCOL] = {
												&LsProtoIdeV10Interface,
												&LsProtoIdeV11Interface
										};

NTSTATUS
LspLookupProtocol(
		IN	ULONG			NdasHw,
		IN	ULONG			NdasHwVersion,
		OUT PLSPROTO_TYPE	Protocol
	) {

	UNREFERENCED_PARAMETER(NdasHw);

	ASSERT(NdasHw ==0);

	if(NdasHwVersion == LANSCSIIDE_VERSION_1_0) {
		*Protocol = LSPROTO_IDE_V10;
	} else if(NdasHwVersion == LANSCSIIDE_VERSION_1_1) {
		*Protocol = LSPROTO_IDE_V11;
	} else if(NdasHwVersion == LANSCSIIDE_VERSION_2_0) {
		*Protocol = LSPROTO_IDE_V11;
	} else {
		return STATUS_NOT_IMPLEMENTED;
	}
	return STATUS_SUCCESS;
}


NTSTATUS
LspUpgradeUserIDWithWriteAccess(
		PLANSCSI_SESSION	LSS
	) {

	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(LSS->UserID & 0xffff0000) {
		KDPrintM(DBG_PROTO_ERROR, ("LanscsiSession(%p) has the write-access UserID(%08lx).\n", LSS->UserID));
		return STATUS_UNSUCCESSFUL;
	}

	LSS->UserID = LSS->UserID | (LSS->UserID << 16);

	return STATUS_SUCCESS;
}


NTSTATUS
LspBuildLoginInfo(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo
	) {

	RtlZeroMemory(LoginInfo, sizeof(LSSLOGIN_INFO));

	LoginInfo->HWType				= LSS->HWType;
	LoginInfo->HWVersion			= LSS->HWVersion;
	LoginInfo->LanscsiTargetID		= LSS->LanscsiTargetID;
	LoginInfo->LanscsiLU			= LSS->LanscsiLU;
	LoginInfo->LoginType			= LSS->LoginType;
	LoginInfo->MaxBlocksPerRequest	= LSS->MaxBlocksPerRequest;
	LoginInfo->BlockInBytes			= LSS->BlockInBytes;
	LoginInfo->IsEncryptBuffer		= (LSS->EncryptBuffer !=  NULL);

	RtlCopyMemory(&LoginInfo->UserID, &LSS->UserID,sizeof(UINT32));
	RtlCopyMemory(&LoginInfo->Password, &LSS->Password, sizeof(UINT64));

	return STATUS_SUCCESS;
}

VOID
LspCopy(
		PLANSCSI_SESSION	ToLSS,
		PLANSCSI_SESSION	FromLSS
	) {	
	RtlCopyMemory(ToLSS, FromLSS, sizeof(LANSCSI_SESSION));
}


VOID
LspSetTimeOut(
		PLANSCSI_SESSION	LSS,
		LARGE_INTEGER		TimeOut[]
	) {

	LSS->TimeOuts[0].QuadPart = TimeOut[0].QuadPart;
	KDPrintM(DBG_PROTO_TRACE, ("GenericTimeOut:%I64d\n", LSS->TimeOuts[0].QuadPart));
}


VOID
LspGetAddresses(
		PLANSCSI_SESSION	LSS,
		PTA_LSTRANS_ADDRESS	BindingAddress,
		PTA_LSTRANS_ADDRESS	TargetAddress
	) {

	ASSERT(LSS->SourceAddress.TAAddressCount == 0 ||
			LSS->SourceAddress.TAAddressCount == 1	);

	RtlCopyMemory(BindingAddress, &LSS->SourceAddress, sizeof(TA_LSTRANS_ADDRESS));
	RtlCopyMemory(TargetAddress, &LSS->LSNodeAddress, sizeof(TA_LSTRANS_ADDRESS));
}


NTSTATUS
LspConnect(
		IN OUT PLANSCSI_SESSION LSS,
		IN PTA_LSTRANS_ADDRESS SrcAddr,
		IN PTA_LSTRANS_ADDRESS DestAddr
	) {
	NTSTATUS		ntStatus;

	ASSERT(LSS);
	ASSERT(DestAddr);

	if(SrcAddr->Address[0].AddressType != DestAddr->Address[0].AddressType)
		return STATUS_INVALID_PARAMETER;

	//
	//	Save dest and src addresses
	//
	RtlCopyMemory(&LSS->LSNodeAddress, DestAddr, sizeof(TA_LSTRANS_ADDRESS));
	if(SrcAddr)
		RtlCopyMemory(&LSS->SourceAddress, SrcAddr, sizeof(TA_LSTRANS_ADDRESS));

	ntStatus = LstransOpenAddress(SrcAddr, &LSS->AddressFile);
	if( !NT_SUCCESS(ntStatus) ) {
		goto cleanup;
	}

	ntStatus = LstransOpenConnection(NULL, DestAddr->Address[0].AddressType, &LSS->ConnectionFile);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}

	ntStatus = LstransAssociate(&LSS->AddressFile, &LSS->ConnectionFile);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}

	ntStatus = LstransConnect(&LSS->ConnectionFile, DestAddr, &LSS->TimeOuts[0]);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransDisassociate(&LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}

	LSS->DataEncryptAlgo = 0;
	LSS->EncryptBuffer = NULL;
/*
	ntStatus = LstransRegisterDisconnectHandler(
						&LSS->AddressFile,
						LspDisconHandler,
						&LSS
					);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransDisassociate(&LSS->AddressFile, &LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}
*/

cleanup:
	return ntStatus;
}



NTSTATUS
LspReconnectAndLogin(
		IN OUT PLANSCSI_SESSION LSS,
		IN LSTRANS_TYPE LstransType,
		IN BOOLEAN		IsEncryptBuffer
	) {
	NTSTATUS		ntStatus;
	LSSLOGIN_INFO	LoginInfo;
	LSPROTO_TYPE	LSProto;

	ASSERT(LSS);

	LoginInfo.LoginType	= LOGIN_TYPE_NORMAL;
	RtlCopyMemory(&LoginInfo.UserID, &LSS->UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LoginInfo.Password, &LSS->Password, LSPROTO_PASSWORD_LENGTH);
	LoginInfo.MaxBlocksPerRequest = LSS->MaxBlocksPerRequest;
	LoginInfo.LanscsiTargetID = LSS->LanscsiTargetID;
	LoginInfo.LanscsiLU = LSS->LanscsiLU;
	LoginInfo.HWType = LSS->HWType;
	LoginInfo.HWVersion = LSS->HWVersion;
	LoginInfo.IsEncryptBuffer = IsEncryptBuffer;
	LoginInfo.BlockInBytes = LSS->BlockInBytes;

	ntStatus = LspLookupProtocol(LSS->HWType, LSS->HWVersion, &LSProto);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("HWVersion wrong.\n"));
		return STATUS_NOT_IMPLEMENTED;
	}

	ntStatus = LstransOpenAddress(&LSS->SourceAddress, &LSS->AddressFile);
	if( !NT_SUCCESS(ntStatus) ) {
		goto cleanup;
	}

	ntStatus = LstransOpenConnection(NULL, LstransType, &LSS->ConnectionFile);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}

	ntStatus = LstransAssociate(&LSS->AddressFile, &LSS->ConnectionFile);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}

	ntStatus = LstransConnect(&LSS->ConnectionFile, &LSS->LSNodeAddress, &LSS->TimeOuts[0]);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransDisassociate(&LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}
/*
	ntStatus = LstransRegisterDisconnectHandler(
						&LSS->AddressFile,
						LspDisconHandler,
						&LSS
					);
	if( !NT_SUCCESS(ntStatus) ) {
		LstransDisassociate(&LSS->AddressFile, &LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}
*/

	ntStatus = LspLogin(
					LSS,
					&LoginInfo,
					LSProto
				);
	if(!NT_SUCCESS(ntStatus)) {
		LstransDisconnect(&LSS->ConnectionFile, 0);
		LstransDisassociate(&LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}
cleanup:
	return ntStatus;
}


NTSTATUS
LspDisconnect(
		PLANSCSI_SESSION LSS
	) {
	NTSTATUS ntStatus;

	ASSERT(LSS);

	LstransDisassociate(&LSS->ConnectionFile);
	LstransCloseConnection(&LSS->ConnectionFile);
	ntStatus = LstransCloseAddress(&LSS->AddressFile);

	LSS->LanscsiProtocol = NULL;

	return ntStatus;
}


NTSTATUS
LspNoOperation(
	   IN PLANSCSI_SESSION	LSS,
		IN UINT32			TargetId,
	   OUT PBYTE			PduResponse
	)
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_H2R_PDU_HEADER				pRequestHeader;
	LANSCSI_PDU_POINTERS				pdu;
	NTSTATUS							ntStatus;

	//
	// Check Parameters.
	//
	if(PduResponse == NULL) {
		KDPrintM(DBG_PROTO_ERROR, ("pResult is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_H2R_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = NOP_H2R;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->PathCommandTag = HTONL(++LSS->CommandTag);
	pRequestHeader->TargetID = TargetId;

	//
	// Send Request.
	//
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	ntStatus = LspSendRequest(LSS, &pdu);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request.\n"));
		return ntStatus;
	}

	*PduResponse = LANSCSI_RESPONSE_SUCCESS;

	return STATUS_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
//	Stub function for Lanscsi protocol interface
//
NTSTATUS
LspSendRequest(
			PLANSCSI_SESSION			LSS,
			PLANSCSI_PDU_POINTERS		Pdu
	) {
	ASSERT(LSS);
	
	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoSendRequest) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoSendRequest(
					LSS,
					Pdu
				);
}


NTSTATUS
LspReadReply(
		PLANSCSI_SESSION			LSS,
		PCHAR						Buffer,
		PLANSCSI_PDU_POINTERS		Pdu
	) {
	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoReadReply) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoReadReply(
					LSS,
					Buffer,
					Pdu
				);
}

NTSTATUS
LspLogin(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo,
		LSPROTO_TYPE		LSProto
	) {
	NTSTATUS	status;

	ASSERT(LSS);
	ASSERT(LoginInfo);

	if(LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(LSProto < 0 || LSProto >= LsprotoCnt ) {
		return STATUS_INVALID_PARAMETER;
	}

	LSS->LanscsiProtocol = LsprotoList[LSProto];

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogin) {
		return STATUS_NOT_IMPLEMENTED;
	}

	//
	//	initialize LanscsiSession
	//
	LSS->LoginType				= LoginInfo->LoginType;
	RtlCopyMemory(&LSS->UserID, &LoginInfo->UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LSS->Password, &LoginInfo->Password, LSPROTO_PASSWORD_LENGTH);
	LSS->MaxBlocksPerRequest	= LoginInfo->MaxBlocksPerRequest;
	LSS->LanscsiTargetID		= LoginInfo->LanscsiTargetID;
	LSS->LanscsiLU				= LoginInfo->LanscsiLU;
	LSS->HWType					= LoginInfo->HWType;
	LSS->HWVersion				= LoginInfo->HWVersion;
	LSS->BlockInBytes			= LoginInfo->BlockInBytes;
	LSS->HWProtoType			= HARDWARETYPE_TO_PROTOCOLTYPE(LoginInfo->HWType);
	LSS->HWProtoVersion			= (BYTE)LSProto;
	LSS->HPID					= 0;
	LSS->RPID					= 0;
	LSS->CommandTag				= 0;

	status = LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogin(
					LSS,
					LoginInfo
				);

	if(NT_SUCCESS(status)) {
		if(LoginInfo->IsEncryptBuffer) {
			ULONG	encBuffSize;

			encBuffSize = LSS->MaxBlocksPerRequest * LSS->BlockInBytes;
			KDPrintM(DBG_PROTO_ERROR, ("Allocating a encryption buffer. BufferSize:%lu\n", encBuffSize));

			LSS->EncryptBuffer = ExAllocatePoolWithTag( NonPagedPool, encBuffSize, LSS_ENCRYPTBUFFER_POOLTAG);
			if(!LSS->EncryptBuffer) {
				LSS->EncryptBufferLength = 0;
				KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the encrypt buffer! Performace may slow down.\n"));
			} else {
				LSS->EncryptBufferLength = encBuffSize;
			}
		}
	} else {
		LSS->LanscsiProtocol = NULL;
	}

	return status;
}


NTSTATUS
LspLogout(
		PLANSCSI_SESSION LSS
	) {
	NTSTATUS	status;

	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}
	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogout) {
		return STATUS_NOT_IMPLEMENTED;
	}

	status = LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogout(
					LSS
				);
	if(/* status == STATUS_SUCCESS && */
		LSS->EncryptBuffer &&
		LSS->EncryptBufferLength
		) {

		ExFreePoolWithTag(LSS->EncryptBuffer, LSS_ENCRYPTBUFFER_POOLTAG);
		LSS->EncryptBuffer = NULL;
		LSS->EncryptBufferLength = 0;
	}

	return status;
}


NTSTATUS
LspTextTargetList(
		PLANSCSI_SESSION	LSS,
		PTARGETINFO_LIST	TargetList
	) {
	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoTextTargetList) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoTextTargetList(
					LSS,
					TargetList
				);
}

NTSTATUS
LspTextTartgetData(
		PLANSCSI_SESSION LSS,
		BOOLEAN	GetorSet,
		UINT32	TargetID,
		PTARGET_DATA	TargetData
	) {
	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoTextTargetData) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoTextTargetData(
					LSS,
					GetorSet,
					TargetID,
					TargetData
				);
}

NTSTATUS
LspRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse
	) {
	ASSERT(LSS);
	ASSERT(PduResponse);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequest) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequest(
					LSS,
					PduDesc,
					PduResponse
				);
}


NTSTATUS
LspPacketRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse,
		PUCHAR				PduRegister
				 )
{
	NTSTATUS	result;
	
	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequestATAPI) {
		return STATUS_NOT_IMPLEMENTED ;
	}

	result =  LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequestATAPI(
					LSS,
					PduDesc,
					PduResponse,
					PduRegister
				) ;

	return result;
}

