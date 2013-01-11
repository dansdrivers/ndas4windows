#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndastransport"

#if __NDAS_SCSI_LPXTDI_V2__


NDAS_TRANSPORT_INTERFACE	NdasTransLpxV2Interface = {

		sizeof(NDAS_TRANSPORT_INTERFACE),
		LSSTRUC_TYPE_TRANSPORT_INTERFACE,
		TDI_ADDRESS_TYPE_LPX,
		TDI_ADDRESS_LENGTH_LPX,
		LSTRANS_LPX_V1,				// version 1.0
		0,
		{
			LpxTdiV2OpenAddress,
			LpxTdiV2CloseAddress,
			LpxTdiV2OpenConnection,
			LpxTdiV2CloseConnection,
			LpxTdiV2AssociateAddress,
			LpxTdiV2DisassociateAddress,
			LpxTdiV2Connect,
			LpxTdiV2Listen,
			LpxTdiV2Disconnect,
			LpxTdiV2Send,
			LpxTdiV2Recv,
			LpxTdiV2SendDatagram,
			LpxTdiV2RegisterDisconnectHandler,
			LpxTdiV2RegisterRecvDatagramHandler,
			LpxTdiV2QueryInformation,
			LpxTdiV2SetInformation,
			LpxTdiV2OpenControl,
			LpxTdiV2CloseControl,
			LpxTdiV2IoControl,
			LpxTdiV2GetAddressList,
			LpxTdiV2GetTransportAddress,
			LpxTdiV2CompleteRequest,
			LpxTdiV2CancelRequest,
			LpxTdiV2MoveOverlappedContext
		},
	};

UINT32						NdasTransCnt  = NR_NDAS_TRANS_PROTOCOL;
PNDAS_TRANSPORT_INTERFACE	NdasTransList[NR_NDAS_TRANS_MAX_PROTOCOL] = {&NdasTransLpxV2Interface};

//
//	Translate NdasTrans address type to trans type.
//

