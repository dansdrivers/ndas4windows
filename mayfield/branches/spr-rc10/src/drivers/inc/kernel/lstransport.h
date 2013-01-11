#ifndef LANSCSI_TRANSPORT_H
#define LANSCSI_TRANSPORT_H

#include "basetsdex.h"
#include "lanscsi.h"


#define LSTRANS_POOLTAG_TA	'atTL'

#define LSTRANS_MAX_BINDADDR	16


//////////////////////////////////////////////////////////////////////////
//
//
//
#define LSTRANS_COPY_LPXADDRESS(PTA_TRANSADDR, PLPXADDRESS) {												\
			(PTA_TRANSADDR)->TAAddressCount = 1;															\
			(PTA_TRANSADDR)->Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;								\
			(PTA_TRANSADDR)->Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;									\
			RtlCopyMemory(&(PTA_TRANSADDR)->Address[0].Address, (PLPXADDRESS), TDI_ADDRESS_LENGTH_LPX); }

#define LSTRANS_COPY_TO_LPXADDRESS(PLPXADDRESS, PTA_TRANSADDR) {											\
			ASSERT((PTA_TRANSADDR)->TAAddressCount == 1);													\
			ASSERT((PTA_TRANSADDR)->Address[0].AddressType == TDI_ADDRESS_TYPE_LPX);						\
			ASSERT((PTA_TRANSADDR)->Address[0].AddressLength == TDI_ADDRESS_LENGTH_LPX);					\
			RtlCopyMemory((PLPXADDRESS), &(PTA_TRANSADDR)->Address[0].Address, TDI_ADDRESS_LENGTH_LPX); }

#define TDI_FAKE_CONTEXT ((PVOID)(-1))

typedef struct _TDI_SEND_RESULT {
	ULONG Retransmits;
} TDI_SEND_RESULT, *PTDI_SEND_RESULT;

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Transport interface
//
#define MAX_LSTRANS_NETWORK_INTERFACE		8

typedef NTSTATUS (*LSTRANS_OPEN_ADDRESS_FUNC)(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PHANDLE			AddressFileHandle,
		OUT	PFILE_OBJECT	*AddressFileObject
	);

typedef NTSTATUS (*LSTRANS_CLOSE_ADDRESS_FUNC)(
		IN HANDLE			AddressFileHandle,
		IN	PFILE_OBJECT	AddressFileObject
	);

typedef NTSTATUS (*LSTRANS_OPEN_CONNECTION_FUNC)(
		IN PVOID			ConnectionContext,
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject
	);

typedef NTSTATUS (*LSTRANS_CLOSE_CONNECTION_FUNC)(
		IN  HANDLE			ConnectionFileHandle,
		IN	PFILE_OBJECT	ConnectionFileObject
	);

typedef NTSTATUS (*LSTRANS_ASSOCIATE_FUNC)(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	HANDLE			AddressFileHandle
	);

typedef NTSTATUS (*LSTRANS_DISASSOCIATE_FUNC)(
		IN	PFILE_OBJECT	ConnectionFileObject
	);

typedef NTSTATUS (*LSTRANS_CONNECT_FUNC)(
		IN	PFILE_OBJECT		ConnectionFileObject,
		IN	PTA_LSTRANS_ADDRESS	TaAddress,
		IN	PLARGE_INTEGER		TimeOut
	);

typedef NTSTATUS (*LSTRANS_DISCONNECT_FUNC)(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	ULONG			Flags
	);

typedef NTSTATUS (*LSTRANS_LISTEN_FUNC)(
		IN	PFILE_OBJECT		ConnectionFileObject,
		IN  PVOID				CompletionContext,
		IN  PULONG				Flags,
		IN	PLARGE_INTEGER		TimeOut
	);

typedef NTSTATUS (*LSTRANS_SEND_FUNC)(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN	PLARGE_INTEGER	TimeOut
	);

