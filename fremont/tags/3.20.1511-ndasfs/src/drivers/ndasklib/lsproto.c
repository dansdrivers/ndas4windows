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
	LoginInfo->HWRevision			= LSS->HWRevision;
	LoginInfo->LanscsiTargetID		= LSS->LanscsiTargetID;
	LoginInfo->LanscsiLU			= LSS->LanscsiLU;
	LoginInfo->LoginType			= LSS->LoginType;
	LoginInfo->MaxDataTransferLength= LSS->MaxDataTransferLength;
	LoginInfo->IsEncryptBuffer		= (LSS->EncryptBuffer !=  NULL);

	RtlCopyMemory(&LoginInfo->UserID, &LSS->UserID,sizeof(UINT32));
	RtlCopyMemory(&LoginInfo->Password, &LSS->Password, sizeof(UINT64));

	return STATUS_SUCCESS;
}

//
// Copy LSS structure.
// NOTE: This structure does not allocate extra encryption buffer and
//    write-check buffer. It just copy the pointers.
//

VOID
LspCopy(
		PLANSCSI_SESSION	ToLSS,
		PLANSCSI_SESSION	FromLSS,
		BOOLEAN				CopyBufferPointers
	) {
	ULONG	encBufferLen;
	PVOID	encBuffer;

	//
	// Save buffer information of the target LSS
	//
	if(CopyBufferPointers == FALSE) {
		encBufferLen = ToLSS->EncryptBufferLength;
		encBuffer = ToLSS->EncryptBuffer;
	}

	RtlCopyMemory(ToLSS, FromLSS, sizeof(LANSCSI_SESSION));

	//
	// Restore buffer information
	//

	if(CopyBufferPointers == FALSE) {
		ToLSS->EncryptBufferLength = encBufferLen;
		ToLSS->EncryptBuffer = encBuffer;
	}
}


VOID
LspSetDefaultTimeOut(
	IN PLANSCSI_SESSION	LSS,
	IN PLARGE_INTEGER	TimeOut
	) {

	LSS->DefaultTimeOut.QuadPart = TimeOut->QuadPart;
	KDPrintM(DBG_PROTO_INFO, ("DefaultTimeOut:%I64d * 100 ns\n", LSS->DefaultTimeOut.QuadPart));
}


#define IF_NULL_TIMEOUT_THEN_DEFAULT(LSS_POINTER, TIMEOUT_POINTER)	((TIMEOUT_POINTER)?(TIMEOUT_POINTER):(&(LSS_POINTER)->DefaultTimeOut))