NTSTATUS
NdasTransAddrTypeToTransType (
	IN	USHORT				AddrType,
	OUT PNDAS_TRANS_TYPE	Transport
	) 
{
	switch(AddrType) {
	
	case TDI_ADDRESS_TYPE_LPX:
	
		*Transport = NDAS_TRANS_LPX_V1;
		break;

	case TDI_ADDRESS_TYPE_IP:

		*Transport = NDAS_TRANS_TCP;
		break;

	default:

		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasTransGetType (
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN PNDAS_TRANS_TYPE			TransType
) {

	ASSERT( NdastAddressFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	*TransType = NdastAddressFile->Protocol->NdasTransType;

	return STATUS_SUCCESS;
}


NTSTATUS
NdasTransOpenAddress (
	IN PTA_ADDRESS				TaAddress,
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile
	) 
{
	NDAS_TRANS_TYPE		transType;
	NTSTATUS			status;
	PTDI_ADDRESS_LPX	lpxAddress;

	ASSERT( TaAddress );
	ASSERT( NdastAddressFile );
	ASSERT( !NdastAddressFile->Protocol );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("entered, NdastAddressFile = %p\n", NdastAddressFile) );

	status = NdasTransAddrTypeToTransType( TaAddress->AddressType, &transType );
	
	if (status != STATUS_SUCCESS) {
		
		NDASSCSI_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	NdastAddressFile->Protocol = NdasTransList[transType];

	if (!NdastAddressFile->Protocol->NdasTransFunc.OpenAddress) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	lpxAddress = (PTDI_ADDRESS_LPX)TaAddress->Address;

	status = NdastAddressFile->Protocol->
				NdasTransFunc.OpenAddress( lpxAddress, 
										   &NdastAddressFile->AddressFileHandle,
										   &NdastAddressFile->AddressFileObject );


	if (status != STATUS_SUCCESS) {
	
		NdastAddressFile->Protocol = NULL;
	}

	return status;
}

VOID
NdasTransCloseAddress (
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile
	) 
{
	ASSERT( NdastAddressFile );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("enter NdastAddressFile = %p\n", NdastAddressFile) );

	if (!NdastAddressFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.CloseAddress) {
	
		NDASSCSI_ASSERT( FALSE );
		return;
	}

	NdastAddressFile->Protocol->
		NdasTransFunc.CloseAddress( NdastAddressFile->AddressFileHandle,
									NdastAddressFile->AddressFileObject );

	NdastAddressFile->AddressFileHandle = NULL;
	NdastAddressFile->AddressFileObject = NULL;
	NdastAddressFile->Protocol = NULL;

	return;
}


NTSTATUS
NdasTransOpenConnection (
	IN   PVOID							ConnectionContext,
	IN	 USHORT							AddressType,
	OUT  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	) 
{
	NDAS_TRANS_TYPE			transType;
	NTSTATUS				status;

	ASSERT( NdastConnectionFile );
	ASSERT( !NdastConnectionFile->Protocol );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("enter NdastConnectionFile = %p\n", NdastConnectionFile) );

	status = NdasTransAddrTypeToTransType( AddressType, &transType );

	if (status != STATUS_SUCCESS) {
		
		NDASSCSI_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	NdastConnectionFile->Protocol = NdasTransList[transType];

	if (!NdastConnectionFile->Protocol->NdasTransFunc.OpenAddress) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdastConnectionFile->Protocol->
				NdasTransFunc.OpenConnection( ConnectionContext,
											  &NdastConnectionFile->ConnectionFileHandle,
											  &NdastConnectionFile->ConnectionFileObject,
											  &NdastConnectionFile->OverlappedContext );

	if (status != STATUS_SUCCESS) {

		NdastConnectionFile->Protocol = NULL;
	}

	return status;
}

VOID
NdasTransCloseConnection (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("enter NdastConnectionFile = %p\n", NdastConnectionFile) );

	if (!NdastConnectionFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.CloseConnection) {

		NDASSCSI_ASSERT( FALSE );
		return;
	}

	NdastConnectionFile->Protocol->
			NdasTransFunc.CloseConnection( NdastConnectionFile->ConnectionFileHandle,
										   NdastConnectionFile->ConnectionFileObject,
										   &NdastConnectionFile->OverlappedContext );

	NdastConnectionFile->ConnectionFileHandle = NULL;
	NdastConnectionFile->ConnectionFileObject = NULL;
	NdastConnectionFile->Protocol = NULL;

	return;
}

NTSTATUS
NdasTransAssociate (
	IN PNDAS_TRANS_ADDRESS_FILE		NdastAddressFile,
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	) 
{
	ASSERT( NdastAddressFile );
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (NdastConnectionFile->Protocol != NdastAddressFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.AssociateAddress) {
	
		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
			NdasTransFunc.AssociateAddress( NdastConnectionFile->ConnectionFileObject,
											NdastAddressFile->AddressFileHandle );
}

VOID
NdasTransDisassociateAddress (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.DisassociateAddress) {
	
		return;
	}

	NdastConnectionFile->Protocol->
		NdasTransFunc.DisassociateAddress( NdastConnectionFile->ConnectionFileObject );
}


NTSTATUS
NdasTransConnect (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PTA_ADDRESS					RemoteAddress,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	) 
{
	PTDI_ADDRESS_LPX	lpxAddress;

	ASSERT( NdastConnectionFile );
	ASSERT( RemoteAddress );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Connection) {
	
		return STATUS_NOT_IMPLEMENTED;
	}

	lpxAddress = (PTDI_ADDRESS_LPX)RemoteAddress->Address;

	return NdastConnectionFile->Protocol->
			NdasTransFunc.Connection( NdastConnectionFile->ConnectionFileObject,
								      lpxAddress,
								      TimeOut,
								      OverlappedContext );

}

NTSTATUS
NdasTransListen (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PULONG						RequestOptions,
	IN  PULONG						ReturnOptions,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	OUT PUCHAR						AddressBuffer
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {
	
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Listen) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
				NdasTransFunc.Listen( NdastConnectionFile->ConnectionFileObject,
								      RequestOptions,
									  ReturnOptions,
									  Flags,
									  TimeOut,
									  OverlappedContext,
									  AddressBuffer );
}

