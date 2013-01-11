#ifdef __NDASBOOT__
#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif
#include "ndasboot.h"
#endif

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

#ifdef __NDASBOOT__
enum LpxStatus ProtoStatus = STATUS_LPX_MINIPORT;
#endif

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


BOOLEAN
LspIsConnected(
		PLANSCSI_SESSION	LSS
){
	ASSERT(LSS);

	return	LSS->AddressFile.AddressFileHandle &&
			LSS->AddressFile.AddressFileObject &&
			LSS->ConnectionFile.ConnectionFileHandle &&
			LSS->ConnectionFile.ConnectionFileObject;
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

#ifdef __NDASBOOT__

	LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_NONE;

	if(STATUS_LPX_MINIPORT == ProtoStatus) {	
		ntStatus = LpxConnect(DestAddr, &LSS->ConnectionFile.ConnectionSock);
		if( !NT_SUCCESS(ntStatus) ) {
			goto cleanup;
		}			
		LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_SOCKET;
	} 
	else {	

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
		LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_FILE_OBJECT;
	}
#else

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
#endif __NDASBOOT__

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
LspConnectBindAddrList(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddrList
){
	NTSTATUS	status;
	ULONG		idx_addr;
	TA_LSTRANS_ADDRESS	bindAddr;

	//
	//	Parameter check
	//
	if(DestAddr->TAAddressCount != 1) {
		return STATUS_INVALID_PARAMETER;
	}
	if(BindAddrList->TAAddressCount < 1) {
		return STATUS_INVALID_PARAMETER;
	}

	if(DestAddr->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {
		return STATUS_NOT_IMPLEMENTED;
	}

	bindAddr.TAAddressCount = 1;
	status = STATUS_UNSUCCESSFUL;

	for(idx_addr = 0; idx_addr < (ULONG)BindAddrList->TAAddressCount; idx_addr++) {
		RtlCopyMemory(	bindAddr.Address,
						&BindAddrList->Address[idx_addr],
						sizeof(struct  _AddrLstrans));

		status = LspConnect(LSS, &bindAddr, DestAddr);
		if(NT_SUCCESS(status)) {
			if(BoundAddr) {
				BoundAddr->TAAddressCount = 1;
				RtlCopyMemory(	BoundAddr->Address,
								&BindAddrList->Address[idx_addr],
								sizeof(struct  _AddrLstrans));
			}
			break;
		}
	}

	return status;
}


NTSTATUS
LspConnectEx(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr2,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN BOOLEAN				BindAnyAddress
	) {
	NTSTATUS			status;
	PTA_LSTRANS_ADDRESS	addrList;
	ULONG				addrListLen;
	PTA_LSTRANS_ADDRESS	addrListPrioritized;
	ULONG				idx_addr;
	LSTRANS_TYPE		transport;


	ASSERT(LSS);
	ASSERT(DestAddr);


	//
	//	Parameter check
	//

	if(DestAddr->TAAddressCount != 1) {
		return STATUS_INVALID_PARAMETER;
	}

	if(DestAddr->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {
		return STATUS_NOT_IMPLEMENTED;
	}
	if(BindAddr && DestAddr->Address[0].AddressType != BindAddr->Address[0].AddressType)
		return STATUS_INVALID_PARAMETER;
	if(BindAddr2 && DestAddr->Address[0].AddressType != BindAddr2->Address[0].AddressType)
		return STATUS_INVALID_PARAMETER;


	//
	//	Initialize variables
	//

	addrList = NULL;
	addrListPrioritized = NULL;


	do {

		//
		//	Query binding address list
		//
	
		addrList = LstranAllocateAddr(
						LSTRANS_MAX_BINDADDR,
						&addrListLen
					);
		if(!addrList) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}


		status = LstransAddrTypeToTransType(DestAddr->Address[0].AddressType, &transport);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_PROTO_ERROR, ("Address type not supported. STATUS=%08lx\n", status));
			break;
		}

		status = LstransQueryBindingDevices(transport, addrListLen, addrList, NULL);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_PROTO_ERROR, ("LstransQueryBindingDevices() failed. STATUS=%08lx\n", status));
			break;
		}


		//
		//	Validate binding address
		//

		if(BindAddr) {
			status = LstransIsInAddressList(addrList, BindAddr, TRUE);
			if(status == STATUS_OBJECT_NAME_NOT_FOUND) {
				BindAddr = NULL;
			} else if(!NT_SUCCESS(status)) {
				break;
			}
		}
		if(BindAddr2) {
			status = LstransIsInAddressList(addrList, BindAddr2, TRUE);
			if(status == STATUS_OBJECT_NAME_NOT_FOUND) {
				BindAddr2 = NULL;
			} else if(!NT_SUCCESS(status)) {
				break;
			}
		}

		//
		//	Try bind addresses in order of BindAddr, BindAddr2, and any addresses available.
		//
		addrListPrioritized = LstranAllocateAddr(
								LSTRANS_MAX_BINDADDR,
								NULL);
		if(!addrListPrioritized) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		idx_addr = 0;
		addrListPrioritized->TAAddressCount = 0;

		if(BindAddr) {
			RtlCopyMemory(&addrListPrioritized->Address[idx_addr], BindAddr->Address, sizeof(struct  _AddrLstrans));
			addrListPrioritized->TAAddressCount ++;
			idx_addr ++;
		}

		if(BindAddr2) {
			RtlCopyMemory(&addrListPrioritized->Address[idx_addr], BindAddr2->Address, sizeof(struct  _AddrLstrans));
			addrListPrioritized->TAAddressCount ++;
			idx_addr ++;
		}

		if(BindAnyAddress) {
			ULONG	idx_bindaddr;
			for(idx_bindaddr = 0; idx_bindaddr < (ULONG)addrList->TAAddressCount; idx_bindaddr ++) {


				//
				//	If an address has a valid length, copy into prioritized address list.
				//	Set zero legnth to the same address as BindAddr and BindAddr2 in LstransIsInAddressList().
				//

				if(addrList->Address[idx_bindaddr].AddressLength) {
					RtlCopyMemory(	&addrListPrioritized->Address[idx_addr],
									&addrList->Address[idx_bindaddr],
									sizeof(TA_LSTRANS_ADDRESS));
					addrListPrioritized->TAAddressCount ++;
					idx_addr ++;
				}
			}
		}


		//
		//	Try connecting with prioritized binding address.
		//

		status = LspConnectBindAddrList(LSS, BoundAddr, DestAddr, addrListPrioritized);

	} while(FALSE);

	if(addrListPrioritized)
		ExFreePool(addrListPrioritized);
	if(addrList)
		ExFreePool(addrList);

	return status;
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

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

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

