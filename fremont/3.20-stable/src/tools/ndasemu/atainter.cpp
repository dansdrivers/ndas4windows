/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"


extern NDEMU_INTERFACE NdemuAtaDiskInterface;

#ifdef __TMD_SUPPORT__

extern NDEMU_INTERFACE NdemuTMDiskInterface;

PNDEMU_INTERFACE NdemuInterface[NDEMU_MAX_INTERFACE] = {
					&NdemuAtaDiskInterface,
					&NdemuTMDiskInterface};

#else

PNDEMU_INTERFACE NdemuInterface[NDEMU_MAX_INTERFACE] = {
					&NdemuAtaDiskInterface,
					NULL};

#endif


//
//	Retrieve ATA Disk information using WIN_IDENTIFY command
//

BOOL
RetrieveAtaDiskInfo(
	PNDEMU_DEV		EmuDev,
	PATADISK_INFO	DiskInfo,
	ULONG			DiskInfoLen
){
	BOOL		bret;
	ATA_COMMAND	ataCommand;

	memset(&ataCommand, 0, sizeof(ATA_COMMAND));
	ataCommand.DataTransferBuffer = (PUCHAR)DiskInfo;
	ataCommand.BufferLength = DiskInfoLen;
	ataCommand.Command = WIN_IDENTIFY;

	bret = NdemuDevRequest(EmuDev, &ataCommand);
	if(bret == FALSE) {
		return FALSE;
	}
	if(ataCommand.Command & ERR_STAT)
		bret = FALSE;
	else
		bret = TRUE;

	return bret;
}
