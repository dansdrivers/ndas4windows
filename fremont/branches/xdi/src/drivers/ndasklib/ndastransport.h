#ifndef NDAS_TRANSPORT_H
#define NDAS_TRANSPORT_H


#define LSSTRUC_TYPE_TRANSPORT_INTERFACE	0x0008


#define NDAS_TRANS_POOLTAG_TA			'atTL'
#define NDAS_TRANS_POOLTAG_TIMEOUT		'otTL'

#define NDAS_TRANS_MAX_BINDADDR	16

#define LSTRANS_ADDRESS_LENGTH	18	// must be larger than any address type

typedef	struct {
	UCHAR	Address[LSTRANS_ADDRESS_LENGTH];
} TDI_NDAS_TRANS_ADDRESS, *PTDI_NDAS_TRANS_ADDRESS;

typedef struct _TA_NDAS_TRANS_ADDRESS {

	LONG  TAAddressCount;
	struct  _AddrNdasTrans {
		USHORT					AddressLength;
		USHORT					AddressType;
		TDI_NDAS_TRANS_ADDRESS		Address;
	} Address [1];

} TA_NDAS_TRANS_ADDRESS, *PTA_NDAS_TRANS_ADDRESS;


//////////////////////////////////////////////////////////////////////////
//
//
//
#define NDAS_TRANS_COPY_LPXADDRESS(PTA_TRANSADDR, PLPXADDRESS) {												\
			(PTA_TRANSADDR)->TAAddressCount = 1;															\
			(PTA_TRANSADDR)->Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;								\
			(PTA_TRANSADDR)->Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;									\
			RtlCopyMemory(&(PTA_TRANSADDR)->Address[0].Address, (PLPXADDRESS), TDI_ADDRESS_LENGTH_LPX); }

#define NDAS_TRANS_COPY_TO_LPXADDRESS(PLPXADDRESS, PTA_TRANSADDR) {											\
			ASSERT((PTA_TRANSADDR)->TAAddressCount == 1);													\
			ASSERT((PTA_TRANSADDR)->Address[0].AddressType == TDI_ADDRESS_TYPE_LPX);						\
			ASSERT((PTA_TRANSADDR)->Address[0].AddressLength == TDI_ADDRESS_LENGTH_LPX);					\
			RtlCopyMemory((PLPXADDRESS), &(PTA_TRANSADDR)->Address[0].Address, TDI_ADDRESS_LENGTH_LPX); }

#define TDI_FAKE_CONTEXT ((PVOID)(-1))


//
// Get statistics about protocol operation. 
// Currently get information about packet retransmission or loss during operation.
// Should be synchronized with TRANS_STAT in socketlpx.h
//
typedef struct _NDAS_TRANS_STAT {
    ULONG Retransmits;
    ULONG PacketLoss;
} NDAS_TRANS_STAT, *PNDAS_TRANS_STAT;


//
// Timeout support
//

typedef struct _NDAS_TRANS_TIMEOUT_TIMER {

	LONG			ReferenceCount;
	KTIMER			Timer;
	KDPC			TimerDpc;
	PIRP			Irp;
	PKTHREAD		TimerDpcThread;
	KSPIN_LOCK		TimerSpinlock;

} NDAS_TRANS_TIMEOUT_TIMER, *PNDAS_TRANS_TIMEOUT_TIMER;


//
//	Overlapped IO structure
//

typedef struct _NDAS_TRANS_OVERLAPPED NDAS_TRANS_OVERLAPPED, *PNDAS_TRANS_OVERLAPPED;

typedef
VOID
(*PNDAS_TRANS_IOCOMPLETE_ROUTINE)(	IN PNDAS_TRANS_OVERLAPPED	OverlappedData);

typedef struct _NDAS_TRANS_OVERLAPPED {

	PVOID						SystemUse;			// Do not use.
													// Reserved for protocol TDI interface
	PNDAS_TRANS_TIMEOUT_TIMER		TimeoutTimer;		// Reserved for TDI.
	PNDAS_TRANS_IOCOMPLETE_ROUTINE	IoCompleteRoutine;
	PVOID						UserContext;
	IO_STATUS_BLOCK				IoStatusBlock;
	PKEVENT						CompletionEvent;

} NDAS_TRANS_OVERLAPPED, *PNDAS_TRANS_OVERLAPPED;


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Transport interface
//
#define MAX_NDAS_TRANS_NETWORK_INTERFACE		8