#ifdef __NDASBOOT__
	UNREFERENCED_PARAMETER(LstransType);

	LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_NONE;

	if(STATUS_LPX_MINIPORT == ProtoStatus) {
		KDPrintM(DBG_PROTO_ERROR, ("Try NDASSCSI embedded LPX.\n"));

		ntStatus = LpxConnect(&LSS->LSNodeAddress, &LSS->ConnectionFile.ConnectionSock);
		if( !NT_SUCCESS(ntStatus) ) {
			goto cleanup;
		}
		LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_SOCKET;

		KDPrintM(DBG_PROTO_ERROR, ("Connected with CONNECTION_TYPE_SOCKET\n"));
	} 
	else {
		KDPrintM(DBG_PROTO_ERROR, ("Try LPX driver.\n"));

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

		LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_FILE_OBJECT;
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
		KDPrintM(DBG_PROTO_ERROR, ("Connected with CONNECTION_TYPE_FILE_OBJECT\n"));
	}

#else

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
#endif __NDASBOOT__

	ntStatus = LspLogin(
					LSS,
					&LoginInfo,
					LSProto,
					TRUE
				);
#ifdef __NDASBOOT__	
	if(!NT_SUCCESS(ntStatus)) {
		if(CONNECTION_TYPE_SOCKET == LSS->ConnectionFile.ConnectionType) {	
			LpxDisConnect(LSS->ConnectionFile.ConnectionSock);
			goto cleanup;
		}
		else if(CONNECTION_TYPE_FILE_OBJECT == LSS->ConnectionFile.ConnectionType) {
			LstransDisconnect(&LSS->ConnectionFile, 0);
			LstransDisassociate(&LSS->ConnectionFile);
			LstransCloseConnection(&LSS->ConnectionFile);
			LstransCloseAddress(&LSS->AddressFile);
			goto cleanup;
		} else {
			ntStatus = STATUS_INVALID_PARAMETER;
			goto cleanup;
		}
	}
