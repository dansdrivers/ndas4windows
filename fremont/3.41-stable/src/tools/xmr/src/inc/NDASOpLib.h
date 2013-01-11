#ifndef _NDAS_OP_LIB_H_
#define _NDAS_OP_LIB_H_

#include <stdio.h>
#include <WinSock2.h>
#include "../Inc/LanScsi.h"
#include "../Inc/BinaryParameters.h"
#include "../Inc/Hash.h"
#include "../Inc/hdreg.h"
#include "../Inc/SocketLpx.h"
#include "../inc/LanDisk.h"

typedef struct _IDE_COMMAND_IO
{
	unsigned _int8 command;
	signed _int64 iSector;
	_int8 data[512];
} IDE_COMMAND_IO, *PIDE_COMMAND_IO;

#ifdef  __cplusplus
extern "C"
{
#endif 

typedef struct _NDAS_STATUS
{
	// HW
	unsigned _int8 IsAlive;
	unsigned _int8 HWVersion;
	unsigned _int8 HWProtoVersion;
	unsigned _int8 IsDiscovered;

	// registry
	unsigned _int8 IsRegistered;
	unsigned _int8 IsRegisteredWritable;

	// usage
	unsigned _int8 NrUserReadOnly;
	unsigned _int8 NrUserReadWrite;

	// XArea
	unsigned _int32 MajorVersion; // 0.1 means V1
	unsigned _int32 MinorVersion;
	unsigned _int8 IsSupported;
	unsigned _int8 DiskType;
} NDAS_STATUS, *PNDAS_STATUS;

BOOL NDAS_GetStatus(UNIT_DISK_LOCATION *pUnitDisk, PNDAS_STATUS pStatus);

BOOL NDAS_ClearInfo(UNIT_DISK_LOCATION *pUnitDisk);

BOOL NDAS_SetPassword(unsigned char *szAddress, unsigned _int64 *piPassword);
BOOL NDAS_DisabledByUser(UNIT_DISK_LOCATION *pUnitDisk, int iDisabled);
BOOL NDAS_Bind(UINT nDiskCount, UNIT_DISK_LOCATION *aUnitDisks, int iMirrorLevel);
BOOL NDAS_Unbind(UNIT_DISK_LOCATION *pUnitDisk);
BOOL NDAS_IsDirty(UNIT_DISK_LOCATION *pUnitDisk, unsigned _int32 *pFlagDirty);

#ifdef  __cplusplus
}
#endif 

#endif