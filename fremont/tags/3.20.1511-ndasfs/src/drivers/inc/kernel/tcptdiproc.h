#ifndef __TCPTDIPROC_H__
#define __TCPTDIPROC_H__

#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>

//#pragma warning(error:4100)   // Unreferenced formal parameter
//#pragma warning(error:4101)   // Unreferenced local variable

#define DD_TCP_DEVICE_NAME      L"\\Device\\Tcp"
#define TCPTRANSPORT_NAME			DD_TCP_DEVICE_NAME
#define	TCPADDRESS_EA_BUFFER_LENGTH	(								\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)  \
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1				\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)		\
					+ FIELD_OFFSET(TA_ADDRESS, Address)				\
					+ TDI_ADDRESS_LENGTH_IP						\
					)

#define TCPCONNECTION_EA_BUFFER_LENGTH	(								\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)	\
					+ TDI_CONNECTION_CONTEXT_LENGTH + 1				\
					+ sizeof(PVOID)									\
					)

#define	TPADDR_IP_LENGTH	(FIELD_OFFSET(TRANSPORT_ADDRESS, Address)			\
										+ FIELD_OFFSET(TA_ADDRESS, Address)		\
										+ TDI_ADDRESS_LENGTH_IP )


#define TCPTDI_BYTEPERPACKET	0x4000

typedef struct _TCP_TDI_LISTEN_CONTEXT
{
	KEVENT						CompletionEvent;
	TDI_ADDRESS_IP					RemoteAddress;

	TDI_CONNECTION_INFORMATION  RequestConnectionInfo ;
	TDI_CONNECTION_INFORMATION  ReturnConnectionInfo ;
	UCHAR						AddressBuffer[TPADDR_IP_LENGTH];
	
	PIRP						Irp;
	NTSTATUS					Status ;

}TCP_TDI_LISTEN_CONTEXT, *PTCP_TDI_LISTEN_CONTEXT;


typedef struct _TCP_TDI_RECEIVE_CONTEXT
{
	KEVENT	CompletionEvent;
	LONG	Result;
	PIRP	Irp;
	
}TCP_TDI_RECEIVE_CONTEXT, *PTCP_TDI_RECEIVE_CONTEXT;





#if DBG
extern LONG	TCPDebugLevel;
#define TCPLtDebugPrint(l, x)			\
		if(l <= TCPDebugLevel) {		\
			DbgPrint x; 			\
		}
#else	
#define TCPLtDebugPrint(l, x) 
#endif



NTSTATUS
TcpTdiOpenAddress(
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_IP		Address
	);



NTSTATUS
TcpTdiOpenConnection (
	OUT PHANDLE					ConnectionFileHandle, 
	OUT	PFILE_OBJECT			*ConnectionFileObject,
	IN PVOID					ConnectionContext
	);

NTSTATUS
TcpTdiCloseAddress (
	IN HANDLE			AddressFileHandle,
	IN	PFILE_OBJECT	AddressFileObject
	);

NTSTATUS
TcpTdiCloseConnection (
	IN	HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
	);


NTSTATUS
TcpTdiOpenCtrlChannel (
	OUT PHANDLE					CtrlChannelFileHandle, 
	OUT	PFILE_OBJECT			*CtrlChannelFileObject
	);


NTSTATUS
TcpTdiCloseCtrlChannel (
	IN	HANDLE			CtrlChannelFileHandle,
	IN	PFILE_OBJECT	CtrlChannelFileObject
	);

NTSTATUS
TcpTdiIoCallDriver(
    IN		PDEVICE_OBJECT		DeviceObject,
    IN OUT	PIRP				Irp,
	IN		PIO_STATUS_BLOCK	IoStatusBlock,
	IN		PKEVENT				Event
    );

NTSTATUS
TcpTdiConnect(
	IN	PFILE_OBJECT		ControlFileObject,
	IN	PTDI_ADDRESS_IP		Address ,
	OUT PTDI_ADDRESS_IP		RetAddress
	);

NTSTATUS
TcpTdiDisconnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	);

NTSTATUS
TcpTdiAssociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	);

NTSTATUS
TcpTdiDisassociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
TcpTdiSend(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	);

NTSTATUS
TcpTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result
	);

NTSTATUS
TcpSetDisconnectHandler(
						IN	PFILE_OBJECT	AddressFileObject,
						IN	PVOID			InEventHandler,
						IN	PVOID			InEventContext
						);
							
#endif