#else
	if(!NT_SUCCESS(ntStatus)) {
		LstransDisconnect(&LSS->ConnectionFile, 0);
		LstransDisassociate(&LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		LstransCloseAddress(&LSS->AddressFile);
		goto cleanup;
	}
#endif __NDASBOOT__

cleanup:
	return ntStatus;
}


NTSTATUS
LspDisconnect(
		PLANSCSI_SESSION LSS
	) {
	NTSTATUS ntStatus;

	ASSERT(LSS);

#ifdef __NDASBOOT__
	if(CONNECTION_TYPE_SOCKET == LSS->ConnectionFile.ConnectionType) {	
		ntStatus = LpxDisConnect(LSS->ConnectionFile.ConnectionSock);
		LSS->ConnectionFile.ConnectionSock = NULL;
	}
	else if(CONNECTION_TYPE_FILE_OBJECT == LSS->ConnectionFile.ConnectionType) {
		LstransDisassociate(&LSS->ConnectionFile);
		LstransCloseConnection(&LSS->ConnectionFile);
		ntStatus = LstransCloseAddress(&LSS->AddressFile);
	}
	else {
		ntStatus = STATUS_INVALID_PARAMETER;
	}
	LSS->ConnectionFile.ConnectionType = CONNECTION_TYPE_NONE;
#else 

	LstransDisconnect(&LSS->ConnectionFile, 0);
	LstransDisassociate(&LSS->ConnectionFile);
	LstransCloseConnection(&LSS->ConnectionFile);
	ntStatus = LstransCloseAddress(&LSS->AddressFile);
#endif __NDASBOOT__

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

	ntStatus = LspSendRequest(LSS, &pdu, NULL);
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
			PLANSCSI_PDU_POINTERS		Pdu,
			PLSTRANS_OVERLAPPED			OverlappedData
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
					Pdu,
					OverlappedData);
}


NTSTATUS
LspReadReply(
		PLANSCSI_SESSION			LSS,
		PCHAR						Buffer,
		PLANSCSI_PDU_POINTERS		Pdu,
		PLSTRANS_OVERLAPPED			OverlappedData
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
					Pdu,
					OverlappedData
				);
}

