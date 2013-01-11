#ifndef _LAN_SCSI_OP_LIB_H_
#define _LAN_SCSI_OP_LIB_H_

#include <winsock2.h>
#include "lanscsi.h"
#include "binparam.h"
#include "hash.h"
#include "hdreg.h"

#undef	SEC
#define	SEC	1000
#define	RECV_TIME_OUT	SEC * 6		// 6 Sec.

#define	NR_MAX_TARGET	4

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	_int8			NRRWHost;
	_int8			NRROHost;
	unsigned _int64	TargetData;
	
	// IDE Info.
	BOOL			bLBA;
	BOOL			bLBA48;
	unsigned _int64	SectorCount;

	_int8			Model[40];
	_int8			FwRev[8];
	_int8			SerialNo[20];

} TARGET_DATA, *PTARGET_DATA;

typedef struct _LANSCSI_PATH {

	SOCKET					connsock;

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

#ifdef  __cplusplus
}
#endif 

#endif