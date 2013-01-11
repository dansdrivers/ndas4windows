#ifndef _LAN_SCSI_OP_LIB_H_
#define _LAN_SCSI_OP_LIB_H_

#include <winsock2.h>
#include "lanscsi.h"
#include "binparams.h"
#include "hash.h"
#include "hdreg.h"
#include <socketlpx.h>

#undef	SEC
#define	SEC	1000
#define	RECV_TIME_OUT	SEC * 15		// 15 Sec.

#define	NR_MAX_TARGET	4
#define LANSCSI_BLOCK_SIZE				512
#define LANSCSI_MAX_REQUESTBLOCK 128
#define	LANSCSI_MAX_TRANSFER_SIZE		(LANSCSI_MAX_REQUESTBLOCK * LANSCSI_BLOCK_SIZE)
#define LANSCSI_MAX_TRANSFER_BLOCKS		 (LANSCSI_MAX_TRANSFER_SIZE / LANSCSI_BLOCK_SIZE)

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	_int8			NRRWHost;
	_int8			NRROHost;
	unsigned _int64	TargetData;

	// IDE Info.
	BOOL			bLBA;
	BOOL			bLBA48;
	BOOL			bPIO;
	BOOL			bDma;
	BOOL			bUDma;
	unsigned _int64	SectorCount;

	_int8			Model[40];
	_int8			FwRev[8];
	_int8			SerialNo[20];

#define MEDIA_TYPE_UNKNOWN_DEVICE		0		// Unknown(not supported)
#define MEDIA_TYPE_BLOCK_DEVICE			1		// Non-packet mass-storage device (HDD)
#define MEDIA_TYPE_COMPACT_BLOCK_DEVICE 2		// Non-packet compact storage device (Flash card)
#define MEDIA_TYPE_CDROM_DEVICE			3		// CD-ROM device (CD/DVD)
#define MEDIA_TYPE_OPMEM_DEVICE			4		// Optical memory device (MO)

	unsigned _int16			MediaType;
} TARGET_DATA, *PTARGET_DATA;

typedef struct _LANSCSI_PATH {
	SOCKET					connsock;
	LPX_ADDRESS				address; // AING : debugging

	unsigned _int32			HPID;
	unsigned _int16			RPID;
	unsigned _int16			CPSlot;
	unsigned _int16			iSubSequence;
	unsigned _int32			iCommandTag;

	unsigned _int32			CHAP_C;
	unsigned _int8			iSessionPhase;
	unsigned _int64			iPassword;
	unsigned _int32			iUserID;

	unsigned _int8			HWType;
	unsigned _int8			HWVersion;
	unsigned _int8			HWProtoType;
	unsigned _int8			HWProtoVersion;

	unsigned _int32			iNumberofSlot;
	unsigned _int32			iMaxBlocks;
	unsigned _int32			iMaxTargets;
	unsigned _int32			iMaxLUs;
	unsigned _int16			iHeaderEncryptAlgo;
	unsigned _int16			iDataEncryptAlgo;

	unsigned _int8			iNRTargets;
	TARGET_DATA				PerTarget[NR_MAX_TARGET];

} LANSCSI_PATH, *PLANSCSI_PATH;

//
// Prototype
//

#ifdef  __cplusplus
extern "C"
{
#endif

int
Login(
	  PLANSCSI_PATH				pPath,
	  UCHAR						cLoginType
	  );

int
Logout(
	   PLANSCSI_PATH	pPath
	   );

int
TextTargetList(
			   PLANSCSI_PATH	pPath
			   );

int
TextTargetData(
			   PLANSCSI_PATH	pPath,
			   UCHAR			cGetorSet,
			   UINT				TargetID,
			   unsigned _int64	*pData
			   );

int
IdeCommand(
		   PLANSCSI_PATH	pPath,
		   _int32			TargetId,
		   _int64			LUN,
		   UCHAR			Command,
		   _int64			Location,
		   _int16			SectorCount,
		   _int8			Feature,
		   PCHAR			pData,
		   unsigned _int8	*pResponse
		   );

int
Discovery(
		  PLANSCSI_PATH		pPath
		  );

int
GetDiskInfo(
			PLANSCSI_PATH	pPath,
			UINT			TargetId
			);




////////////////////////////////////////////////////////////////////////////
//
//
//			WANScsiOpLiv API
//
//


int
WANConnectNDAS(
		   SOCKET				ControlSock,
		   PLPX_ADDRESS			pNDAS_Address,
		   PLANSCSI_PATH		pPath,
		   int					TargetID,
		   PWANSCSI_COMMANDREP_HEADER pRepHdr
			);

int
WANCloseNDASConnection(
				SOCKET				ControlSock,
				PWANSCSI_COMMANDREP_HEADER	pRepHdr
				);


int
WANLogin(
	  PLANSCSI_PATH					pPath,
	  UCHAR							cLoginType,
	  PWANSCSI_COMMANDREP_HEADER	pRepHdr
	  );

int
WANLogout(
	   PLANSCSI_PATH	pPath,
	   PWANSCSI_COMMANDREP_HEADER			pRepHdr
	   );


int
WANTextTargetData(
			   PLANSCSI_PATH	pPath,
			   UCHAR			cGetorSet,
			   UINT				TargetID,
			   unsigned _int64	*pData,
			   PWANSCSI_COMMANDREP_HEADER			pRepHdr
			   );

int
WANTextTargetList(
			   PLANSCSI_PATH	pPath,
			   PWANSCSI_COMMANDREP_HEADER			pRepHdr
			   );

int
WANIdeCommand(
		   PLANSCSI_PATH	pPath,
		   _int32			TargetId,
		   _int64			LUN,
		   UCHAR			Command,
		   _int64			Location,
		   _int16			SectorCount,
		   _int8			Feature,
		   PCHAR			pData,
		   unsigned _int8	*pResponse,
		   PWANSCSI_COMMANDREP_HEADER		pRepHdr
		   );

int
WANDiscovery(
		  PLANSCSI_PATH		pPath,
		  PWANSCSI_COMMANDREP_HEADER pRephdr
		  );

int
WANGetDiskInfo(
			PLANSCSI_PATH	pPath,
			UINT			TargetId,
			PWANSCSI_COMMANDREP_HEADER pRephdr
			);

BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   IN OUT	PLANSCSI_PATH		pPath
			   );

#ifdef  __cplusplus
}
#endif

#endif