NTSTATUS
LspLogin(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo,
		LSPROTO_TYPE		LSProto,
		BOOLEAN				LockCleanup
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
	LSS->WriteSplitSize			= 8; // Start with small value. LSS->MaxBlocks is not yet initialized.
	LSS->ReadSplitSize			= 8;
	
	status = LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogin(
					LSS,
					LoginInfo
				);

	if(NT_SUCCESS(status)) {
		LONG	lockMax;
		LONG	lockIdx;

		LSS->WriteSplitSize	= LSS->MaxBlocksPerRequest; // Start with max possible blocks.
		LSS->ReadSplitSize	= LSS->MaxBlocksPerRequest;
		ASSERT(LSS->WriteSplitSize <=128);
		ASSERT(LSS->WriteSplitSize >0);

		if(LoginInfo->IsEncryptBuffer) {
			ULONG	encBuffSize;

			encBuffSize = LSS->MaxBlocksPerRequest * LSS->BlockInBytes;
			KDPrintM(DBG_PROTO_TRACE, ("Allocating a encryption buffer. BufferSize:%lu\n", encBuffSize));

			LSS->EncryptBuffer = ExAllocatePoolWithTag( NonPagedPool, encBuffSize, LSS_ENCRYPTBUFFER_POOLTAG);
			if(!LSS->EncryptBuffer) {
				LSS->EncryptBufferLength = 0;
				KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the encrypt buffer! Performace may slow down.\n"));
			} else {
				LSS->EncryptBufferLength = encBuffSize;
			}
		}

		LSS->WriteCheckBuffer = ExAllocatePoolWithTag( NonPagedPool, LSS->MaxBlocksPerRequest * LSS->BlockInBytes, LSS_WRITECHECK_BUFFER_POOLTAG);
		if(!LSS->WriteCheckBuffer) {
			KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the write-check buffer!\n"));
		}

		//
		//	Clean up locks
		//

		if(LockCleanup) {
			if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
				lockMax = LANSCSIIDE_MAX_LOCK_VER11;
			} else {
				lockMax = 0;
			}

			KDPrintM(DBG_PROTO_TRACE, ("Clean up locks of %u\n", lockMax));

			for(lockIdx = 0; lockIdx < lockMax; lockIdx ++) {
				LspAcquireLock(LSS, (UCHAR)lockIdx, NULL);
				LspReleaseLock(LSS, (UCHAR)lockIdx, NULL);
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
	if(LSS->WriteCheckBuffer) {
		ExFreePoolWithTag(LSS->WriteCheckBuffer, LSS_WRITECHECK_BUFFER_POOLTAG);
		LSS->WriteCheckBuffer = NULL;
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


NTSTATUS
LspVendorRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse
){
	ASSERT(LSS);
	ASSERT(PduResponse);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequestVendor) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequestVendor(
					LSS,
					PduDesc,
					PduResponse
				);
}

//////////////////////////////////////////////////////////////////////////
//
//	NDAS device lock control
//
NTSTATUS
LspAcquireLock(
	PLANSCSI_SESSION		LSS,
	UCHAR					LockNo,
	PUINT32					LockCount

){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_SET_SEMA;
	pduDesc.Param64 = ((UINT64)(LockNo & 0x3)) << 32;	// Lock number: 32~33 bit.
	status = LspVendorRequest(
							LSS,
							&pduDesc,
							&pduResponse
						);
	if(NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_TRACE,	("Acquired lock #%u\n", LockNo));

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Acquiring lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_WAS_LOCKED;
		}

		if(LockCount)
			*LockCount = NTOHL(pduDesc.Param32[1]);
	}

	return status;
}


NTSTATUS
LspReleaseLock(
	PLANSCSI_SESSION		LSS,
	UCHAR					LockNo,
	PUINT32					LockCount
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_FREE_SEMA;
	pduDesc.Param64 = ((UINT64)(LockNo & 0x3)) << 32;	// Lock number: 32~33 bit.
	status = LspVendorRequest(
							LSS,
							&pduDesc,
							&pduResponse
						);

	if(NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_TRACE,	("Released lock #%u\n", LockNo));

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Releasing lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_WAS_LOCKED;
		}

		if(LockCount)
			*LockCount = NTOHL(pduDesc.Param32[1]);
	}

	return status;
}


