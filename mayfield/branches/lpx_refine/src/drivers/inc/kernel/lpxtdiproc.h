/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef __LPXTDIPROC_H__
#define __LPXTDIPROC_H__

#include "socketlpx.h"


#define NANO100_PER_SEC			(LONGLONG)(10 * 1000 * 1000)
#define HZ						NANO100_PER_SEC
//
//	Not true.
//	LPX may not disconnect within 10 seconds.
//
#define TDI_TIME_OUT	(LONGLONG)((LONGLONG)NANO100_PER_SEC * (LONGLONG)100) // 10 Sec. LPX knows connection is Disconnect when 5 sec later.


#define LPXTDI_BYTEPERPACKET	0x4000		// maximum data length in one transaction: 16 KB.

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
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	);

NTSTATUS
LpxTdiSend_TimeOut(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result,
	IN	PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LpxTdiSendEx(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	IN	PKEVENT			CompEvent,
	IN	PLARGE_INTEGER	TimeOut,
	IN OUT PVOID		CompletionContext,
	OUT	PIO_STATUS_BLOCK	IoStatusBlock
	);

NTSTATUS
LpxTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result
	);

NTSTATUS
LpxTdiRecv_TimeOut(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result,
	IN	PLARGE_INTEGER	TimeOut
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

#define	TPADDR_LPX_LENGTH	(FIELD_OFFSET(TRANSPORT_ADDRESS, Address)			\
										+ FIELD_OFFSET(TA_ADDRESS, Address)		\
										+ TDI_ADDRESS_LENGTH_LPX )

#include <tdi.h>
#include <tdikrnl.h>

typedef struct _TDI_LISTEN_CONTEXT
{
	KEVENT						CompletionEvent;
	LPX_ADDRESS					RemoteAddress;

	TDI_CONNECTION_INFORMATION  RequestConnectionInfo ;
	TDI_CONNECTION_INFORMATION  ReturnConnectionInfo ;
	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	
	PIRP						Irp;
	NTSTATUS					Status ;

}TDI_LISTEN_CONTEXT, *PTDI_LISTEN_CONTEXT;

// You must wait Completion Event. If not, It can be crashed !!!

NTSTATUS
LpxTdiListenWithCompletionEvent(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags
	);


typedef struct _TDI_RECEIVE_CONTEXT
{
	KEVENT	CompletionEvent;
	LONG	Result;
	PIRP	Irp;
	
}TDI_RECEIVE_CONTEXT, *PTDI_RECEIVE_CONTEXT;


NTSTATUS
LpxTdiRecvWithCompletionEvent(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN  PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG					Flags
	);

NTSTATUS
LpxTdiRecvWithCompletionEvent_TimeOut(
	IN	PFILE_OBJECT			ConnectionFileObject,
	IN  PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR					RecvBuffer,
	IN	ULONG					RecvLength,
	IN	ULONG					Flags,
	IN	PLARGE_INTEGER			TimeOut
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


#endif