typedef NTSTATUS (*LSTRANS_RECEIVE_FUNC)(
		IN	PFILE_OBJECT	ConnectionFileObject,
		OUT	PUCHAR			RecvBuffer,
		IN	ULONG			RecvLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN	PLARGE_INTEGER	TimeOut
	);

typedef NTSTATUS (*LSTRANS_SEND_DATAGRAM_FUNC)(
		IN	PFILE_OBJECT	AddressFileObject,
		PTA_LSTRANS_ADDRESS	RemoteAddress,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext
	);

typedef NTSTATUS (*LSTRANS_REGISTER_RCVDG_HANDLER_FUNC)(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	);

typedef NTSTATUS (*LSTRANS_REGISTER_DISCON_HANDLER_FUNC)(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	);

typedef NTSTATUS (*LSTRANS_CONTROL_FUNC)(
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
	    IN	ULONG			OutputBufferLength
	);

typedef NTSTATUS (*LSTRANS_QUERY_BINDINGDEVICES_FUNC)(
		IN	ULONG				AddressListLen,
		IN	PTA_LSTRANS_ADDRESS	AddressList,
		OUT	PULONG				OutLength
	);

typedef struct _LSTRANS_FUNC {
	//
	//	32 entries
	//
	LSTRANS_OPEN_ADDRESS_FUNC				LstransOpenAddress;
	LSTRANS_CLOSE_ADDRESS_FUNC				LstransCloseAddress;
	LSTRANS_OPEN_CONNECTION_FUNC			LstransOpenConnection;
	LSTRANS_CLOSE_CONNECTION_FUNC			LstransCloseConnection;
	LSTRANS_ASSOCIATE_FUNC					LstransAssociate;
	LSTRANS_DISASSOCIATE_FUNC				LstransDisassociate;
	LSTRANS_CONNECT_FUNC					LstransConnect;
	LSTRANS_DISCONNECT_FUNC					LstransDisconnect;
	LSTRANS_LISTEN_FUNC						LstransListen;
	LSTRANS_SEND_FUNC						LstransSend;
	LSTRANS_RECEIVE_FUNC					LstransReceive;
	LSTRANS_SEND_DATAGRAM_FUNC				LstransSendDatagram;
	LSTRANS_REGISTER_RCVDG_HANDLER_FUNC		LstransRegisterRcvDgHandler;
	LSTRANS_REGISTER_DISCON_HANDLER_FUNC	LstransRegisterDisconHandler;
	LSTRANS_CONTROL_FUNC					LstransControl;
	LSTRANS_QUERY_BINDINGDEVICES_FUNC		LstransQueryBindingDevices;
	PVOID									Reserved[15];
	PVOID									LstransExtension;

} LSTRANS_FUNC, *PLSTRANS_FUNC;


#define LSTRANS_LPX_V1		0x0000
#define LSTRANS_TCP			0x0001

#define LSTRANS_CHAR_WAN	0x0001

typedef	UINT16 LSTRANS_TYPE, *PLSTRANS_TYPE;

typedef struct _LANSCSITRANSPORT_INTERFACE {

	UINT16			Length;
	UINT16			Type;
	UINT16			AddressType;
	UINT16			AddressLength;
	LSTRANS_TYPE	LstransType;
	UINT32			LstransCharacteristic;
	LSTRANS_FUNC	LstransFunc;

} LANSCSITRANSPORT_INTERFACE, *PLANSCSITRANSPORT_INTERFACE;


#define NR_LSTRANS_PROTOCOL		1
#define NR_LSTRANS_MAX_PROTOCOL	16

extern PLANSCSITRANSPORT_INTERFACE	LstransList[NR_LSTRANS_MAX_PROTOCOL];

typedef struct _LSTRANS_ADDRESS_FILE {

	PLANSCSITRANSPORT_INTERFACE	Protocol;
	HANDLE						AddressFileHandle;
	PFILE_OBJECT				AddressFileObject;

} LSTRANS_ADDRESS_FILE, *PLSTRANS_ADDRESS_FILE;


