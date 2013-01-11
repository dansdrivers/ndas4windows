/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef __LPXTDIPROC_H__
#define __LPXTDIPROC_H__

#include "lpxtdiv2.h"

typedef struct _TDI_LISTEN_CONTEXT {

	KEVENT						CompletionEvent;
	LPX_ADDRESS					RemoteAddress;

	TDI_CONNECTION_INFORMATION  RequestConnectionInfo ;
	TDI_CONNECTION_INFORMATION  ReturnConnectionInfo ;
	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	
	PIRP						Irp;
	NTSTATUS					Status ;

} TDI_LISTEN_CONTEXT, *PTDI_LISTEN_CONTEXT;


typedef struct _TDI_RECEIVE_CONTEXT {

	IN  PVOID	SystemUse;		// Do not use. Reserved for LPXTDI.
	IN  KEVENT	CompletionEvent;
	OUT LONG	Result;
	OUT PIRP	Irp;

} TDI_RECEIVE_CONTEXT, *PTDI_RECEIVE_CONTEXT;


NTSTATUS
LpxTdiOpenControl (
	OUT PHANDLE			ControlFileHandle, 
	OUT	PFILE_OBJECT	*ControlFileObject,
	IN PVOID			ControlContext
	);

NTSTATUS
LpxTdiCloseControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	);

NTSTATUS
LpxTdiIoControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
    IN	ULONG			IoControlCode,
    IN	PVOID			InputBuffer OPTIONAL,
    IN	ULONG			InputBufferLength,
    OUT PVOID			OutputBuffer OPTIONAL,
    IN	ULONG			OutputBufferLength
	);

NTSTATUS
LpxTdiOpenAddress(
	OUT	PHANDLE			AddressFileHandle,
	OUT	PFILE_OBJECT	*AddressFileObject,
	IN	PLPX_ADDRESS	lpxAddress
	);

NTSTATUS
LpxTdiCloseAddress (
	IN HANDLE			AddressFileHandle,
	IN	PFILE_OBJECT	AddressFileObject
	);

NTSTATUS
LpxTdiOpenConnection (
	OUT PHANDLE			ConnectionFileHandle, 
	OUT	PFILE_OBJECT	*ConnectionFileObject,
	IN PVOID			ConnectionContext
	);

NTSTATUS
LpxTdiCloseConnection (
	IN  HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
LpxTdiAssociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	);

NTSTATUS
LpxTdiDisassociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
LpxTdiConnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PLPX_ADDRESS	lpxAddress
	);

NTSTATUS
LpxTdiDisconnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	);


NTSTATUS
LpxTdiSend(
	IN PFILE_OBJECT		ConnectionFileObject,
	IN PUCHAR			SendBuffer,
	IN ULONG			SendLength,
	IN ULONG			Flags,
	IN PLARGE_INTEGER	TimeOut,
	IN	PVOID			Reserved,
	OUT PLONG			Result
	);

NTSTATUS
LpxTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLARGE_INTEGER	TimeOut,
	IN	PVOID			Reserved,
	OUT	PLONG			Result
	);

NTSTATUS
LpxSetDisconnectHandler(
						//IN	PFILE_OBJECT	ConnectionFileObject,
						IN	PFILE_OBJECT	AddressFileObject,
						IN	PVOID			InEventHandler,
						IN	PVOID			InEventContext
						);

NTSTATUS
LpxTdiGetAddressList(
		PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
    );


// You must wait Completion Event. If not, It can be crashed !!!

NTSTATUS
LpxTdiListenWithCompletionEvent(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags
	);


NTSTATUS
LpxTdiSendDataGram(
	IN	PFILE_OBJECT	AddressFileObject,
	PLPX_ADDRESS		LpxRemoteAddress,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	) ;


NTSTATUS
LpxTdiSetReceiveDatagramHandler(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	) ;



//
//	OBSOLETE: used only by LfsFilt
//	Receive data asynchronous and set the event to signal at completion
//

NTSTATUS
LpxTdiRecvWithCompletionEvent(
	IN	PFILE_OBJECT			ConnectionFileObject,
	IN  PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR					RecvBuffer,
	IN	ULONG					RecvLength,
	IN	ULONG					Flags,
	IN PVOID					Reserved,
	IN PLARGE_INTEGER			TimeOut
	);


NTSTATUS
LpxTdiQueryInformation(
	IN	PFILE_OBJECT	ConnectionFileObject,
    IN  ULONG			QueryType,
	IN  PVOID			Buffer,
	IN  ULONG			BufferLen
	);

#endif