VOID
NdasTransDisconnect (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  ULONG						Flags
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {
	
		NDASSCSI_ASSERT( FALSE );
		return;;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Disconnect) {

		return;;
	}

	NdastConnectionFile->Protocol->
		NdasTransFunc.Disconnect( NdastConnectionFile->ConnectionFileObject,
							      Flags );
}

NTSTATUS
NdasTransSend (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PUCHAR						SendBuffer,
	IN  ULONG						SendLength,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT PLONG						Result
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Send) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
				NdasTransFunc.Send( NdastConnectionFile->ConnectionFileObject,
								    SendBuffer,
									SendLength,
									Flags,
									TimeOut,
									OverlappedContext,
									RequestIdx,
									Result );
}


NTSTATUS
NdasTransRecv (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN	PUCHAR						RecvBuffer,
	IN	ULONG						RecvLength,
	IN	ULONG						Flags,
	IN	PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT	PLONG						Result
	) 
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Recv) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
				NdasTransFunc.Recv( NdastConnectionFile->ConnectionFileObject,
									RecvBuffer,
									RecvLength,
									Flags,
									TimeOut,
									OverlappedContext,
									RequestIdx,
									Result );
}

NTSTATUS
NdasTransSendDatagram (
	IN  PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN  PTA_ADDRESS					RemoteAddress,
	IN  PUCHAR						SendBuffer,
	IN  ULONG						SendLength,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT PLONG						Result
	) 
{
	PTDI_ADDRESS_LPX	lpxAddress;

	ASSERT( NdastAddressFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.SendDatagram) {

		return STATUS_NOT_IMPLEMENTED;
	}

	lpxAddress = (PTDI_ADDRESS_LPX)RemoteAddress->Address;

	return NdastAddressFile->Protocol->
				NdasTransFunc.SendDatagram( NdastAddressFile->AddressFileObject,
										    lpxAddress,
											SendBuffer,
											SendLength,
											TimeOut,
											OverlappedContext,
											RequestIdx,
											Result );

}

NTSTATUS
NdasTransRegisterDisconnectHandler (
	IN  PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN	PVOID						InEventHandler,
	IN	PVOID						InEventContext
	) 
{
	ASSERT( NdastAddressFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.RegisterDisconnectHandler) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastAddressFile->Protocol->
			NdasTransFunc.RegisterDisconnectHandler( NdastAddressFile->AddressFileObject,
													 InEventHandler,
													 InEventContext );
}

NTSTATUS
NdasTransRegisterRecvDatagramHandler (
	IN  PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN	PVOID						InEventHandler,
	IN	PVOID						InEventContext
	) 
{
	ASSERT( NdastAddressFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.RegisterRecvDatagramHandler) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastAddressFile->Protocol->
			NdasTransFunc.RegisterRecvDatagramHandler( NdastAddressFile->AddressFileObject,
													   InEventHandler,
													   InEventContext );
}

NTSTATUS
NdasTransQueryInformation (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  ULONG						QueryType,
	IN  PVOID						Buffer,
	IN  ULONG						BufferLen
	)
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.QueryInformation) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
			NdasTransFunc.QueryInformation( NdastConnectionFile->ConnectionFileObject,
											QueryType,
											Buffer,
											BufferLen );
}

NTSTATUS
NdasTransSetInformation (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN ULONG						SetType,
	IN  PVOID						Buffer,
	IN  ULONG						BufferLen
	)
{
	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.SetInformation) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
			NdasTransFunc.SetInformation( NdastConnectionFile->ConnectionFileObject,
										  SetType,
										  Buffer,
										  BufferLen );
}

NTSTATUS
NdasTransIoControl (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN	ULONG    					IoControlCode,
	IN	PVOID						InputBuffer OPTIONAL,
	IN	ULONG						InputBufferLength,
	OUT PVOID					  	OutputBuffer OPTIONAL,
	IN	ULONG				 		OutputBufferLength
	) 
{

	ASSERT( NdastConnectionFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.IoControl) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
			NdasTransFunc.IoControl( NdastConnectionFile->ConnectionFileHandle,
									 NdastConnectionFile->ConnectionFileObject,
									 IoControlCode,
									 InputBuffer,
									 InputBufferLength,
									 OutputBuffer,
									 OutputBufferLength );
}

