/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef __LPXTDIV2PROC_H__
#define __LPXTDIV2PROC_H__

#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <ntintsafe.h>

#include "ndascommonheader.h"

#if 0

#define NDAS_ASSERT_INSUFFICIENT_RESOURCES	FALSE
#define NDAS_ASSERT_NETWORK_FAIL			FALSE
#define NDAS_ASSERT_NODE_UNRECHABLE			FALSE
#define NDAS_ASSERT_PAKCET_TEST				FALSE

#else

#define NDAS_ASSERT_INSUFFICIENT_RESOURCES	TRUE
#define NDAS_ASSERT_NETWORK_FAIL			TRUE
#define NDAS_ASSERT_NODE_UNRECHABLE			TRUE
#define NDAS_ASSERT_PAKCET_TEST				TRUE

#endif

extern BOOLEAN NdasTestBug;

#if DBG

#define NDAS_ASSERT(exp)	ASSERT(exp)

#else

#define NDAS_ASSERT(exp)				\
	((NdasTestBug && (exp) == FALSE) ?	\
	NdasDbgBreakPoint() :				\
	FALSE)

#endif

#include "socketlpx.h"

#define	TPADDR_LPX_LENGTH	(FIELD_OFFSET(TRANSPORT_ADDRESS, Address)			\
										+ FIELD_OFFSET(TA_ADDRESS, Address)		\
										+ TDI_ADDRESS_LENGTH_LPX )

//	Overlapped IO structure

#define LPXTDIV2_SANITY_CHECK_TAG	'sXPL'

#define LPXTDIV2_DEFAULT_TIMEOUT	(30 * NANO100_PER_SEC)

#define LPXTDIV2_MAX_REQUEST		1

typedef struct _LPXTDI_OVERLAPPED_CONTEXT LPXTDI_OVERLAPPED_CONTEXT, *PLPXTDI_OVERLAPPED_CONTEXT;

typedef
VOID
(*PLPXTDI_IOCOMPLETE_ROUTINE) (
	IN PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	);

#define LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED		0x00000001
#define LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_MOVED				0x00000002

#define LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG	0x00000010

#define LpxTdiV2IsRequestPending( OverlappedContext, RequestIdx ) \
	(FlagOn((OverlappedContext)->Request[(RequestIdx)].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG))	

typedef struct _LPXTDI_OVERLAPPED_CONTEXT {

	// private member used by lpxtdi

	ULONG						Flags1;

	struct {

		// public member

		// set by user

		PLPXTDI_IOCOMPLETE_ROUTINE	IoCompleteRoutine;
		PVOID						UserContext;

		// set by lpxtdi

		KEVENT						CompletionEvent;

		IO_STATUS_BLOCK				IoStatusBlock;
		UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];

		// private member used by lpxtdi

		ULONG						Flags2;
		PIRP						Irp;

		TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	
	} Request[LPXTDIV2_MAX_REQUEST];

	union {

		struct {

			TDI_CONNECTION_INFORMATION  RequestConnectionInfo;
			TDI_CONNECTION_INFORMATION  ReturnConnectionInfo;
		};

		TDI_CONNECTION_INFORMATION	ConnectionInfomation;
	};

	PVOID	CheckBuffer;	// sanity Check If you call open connection, you must call close connection

} LPXTDI_OVERLAPPED_CONTEXT, *PLPXTDI_OVERLAPPED_CONTEXT;


NTSTATUS
LpxTdiV2OpenAddress (
	IN	PTDI_ADDRESS_LPX	LpxAddress,
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject
	);

VOID
LpxTdiV2CloseAddress (
	IN HANDLE		AddressFileHandle,
	IN PFILE_OBJECT	AddressFileObject
	);

NTSTATUS
LpxTdiV2OpenConnection (
	IN	PVOID						ConnectionContext,
	OUT	PHANDLE						ConnectionFileHandle,
	OUT	PFILE_OBJECT				*ConnectionFileObject,
	OUT PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	); 

VOID
LpxTdiV2CloseConnection (
	IN	HANDLE						ConnectionFileHandle,
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	);

NTSTATUS
LpxTdiV2AssociateAddress (
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	);

VOID
LpxTdiV2DisassociateAddress (
	IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
LpxTdiV2Connect (
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX			LpxAddress,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	);

NTSTATUS
LpxTdiV2Listen (
	IN  PFILE_OBJECT				ConnectionFileObject,
	IN  PULONG						RequestOptions,
	IN  PULONG						ReturnOptions,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	OUT PUCHAR						AddressBuffer
	);

VOID
LpxTdiV2Disconnect (
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	);

NTSTATUS
LpxTdiV2Send (
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
LpxTdiV2Recv (
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
LpxTdiV2SendDatagram (
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
LpxTdiV2RegisterDisconnectHandler (
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
	);

NTSTATUS
LpxTdiV2RegisterRecvDatagramHandler (
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
	);

NTSTATUS
LpxTdiV2QueryInformation (
	IN	PFILE_OBJECT	ConnectionFileObject,
    IN  ULONG			QueryType,
	IN  PVOID			Buffer,
	IN  ULONG			BufferLen
	);

NTSTATUS
LpxTdiV2SetInformation (
	IN	PFILE_OBJECT	ConnectionFileObject,
    IN  ULONG			SetType,
	IN  PVOID			Buffer,
	IN  ULONG			BufferLen
	);

NTSTATUS
LpxTdiV2OpenControl (
	IN  PVOID			ControlContext,
	OUT PHANDLE			ControlFileHandle, 
	OUT	PFILE_OBJECT	*ControlFileObject
	);

VOID
LpxTdiV2CloseControl (
	IN	HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	);

NTSTATUS
LpxTdiV2IoControl (
	IN	PFILE_OBJECT	ControlFileObject,
	IN	ULONG    		IoControlCode,
	IN	PVOID    		InputBuffer OPTIONAL,
	IN	ULONG    		InputBufferLength,
	OUT PVOID	    	OutputBuffer OPTIONAL,
	IN	ULONG    		OutputBufferLength
	);

NTSTATUS
LpxTdiV2GetAddressList (
	IN OUT	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
	);

NTSTATUS
LpxTdiV2GetTransportAddress (
	IN	ULONG				AddressListLen,
	IN	PTRANSPORT_ADDRESS	AddressList,
	OUT	PULONG				OutLength
	); 

VOID
LpxTdiV2CompleteRequest (
	IN PLPXTDI_OVERLAPPED_CONTEXT OverlappedContext, 
	IN	ULONG					  RequestIdx
	);

VOID
LpxTdiV2CancelRequest (
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN	ULONG						RequestIdx,
	IN  BOOLEAN						DisConnect,
	IN	ULONG						DisConnectFlags
	);

NTSTATUS
LpxTdiV2MoveOverlappedContext (
	IN PLPXTDI_OVERLAPPED_CONTEXT DestOverlappedContext,
	IN PLPXTDI_OVERLAPPED_CONTEXT SourceOverlappedContext
	);

#endif