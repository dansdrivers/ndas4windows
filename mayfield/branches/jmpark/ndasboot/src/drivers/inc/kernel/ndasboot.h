#ifndef __NDASBOOT__H
#define __NDASBOOT__H

#include <ntddk.h>
#include <scsi.h>
#include <stdio.h>
#include <stdarg.h>

#include "lanscsi.h"

#define LSMP_PTAG_NDASBOOT	'bnML'

#define __LITTLE_ENDIAN
#define __LITTLE_ENDIAN_BITFIELD

int NDASBootInitialize(void);
int NDASBootDestroy(void);

int NetProtoReInitialize(void);

NTSTATUS
LpxConnect(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PVOID					*pSock
);

NTSTATUS
LpxDisConnect(	
		IN	PVOID					Sock
);

NTSTATUS
LpxReceive(
	   IN	PVOID			Sock,
	   IN	PCHAR			buf, 
	   IN	int				size,
	   IN	ULONG			flags, 
	   IN	PLARGE_INTEGER	TimeOut,
	   OUT	PLONG			result	   
	   );

NTSTATUS
LpxSend(
	   IN	PVOID			Sock,
	   IN	PCHAR			buf, 
	   IN	int				size,
	   IN	ULONG			flags,   
	   OUT	PULONG			sent_len
	   );

PUCHAR GetNICAddr(VOID);

#define STATUS_NIC_OK			0
#define STATUS_NIC_CORRUPTED	1

int GetNICStatus(void);

#endif