NTSTATUS
NdasTransGetTransportAddress (
	IN USHORT				AddressType,
	IN	ULONG				AddressListLen,
	IN	PTRANSPORT_ADDRESS	AddressList,
	OUT	PULONG				OutLength
	)
{
	NDAS_TRANS_TYPE		transType;
	NTSTATUS			status;

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdasTransAddrTypeToTransType( AddressType, &transType );
	
	if (status != STATUS_SUCCESS) {
		
		NDASSCSI_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdasTransList[transType]->NdasTransFunc.GetTransportAddress) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdasTransList[transType]->
				NdasTransFunc.GetTransportAddress( AddressListLen, 
												   AddressList,
												   OutLength );
	return status;
}


VOID
NdasTransCompleteRequest (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext, 
	IN ULONG						RequestIdx
	)
{
	NdastConnectionFile->Protocol->
			NdasTransFunc.CompleteRequest( OverlappedContext, RequestIdx );
}

VOID
NdasTransCancelRequest (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN	ULONG						RequestIdx,
	IN  BOOLEAN						DisConnect,
	IN	ULONG						DisConnectFlags
	) 
{
	NdastConnectionFile->Protocol->
		NdasTransFunc.CancelRequest( NdastConnectionFile->ConnectionFileObject,
									 OverlappedContext, 
									 RequestIdx,
									 DisConnect,
									 DisConnectFlags );
}

PTRANSPORT_ADDRESS
NdasTransAllocateAddr (
	USHORT	AddressType,
	ULONG	AddressCnt,
	PULONG	Length
	)
{
	ULONG				length;	
	PTRANSPORT_ADDRESS	transportAddr;

	if (AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDASSCSI_ASSERT( FALSE );
		return NULL;
	}

	length = FIELD_OFFSET( TRANSPORT_ADDRESS, Address );
	length += (FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX) * AddressCnt;

	transportAddr = ExAllocatePoolWithTag( NonPagedPool, length, NDAS_TRANS_POOLTAG_TA );

	RtlZeroMemory( transportAddr, length );

	if (Length) {

		*Length = length;
	}

	return transportAddr;
}

NTSTATUS
NdasTransIsInAddressList (
	IN PTRANSPORT_ADDRESS	AddrList,
	IN PTA_ADDRESS			Address,
	IN BOOLEAN				InvalidateIfMatch
	)	
{
	NTSTATUS	status;
	ULONG		idx_addr;
	PTA_ADDRESS	taAddress;

	//	Parameter check

	if (!Address) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!AddrList || AddrList->TAAddressCount < 1) {
	
		return STATUS_INVALID_PARAMETER;
	}


	//	Match address

	status = STATUS_OBJECT_NAME_NOT_FOUND;

	taAddress = NULL;

	for (idx_addr = 0; idx_addr < (ULONG)AddrList->TAAddressCount; idx_addr++) {
	
		if (taAddress == NULL) {

			taAddress = AddrList->Address;

		} else {

			taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
									  (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									  taAddress->AddressLength);
		}

		if (Address->AddressType == taAddress->AddressType &&
			Address->AddressLength == taAddress->AddressLength &&
			RtlCompareMemory(	&Address->Address,
								&taAddress->Address,
								Address->AddressLength) == Address->AddressLength) {

			if (InvalidateIfMatch) {
	
				taAddress->AddressType = TDI_ADDRESS_TYPE_INVALID; // invalid type
			}

			status = STATUS_SUCCESS;
			break;
		}
	}

	return status;
}

#if 0

NTSTATUS
NdasTransQueryBindingDevices(
		IN	UINT32				Protocol,
		IN	ULONG				AddressListLen,
		IN	PTA_NDAS_TRANS_ADDRESS	AddressList,
		OUT PULONG				OutLength
	) {

	DebugTrace(DBG_TRANS_TRACE, ("entered.\n"));

	if(/* Protocol < 0 ||*/ Protocol >= NdasTransCnt ) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!NdasTransList[Protocol]->NdasTransFunc.NdasTransQueryBindingDevices) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return NdasTransList[Protocol]->NdasTransFunc.NdasTransQueryBindingDevices(
					AddressListLen, AddressList, OutLength
				);
}




