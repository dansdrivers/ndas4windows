#include "ndasscsiproc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "ndasproto"


UINT32						LsprotoCnt  = NR_LSPROTO_PROTOCOL;
PLANSCSIPROTOCOL_INTERFACE	LsprotoList[NR_LSPROTO_MAX_PROTOCOL] = { &LsProtoIdeV10Interface,
																	 &LsProtoIdeV11Interface };

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
LspCopy (
	PLANSCSI_SESSION	ToLSS,
	PLANSCSI_SESSION	FromLSS,
	BOOLEAN				CopyBufferPointers
	) 
{
	ULONG	encBufferLen;
	PVOID	encBuffer;

	// Save buffer information of the target LSS

	if (CopyBufferPointers == FALSE) {

		encBufferLen = ToLSS->EncryptBufferLength;
		encBuffer = ToLSS->EncryptBuffer;
	}

	RtlCopyMemory(ToLSS, FromLSS, sizeof(LANSCSI_SESSION));

	// Restore buffer information

	if (CopyBufferPointers == FALSE) {

		ToLSS->EncryptBufferLength = encBufferLen;
		ToLSS->EncryptBuffer = encBuffer;
	}

	if (FromLSS->NdastConnectionFile.Protocol) {
		
		FromLSS->NdastConnectionFile.Protocol->
			NdasTransFunc.MoveOverlappedContext( &ToLSS->NdastConnectionFile.OverlappedContext,
												 &FromLSS->NdastConnectionFile.OverlappedContext );
	}
}


VOID
LspSetDefaultTimeOut(
	IN PLANSCSI_SESSION	LSS,
	IN PLARGE_INTEGER	TimeOut
	) {

	NDAS_BUGON( TimeOut->QuadPart < 0 && (-TimeOut->QuadPart) <= 1000 *NANO100_PER_SEC );

	LSS->DefaultTimeOut.QuadPart = TimeOut->QuadPart;
	KDPrintM(DBG_PROTO_INFO, ("DefaultTimeOut:%I64d * 100 ns\n", LSS->DefaultTimeOut.QuadPart));
}


#define IF_NULL_TIMEOUT_THEN_DEFAULT(LSS_POINTER, TIMEOUT_POINTER)	((TIMEOUT_POINTER)?(TIMEOUT_POINTER):(&(LSS_POINTER)->DefaultTimeOut))

VOID
LspGetAddresses(
	PLANSCSI_SESSION	LSS,
	PTA_ADDRESS			BindingAddress,
	PTA_ADDRESS			TargetAddress
	) 
{
	if (BindingAddress) {

		RtlCopyMemory( BindingAddress, 
					   &LSS->NdasBindAddress, 
					   (FIELD_OFFSET(TA_ADDRESS, Address)) + 
					   LSS->NdasBindAddress.AddressLength);
	}

	if (TargetAddress) {

		RtlCopyMemory( TargetAddress, 
					   &LSS->NdasNodeAddress, 
					   (FIELD_OFFSET(TA_ADDRESS, Address)) + 
					   LSS->NdasNodeAddress.AddressLength);
	}
}


BOOLEAN
LspIsConnected(
		PLANSCSI_SESSION	LSS
){
	ASSERT(LSS);

	return	LSS->NdastAddressFile.AddressFileHandle &&
			LSS->NdastAddressFile.AddressFileObject &&
			LSS->NdastConnectionFile.ConnectionFileHandle &&
			LSS->NdastConnectionFile.ConnectionFileObject;
}

//
// Connect the source address to the destination address
// using LanScsi Session.
//

