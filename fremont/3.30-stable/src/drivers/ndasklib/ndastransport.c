#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndastransport"


NDAS_TRANSPORT_INTERFACE	NdasTransLpxV2Interface = {

		sizeof(NDAS_TRANSPORT_INTERFACE),
		LSSTRUC_TYPE_TRANSPORT_INTERFACE,
		TDI_ADDRESS_TYPE_LPX,
		TDI_ADDRESS_LENGTH_LPX,
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

UINT32						NdasTransCnt  = NR_NDAS_PROTOCOL;
PNDAS_TRANSPORT_INTERFACE	NdasTransportList[NR_NDAS_MAX_PROTOCOL] = { &NdasTransLpxV2Interface };

//
//	Translate NdasTrans address type to trans type.
//

NTSTATUS
NdasTransAddrTypeToTransType (
	IN	USHORT				AddrType,
	OUT PNDAS_TRANS_TYPE	Transport
	) 
{
	switch (AddrType) {
	
	case TDI_ADDRESS_TYPE_LPX:
	
		*Transport = NDAS_LPX_V1;
		break;

	case TDI_ADDRESS_TYPE_IP:

		*Transport = NDAS_TCP;
		break;

	default:

		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasTransOpenAddress (
	IN PTA_NDAS_ADDRESS			TaAddress,
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile
	) 
{
	NDAS_TRANS_TYPE		transType;
	NTSTATUS			status;

	ASSERT( TaAddress );
	ASSERT( NdastAddressFile );
	ASSERT( !NdastAddressFile->Protocol );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("entered, NdastAddressFile = %p\n", NdastAddressFile) );

	status = NdasTransAddrTypeToTransType( TaAddress->Address[0].AddressType, &transType );
	
	if (status != STATUS_SUCCESS) {
		
		NDAS_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	NdastAddressFile->Protocol = NdasTransportList[transType];

	if (!NdastAddressFile->Protocol->NdasTransFunc.OpenAddress) {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdastAddressFile->Protocol->
				NdasTransFunc.OpenAddress( &TaAddress->Address[0].NdasAddress.Lpx, 
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

		NDAS_ASSERT( FALSE );
		return;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.CloseAddress) {
	
		NDAS_ASSERT( FALSE );
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
	NDAS_TRANS_TYPE	transType;
	NTSTATUS		status;

	ASSERT( NdastConnectionFile );
	ASSERT( !NdastConnectionFile->Protocol );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("enter NdastConnectionFile = %p\n", NdastConnectionFile) );

	status = NdasTransAddrTypeToTransType( AddressType, &transType );

	if (status != STATUS_SUCCESS) {
		
		NDAS_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	NdastConnectionFile->Protocol = NdasTransportList[transType];

	if (!NdastConnectionFile->Protocol->NdasTransFunc.OpenAddress) {

		NDAS_ASSERT( FALSE );
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

		NDAS_ASSERT( FALSE );
		return;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.CloseConnection) {

		NDAS_ASSERT( FALSE );
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

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (NdastConnectionFile->Protocol != NdastAddressFile->Protocol) {

		NDAS_ASSERT( FALSE );
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

		NDAS_ASSERT( FALSE );
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
	IN  PTA_NDAS_ADDRESS			RemoteAddress,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	) 
{
	ASSERT( NdastConnectionFile );
	ASSERT( RemoteAddress );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastConnectionFile->Protocol) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastConnectionFile->Protocol->NdasTransFunc.Connection) {
	
		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastConnectionFile->Protocol->
			NdasTransFunc.Connection( NdastConnectionFile->ConnectionFileObject,
								      &RemoteAddress->Address[0].NdasAddress.Lpx,
								      TimeOut,
								      OverlappedContext );

}

NTSTATUS
NdasTransportListen (
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
	
		NDAS_ASSERT( FALSE );
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
	
	NDAS_ASSERT( SendLength % 4 == 0 ); // netdisk chip align requirement

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
	IN  PTA_NDAS_ADDRESS			RemoteAddress,
	IN  PUCHAR						SendBuffer,
	IN  ULONG						SendLength,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT PLONG						Result
	) 
{
	ASSERT( NdastAddressFile );

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (!NdastAddressFile->Protocol) {

		return STATUS_INVALID_PARAMETER;
	}

	if (!NdastAddressFile->Protocol->NdasTransFunc.SendDatagram) {

		return STATUS_NOT_IMPLEMENTED;
	}

	return NdastAddressFile->Protocol->
				NdasTransFunc.SendDatagram( NdastAddressFile->AddressFileObject,
										    &RemoteAddress->Address[0].NdasAddress.Lpx,
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
			NdasTransFunc.IoControl( NdastConnectionFile->ConnectionFileObject,
									 IoControlCode,
									 InputBuffer,
									 InputBufferLength,
									 OutputBuffer,
									 OutputBufferLength );
}

NTSTATUS
NdasTransGetTransportAddress (
	IN  USHORT				AddressType,
	IN	ULONG				AddressListLen,
	IN	PTRANSPORT_ADDRESS	AddressList,
	OUT	PULONG				OutLength
	)
{
	NDAS_TRANS_TYPE		transType;
	NTSTATUS			status;

	DebugTrace( DBG_TRANS_TRACE, ("entered.\n") );

	if (AddressType != TDI_ADDRESS_TYPE_LPX) {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdasTransAddrTypeToTransType( AddressType, &transType );
	
	if (status != STATUS_SUCCESS) {
		
		NDAS_ASSERT( FALSE );
		return status;
	}

	if (transType >= NdasTransCnt ) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	if (!NdasTransportList[transType]->NdasTransFunc.GetTransportAddress) {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	status = NdasTransportList[transType]->
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

		NDAS_ASSERT( FALSE );
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
	IN PTA_NDAS_ADDRESS		Address,
	IN BOOLEAN				InvalidateIfMatch
	)	
{
	NTSTATUS	status;
	LONG		idx_addr;
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

	for (idx_addr = 0; idx_addr < AddrList->TAAddressCount; idx_addr++) {
	
		if (taAddress == NULL) {

			taAddress = AddrList->Address;

		} else {

			taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
									  (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									  taAddress->AddressLength);
		}

		if (Address->Address[0].AddressType == taAddress->AddressType &&
			Address->Address[0].AddressLength == taAddress->AddressLength &&
			RtlCompareMemory(	&Address->Address[0].NdasAddress,
								&taAddress->Address,
								Address->Address[0].AddressLength) == Address->Address[0].AddressLength) {

			if (InvalidateIfMatch) {
	
				taAddress->AddressType = TDI_ADDRESS_TYPE_INVALID; // invalid type
			}

			status = STATUS_SUCCESS;
			break;
		}
	}

	return status;
}