NTSTATUS
LspCleanupLock(
   PLANSCSI_SESSION	LSS,
   UCHAR			LockNo
){
	NTSTATUS		status;
	LONG			i;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;
	PLANSCSI_SESSION	tempLSS;
	LONG			maxConn;
	LARGE_INTEGER	GenericTimeOut;
	TA_LSTRANS_ADDRESS	BindingAddress, TargetAddress;
	LSSLOGIN_INFO	loginInfo;
	LSPROTO_TYPE	LSProto;
	ULONG			cleaned;

	//
	//	Parameter check
	//

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	if(LSS->HWVersion >= 1) {
		maxConn = LANSCSIIDE_MAX_CONNECTION_VER11 - 1;
	} else {
		return STATUS_NOT_SUPPORTED;
	}

	tempLSS = ExAllocatePoolWithTag(
							NonPagedPool,
							maxConn * sizeof(LANSCSI_SESSION),
							LSS_POOLTAG);
	RtlZeroMemory(tempLSS, maxConn * sizeof(LANSCSI_SESSION));

	//
	//	Init variables
	//

	status = STATUS_SUCCESS;
	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Param64 = ((UINT64)(LockNo & 0x3)) << 32;	// Lock number: 32~33 bit.
	LspGetAddresses(LSS, &BindingAddress, &TargetAddress);
	GenericTimeOut.QuadPart = NANO100_PER_SEC * 5;
	cleaned = 0;


	//
	//	Try to make connections to fill up connection pool of the NDAS device.
	//	So, we can clear invalid acquisition of the locks.
	//
	KDPrintM(DBG_LURN_ERROR,("Try to clean up lock\n"));
	for (i=0 ; i < maxConn; i++) {


		//
		//	Connect and log in with read-only access.
		//

		//	connect
		LspSetTimeOut(&tempLSS[i], &GenericTimeOut);
		status = LspConnect(
					&tempLSS[i],
					&BindingAddress,
					&TargetAddress
					);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
			break;
		}
	}


	for (i=0; i < maxConn; i++) {

		if(!LspIsConnected(&tempLSS[i])) {
			continue;
		}

		KDPrintM(DBG_LURN_TRACE, ("Con#%u\n", i));
		cleaned++;

		//	extract login information from the existing LSS.
		LspBuildLoginInfo(LSS, &loginInfo);
		status = LspLookupProtocol(loginInfo.HWType, loginInfo.HWVersion, &LSProto);
		if(!NT_SUCCESS(status)) {
			LspDisconnect(&tempLSS[i]);
			KDPrintM(DBG_LURN_ERROR, ("Wrong hardware version.\n"));
			continue;
		}

		//	Log in with read-only access.
		loginInfo.UserID &= 0x0000ffff;
		status = LspLogin(
						&tempLSS[i],
						&loginInfo,
						LSProto,
						FALSE
					);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("LspLogin() failed. STATUS:0x%08x\n", status));
			LspDisconnect(&tempLSS[i]);
			ASSERT(FALSE);
			continue;
		}


		//
		//	Acquire the lock on the NDAS device.
		//

		pduDesc.Command = VENDOR_OP_SET_SEMA;
		status = LspVendorRequest(
						&tempLSS[i],
						&pduDesc,
						&pduResponse
						);
		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Acquiring lock #%u denied by NDAS device\n", LockNo));
		}
		if(!NT_SUCCESS(status)) {
			LspDisconnect(&tempLSS[i]);
			KDPrintM(DBG_LURN_ERROR, ("LspVendorRequest() failed. STATUS=%08lx\n", status));
			continue;
		}

		//
		//	Release the lock on the NDAS device.
		//

		pduDesc.Command = VENDOR_OP_FREE_SEMA;
		status = LspVendorRequest(
						&tempLSS[i],
						&pduDesc,
						&pduResponse
						);
		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Releasing lock #%u denied by NDAS device\n", LockNo));
		}
		if(!NT_SUCCESS(status)) {
			LspDisconnect(&tempLSS[i]);
			KDPrintM(DBG_LURN_ERROR, ("LspVendorRequest() failed. STATUS=%08lx\n", status));
			continue;
		}


		//
		//	Log out and disconnect.
		//

		LspLogout(
			&tempLSS[i]
			);

		LspDisconnect(
			&tempLSS[i]
			);
	}

	KDPrintM(DBG_LURN_INFO, ("Cleaned #%u\n", cleaned));

	ExFreePool(tempLSS);

	return status;
}