NTSTATUS
LspConnect (
	IN PLANSCSI_SESSION		LSS,
	IN PTA_ADDRESS			SrcAddr,
	IN PTA_ADDRESS			DestAddr,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN PLSTRANS_OVERLAPPED	Overlapped,
	IN PLARGE_INTEGER		TimeOut
	) 
{
	NTSTATUS	status;

	NDIS_OID	ndisOid;
	ULONG		mediumSpeed;
	PUCHAR		buffer;
	LONG		repeat = 0;


	ASSERT( LSS );
	ASSERT( SrcAddr );
	ASSERT( DestAddr );

	UNREFERENCED_PARAMETER( Overlapped );
	NDAS_BUGON( Overlapped == NULL );

	if (SrcAddr->AddressType != DestAddr->AddressType) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	NDAS_BUGON( ((PTDI_ADDRESS_LPX)SrcAddr->Address)->Port == 0 );

	do {

		RtlCopyMemory( &LSS->NdasNodeAddress, 
					   DestAddr, 
					   (FIELD_OFFSET(TA_ADDRESS, Address) + 
					   TDI_ADDRESS_LENGTH_LPX) );

		RtlCopyMemory( &LSS->NdasBindAddress, 
					   SrcAddr, 
					   (FIELD_OFFSET(TA_ADDRESS, Address) + 
					   TDI_ADDRESS_LENGTH_LPX) );

		status = NdasTransOpenAddress( SrcAddr, &LSS->NdastAddressFile );

		if (status != STATUS_SUCCESS) {
		
			return status;
		}

		status = NdasTransOpenConnection( NULL, DestAddr->AddressType, &LSS->NdastConnectionFile );
	
		if ( status != STATUS_SUCCESS) {

			NdasTransCloseAddress( &LSS->NdastAddressFile );
			return status;
		}

		status = NdasTransAssociate( &LSS->NdastAddressFile, &LSS->NdastConnectionFile );

		if (status != STATUS_SUCCESS) {

			NdasTransCloseConnection( &LSS->NdastConnectionFile );
			NdasTransCloseAddress( &LSS->NdastAddressFile );
			return status;
		}

		status = NdasTransConnect( &LSS->NdastConnectionFile,
								   DestAddr,
								   IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut),
								   NULL );

		if (status != STATUS_SUCCESS) {

			NdasTransDisassociateAddress( &LSS->NdastConnectionFile );
			NdasTransCloseConnection( &LSS->NdastConnectionFile );
			NdasTransCloseAddress( &LSS->NdastAddressFile );
		
			if (repeat++ < 3) {

				//continue;
			}

			return status;
		}

		break;

	} while (1);

	NdasFcInitialize( &LSS->SendNdasFcStatistics );
	NdasFcInitialize( &LSS->RecvNdasFcStatistics );

	LSS->DataEncryptAlgo = 0;
	LSS->EncryptBuffer = NULL;

	status = NdasTransRegisterDisconnectHandler( &LSS->NdastAddressFile,
											     InDisconnectHandler,
												 InDisconnectEventContext );

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		NdasTransDisassociateAddress( &LSS->NdastConnectionFile );
		NdasTransCloseConnection( &LSS->NdastConnectionFile );
		NdasTransCloseAddress( &LSS->NdastAddressFile );

		return status;
	}

	ndisOid = OID_GEN_LINK_SPEED;
	mediumSpeed = 0;

#if 0
	status = NdasTransIoControl ( &LSS->NdastConnectionFile,
								  IOCTL_NDIS_QUERY_GLOBAL_STATS,
								  &ndisOid,
								  sizeof(ndisOid),
								  &mediumSpeed,
								  sizeof(mediumSpeed) );

#endif

	buffer = sizeof(ndisOid) > sizeof(mediumSpeed) ? (PUCHAR)&ndisOid : (PUCHAR)&mediumSpeed;
	*((PNDIS_OID)buffer) = OID_GEN_LINK_SPEED;

	status = NdasTransQueryInformation( &LSS->NdastConnectionFile,
										LPX_NDIS_QUERY_GLOBAL_STATS,
										buffer,
										sizeof(ndisOid) > sizeof(mediumSpeed) ? sizeof(ndisOid) : sizeof(mediumSpeed) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("ndiskOid = %x, status = %x, mediumSpeed = %d, ndisOid = %p, mediumSpeed = %p\n", 
				 ndisOid, status, *((PULONG)buffer), &ndisOid, buffer) );

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		NdasTransDisassociateAddress( &LSS->NdastConnectionFile );
		NdasTransCloseConnection( &LSS->NdastConnectionFile );
		NdasTransCloseAddress( &LSS->NdastAddressFile );

		return status;
	}

	LSS->MediumSpeed = *((PULONG)buffer);

	return status;
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


// Connect to a destination address with binding address list.

