#include <ntddk.h>
#include <TdiKrnl.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSTransport.h"
#include "SocketLpx.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSTrans"

UINT32						LstransCnt  = NR_LSTRANS_PROTOCOL;
PLANSCSITRANSPORT_INTERFACE	LstransList[NR_LSTRANS_MAX_PROTOCOL] = {&LstransLPXV10Interface};


NTSTATUS
LstransAddrTypeToTransType(
		USHORT			AddrType,
		PLSTRANS_TYPE	Transport
	) {
	switch(AddrType) {
	case TDI_ADDRESS_TYPE_LPX:
		*Transport = LSTRANS_LPX_V1;
		break;
	case TDI_ADDRESS_TYPE_IP:
		*Transport = LSTRANS_TCP;
		break;
	default:
		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LstransGetType(
		PLSTRANS_ADDRESS_FILE	AddressFile,
		PLSTRANS_TYPE			TransType
) {

	if(!AddressFile)
		return STATUS_INVALID_PARAMETER;

	if(!AddressFile->Protocol)
		return STATUS_INVALID_PARAMETER;

	*TransType = AddressFile->Protocol->LstransType;

	return STATUS_SUCCESS;
}


NTSTATUS
LstransOpenAddress(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PLSTRANS_ADDRESS_FILE	AddressFile
) {
	LSTRANS_TYPE			TransType;
	NTSTATUS				status;

	ASSERT(Address);
	ASSERT(AddressFile);
	ASSERT(!AddressFile->Protocol);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	status = LstransAddrTypeToTransType(Address->Address[0].AddressType, &TransType);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if(TransType < 0 || TransType >= LstransCnt ) {
		return STATUS_INVALID_PARAMETER;
	}


	AddressFile->Protocol = LstransList[TransType];

	if(!AddressFile->Protocol->LstransFunc.LstransOpenAddress) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return AddressFile->Protocol->LstransFunc.LstransOpenAddress(
					Address,
					&AddressFile->AddressFileHandle,
					&AddressFile->AddressFileObject
				);
}

NTSTATUS
LstransCloseAddress(
		IN PLSTRANS_ADDRESS_FILE	AddressFile
	) {
	NTSTATUS	status;
	ASSERT(AddressFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!AddressFile->Protocol) {
		KDPrintM(DBG_TRANS_ERROR, ("Protocol NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!AddressFile->Protocol->LstransFunc.LstransCloseAddress) {
		KDPrintM(DBG_TRANS_ERROR, ("LstransCloseAddress not implemented.\n"));
		return STATUS_NOT_IMPLEMENTED;
	}

	status = AddressFile->Protocol->LstransFunc.LstransCloseAddress(
					AddressFile->AddressFileHandle,
					AddressFile->AddressFileObject
				);

	AddressFile->Protocol = NULL;

	return status;
}


NTSTATUS
LstransOpenConnection(
		IN PVOID							ConnectionContext,
		IN USHORT							AddressType,
		IN OUT	PLSTRANS_CONNECTION_FILE	ConnectionFile
	) {
	LSTRANS_TYPE			TransType;
	NTSTATUS				status;

	ASSERT(ConnectionFile);
	ASSERT(!ConnectionFile->Protocol);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	status = LstransAddrTypeToTransType(AddressType, &TransType);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if(TransType < 0 || TransType >= LstransCnt ) {
		return STATUS_INVALID_PARAMETER;
	}

	ConnectionFile->Protocol = LstransList[TransType];

	if(!ConnectionFile->Protocol->LstransFunc.LstransOpenConnection) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransOpenConnection(
					ConnectionContext,
					&ConnectionFile->ConnectionFileHandle,
					&ConnectionFile->ConnectionFileObject
				);
}

NTSTATUS
LstransCloseConnection(
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
	) {
	NTSTATUS	status;

	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!ConnectionFile->Protocol->LstransFunc.LstransCloseConnection) {
		return STATUS_NOT_IMPLEMENTED;
	}

	status = ConnectionFile->Protocol->LstransFunc.LstransCloseConnection(
					ConnectionFile->ConnectionFileHandle,
					ConnectionFile->ConnectionFileObject
				);

	ConnectionFile->Protocol = NULL;

	return status;
}


NTSTATUS
LstransAssociate(
		IN	PLSTRANS_ADDRESS_FILE		AddressFile,
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
) {
	ASSERT(AddressFile);
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!AddressFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("AddressFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!ConnectionFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("ConnectionFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(ConnectionFile->Protocol != AddressFile->Protocol) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!ConnectionFile->Protocol->LstransFunc.LstransAssociate) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransAssociate(
					ConnectionFile->ConnectionFileObject,
					AddressFile->AddressFileHandle
				);
}

NTSTATUS
LstransDisassociate(
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
) {
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		return STATUS_INVALID_PARAMETER;
	}
	if(!ConnectionFile->Protocol->LstransFunc.LstransDisassociate) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransDisassociate(
					ConnectionFile->ConnectionFileObject
				);
}


NTSTATUS
LstransConnect(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN PTA_LSTRANS_ADDRESS		RemoteAddress,
		IN PLARGE_INTEGER			TimeOut
	) {
	ASSERT(ConnectionFile);
	ASSERT(RemoteAddress);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("ConnectionFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!ConnectionFile->Protocol->LstransFunc.LstransConnect) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransConnect(
					ConnectionFile->ConnectionFileObject,
					RemoteAddress,
					TimeOut
				);

}



NTSTATUS
LstransDisconnect(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN ULONG					Flags
	) {
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		return STATUS_INVALID_PARAMETER;
	}
	if(!ConnectionFile->Protocol->LstransFunc.LstransDisconnect) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransDisconnect(
					ConnectionFile->ConnectionFileObject,
					Flags
				);

}

NTSTATUS
LstransListen(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN PVOID					CompletionContext,
		IN PULONG					Flags,
		IN PLARGE_INTEGER			TimeOut
	) {
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("ConnectionFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!ConnectionFile->Protocol->LstransFunc.LstransListen) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransListen(
					ConnectionFile->ConnectionFileObject,
					CompletionContext,
					Flags,
					TimeOut
				);
}


NTSTATUS
LstransSend(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN	PUCHAR					SendBuffer,
		IN 	ULONG					SendLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext,
		IN PLARGE_INTEGER			TimeOut
	) {
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(!ConnectionFile->Protocol->LstransFunc.LstransSend) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransSend(
					ConnectionFile->ConnectionFileObject,
					SendBuffer,
					SendLength,
					Flags,
					Result,
					CompletionContext,
					TimeOut
				);
}


NTSTATUS
LstransReceive(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN	PUCHAR					RecvBuffer,
		IN 	ULONG					RecvLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext,
		IN PLARGE_INTEGER			TimeOut
	) {
	ASSERT(ConnectionFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!ConnectionFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(!ConnectionFile->Protocol->LstransFunc.LstransReceive) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return ConnectionFile->Protocol->LstransFunc.LstransReceive(
					ConnectionFile->ConnectionFileObject,
					RecvBuffer,
					RecvLength,
					Flags,
					Result,
					CompletionContext,
					TimeOut
				);
}



NTSTATUS
LstransSendDatagram(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PTA_LSTRANS_ADDRESS		RemoteAddress,
		IN	PUCHAR					SendBuffer,
		IN 	ULONG					SendLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext
	) {
	ASSERT(AddressFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!AddressFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(!AddressFile->Protocol->LstransFunc.LstransSendDatagram) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return AddressFile->Protocol->LstransFunc.LstransSendDatagram(
					AddressFile->AddressFileObject,
					RemoteAddress,
					SendBuffer,
					SendLength,
					Flags,
					Result,
					CompletionContext
				);
}


NTSTATUS
LstransRegisterRecvDatagramHandler(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PVOID					EventHandler,
		IN	PVOID					EventContext
	) {
	ASSERT(AddressFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!AddressFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("AddressFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!AddressFile->Protocol->LstransFunc.LstransRegisterRcvDgHandler) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return AddressFile->Protocol->LstransFunc.LstransRegisterRcvDgHandler(
					AddressFile->AddressFileObject,
					EventHandler,
					EventContext
				);
}


NTSTATUS
LstransRegisterDisconnectHandler(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PVOID					EventHandler,
		IN	PVOID					EventContext
	) {
	ASSERT(AddressFile);

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(!AddressFile->Protocol) {
		KDPrintM(DBG_TRANS_TRACE, ("AddressFile No protocol.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if(!AddressFile->Protocol->LstransFunc.LstransDisconnect) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return AddressFile->Protocol->LstransFunc.LstransRegisterDisconHandler(
					AddressFile->AddressFileObject,
					EventHandler,
					EventContext
				);
}


NTSTATUS
LstransControl(
		IN	UINT32			Protocol,
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
	    IN	ULONG			OutputBufferLength
	) {

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(Protocol < 0 || Protocol >= LstransCnt ) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LstransList[Protocol]->LstransFunc.LstransControl) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LstransList[Protocol]->LstransFunc.LstransControl(
					IoControlCode,
					InputBuffer,
					InputBufferLength,
					OutputBuffer,
					OutputBufferLength
				);
}


NTSTATUS
LstransQueryBindingDevices(
		IN	UINT32				Protocol,
		IN PTA_LSTRANS_ADDRESS	AddressList
	) {

	KDPrintM(DBG_TRANS_TRACE, ("entered.\n"));

	if(Protocol < 0 || Protocol >= LstransCnt ) {
		return STATUS_INVALID_PARAMETER;
	}

	if(!LstransList[Protocol]->LstransFunc.LstransQueryBindingDevices) {
		return STATUS_NOT_IMPLEMENTED;
	}

	return LstransList[Protocol]->LstransFunc.LstransQueryBindingDevices(
					AddressList
				);
}