VOID
LspGetAddresses(
		PLANSCSI_SESSION	LSS,
		PTA_LSTRANS_ADDRESS	BindingAddress,
		PTA_LSTRANS_ADDRESS	TargetAddress
	) {

	ASSERT(LSS->BindAddress.TAAddressCount == 0 ||
			LSS->BindAddress.TAAddressCount == 1	);

	RtlCopyMemory(BindingAddress, &LSS->BindAddress, sizeof(TA_LSTRANS_ADDRESS));
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

//
// Connect the source address to the destination address
// using LanScsi Session.
//

NTSTATUS
LspConnect(
		IN OUT PLANSCSI_SESSION	LSS,
		IN PTA_LSTRANS_ADDRESS	SrcAddr,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN PLSTRANS_OVERLAPPED	Overlapped,
		IN PLARGE_INTEGER		TimeOut
	) {
	NTSTATUS		ntStatus;


	ASSERT(LSS);
	ASSERT(SrcAddr);
	ASSERT(DestAddr);

	if(SrcAddr->Address[0].AddressType != DestAddr->Address[0].AddressType)
		return STATUS_INVALID_PARAMETER;

	//
	//	Save dest and src addresses
	//
	RtlCopyMemory(&LSS->LSNodeAddress, DestAddr, sizeof(TA_LSTRANS_ADDRESS));
	RtlCopyMemory(&LSS->BindAddress, SrcAddr, sizeof(TA_LSTRANS_ADDRESS));

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

	ntStatus = LstransConnect(
						&LSS->ConnectionFile,
						DestAddr,
						IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut),
						Overlapped);
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

//
// Completion routine for asynchronous connect in LspConnectBindAddrList()
//

static
VOID
LstransConnectComp(
		IN PLSTRANS_OVERLAPPED	OverlappedData
){
	if(OverlappedData->CompletionEvent) {
		KeSetEvent(OverlappedData->CompletionEvent, IO_NO_INCREMENT, FALSE);
	}
}


//
// Connect to a destination address with binding address list.
//

static
NTSTATUS
LspConnectBindAddrList(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddrList,
		IN PLARGE_INTEGER		TimeOut
){
	NTSTATUS	status;
	ULONG		idx_addr, idx_cleanup_addr;
	TA_LSTRANS_ADDRESS	bindAddr;
	PLSTRANS_OVERLAPPED	overlapped;
	PKEVENT				overlappedEvents;
	PKWAIT_BLOCK		overlappedWaitBlocks;
	PLANSCSI_SESSION	overlappedLSS;
	PKEVENT				overlappedWait[MAXIMUM_WAIT_OBJECTS];

	//
	//	Parameter check
	//
	if(DestAddr->TAAddressCount != 1) {
		return STATUS_INVALID_PARAMETER;
	}
	if(BindAddrList->TAAddressCount < 1) {
		return STATUS_INVALID_PARAMETER;
	}
	// Windows does not allow to wait for more than MAXIMUM_WAIT_OBJECTS
	// even using wait block buffer.
	if(BindAddrList->TAAddressCount > MAXIMUM_WAIT_OBJECTS) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	if(DestAddr->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {
		return STATUS_NOT_IMPLEMENTED;
	}


	bindAddr.TAAddressCount = 1;
	status = STATUS_UNSUCCESSFUL;

	//
	// Allocate overlapped context.
	//
	overlapped = ExAllocatePoolWithTag(
					NonPagedPool,
					sizeof(LSTRANS_OVERLAPPED) * BindAddrList->TAAddressCount,
					LSS_OVERLAPPED_BUFFER_POOLTAG
					);
	if(overlapped == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	//
	// Allocate completion events
	//
	overlappedEvents = ExAllocatePoolWithTag(
					NonPagedPool,
					sizeof(KEVENT) * BindAddrList->TAAddressCount,
					LSS_OVERLAPPED_EVENT_POOLTAG
					);;
	if(overlappedEvents == NULL) {
		ExFreePoolWithTag(overlapped, LSS_OVERLAPPED_BUFFER_POOLTAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//
	// Allocate wait blocks.
	// Need this buffer to wait for more than THREAD_WAIT_OBJECTS.
	//
	overlappedWaitBlocks = ExAllocatePoolWithTag(
					NonPagedPool,
					sizeof(KWAIT_BLOCK) * BindAddrList->TAAddressCount,
					LSS_OVERLAPPED_WAITBLOCK_POOLTAG);
	if(overlappedWaitBlocks == NULL) {
		ExFreePoolWithTag(overlappedEvents, LSS_OVERLAPPED_EVENT_POOLTAG);
		ExFreePoolWithTag(overlapped, LSS_OVERLAPPED_BUFFER_POOLTAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//
	// Allocate LSSes
	//LSS_OVERLAPPED_LSS_POOLTAG
	overlappedLSS = ExAllocatePoolWithTag(
					NonPagedPool,
					sizeof(LANSCSI_SESSION) * BindAddrList->TAAddressCount,
					LSS_OVERLAPPED_LSS_POOLTAG);
	if(overlappedLSS == NULL){
		ExFreePoolWithTag(overlappedWaitBlocks, LSS_OVERLAPPED_WAITBLOCK_POOLTAG);
		ExFreePoolWithTag(overlappedEvents, LSS_OVERLAPPED_EVENT_POOLTAG);
		ExFreePoolWithTag(overlapped, LSS_OVERLAPPED_BUFFER_POOLTAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Connect asynchronously for each binding address
	//

	for(idx_addr = 0; idx_addr < (ULONG)BindAddrList->TAAddressCount; idx_addr++) {

		RtlCopyMemory(	bindAddr.Address,
						&BindAddrList->Address[idx_addr],
						sizeof(struct  _AddrLstrans));

		KDPrintM(DBG_PROTO_ERROR, ("BindingAddr%d=%02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x\n",
			idx_addr,
			bindAddr.Address[0].Address.Address[0],
			bindAddr.Address[0].Address.Address[1],
			bindAddr.Address[0].Address.Address[2],
			bindAddr.Address[0].Address.Address[3],
			bindAddr.Address[0].Address.Address[4],
			bindAddr.Address[0].Address.Address[5],
			bindAddr.Address[0].Address.Address[6],
			bindAddr.Address[0].Address.Address[7]
			));

		// We do not need encryption buffer for connect operation.
		overlappedLSS[idx_addr].EncryptBuffer = NULL;
		overlappedLSS[idx_addr].EncryptBufferLength = 0;

		LspCopy(&overlappedLSS[idx_addr], LSS, FALSE);

		//
		// Initialize overlapped buffer
		//

		KeInitializeEvent(&overlappedEvents[idx_addr], NotificationEvent, FALSE);
		overlappedWait[idx_addr] = &overlappedEvents[idx_addr];
		overlapped[idx_addr].IoCompleteRoutine = LstransConnectComp;
		overlapped[idx_addr].TransStat = NULL;
		overlapped[idx_addr].CompletionEvent = &overlappedEvents[idx_addr];
		overlapped[idx_addr].UserContext = NULL;

		//
		// Connect asynchronously
		//

		status = LspConnect(&overlappedLSS[idx_addr], &bindAddr, DestAddr, &overlapped[idx_addr], IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
		if(!NT_SUCCESS(status)) {
			// Set the completion event to continue with KeWaitForMultipleObjects()
			// even though this address fails.
			KeSetEvent(overlapped[idx_addr].CompletionEvent, IO_NO_INCREMENT, FALSE);
			KDPrintM(DBG_PROTO_ERROR,("LspConnect() failed. idx=%d STATUS=%08lx\n", idx_addr, status));
		}
	}

	do {
		//
		// Wait for all connect completion events
		//

		status = KeWaitForMultipleObjects(
						BindAddrList->TAAddressCount,
						overlappedWait,
						WaitAll,
						Executive,
						KernelMode,
						FALSE,
						NULL,
						overlappedWaitBlocks);
		if(status != STATUS_SUCCESS) {
			ASSERT(FALSE);
			break;
		}

		//
		// Find the first connected address
		//

		for(idx_addr = 0; idx_addr < (ULONG)BindAddrList->TAAddressCount; idx_addr++) {
			status = overlapped[idx_addr].IoStatusBlock.Status;
			if(status == STATUS_SUCCESS) {
				break;
			}
		}

		//
		// Clean up unsuccessful or non-selected connections.
		//
		for(idx_cleanup_addr = 0; idx_cleanup_addr < (ULONG)BindAddrList->TAAddressCount; idx_cleanup_addr++) {
			if(idx_cleanup_addr != idx_addr)
				LspDisconnect(&overlappedLSS[idx_cleanup_addr]);
		}

		//
		// set return value
		//
		if(status == STATUS_SUCCESS) {
			if(BoundAddr) {
				LspCopy(LSS, &overlappedLSS[idx_addr], FALSE);

				BoundAddr->TAAddressCount = 1;
				RtlCopyMemory(	BoundAddr->Address,
					&BindAddrList->Address[idx_addr],
					sizeof(struct  _AddrLstrans));
			}
		} else {
			KDPrintM(DBG_PROTO_ERROR, ("Could not connect to the dest\n"));
		}

	} while(FALSE);


	ExFreePoolWithTag(overlappedLSS, LSS_OVERLAPPED_LSS_POOLTAG);
	ExFreePoolWithTag(overlappedWaitBlocks, LSS_OVERLAPPED_WAITBLOCK_POOLTAG);
	ExFreePoolWithTag(overlappedEvents, LSS_OVERLAPPED_EVENT_POOLTAG);
	ExFreePoolWithTag(overlapped, LSS_OVERLAPPED_BUFFER_POOLTAG);


	return status;
}

//
// Connect to a destination address with two binding address.
// Binding address 1 has priority over binding address address.
// If AnyAddress is set, try all other addresses on the system
// which is not specified in arguments.
//
// BInding address order:
//			- Binding address 1
//			- Binding address 2
//			- The other binding addresses
//

NTSTATUS
LspConnectMultiBindAddr(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr1,
		IN PTA_LSTRANS_ADDRESS	BindAddr2,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN BOOLEAN				BindAnyAddress,
		IN PLARGE_INTEGER		TimeOut
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
	if(BindAddr1 && DestAddr->Address[0].AddressType != BindAddr1->Address[0].AddressType)
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

		if(BindAddr1) {
			status = LstransIsInAddressList(addrList, BindAddr1, TRUE);
			if(status == STATUS_OBJECT_NAME_NOT_FOUND) {
				BindAddr1 = NULL;
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

		if(BindAddr1) {
			RtlCopyMemory(&addrListPrioritized->Address[idx_addr], BindAddr1->Address, sizeof(struct  _AddrLstrans));
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

		status = LspConnectBindAddrList(LSS, BoundAddr, DestAddr, addrListPrioritized, IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));

	} while(FALSE);

	if(addrListPrioritized)
		ExFreePool(addrListPrioritized);
	if(addrList)
		ExFreePool(addrList);

	return status;
}



NTSTATUS
LspReconnect(
		IN OUT PLANSCSI_SESSION LSS,
		IN PLARGE_INTEGER		TimeOut
	) {
	NTSTATUS		ntStatus;

	ASSERT(LSS);

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	// Connect through any available addresses, 
	// but connect through the current address if possible.
	//

	ntStatus = LspConnectMultiBindAddr(
							LSS,
							&LSS->BindAddress, // Returned bound address
							&LSS->BindAddress, // Preferred binding address
							NULL,				// Next preferred binding address
							&LSS->LSNodeAddress, // Destination address
							TRUE,
							IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
	if( !NT_SUCCESS(ntStatus) ) {
		goto cleanup;
	}

cleanup:
	return ntStatus;
}

//
// Login with current LSS information
// Assume LspReconnect is called before this call.
//
NTSTATUS
LspRelogin(
		IN OUT PLANSCSI_SESSION LSS,
		IN BOOLEAN		IsEncryptBuffer,
		IN PLARGE_INTEGER		TimeOut
	) {
	NTSTATUS		ntStatus;
	LSSLOGIN_INFO	LoginInfo;
	LSPROTO_TYPE	LSProto;
	ASSERT(LSS);

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	LoginInfo.LoginType	= LOGIN_TYPE_NORMAL;
	RtlCopyMemory(&LoginInfo.UserID, &LSS->UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LoginInfo.Password, &LSS->Password, LSPROTO_PASSWORD_LENGTH);
	LoginInfo.MaxDataTransferLength = LSS->MaxDataTransferLength;
	LoginInfo.LanscsiTargetID = LSS->LanscsiTargetID;
	LoginInfo.LanscsiLU = LSS->LanscsiLU;
	LoginInfo.HWType = LSS->HWType;
	LoginInfo.HWVersion = LSS->HWVersion;
	LoginInfo.HWRevision = LSS->HWRevision;	
	LoginInfo.IsEncryptBuffer = IsEncryptBuffer;

	ntStatus = LspLookupProtocol(LSS->HWType, LSS->HWVersion, &LSProto);
	if( !NT_SUCCESS(ntStatus) ) {
		KDPrintM(DBG_PROTO_ERROR, ("HWVersion wrong.\n"));
		return STATUS_NOT_IMPLEMENTED;
	}

	ntStatus = LspLogin(
					LSS,
					&LoginInfo,
					LSProto,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut),
					TRUE
				);
	if(!NT_SUCCESS(ntStatus)) {
		LspDisconnect(LSS);
		goto cleanup;
	}
cleanup:
	return ntStatus;
}

NTSTATUS
LspDisconnect(
		IN PLANSCSI_SESSION LSS
	) {
	NTSTATUS ntStatus;

	ASSERT(LSS);

	LstransDisconnect(&LSS->ConnectionFile, 0);
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
		OUT PBYTE			PduResponse,
		IN PLARGE_INTEGER	TimeOut
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
	LSS->CommandTag++;
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);
	pRequestHeader->TargetID = TargetId;

	//
	// Send Request.
	//
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	ntStatus = LspSendRequest(LSS, &pdu, NULL, IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
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
			PLANSCSI_SESSION		LSS,
			PLANSCSI_PDU_POINTERS	Pdu,
			PLSTRANS_OVERLAPPED		OverlappedData,
			PLARGE_INTEGER			TimeOut
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
					OverlappedData,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
}


NTSTATUS
LspReadReply(
		PLANSCSI_SESSION		LSS,
		PCHAR					Buffer,
		PLANSCSI_PDU_POINTERS	Pdu,
		PLSTRANS_OVERLAPPED		OverlappedData,
		PLARGE_INTEGER			TimeOut
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
					OverlappedData,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
				);
}

NTSTATUS
LspLogin(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo,
		LSPROTO_TYPE		LSProto,
		PLARGE_INTEGER		TimeOut,
		BOOLEAN				LockCleanup
	) {
	NTSTATUS	status;

	ASSERT(LSS);
	ASSERT(LoginInfo);

	if(LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(LSProto >= LsprotoCnt) {
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
	LSS->MaxDataTransferLength	= LoginInfo->MaxDataTransferLength;
	LSS->LanscsiTargetID		= LoginInfo->LanscsiTargetID;
	LSS->LanscsiLU				= LoginInfo->LanscsiLU;
	LSS->HWType				= LoginInfo->HWType;
	LSS->HWVersion				= LoginInfo->HWVersion;
	LSS->HWRevision				= LoginInfo->HWRevision;	
	LSS->HWProtoType			= HARDWARETYPE_TO_PROTOCOLTYPE(LoginInfo->HWType);
	LSS->HWProtoVersion			= (BYTE)LSProto;
	LSS->HPID					= 0;
	LSS->RPID					= 0;
	LSS->CommandTag			= 0;
	
	status = LSS->LanscsiProtocol->LsprotoFunc.LsprotoLogin(
					LSS,
					LoginInfo,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
				);
	if(NT_SUCCESS(status)) {
		LONG	lockMax;
		LONG	lockIdx;
		ULONG	buffSize;

		buffSize = LSS->MaxDataTransferLength;
		if(LoginInfo->IsEncryptBuffer) {
			KDPrintM(DBG_PROTO_TRACE, ("Allocating a encryption buffer. BufferSize:%lu\n", buffSize));
			LSS->EncryptBuffer = ExAllocatePoolWithTag( NonPagedPool, buffSize, LSS_ENCRYPTBUFFER_POOLTAG);
			if(!LSS->EncryptBuffer) {
				LSS->EncryptBufferLength = 0;
				KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the encrypt buffer! Performace may slow down.\n"));
			} else {
				LSS->EncryptBufferLength = buffSize;
			}
		}


#if 0	// for 2.5
		LSS->DigestPatchBuffer = ExAllocatePoolWithTag( NonPagedPool,  LSS->MaxBlocksPerRequest * LSS->BlockInBytes + 16, LSS_DIGEST_PATCH_POOLTAG);
		if(!LSS->DigestPatchBuffer) {
			KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the digest patch buffer! Performace may slow down.\n"));
		}
#endif
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
				LspAcquireLock(LSS, (UCHAR)lockIdx, NULL, IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
				LspReleaseLock(LSS, (UCHAR)lockIdx, NULL, IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));
			}
		}


	} else {
		LSS->LanscsiProtocol = NULL;
	}

	return status;
}


NTSTATUS
LspLogout(
		PLANSCSI_SESSION LSS,
		PLARGE_INTEGER	TimeOut
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
					LSS,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
				);
	if(/* status == STATUS_SUCCESS && */
		LSS->EncryptBuffer &&
		LSS->EncryptBufferLength
		) {

		ExFreePoolWithTag(LSS->EncryptBuffer, LSS_ENCRYPTBUFFER_POOLTAG);
		LSS->EncryptBuffer = NULL;
		LSS->EncryptBufferLength = 0;
	}

#if 0 // for 2.5
	if (LSS->DigestPatchBuffer) {
		ExFreePoolWithTag(LSS->DigestPatchBuffer, LSS_DIGEST_PATCH_POOLTAG);
		LSS->DigestPatchBuffer = 0;
	}
#endif

	return status;
}


NTSTATUS
LspTextTargetList(
		PLANSCSI_SESSION	LSS,
		PTARGETINFO_LIST	TargetList,
		PLARGE_INTEGER		TimeOut
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
					TargetList,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
				);
}

NTSTATUS
LspTextTartgetData(
		PLANSCSI_SESSION LSS,
		BOOLEAN			GetorSet,
		UINT32			TargetID,
		PTARGET_DATA	TargetData,
		PLARGE_INTEGER	TimeOut
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
					TargetData,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
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

	if(PduDesc->TimeOut == NULL)
		PduDesc->TimeOut = &LSS->DefaultTimeOut;

	return LSS->LanscsiProtocol->LsprotoFunc.LsprotoRequest(
					LSS,
					PduDesc,
					PduResponse
				); // Call to LspIdeRequest_V11, LspIdeRequest_V10, LspPacketRequest
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

	if(PduDesc->TimeOut == NULL)
		PduDesc->TimeOut = &LSS->DefaultTimeOut;

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

	if(PduDesc->TimeOut == NULL)
		PduDesc->TimeOut = &LSS->DefaultTimeOut;

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
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	OUT PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_SET_MUTEX;
	pduDesc.Param8[3] = (UCHAR)LockNo;
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
	status = LspVendorRequest(
							LSS,
							&pduDesc,
							&pduResponse
						);
	if(NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_TRACE,	("Acquired lock #%u\n", LockNo));

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_TRACE,	("Acquiring lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_WAS_LOCKED;
		}

		//
		// Convert Network endian to the host endian here.
		//

		if(LockData) {
			if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
				*(PUINT32)LockData = NTOHL(pduDesc.Param32[1]);
			}
		}
	}

	return status;
}


NTSTATUS
LspReleaseLock(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	IN PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_FREE_MUTEX;
	pduDesc.Param8[3] = (UCHAR)LockNo;
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
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

		//
		// Convert Network endian to the host endian here.
		//

		if(LockData) {
			if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
				*(PUINT32)LockData = NTOHL(pduDesc.Param32[1]);
			}
		}
	}

	return status;
}


NTSTATUS
LspGetLockOwner(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	IN PUCHAR			LockOwner,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_OWNER_SEMA;
	pduDesc.Param64 = ((UINT64)(LockNo & 0x3)) << 32;	// Lock number: 32~33 bit.
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
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

		RtlCopyMemory(LockOwner, &pduDesc.Param64, 8);
	}

	return status;
}

NTSTATUS
LspGetLockData(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	IN PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
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
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
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

		//
		// Convert Network endian to the host endian here.
		//

		if(LockData) {
			if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
				*(PUINT32)LockData = NTOHL(pduDesc.Param32[1]);
			}
		}
	}

	return status;
}


NTSTATUS
LspCleanupLock(
   PLANSCSI_SESSION	LSS,
   UCHAR			LockNo,
   PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LONG			i;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;
	PLANSCSI_SESSION	tempLSS;
	LONG			maxConn;
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
	pduDesc.Param8[3] = (UCHAR)LockNo;
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
	LspGetAddresses(LSS, &BindingAddress, &TargetAddress);
	cleaned = 0;


	//
	//	Try to make connections to fill up connection pool of the NDAS device.
	//	So, we can clear invalid acquisition of the locks.
	//
	KDPrintM(DBG_LURN_ERROR,("Try to clean up lock\n"));
	for (i=0 ; i < maxConn; i++) {

		tempLSS[i].DefaultTimeOut.QuadPart = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)->QuadPart;

		//
		//	Connect and log in with read-only access.
		//

		//	connect
		status = LspConnect(
					&tempLSS[i],
					&BindingAddress,
					&TargetAddress,
					NULL,
					NULL
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
						NULL,
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

		pduDesc.Command = VENDOR_OP_SET_MUTEX;
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

		pduDesc.Command = VENDOR_OP_FREE_MUTEX;
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
			&tempLSS[i],
			NULL
			);

		LspDisconnect(
			&tempLSS[i]
			);

		//
		// Init PDU
		//
		pduDesc.Retransmits = 0;
		pduDesc.PacketLoss = 0;
	}

	KDPrintM(DBG_LURN_INFO, ("Cleaned #%u\n", cleaned));

	ExFreePool(tempLSS);

	return status;
}