typedef struct _NDAS_TRANS_FUNC {
	//
	//	32 entries
	//

	NTSTATUS
	(*OpenAddress) (
		IN	PTDI_ADDRESS_LPX	LpxAddress,
		OUT	PHANDLE				AddressFileHandle,
		OUT	PFILE_OBJECT		*AddressFileObject
		);

	VOID
	(*CloseAddress) (
		IN HANDLE		AddressFileHandle,
		IN PFILE_OBJECT	AddressFileObject
		);

	NTSTATUS
	(*OpenConnection) (
		IN	PVOID						ConnectionContext,
		OUT	PHANDLE						ConnectionFileHandle,
		OUT	PFILE_OBJECT				*ConnectionFileObject,
		OUT PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
		); 

	VOID
	(*CloseConnection) (
		IN	HANDLE						ConnectionFileHandle,
		IN	PFILE_OBJECT				ConnectionFileObject,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
		);

	NTSTATUS
	(*AssociateAddress) (
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	HANDLE			AddressFileHandle
		);

	VOID
	(*DisassociateAddress) (
		IN	PFILE_OBJECT	ConnectionFileObject
		);

	NTSTATUS
	(*Connection) (
		IN	PFILE_OBJECT				ConnectionFileObject,
		IN	PTDI_ADDRESS_LPX			LpxAddress,
		IN  PLARGE_INTEGER				TimeOut,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
		);

	NTSTATUS
	(*Listen) (
		IN  PFILE_OBJECT				ConnectionFileObject,
		IN  PULONG						RequestOptions,
		IN  PULONG						ReturnOptions,
		IN  ULONG						Flags,
		IN  PLARGE_INTEGER				TimeOut,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
		OUT PUCHAR						AddressBuffer
		);

	VOID
	(*Disconnect) (
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	ULONG			Flags
		);

	NTSTATUS
	(*Send) (
		IN  PFILE_OBJECT				ConnectionFileObject,
		IN  PUCHAR						SendBuffer,
		IN  ULONG						SendLength,
		IN  ULONG						Flags,
		IN  PLARGE_INTEGER				TimeOut,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
		IN  ULONG						RequestIdx,
		OUT PLONG						Result
		);

	NTSTATUS
	(*Recv) (
		IN	PFILE_OBJECT				ConnectionFileObject,
		IN	PUCHAR						RecvBuffer,
		IN	ULONG						RecvLength,
		IN	ULONG						Flags,
		IN	PLARGE_INTEGER				TimeOut,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
		IN  ULONG						RequestIdx,
		OUT	PLONG						Result
		);

	NTSTATUS
	(*SendDatagram) (
		IN  PFILE_OBJECT				AddressFileObject,
		IN  PLPX_ADDRESS				LpxRemoteAddress,
		IN  PUCHAR						SendBuffer,
		IN  ULONG						SendLength,
		IN  PLARGE_INTEGER				TimeOut,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
		IN  ULONG						RequestIdx,
		OUT PLONG						Result
		);

	NTSTATUS
	(*RegisterDisconnectHandler) (
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
		);

	NTSTATUS
	(*RegisterRecvDatagramHandler) (
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
		);

	NTSTATUS
	(*QueryInformation) (
		IN	PFILE_OBJECT	ConnectionFileObject,
	    IN  ULONG			QueryType,
		IN  PVOID			Buffer,
		IN  ULONG			BufferLen
		);

	NTSTATUS
	(*SetInformation) (
		IN	PFILE_OBJECT	ConnectionFileObject,
	    IN  ULONG			SetType,
		IN  PVOID			Buffer,
		IN  ULONG			BufferLen
		);

	NTSTATUS
	(*OpenControl) (
		IN PVOID			ControlContext,
		OUT PHANDLE			ControlFileHandle, 
		OUT	PFILE_OBJECT	*ControlFileObject
		);

	VOID
	(*CloseControl) (
		IN	HANDLE			ControlFileHandle,
		IN	PFILE_OBJECT	ControlFileObject
		);

	NTSTATUS
	(*IoControl) (
		IN  HANDLE			ControlFileHandle,
		IN	PFILE_OBJECT	ControlFileObject,
		IN	ULONG    		IoControlCode,
		IN	PVOID    		InputBuffer OPTIONAL,
		IN	ULONG    		InputBufferLength,
		OUT PVOID	    	OutputBuffer OPTIONAL,
		IN	ULONG    		OutputBufferLength
		);

	NTSTATUS
	(*GetAddressList) (
		IN OUT	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
		);

	NTSTATUS
	(*GetTransportAddress) (
		IN	ULONG				AddressListLen,
		IN	PTRANSPORT_ADDRESS	AddressList,
		OUT	PULONG				OutLength
		); 

	VOID
	(*CompleteRequest) (
		IN PLPXTDI_OVERLAPPED_CONTEXT OverlappedContext, 
		IN	ULONG					  RequestIdx
		);

	VOID
	(*CancelRequest) (
		IN	PFILE_OBJECT				ConnectionFileObject,
		IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
		IN	ULONG						RequestIdx,
		IN  BOOLEAN						DisConnect,
		IN	ULONG						DisConnectFlags
		);

	NTSTATUS
	(*MoveOverlappedContext) (
		IN PLPXTDI_OVERLAPPED_CONTEXT DestOverlappedContext,
		IN PLPXTDI_OVERLAPPED_CONTEXT SourceOverlappedContext
		);

	PVOID									Reserved[15];
	PVOID									NdasTransExtension;

} NDAS_TRANS_FUNC, *PNDAS_TRANS_FUNC;