typedef struct _LSTRANS_CONNECTION_FILE {

	PLANSCSITRANSPORT_INTERFACE	Protocol;
	HANDLE						ConnectionFileHandle;
	PFILE_OBJECT				ConnectionFileObject;

} LSTRANS_CONNECTION_FILE, *PLSTRANS_CONNECTION_FILE;


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Transport APIs
//
NTSTATUS
LstransAddrTypeToTransType(
		USHORT			AddrType,
		PLSTRANS_TYPE	Transport
	);

NTSTATUS
LstransGetType(
		PLSTRANS_ADDRESS_FILE	AddressFile,
		PLSTRANS_TYPE			TransType
	);

NTSTATUS
LstransOpenAddress(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PLSTRANS_ADDRESS_FILE	AddressFile
	);


NTSTATUS
LstransCloseAddress(
		IN PLSTRANS_ADDRESS_FILE	AddressFile
	);


NTSTATUS
LstransOpenConnection(
		IN PVOID							ConnectionContext,
		IN USHORT							AddressType,
		IN OUT	PLSTRANS_CONNECTION_FILE	ConnectionFile
	);


NTSTATUS
LstransCloseConnection(
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
	);


NTSTATUS
LstransAssociate(
		IN	PLSTRANS_ADDRESS_FILE		AddressFile,
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
	);


NTSTATUS
LstransDisassociate(
		IN	PLSTRANS_CONNECTION_FILE	ConnectionFile
	);


NTSTATUS
LstransConnect(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN PTA_LSTRANS_ADDRESS		RemoteAddress,
		IN PLARGE_INTEGER			TimeOut
	);


NTSTATUS
LstransDisconnect(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN ULONG					Flags
	);


NTSTATUS
LstransListen(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN PVOID					CompletionContext,
		IN PULONG					Flags,
		IN PLARGE_INTEGER			TimeOut
	);


NTSTATUS
LstransSend(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN	PUCHAR					SendBuffer,
		IN 	ULONG					SendLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext,
		IN PLARGE_INTEGER			TimeOut
	);


NTSTATUS
LstransReceive(
		IN PLSTRANS_CONNECTION_FILE	ConnectionFile,
		IN	PUCHAR					RecvBuffer,
		IN 	ULONG					RecvLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext,
		IN PLARGE_INTEGER			TimeOut
	);


NTSTATUS
LstransSendDatagram(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PTA_LSTRANS_ADDRESS		RemoteAddress,
		IN	PUCHAR					SendBuffer,
		IN 	ULONG					SendLength,
		IN	ULONG					Flags,
		OUT	PLONG					Result,
		IN OUT PVOID				CompletionContext
	);

NTSTATUS
LstransRegisterDisconnectHandler(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PVOID					EventHandler,
		IN	PVOID					EventContext
	);


NTSTATUS
LstransRegisterRecvDatagramHandler(
		IN	PLSTRANS_ADDRESS_FILE	AddressFile,
		IN	PVOID					EventHandler,
		IN	PVOID					EventContext
	);


NTSTATUS
LstransControl(
		IN	UINT32			Protocol,
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
	    IN	ULONG			OutputBufferLength
	);


NTSTATUS
LstransQueryBindingDevices(
		IN	UINT32				Protocol,
		IN ULONG				AddressListLen,
		IN PTA_LSTRANS_ADDRESS	AddressList,
		OUT PULONG				OutLength
	);

PTA_LSTRANS_ADDRESS
LstranAllocateAddr(
		ULONG	AddressCnt,
		PULONG	OutLength
	);

NTSTATUS
LstransIsInAddressList(
	IN PTA_LSTRANS_ADDRESS	AddrList,
	IN PTA_LSTRANS_ADDRESS	Address,
	IN BOOLEAN				InvalidateIfMatch
);

NTSTATUS
LstransWaitForAddress(
	IN PTA_LSTRANS_ADDRESS		Address,
	IN ULONG					MaxWaitLoop
);

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi transport interface for LPX.
//
extern LANSCSITRANSPORT_INTERFACE	LstransLPXV10Interface;

#endif