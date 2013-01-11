/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#if _MSC_VER <= 1000
#error "Out of date Compiler"
#endif

#pragma once

#ifdef __TMD_SUPPORT__
#include "tmdemu.h"
#endif


#include "hdreg.h"

typedef struct hd_driveid ATADISK_INFO, *PATADISK_INFO;

//////////////////////////////////////////////////////////////////////////
//
//	Device interface
//
typedef struct _NDEMU_INTERFACE NDEMU_INTERFACE, *PNDEMU_INTERFACE;

typedef PVOID	NDEMU_DEVCTX, *PNDEMU_DEVCTX;

typedef struct _NDEMU_DEV {
		NDEMU_DEVCTX		DevContext;
		struct _NDEMU_DEV *	LowerDevice;
		PNDEMU_INTERFACE	Interface;
	} NDEMU_DEV, *PNDEMU_DEV;

//
//	Device initialization structures
//

typedef PVOID NDEMU_DEV_INIT, *PNDEMU_DEV_INIT;


typedef struct _NDEMU_ATADISK_INIT {

	ULONG			DeviceId;
	ULONGLONG		Capacity;
	UINT16			BytesInBlock;
	UINT16			BytesInBlockBitShift;
	BOOL			UseSparseFile;

} NDEMU_ATADISK_INIT, *PNDEMU_ATADISK_INIT;



typedef struct _ATA_COMMAND {

	//
	//	Buffer
	//

	PUCHAR	DataTransferBuffer;
	ULONG	BufferLength;

	//
	//	Registers
	//

	UINT8	Feature_Prev;
	UINT8	Feature_Curr;			// Error when reply
	UINT8	SectorCount_Prev;
	UINT8	SectorCount_Curr;
	UINT8	LBALow_Prev;
	UINT8	LBALow_Curr;
	UINT8	LBAMid_Prev;
	UINT8	LBAMid_Curr;
	UINT8	LBAHigh_Prev;
	UINT8	LBAHigh_Curr;
	UINT8	Command;				// Status when reply

	union {
		UINT8	Device;
		struct {
			UINT8	LBAHeadNR:4;	// Least 4 bits
			UINT8	DEV:1;
			UINT8	obs1:1;
			UINT8	LBA:1;
			UINT8	obs2:1;
		};
	};
} ATA_COMMAND, *PATA_COMMAND;


typedef BOOL (*NDEMU_INITIALIZE)(IN PNDEMU_DEV EmuDev, IN PNDEMU_DEV LowerDev, IN NDEMU_DEV_INIT EmuDevInit);
typedef BOOL (*NDEMU_DESTROY)(IN PNDEMU_DEV EmuDev);
typedef BOOL (*NDEMU_REQUEST)(IN PNDEMU_DEV EmuDev, IN OUT PATA_COMMAND AtaCommand);


typedef enum _NDEMU_DEVTYPE {
	NDEMU_DEVTYPE_ATADISK = 0,
	NDEMU_DEVTYPE_TMDISK
} NDEMU_DEVTYPE;

typedef struct _NDEMU_INTERFACE {
	NDEMU_DEVTYPE		DevType;
	UINT16				Reserved;

	NDEMU_INITIALIZE	NdemuInitialize;
	NDEMU_DESTROY		NdemuDestroy;
	NDEMU_REQUEST		NdemuRequest;
}NDEMU_INTERFACE, *PNDEMU_INTERFACE;


#define NDEMU_MAX_INTERFACE	2

extern PNDEMU_INTERFACE NdemuInterface[NDEMU_MAX_INTERFACE];



//
//	Interface wrapper
//

__inline
BOOL
NdemuDevInitialize(
		IN PNDEMU_DEV		EmuDev,
		IN PNDEMU_DEV		LowerDev,
		IN NDEMU_DEV_INIT	EmuDevInit,
		IN NDEMU_DEVTYPE	DevType
){

	if(DevType>=NDEMU_MAX_INTERFACE) {
		return FALSE;
	}

	EmuDev->Interface = NdemuInterface[DevType];

	return EmuDev->Interface->NdemuInitialize(EmuDev, LowerDev, EmuDevInit);
}

__inline
BOOL
NdemuDevDestroy(
	IN PNDEMU_DEV EmuDev
){
	if(EmuDev->Interface == NULL) {
		return FALSE;
	}

	return EmuDev->Interface->NdemuDestroy(EmuDev);
}

__inline
BOOL
NdemuDevRequest(
		IN PNDEMU_DEV EmuDev,
		IN OUT PATA_COMMAND AtaCommand
){
	if(EmuDev == NULL)
		return FALSE;
	if(EmuDev->Interface == NULL)
		return FALSE;

	return EmuDev->Interface->NdemuRequest(EmuDev, AtaCommand);
}


BOOL
RetrieveAtaDiskInfo(
	PNDEMU_DEV		EmuDev,
	PATADISK_INFO	DiskInfo,
	ULONG			DiskInfoLen
);
