/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#if _MSC_VER <= 1000
#error "Out of date Compiler"
#endif

#pragma once

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
#include <WinSock2.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <devioctl.h>
#include "..\..\inc\lanscsi.h"
#include "..\..\inc\binparams.h"
#include "..\..\inc\lsprotospec.h"
#include "..\..\inc\lsprotoidespec.h"
#include "..\..\inc\hash.h"
#include "..\..\inc\socketlpx.h"
#include "atainter.h"
#include "ndasemustruc.h"

//
//	Normal user password
//

#define HASH_KEY_USER			0x1F4A50731530EABB


//
//	Super user password
//

#define HASH_KEY_SUPER		0x3e2b321a4750131e


//
//	Port numbers
//

#define	NDASDEV_LISTENPORT_NUMBER	10000
#define	BROADCAST_SOURCEPORT_NUMBER	10001
#define BROADCAST_DESTPORT_NUMBER	(BROADCAST_SOURCEPORT_NUMBER+1)

//////////////////////////////////////////////////////////////////////////
//
//	Debug
//

__inline
void
PrintError(
	int		ErrorCode,
	PCHAR	prefix
){
	LPVOID lpMsgBuf;

	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	fprintf(stderr, "%s: %s", prefix, (LPCSTR)lpMsgBuf);

	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}


//////////////////////////////////////////////////////////////////////////
//
//	Protocol support
//

BOOL
LpxSetDropRate(
	ULONG DropRate
);

int
ReceivePdu(
	IN SOCKET					connSock,
	IN PENCRYPTION_INFO			EncryptInfo,
	IN PNDASDIGEST_INFO			DigestInfo,
	OUT PLANSCSI_PDU_POINTERS	pPdu
);

int
ReceiveBody(
	IN SOCKET			connSock,
	IN PENCRYPTION_INFO	EncryptInfo,
	IN PNDASDIGEST_INFO	digestInfo,
	IN ULONG			DataTransferLength,
	IN ULONG			DataBufferLength,
	OUT PUCHAR			DataBuffer
);

int
SendPdu(
	IN SOCKET					connSock,
	IN PENCRYPTION_INFO			EncryptInfo,
	IN PNDASDIGEST_INFO			DigestInfo,
	IN PLANSCSI_PDU_POINTERS	pPdu
);

int
SendBody(
	IN SOCKET			connSock,
	IN PENCRYPTION_INFO	EncryptInfo,
	IN PNDASDIGEST_INFO	digestInfo,
	IN ULONG			DataTransferLength,
	IN ULONG			DataBufferLength,
	IN PUCHAR			DataBuffer
);

__inline
VOID
ConvertPDUIde2ATACommand(
	IN PLANSCSI_IDE_REQUEST_PDU_HEADER		PduIdeCommand,
	IN PUCHAR								Buffer,		OPTIONAL
	IN LONG									BufferLen,	OPTIONAL
	OUT PATA_COMMAND						AtaCommand
){


	AtaCommand->BufferLength = BufferLen;
	AtaCommand->DataTransferBuffer = Buffer;

	AtaCommand->Feature_Prev = 0;
	AtaCommand->Feature_Curr = PduIdeCommand->Feature;
	AtaCommand->SectorCount_Prev = PduIdeCommand->SectorCount_Prev;
	AtaCommand->SectorCount_Curr = PduIdeCommand->SectorCount_Curr;
	AtaCommand->LBALow_Prev = PduIdeCommand->LBALow_Prev;
	AtaCommand->LBALow_Curr = PduIdeCommand->LBALow_Curr;
	AtaCommand->LBAMid_Prev = PduIdeCommand->LBAMid_Prev;
	AtaCommand->LBAMid_Curr = PduIdeCommand->LBAMid_Curr;
	AtaCommand->LBAHigh_Prev = PduIdeCommand->LBAHigh_Prev;
	AtaCommand->LBAHigh_Curr = PduIdeCommand->LBAHigh_Curr;
	AtaCommand->Command = PduIdeCommand->Command;
	AtaCommand->Device = PduIdeCommand->Device;
}