NTSTATUS
LspConnectBindAddrList (
	IN  PLANSCSI_SESSION	LSS,
	OUT PTA_ADDRESS			BoundAddr,
	IN  PTA_ADDRESS			DestAddr,
	IN  PTRANSPORT_ADDRESS	BindAddrList,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	)
{
	NTSTATUS			status;
	ULONG				idx_addr;
	PTA_ADDRESS			bindAddr;


	if (BindAddrList->TAAddressCount < 1) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (DestAddr->AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDAS_BUGON( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = STATUS_UNSUCCESSFUL;

	// Allocate overlapped context.

	bindAddr = NULL;

	for (idx_addr = 0; idx_addr < (ULONG)BindAddrList->TAAddressCount; idx_addr++) {
			
		if (bindAddr == NULL) {

			bindAddr = BindAddrList->Address;
		
		} else {

			bindAddr = (PTA_ADDRESS)(((PCHAR)bindAddr) + 
									 (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									 bindAddr->AddressLength);
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				    ("bindAddr %02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x\n",
					 bindAddr->Address[0], bindAddr->Address[1],
					 bindAddr->Address[2], bindAddr->Address[3],
					 bindAddr->Address[4], bindAddr->Address[5],
					 bindAddr->Address[6], bindAddr->Address[7]) );


		// Connect synchronously

		status = LspConnect( LSS, 
							 bindAddr, 
							 DestAddr, 
							 InDisconnectHandler,
							 InDisconnectEventContext,
							 NULL, 
							 IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut) );

		if (NT_SUCCESS(status)) {

			NDAS_BUGON( bindAddr->AddressLength == TDI_ADDRESS_LENGTH_LPX );

			RtlCopyMemory( BoundAddr,
						   bindAddr,
						   (FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX) );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					    ("connected to bindAddr %d %02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x\n",
						 idx_addr,
						 bindAddr->Address[0], bindAddr->Address[1],
						 bindAddr->Address[2], bindAddr->Address[3],
						 bindAddr->Address[4], bindAddr->Address[5],
						 bindAddr->Address[6], bindAddr->Address[7]) );

			break;
		}
	}

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
LspConnectMultiBindAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_LSTRANS_ADDRESS	BoundAddr,
	IN	PTA_LSTRANS_ADDRESS	BindAddr1,
	IN  PTA_LSTRANS_ADDRESS	BindAddr2,
	IN  PTA_LSTRANS_ADDRESS	DestAddr,
	IN  BOOLEAN				BindAnyAddress,
	IN  PVOID				InDisconnectHandler,
	IN  PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	) 
{
	NTSTATUS			status;

	PCHAR				boundAddressBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	PTA_ADDRESS			boundTaAddress = (PTA_ADDRESS)boundAddressBuffer;
	PCHAR				bind1AddressBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	PTA_ADDRESS			bind1TaAddress = (PTA_ADDRESS)bind1AddressBuffer;
	PCHAR				bind2AddressBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	PTA_ADDRESS			bind2TaAddress = (PTA_ADDRESS)bind2AddressBuffer;
	PCHAR				destAddressBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	PTA_ADDRESS			destTaAddress = (PTA_ADDRESS)destAddressBuffer;


	NDAS_BUGON( LSS );
	NDAS_BUGON( DestAddr );
	NDAS_BUGON( BindAnyAddress == TRUE );

	//	Parameter check

	if (DestAddr->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDAS_BUGON( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	if (BindAddr1 && BindAddr1->Address[0].AddressType != DestAddr->Address[0].AddressType) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (BindAddr2 && BindAddr2->Address[0].AddressType != DestAddr->Address[0].AddressType) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (BindAddr1) {

		bind1TaAddress->AddressLength = BindAddr1->Address[0].AddressLength;
		bind1TaAddress->AddressType   = BindAddr1->Address[0].AddressType;
		
		RtlCopyMemory( bind1TaAddress->Address, 
					   &BindAddr1->Address[0].Address, 
					   BindAddr1->Address[0].AddressLength );
	
	} else {

		bind1TaAddress = NULL;
	}

	if (BindAddr2) {

		bind2TaAddress->AddressLength = BindAddr2->Address[0].AddressLength;
		bind2TaAddress->AddressType   = BindAddr2->Address[0].AddressType;
		
		RtlCopyMemory( bind2TaAddress->Address, 
					   &BindAddr2->Address[0].Address, 
					   BindAddr2->Address[0].AddressLength );
	
	} else {

		bind2TaAddress = NULL;
	}

	destTaAddress->AddressLength = DestAddr->Address[0].AddressLength;
	destTaAddress->AddressType   = DestAddr->Address[0].AddressType;

	RtlCopyMemory( &destTaAddress->Address[0], 
				   &DestAddr->Address[0].Address, 
				   DestAddr->Address[0].AddressLength );

	
	status = LspConnectMultiBindTaAddr( LSS,
										boundTaAddress,
										bind1TaAddress,
										bind2TaAddress,
										destTaAddress,
										BindAnyAddress,
										InDisconnectHandler,
										InDisconnectEventContext,
										TimeOut ); 


	if (status == STATUS_SUCCESS) {

		BoundAddr->TAAddressCount = 1;
			
		BoundAddr->Address[0].AddressLength = boundTaAddress->AddressLength;
		BoundAddr->Address[0].AddressType = boundTaAddress->AddressType;
			
		RtlCopyMemory( &BoundAddr->Address[0].Address,
					   &boundTaAddress->Address[0], 
				   	   boundTaAddress->AddressLength );
	}

	return status;
}


NTSTATUS
LspConnectMultiBindTaAddr (
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTA_ADDRESS			BoundAddr,
	IN	PTA_ADDRESS			BindAddr1,
	IN  PTA_ADDRESS			BindAddr2,
	IN  PTA_ADDRESS			DestAddr,
	IN  BOOLEAN				BindAnyAddress,
	IN PVOID				InDisconnectHandler,
	IN PVOID				InDisconnectEventContext,
	IN  PLARGE_INTEGER		TimeOut
	) 
{
	NTSTATUS			status;
	PTRANSPORT_ADDRESS	addrList;
	ULONG				addrListLen;
	PTRANSPORT_ADDRESS	addrListPrioritized;
	PTA_ADDRESS	    	taAddress, taAddress2;


	NDAS_BUGON( LSS );
	NDAS_BUGON( DestAddr );
	NDAS_BUGON( BindAnyAddress == TRUE );

	//	Parameter check

	if (DestAddr->AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDAS_BUGON( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	if (BindAddr1 && BindAddr1->AddressType != DestAddr->AddressType) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (BindAddr2 && BindAddr2->AddressType != DestAddr->AddressType) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	//	Initialize variables

	addrList = NULL;
	addrListPrioritized = NULL;

	do {

		//	Query binding address list
	
		addrList = NdasTransAllocateAddr( DestAddr->AddressType, 
										  LSTRANS_MAX_BINDADDR, 
										  &addrListLen );

		if (!addrList) {
		
			NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		status = NdasTransGetTransportAddress( DestAddr->AddressType,
											   addrListLen,
											   addrList,
											   NULL );

		if (!NT_SUCCESS(status)) {

			NDAS_BUGON( FALSE );
			KDPrintM( DBG_PROTO_ERROR, ("LstransQueryBindingDevices() failed. STATUS=%08lx\n", status) );

			break;
		}

		//	Validate binding address

		if (BindAddr1) {

			status = NdasTransIsInAddressList( addrList, BindAddr1, TRUE );
				
			if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
				
				BindAddr1 = NULL;

			} else if(!NT_SUCCESS(status)) {

				break;
			}
		}

		if (BindAddr2) {
		
			status = NdasTransIsInAddressList(addrList, BindAddr2, TRUE);

			if(status == STATUS_OBJECT_NAME_NOT_FOUND) {

				BindAddr2 = NULL;

			} else if (!NT_SUCCESS(status)) {

				break;
			}
		}

		//	Try bind addresses in order of BindAddr, BindAddr2, and any addresses available.
		
		addrListPrioritized = NdasTransAllocateAddr( DestAddr->AddressType, 
												     LSTRANS_MAX_BINDADDR, 
													 NULL );

		if (!addrListPrioritized) {

			NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
	
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		addrListPrioritized->TAAddressCount = 0;

		taAddress = addrListPrioritized->Address;

		if (BindAddr1) {

			RtlCopyMemory( taAddress, 
						   BindAddr1, 
						   (FIELD_OFFSET(TA_ADDRESS, Address) + 
						   TDI_ADDRESS_LENGTH_LPX) );

			taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
									  (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									  taAddress->AddressLength);

			addrListPrioritized->TAAddressCount ++;
		}

		if (BindAddr2) {

			RtlCopyMemory( taAddress, 
						   BindAddr2, 
						   (FIELD_OFFSET(TA_ADDRESS, Address) + 
						   TDI_ADDRESS_LENGTH_LPX) );

			taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
									  (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									  taAddress->AddressLength);

			addrListPrioritized->TAAddressCount ++;
		}

		if (BindAnyAddress) {
		
			ULONG	idx_bindaddr;

			taAddress2 = NULL;

			for (idx_bindaddr = 0; idx_bindaddr < (ULONG)addrList->TAAddressCount; idx_bindaddr ++) {

				if (taAddress2 == NULL) {

					taAddress2 = addrList->Address;

				} else {

					taAddress2 = (PTA_ADDRESS)(((PCHAR)taAddress2) + 
											   (FIELD_OFFSET(TA_ADDRESS, Address)) + 
											   taAddress2->AddressLength);
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						    ("taAddress2 %d = %02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x\n",
							 idx_bindaddr,
							 taAddress2->Address[0], taAddress2->Address[1],
							 taAddress2->Address[2], taAddress2->Address[3],
							 taAddress2->Address[4], taAddress2->Address[5],
							 taAddress2->Address[6], taAddress2->Address[7]) );

				if (taAddress2->AddressType == TDI_ADDRESS_TYPE_LPX) {

					RtlCopyMemory( taAddress,
								   taAddress2,
								   (FIELD_OFFSET(TA_ADDRESS, Address)) + 
								   taAddress2->AddressLength);

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							    ("taAddress2 %d = %02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x\n",
								 idx_bindaddr,
								 taAddress2->Address[0], taAddress2->Address[1],
								 taAddress2->Address[2], taAddress2->Address[3],
								 taAddress2->Address[4], taAddress2->Address[5],
								 taAddress2->Address[6], taAddress2->Address[7]) );

					taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
											   (FIELD_OFFSET(TA_ADDRESS, Address)) + 
											   taAddress->AddressLength);

					addrListPrioritized->TAAddressCount ++;
				}
			}
		}

		if (addrListPrioritized->TAAddressCount < 1) {
	
			NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL );

			status = STATUS_CLUSTER_NETWORK_NOT_FOUND;
			break;
		}

		//
		//	Try connecting with prioritized binding address.
		//

		status = LspConnectBindAddrList( LSS, 
										 BoundAddr, 
										 DestAddr, 
										 addrListPrioritized, 
										 InDisconnectHandler,
										 InDisconnectEventContext,
										 IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut) );

	} while(0);


	if (addrListPrioritized) {

		ExFreePool(addrListPrioritized);
	}

	if (addrList) {
	
		ExFreePool(addrList);
	}

	return status;
}


NTSTATUS
LspReconnect (
	IN PLANSCSI_SESSION LSS,
	IN PVOID			InDisconnectHandler,
	IN PVOID			InDisconnectEventContext,
	IN PLARGE_INTEGER	TimeOut
	) 
{
	NTSTATUS			ntStatus;
	
	TA_LSTRANS_ADDRESS	bindAddress;
	TA_LSTRANS_ADDRESS	nodeAddress;

	NDAS_BUGON( LSS );
	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );

	// Connect through any available addresses, 
	// but connect through the current address if possible.

	bindAddress.TAAddressCount = 1;

	bindAddress.Address[0].AddressLength = LSS->NdasBindAddress.AddressLength;
	bindAddress.Address[0].AddressType = LSS->NdasBindAddress.AddressType;

	RtlCopyMemory( &bindAddress.Address[0].Address,
				   LSS->NdasBindAddress.Address,
				   LSS->NdasBindAddress.AddressLength );	

	nodeAddress.TAAddressCount = 1;

	nodeAddress.Address[0].AddressLength = LSS->NdasNodeAddress.AddressLength;
	nodeAddress.Address[0].AddressType = LSS->NdasNodeAddress.AddressType;

	RtlCopyMemory( &nodeAddress.Address[0].Address,
				   LSS->NdasNodeAddress.Address,
				   LSS->NdasNodeAddress.AddressLength );	

	ntStatus = LspConnectMultiBindAddr( LSS,
										&bindAddress,	// Returned bound address
										NULL,			// Preferred binding address
										NULL,			// Next preferred binding address
										&nodeAddress,	// Destination address
										TRUE,
										InDisconnectHandler,
										InDisconnectEventContext,
										IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut) );
	
	if (ntStatus != STATUS_SUCCESS) {
		
		return ntStatus;
	}

	LSS->NdasBindAddress.AddressLength = bindAddress.Address[0].AddressLength;
	LSS->NdasBindAddress.AddressType   = bindAddress.Address[0].AddressType;

	RtlCopyMemory( LSS->NdasBindAddress.Address, 
				   &bindAddress.Address[0].Address, 
				   bindAddress.Address[0].AddressLength );

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

	// Make sure the LanScsiSession is logged out.
	(VOID)LspLogout(LSS,IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut));

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
LspDisconnect (
	IN PLANSCSI_SESSION LSS
	) 
{
	ASSERT( LSS );

	if (LSS->NdastAddressFile.Protocol) {

		NdasTransRegisterDisconnectHandler( &LSS->NdastAddressFile,
									        NULL,
											NULL );
	}

	if (LSS->NdastConnectionFile.Protocol) {

		NdasTransDisconnect( &LSS->NdastConnectionFile, 0 );
		NdasTransDisassociateAddress( &LSS->NdastConnectionFile );
		NdasTransCloseConnection( &LSS->NdastConnectionFile );
	}

	if (LSS->NdastAddressFile.Protocol) {

		NdasTransCloseAddress( &LSS->NdastAddressFile );
	}

	LSS->LanscsiProtocol = NULL;

	return STATUS_SUCCESS;
}


