/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef __LPXTDIV2PROC_H__
#define __LPXTDIV2PROC_H__

#include "socketlpx.h"

//
// Pool tags
//

#define LPXTDI_COMPBUFF_POOLTAG	'icTL'
#define LPXTDI_TIMEOUT_POOLTAG	'otTL'


//
// Time resolution
//

#define NANO100_PER_SEC			(LONGLONG)(10 * 1000 * 1000)
#define HZ						NANO100_PER_SEC


#define LPXTDI_BYTEPERPACKET	0x4000		// maximum data length in one transaction: 16 KB.


#define	TPADDR_LPX_LENGTH	(FIELD_OFFSET(TRANSPORT_ADDRESS, Address)			\
										+ FIELD_OFFSET(TA_ADDRESS, Address)		\
										+ TDI_ADDRESS_LENGTH_LPX )

#include <tdi.h>
#include <tdikrnl.h>


//
//	Overlapped IO structure
//

typedef struct _LPXTDI_OVERLAPPED LPXTDI_OVERLAPPED, *PLPXTDI_OVERLAPPED;

typedef
VOID
(*PLPXTDI_IOCOMPLETE_ROUTINE) (
	IN PLPXTDI_OVERLAPPED	OverlappedData
	);

typedef struct _LPXTDI_OVERLAPPED {

	IO_STATUS_BLOCK				IoStatusBlock;
	KEVENT						CompletionEvent;

	PLPXTDI_IOCOMPLETE_ROUTINE	IoCompleteRoutine;
	PVOID						UserContext;

	union {

		struct {

			TDI_CONNECTION_INFORMATION  RequestConnectionInfo;
			TDI_CONNECTION_INFORMATION  ReturnConnectionInfo;
		};

		TDI_CONNECTION_INFORMATION	ConnectionInfomation;

		TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	};

	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];

	PIRP						Irp;

} LPXTDI_OVERLAPPED, *PLPXTDI_OVERLAPPED;


NTSTATUS
LpxTdiV2OpenAddress (
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_LPX	LpxAddress
	);


VOID
LpxTdiV2CloseAddress (
	IN HANDLE		AddressFileHandle,
	IN PFILE_OBJECT	AddressFileObject
	);


NTSTATUS
LpxTdiV2OpenConnection (
	OUT PHANDLE			ConnectionFileHandle,
	OUT	PFILE_OBJECT	*ConnectionFileObject,
	IN  PVOID			ConnectionContext
	); 

VOID
LpxTdiV2CloseConnection (
	IN	HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
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
LpxTdiV2SetReceiveDatagramHandler (
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
	);

NTSTATUS
LpxTdiV2Connect (
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX	LpxAddress,
	IN  PLARGE_INTEGER		TimeOut,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	);

VOID
LpxTdiV2Disconnect (
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	);

NTSTATUS
LpxTdiV2OpenControl (
	OUT PHANDLE			ControlFileHandle, 
	OUT	PFILE_OBJECT	*ControlFileObject,
	IN PVOID			ControlContext
	);

VOID
LpxTdiV2CloseControl (
	IN	HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	);

NTSTATUS
LpxTdiV2IoControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
	IN	ULONG    		IoControlCode,
	IN	PVOID    		InputBuffer OPTIONAL,
	IN	ULONG    		InputBufferLength,
	OUT PVOID	    	OutputBuffer OPTIONAL,
	IN	ULONG    		OutputBufferLength
	);


VOID
LpxTdiV2CancelIrpRoutine (
	IN PLPXTDI_OVERLAPPED OverLapped 
	);

VOID
LpxTdiV2CleanupRoutine (
	IN PLPXTDI_OVERLAPPED OverLapped 
	);

NTSTATUS
LpxTdiV2Listen (
	IN  PFILE_OBJECT		ConnectionFileObject,
	IN  PULONG				RequestOptions,
	IN  PULONG				ReturnOptions,
	IN  ULONG				Flags,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PUCHAR				AddressBuffer,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiV2Send (
	IN  PFILE_OBJECT		ConnectionFileObject,
	IN  PUCHAR				SendBuffer,
	IN  ULONG				SendLength,
	IN  ULONG				Flags,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiV2Recv (
	IN	PFILE_OBJECT		ConnectionFileObject,
	OUT	PUCHAR				RecvBuffer,
	IN	ULONG				RecvLength,
	IN	ULONG				Flags,
	IN	PLARGE_INTEGER		TimeOut,
	OUT	PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiV2SendDataGram (
	IN  PFILE_OBJECT		AddressFileObject,
	IN  PLPX_ADDRESS		LpxRemoteAddress,
	IN  PUCHAR				SendBuffer,
	IN  ULONG				SendLength,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiV2GetAddressList (
	IN OUT	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
	) ;

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

#endif