__inline
VOID
ConvertPDUIde2ATACommandV1(
	IN PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	PduIdeCommand,
	IN PUCHAR								Buffer,		OPTIONAL
	IN LONG									BufferLen,	OPTIONAL
	OUT PATA_COMMAND						AtaCommand
){


	AtaCommand->BufferLength = BufferLen;
	AtaCommand->DataTransferBuffer = Buffer;

	AtaCommand->Feature_Prev = PduIdeCommand->Feature_Prev;
	AtaCommand->Feature_Curr = PduIdeCommand->Feature_Curr;
	AtaCommand->SectorCount_Prev = PduIdeCommand->SectorCount_Prev;
	AtaCommand->SectorCount_Curr = PduIdeCommand->SectorCount_Curr;
	AtaCommand->LBALow_Prev = PduIdeCommand->LBALow_Prev;
	AtaCommand->LBALow_Curr = PduIdeCommand->LBALow_Curr;
	AtaCommand->LBAMid_Prev = PduIdeCommand->LBAMid_Prev;
	AtaCommand->LBAMid_Curr = PduIdeCommand->LBAMid_Curr;
	AtaCommand->LBAHigh_Prev = PduIdeCommand->LBAHigh_Prev;
	AtaCommand->LBAHigh_Curr = PduIdeCommand->LBAHigh_Curr;
	AtaCommand->Command = PduIdeCommand->Command;
	AtaCommand->Device = PduIdeCommand->Device;
}

__inline
VOID
SetPDUIdeResult(
	IN PATA_COMMAND						AtaCommand,
	OUT PLANSCSI_IDE_REQUEST_PDU_HEADER	PduIdeCommand
){

	PduIdeCommand->Feature = AtaCommand->Feature_Curr;
	PduIdeCommand->SectorCount_Prev = AtaCommand->SectorCount_Prev;
	PduIdeCommand->SectorCount_Curr = AtaCommand->SectorCount_Curr;
	PduIdeCommand->LBALow_Prev = AtaCommand->LBALow_Prev;
	PduIdeCommand->LBALow_Curr = AtaCommand->LBALow_Curr;
	PduIdeCommand->LBAMid_Prev = AtaCommand->LBAMid_Prev;
	PduIdeCommand->LBAMid_Curr = AtaCommand->LBAMid_Curr;
	PduIdeCommand->LBAHigh_Prev = AtaCommand->LBAHigh_Prev;
	PduIdeCommand->LBAHigh_Curr = AtaCommand->LBAHigh_Curr;
	PduIdeCommand->Command = AtaCommand->Command;
	PduIdeCommand->Device = AtaCommand->Device;
}

__inline
VOID
SetPDUIdeResultV1(
	IN PATA_COMMAND							AtaCommand,
	OUT PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	PduIdeCommand
){

	PduIdeCommand->Feature_Prev = AtaCommand->Feature_Prev;
	PduIdeCommand->Feature_Curr = AtaCommand->Feature_Curr;
	PduIdeCommand->SectorCount_Prev = AtaCommand->SectorCount_Prev;
	PduIdeCommand->SectorCount_Curr = AtaCommand->SectorCount_Curr;
	PduIdeCommand->LBALow_Prev = AtaCommand->LBALow_Prev;
	PduIdeCommand->LBALow_Curr = AtaCommand->LBALow_Curr;
	PduIdeCommand->LBAMid_Prev = AtaCommand->LBAMid_Prev;
	PduIdeCommand->LBAMid_Curr = AtaCommand->LBAMid_Curr;
	PduIdeCommand->LBAHigh_Prev = AtaCommand->LBAHigh_Prev;
	PduIdeCommand->LBAHigh_Curr = AtaCommand->LBAHigh_Curr;
	PduIdeCommand->Command = AtaCommand->Command;
	PduIdeCommand->Device = AtaCommand->Device;
}


//////////////////////////////////////////////////////////////////////////
//
//	UnitDisk
//

//////////////////////////////////////////////////////////////////////////
//
//	TMD support
//