#define NDAS_TRANS_LPX_V1		0x0000
#define NDAS_TRANS_TCP			0x0001

#define NDAS_TRANS_CHAR_WAN	0x0001

typedef	UINT16 NDAS_TRANS_TYPE, *PNDAS_TRANS_TYPE;

typedef struct _NDAS_TRANSPORT_INTERFACE {

	UINT16			Length;
	UINT16			Type;
	UINT16			AddressType;
	UINT16			AddressLength;
	NDAS_TRANS_TYPE	NdasTransType;
	UINT32			NdasTransCharacteristic;
	NDAS_TRANS_FUNC	NdasTransFunc;

} NDAS_TRANSPORT_INTERFACE, *PNDAS_TRANSPORT_INTERFACE;


#define NR_NDAS_TRANS_PROTOCOL		1
#define NR_NDAS_TRANS_MAX_PROTOCOL	16

extern PNDAS_TRANSPORT_INTERFACE	NdasTransList[NR_NDAS_TRANS_MAX_PROTOCOL];



NTSTATUS
NdasTransOpenAddress (
	IN PTA_ADDRESS				TransportAddress,
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile
	); 

VOID
NdasTransCloseAddress (
	IN PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile
	); 

NTSTATUS
NdasTransOpenConnection (
	IN   PVOID							ConnectionContext,
	IN	 USHORT							AddressType,
	OUT  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	); 

VOID
NdasTransCloseConnection (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	);

NTSTATUS
NdasTransAssociate (
	IN PNDAS_TRANS_ADDRESS_FILE		NdastAddressFile,
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	); 

VOID
NdasTransDisassociateAddress (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile
	);

NTSTATUS
NdasTransConnect (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PTA_ADDRESS					RemoteAddress,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	); 

NTSTATUS
NdasTransListen (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PULONG						RequestOptions,
	IN  PULONG						ReturnOptions,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				TimeOut,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	OUT PUCHAR						AddressBuffer
	);

VOID
NdasTransDisconnect (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  ULONG						Flags
	); 

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
	); 

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
	);

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
	); 

NTSTATUS
NdasTransRegisterDisconnectHandler (
	IN  PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN	PVOID						InEventHandler,
	IN	PVOID						InEventContext
	); 

NTSTATUS
NdasTransRegisterRecvDatagramHandler (
	IN  PNDAS_TRANS_ADDRESS_FILE	NdastAddressFile,
	IN	PVOID						InEventHandler,
	IN	PVOID						InEventContext
	); 

NTSTATUS
NdasTransQueryInformation (
	IN  PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  ULONG						QueryType,
	IN  PVOID						Buffer,
	IN  ULONG						BufferLen
	);

NTSTATUS
NdasTransSetInformation (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN ULONG						SetType,
	IN  PVOID						Buffer,
	IN  ULONG						BufferLen
	);

NTSTATUS
NdasTransGetTransportAddress (
	IN USHORT				AddressType,
	IN	ULONG				AddressListLen,
	IN	PTRANSPORT_ADDRESS	AddressList,
	OUT	PULONG				OutLength
	);

VOID
NdasTransCompleteRequest (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext, 
	IN ULONG						RequestIdx
	);

VOID
NdasTransCancelRequest (
	IN PNDAS_TRANS_CONNECTION_FILE	NdastConnectionFile,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN	ULONG						RequestIdx,
	IN  BOOLEAN						DisConnect,
	IN	ULONG						DisConnectFlags
	); 

PTRANSPORT_ADDRESS
NdasTransAllocateAddr (
	USHORT	AddressType,
	ULONG	AddressCnt,
	PULONG	Length
	);

NTSTATUS
NdasTransIsInAddressList (
	IN PTRANSPORT_ADDRESS	AddrList,
	IN PTA_ADDRESS			Address,
	IN BOOLEAN				InvalidateIfMatch
	);	

#endif