NTSTATUS
NdasTransIsInAddressList(
	IN PTA_NDAS_TRANS_ADDRESS	AddrList,
	IN PTA_NDAS_TRANS_ADDRESS	Address,
	IN BOOLEAN				InvalidateIfMatch
){
	NTSTATUS	status;
	ULONG		idx_addr;


	//
	//	Parameter check
	//

	if(!Address || Address->TAAddressCount != 1) {
		return STATUS_INVALID_PARAMETER;
	}
	if(!AddrList || AddrList->TAAddressCount < 1) {
		return STATUS_INVALID_PARAMETER;
	}


	//
	//	Match address
	//

	status = STATUS_OBJECT_NAME_NOT_FOUND;
	for(idx_addr = 0; idx_addr < (ULONG)AddrList->TAAddressCount; idx_addr++) {

		if( Address->Address[0].AddressType == AddrList->Address[idx_addr].AddressType &&
			Address->Address[0].AddressLength == AddrList->Address[idx_addr].AddressLength &&
			RtlCompareMemory(	&Address->Address[0].Address,
								&AddrList->Address[idx_addr].Address,
								Address->Address[0].AddressLength) == Address->Address[0].AddressLength) {

			if(InvalidateIfMatch) {
					AddrList->Address[idx_addr].AddressLength = 0;
			}

			status = STATUS_SUCCESS;
			break;
		}

	}

	return status;
}

//
//	Wait until the address and the device of the address is accessible.
//
//	We have to wait for the added address is accessible.
//	Especially, we can not open LPX device early time on MP systems.
//

NTSTATUS
NdasTransWaitForAddress(
	IN PTA_NDAS_TRANS_ADDRESS		Address,
	IN ULONG					MaxWaitLoop
){
	PTA_NDAS_TRANS_ADDRESS	addrList;
	ULONG				addrListLen;
	LARGE_INTEGER		interval;
	ULONG				waitcnt;
	NTSTATUS			status;
	NDAS_TRANS_TYPE		transport;

	addrList = NdasTransAllocateAddr(
					NDAS_TRANS_MAX_BINDADDR,
					&addrListLen
				);
	if(!addrList) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = NdasTransAddrTypeToTransType(Address->Address[0].AddressType, &transport);
	if(!NT_SUCCESS(status)) {
		DebugTrace(DBG_PROTO_ERROR, ("NdasTransAddrTypeToTransType() failed. STATUS=08lx\n", status));
		return status;
	}


	//
	//	Wait for binding address
	//
	interval.QuadPart = - 10 * 1000 * 500;	// 500 ms

	for(waitcnt = 0; waitcnt < MaxWaitLoop; waitcnt++) {

		status = NdasTransQueryBindingDevices(transport, addrListLen, addrList, NULL);
		if(!NT_SUCCESS(status)) {
			DebugTrace(DBG_PROTO_ERROR, ("NdasTransQueryBindingDevices() failed. STATUS=08lx\n", status));

			KeDelayExecutionThread(KernelMode, FALSE, &interval);

			status = STATUS_SUCCESS;
			continue;
		}

		status = NdasTransIsInAddressList(addrList, Address, TRUE);
		if(!NT_SUCCESS(status)) {
			if(status != STATUS_OBJECT_NAME_NOT_FOUND) {
				DebugTrace(DBG_PROTO_ERROR, ("The address not found. Wait...\n"));
				KeDelayExecutionThread(KernelMode, FALSE, &interval);

				status = STATUS_SUCCESS;
				continue;

			} else  {
				DebugTrace(DBG_PROTO_ERROR, ("NdasTransIsInAddressList() failed. STATUS=%08lx\n", status));
				ExFreePool(addrList);
				return status;
			}
		}

		DebugTrace(DBG_PROTO_ERROR, ("The address found.\n"));
		break;

	}

	ExFreePool(addrList);

	return STATUS_SUCCESS;
}

#endif

#endif