NTSTATUS
LspNoOperation (
	IN PLANSCSI_SESSION	LSS,
	IN UINT32			TargetId,
	OUT PBYTE			PduResponse,
	IN PLARGE_INTEGER	TimeOut
	)
{
	UINT8						PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_H2R_PDU_HEADER		pRequestHeader;
	LANSCSI_PDU_POINTERS		pdu;
	NTSTATUS					status;

	// Check Parameters.

	if (PduResponse == NULL) {

		NDAS_BUGON( FALSE );
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

	// Send Request.

	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	status = LspSendRequest( LSS, &pdu, NULL, IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut) );

	if (status != STATUS_SUCCESS) {
	
		NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );

		KDPrintM( DBG_PROTO_ERROR, ("Error when Send Request.\n") );
		return status;
	}

	*PduResponse = LANSCSI_RESPONSE_SUCCESS;

	return STATUS_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
//	Stub function for Lanscsi protocol interface
//
NTSTATUS
LspSendRequest (
	PLANSCSI_SESSION			LSS,
	PLANSCSI_PDU_POINTERS		Pdu,
	PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
	PLARGE_INTEGER				TimeOut
	) 
{
	ASSERT(LSS);

	NDAS_BUGON( OverlappedData == NULL );
	
	if (!LSS->LanscsiProtocol) {

		NDAS_BUGON( FALSE );	
		return STATUS_INVALID_PARAMETER;
	}

	if (!LSS->LanscsiProtocol->LsprotoFunc.LspSendRequest) {
	
		NDAS_BUGON( FALSE );	
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LspSendRequest(
					LSS,
					Pdu,
					OverlappedData,
					0, 
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

	if(!LSS->LanscsiProtocol->LsprotoFunc.LspReadReply) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LspReadReply(
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

	if(!LSS->LanscsiProtocol->LsprotoFunc.LspLogin) {
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
	
	status = LSS->LanscsiProtocol->LsprotoFunc.LspLogin(
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
		LSS->DigestPatchBuffer = ExAllocatePoolWithTag( NonPagedPool, LSS->MaxBlocksPerRequest * LSS->BlockInBytes + 16, LSS_DIGEST_PATCH_POOLTAG);
		if(!LSS->DigestPatchBuffer) {
			KDPrintM(DBG_PROTO_ERROR, ("Error! Could not allocate the digest patch buffer! Performace may slow down.\n"));
		}
#endif
		//
		// Workaround for lock leakage.
		//
		// HWVersion 2.0 Rev 0
		// Clean up locks all the time by repeating release operation for each lock.
		// To perform complete clean up excluding the current session/connection,
		// call LspWorkaroundCleanupLock().
		//

		if(LockCleanup) {
			if((LSS->HWVersion == LANSCSIIDE_VERSION_2_0 &&
				 LSS->HWRevision == LANSCSIIDE_VER20_REV_1G_ORIGINAL)) {
				lockMax = LANSCSIIDE_MAX_LOCK_VER11;
			} else {
				lockMax = 0;
			}

			KDPrintM(DBG_PROTO_TRACE, ("Clean up locks of %u\n", lockMax));

			for(lockIdx = 0; lockIdx < lockMax; lockIdx ++) {
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
	if(!LSS->LanscsiProtocol->LsprotoFunc.LspLogout) {
		return STATUS_NOT_IMPLEMENTED;
	}

	status = LSS->LanscsiProtocol->LsprotoFunc.LspLogout(
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
	IN	PLANSCSI_SESSION	LSS,
	OUT	PTARGETINFO_LIST	TargetList,
	IN	ULONG				TargetListLength,
	IN	PLARGE_INTEGER		TimeOut
	) {
	ASSERT(LSS);

	if(!LSS->LanscsiProtocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LSS->LanscsiProtocol->LsprotoFunc.LspTextTargetList) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LspTextTargetList(
					LSS,
					TargetList,
					TargetListLength,
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

	if(!LSS->LanscsiProtocol->LsprotoFunc.LspTextTargetData) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LSS->LanscsiProtocol->LsprotoFunc.LspTextTargetData(
					LSS,
					GetorSet,
					TargetID,
					TargetData,
					IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)
				);
}

NTSTATUS
LspRequest (
	IN  PLANSCSI_SESSION	LSS,
	IN  PLANSCSI_PDUDESC	PduDesc,
	OUT PBYTE				PduResponse,
	IN  PLANSCSI_SESSION	LSS2,
	IN  PLANSCSI_PDUDESC	PduDesc2,
	OUT PBYTE				PduResponse2
	) 
{
	NTSTATUS	status;
	INT			i;

	if (LSS->LanscsiProtocol == NULL) {
		
		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (LSS->LanscsiProtocol->LsprotoFunc.LspRequest == NULL) {

		NDAS_BUGON( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	if (PduDesc->TimeOut == NULL) {

		PduDesc->TimeOut = &LSS->DefaultTimeOut;
	}

	if (LSS2) {

		NDAS_BUGON( FALSE );

		if (LSS2->LanscsiProtocol == NULL) {
		
			NDAS_BUGON( FALSE );
			return STATUS_INVALID_PARAMETER;
		}


		if (LSS2->LanscsiProtocol->LsprotoFunc.LspRequest == NULL) {

			NDAS_BUGON( FALSE );
			return STATUS_NOT_IMPLEMENTED;
		}

		if (PduDesc2->TimeOut == NULL) {

			PduDesc2->TimeOut = &LSS2->DefaultTimeOut;
		}
	}

	for (i=0; i<5; i++) {

		status = LSS->LanscsiProtocol->LsprotoFunc.LspRequest( LSS,
															   PduDesc,
															   PduResponse,
															   LSS2,
															   PduDesc2,
															   PduResponse2,
															   (i==4) ? TRUE : FALSE );
		if (status != STATUS_SUCCESS) {

			break;
		}

		if (*PduResponse == LANSCSI_RESPONSE_SUCCESS && (PduResponse2 == NULL || *PduResponse2 == LANSCSI_RESPONSE_SUCCESS)) {

			if (i != 0)
				DbgPrint( "i=%d\n", i );

			break;
		}
	}

	NDAS_BUGON( status != STATUS_SUCCESS					|| 
				*PduResponse == LANSCSI_RESPONSE_SUCCESS	||
				*PduResponse == LANSCSI_RESPONSE_T_COMMAND_FAILED );

	return status;
	// Call to IdeLspRequest_V11, IdeLspRequest_V10
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

	if(!LSS->LanscsiProtocol->LsprotoFunc.LspPacketRequest) {
		return STATUS_NOT_IMPLEMENTED ;
	}

	if(PduDesc->TimeOut == NULL)
		PduDesc->TimeOut = &LSS->DefaultTimeOut;

	//NDAS_BUGON( PduDesc->TimeOut->QuadPart < 0 );

	result =  LSS->LanscsiProtocol->LsprotoFunc.LspPacketRequest(
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
	)
{
	NTSTATUS status;

	ASSERT(LSS);
	ASSERT(PduResponse);
	
	if (!LSS->LanscsiProtocol) {

		NDAS_BUGON( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!LSS->LanscsiProtocol->LsprotoFunc.LspVendorRequest) {

		NDAS_BUGON( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	if(PduDesc->TimeOut == NULL)
		PduDesc->TimeOut = &LSS->DefaultTimeOut;

	status = LSS->LanscsiProtocol->LsprotoFunc.LspVendorRequest(
					LSS,
					PduDesc,
					PduResponse
				);


	return status;

}

//////////////////////////////////////////////////////////////////////////
//
//	NDAS device lock control
//
NTSTATUS
LspAcquireLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		NDAS_BUGON( FALSE );
		return STATUS_NOT_SUPPORTED;
	}

	pduResponse = LANSCSI_RESPONSE_SUCCESS;

	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Command = VENDOR_OP_SET_MUTEX;
	pduDesc.Param8[3] = (UCHAR)LockNo;
	
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);

	if (pduDesc.TimeOut && pduDesc.TimeOut->QuadPart == 0) {

		pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, NULL);
	};

	status = LspVendorRequest(
							LSS,
							&pduDesc,
							&pduResponse
						);
	if(NT_SUCCESS(status)) {

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_TRACE,	("Acquiring lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_LOCK_NOT_GRANTED;
		} else {
			KDPrintM(DBG_LURN_TRACE,	("Acquired lock #%u\n", LockNo));
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
	IN ULONG			LockNo,
	IN PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		NDAS_BUGON( FALSE );
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

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Releasing lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_LOCK_NOT_GRANTED;
		} else {
			KDPrintM(DBG_LURN_TRACE,	("Released lock #%u\n", LockNo));
		}

		//
		// Convert Network endian to the host endian here.
		//

		if(LockData) {
			UINT32	lockData = NTOHL(pduDesc.Param32[1]);


			//
			// NDAS chip 1.1 returns the increased lock counter.
			// Decrease it for the chip version abstraction.
			//
			if(LSS->HWVersion == LANSCSIIDE_VERSION_1_1) {
				lockData --;
			}
			if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
				*(PUINT32)LockData = lockData;
			}
		}
	}

	return status;
}


NTSTATUS
LspGetLockOwner(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	IN PBYTE			LockOwner,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		NDAS_BUGON( FALSE );
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

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Get owner of lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_LOCK_NOT_GRANTED;
		} else {
			KDPrintM(DBG_LURN_TRACE,	("owner #%u\n", LockNo));
		}

		RtlCopyMemory(LockOwner, &pduDesc.Param64, 8);
	}

	return status;
}

NTSTATUS
LspGetLockData(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockNo,
	IN PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;

	if(LSS->HWProtoVersion <= LSIDEPROTO_VERSION_1_0) {
		NDAS_BUGON( FALSE );
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

		if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR,	("Releasing lock #%u denied by NDAS device\n", LockNo));
			status = STATUS_LOCK_NOT_GRANTED;
		} else {
			KDPrintM(DBG_LURN_TRACE,	("Lock data of lock #%u\n", LockNo));
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

//
// Clean up leaked device locks on the NDAS device.
//
// Workaround routine is also implemented in LspLogin().
// It clean up only its connection.
// Please be aware of it when modifying this function.
//
// Workaround for lock leakage of HWVersion 1.1 / 2.0 Rev 0.
// Clean up locks by repeating acquire/release operation
// for maximum of NDAS connection capacity.
//

NTSTATUS
LspWorkaroundCleanupLock(
   PLANSCSI_SESSION	LSS,
   ULONG			LockNo,
   PLARGE_INTEGER	TimeOut
){
	NTSTATUS		status;
	LONG			i;
	LANSCSI_PDUDESC	pduDesc;
	BYTE			pduResponse;
	PLANSCSI_SESSION	tempLSS;
	LONG			maxConn;
	UCHAR			bindingBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	UCHAR			targetBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
	PTA_ADDRESS		BindingAddress = (PTA_ADDRESS)bindingBuffer;
	PTA_ADDRESS  	TargetAddress = (PTA_ADDRESS)targetBuffer;
	LSSLOGIN_INFO	loginInfo;
	LSPROTO_TYPE	LSProto;
	ULONG			cleaned;

	//
	//	Parameter check
	//
	// Perform clean-up only for NDAS 2.0 rev 0.
	//

	if((LSS->HWVersion == LANSCSIIDE_VERSION_2_0 &&
		 LSS->HWRevision == LANSCSIIDE_VER20_REV_1G_ORIGINAL)) {

		// Get maximum connections excluding the current session/connection.
		maxConn = LANSCSIIDE_MAX_CONNECTION_VER11 - 1;
	} else {
		return STATUS_SUCCESS;
	}

	tempLSS = ExAllocatePoolWithTag(
							NonPagedPool,
							maxConn * sizeof(LANSCSI_SESSION),
							LSS_POOLTAG);
	if(tempLSS == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	RtlZeroMemory(tempLSS, maxConn * sizeof(LANSCSI_SESSION));

	//
	//	Init variables
	//

	status = STATUS_SUCCESS;
	RtlZeroMemory(&pduDesc, sizeof(LANSCSI_PDUDESC));
	pduDesc.Param8[3] = (UCHAR)LockNo;
	pduDesc.TimeOut = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut);
	LspGetAddresses(LSS, BindingAddress, TargetAddress);
	cleaned = 0;


	//
	//	Try to make connections to fill up connection pool of the NDAS device.
	//	So, we can clear invalid acquisition of the locks.
	//
	KDPrintM(DBG_LURN_ERROR, ("Try to clean up lock\n"));
	for (i=0 ; i < maxConn; i++) {

		tempLSS[i].DefaultTimeOut.QuadPart = IF_NULL_TIMEOUT_THEN_DEFAULT(LSS, TimeOut)->QuadPart;

		//
		//	Connect and log in with read-only access.
		//

		//	connect
		status = LspConnect(
					&tempLSS[i],
					BindingAddress,
					TargetAddress,
					NULL,
					NULL,
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
			continue;
		}

		// Do need to acquire the lock to release.
		// NDAS chip 2.0 original allow to release the lock 
		// regardless of the ownership if the connection number
		// in the lock information of NDAS chip 2.0
		// is same.
#if 0
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
#endif
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
