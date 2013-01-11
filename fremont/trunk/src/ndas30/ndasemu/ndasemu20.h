/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2008, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 12/11/2008
 -------------------------------------------------------------------------
*/

#pragma once

#include "stdafx.h"

//	Normal user password

#define HASH_KEY_USER		0x1F4A50731530EABB

//	Super user password

#define HASH_KEY_SUPER		0x3e2b321a4750131e

typedef	struct _TARGET_DATA {

	BOOL	bPresent;
	BOOL	bLBA;
	BOOL	bLBA48;

	union {

		struct {
		
			UINT8	NRRWHost;
			UINT8	NRROHost;
			UINT16	Reserved1;

		} V1; 	// NDAS 1.0, 1.1, 2.0

		struct {

			UINT8	NREWHost;
			UINT8	NRSWHost;
			UINT8	NRROHost;
			UINT8	Reserved1;

		} V2;	// NDAS 2.5
	};
	
	UINT64	TargetData;
	CHAR	*ExportDev;
	INT		Export;
	
	// IDE Info

	INT64	Size;

	UINT16	pio_mode;
	UINT16	dma_mword;
	UINT16	dma_ultra;
	
} TARGET_DATA, *PTARGET_DATA;

typedef struct _NDASDIGEST_INFO {

	BOOL	HeaderDigestAlgo;
	BOOL	BodyDigestAlgo;

} NDASDIGEST_INFO, *PNDASDIGEST_INFO;

typedef struct	_SESSION_DATA {
	
	SOCKET		connSock;

	UINT16		TargetID;
	UINT32		LUN;
	INT32		iSessionPhase;
	UINT16		CSubPacketSeq;
	UINT8		iLoginType;
	UINT16		CPSlot;
	UINT32		PathCommandTag;
	UINT32		iUser;
	INT32		UserNum;
	INT32		Permission;
	UINT8		Password[16]; // Order is reversed from password in Prom 
	INT32		AccessCountIncreased;
	UINT32		HPID;
	UINT16		RPID;
	UINT64		SessionId;

	UINT32		CHAP_I;
	UINT32		CHAP_C[4]; // MSB??
	
	UCHAR		CryptoPassword8[8];

	UINT8	HostMacAddress[6];

	union {
	
		UINT8	Options;

		struct {

			UINT8 DataEncryption:1;  // Bit 0?
			UINT8 HeaderEncryption:1; 
			UINT8 DataCrc:1;
			UINT8 HeaderCrc:1;
			UINT8 JumboFrame:1;
			UINT8 NoHeartFrame:1;
			UINT8 ReservedOption:2;
		};
	};

	NDASDIGEST_INFO	DigestInfo;

} SESSION_DATA, *PSESSION_DATA;


#include <pshpack1.h>

// This structure is LSB (Little Endian)
// If EEPROM reset is asserted, 
// 0x10~0xaf(MaxConnectionTimeout~UserPasswords) is set to default value.

typedef struct _PROM_DATA_OLD {

	UINT8	EthAddr[6]; 
	UINT8	Signature[2];
	UINT8	Reserved1[8];

	union {

		UINT64	SuperPassword64;
		UCHAR	SuperPassword8[8];
	};

	union {
	
		UINT64	UserPassword64;
		UCHAR	UserPassword8[8];
	};

	union {

		UINT8 Buffer[4];

		struct {

			UINT32	MaxConnTime:9;
			UINT32	MaxRetTime:15;

			// UINT32 EncOption:8;

			UINT32 DataEncryptAlgo		:1;	// Bit 0?
			UINT32 HeaderEncryptAlgo	:1; 
			UINT32 EncOptionReserved	:6; 
		};
	};

	UINT32	StandbyTimeout;

	UINT8	Reserved2[8];

	UINT8	DmaSet[48];

	UINT8	Reserved3[80];

	UINT8	ProductNo[32];

	UINT8	Reserved4[1024-208];

	UINT8	UserArea[1024];

} PROM_DATA_OLD, *PPROM_DATA_OLD;

C_ASSERT( sizeof(PROM_DATA_OLD) == 2048 );

typedef struct _PNP_MESSAGE {

	UINT8	ucType;
	UINT8	ucVersion;

} PNP_MESSAGE, *PPNP_MESSAGE;

#include <poppack.h>

typedef struct _GENERAL_PURPOSE_LOCK {

	BOOL	Acquired;
	ULONG	Counter;
	UINT64	SessionId;

} GENERAL_PURPOSE_LOCK, *PGENERAL_PURPOSE_LOCK;

typedef struct _RAM_DATA_OLD {

	// protect General purpose locks.

	HANDLE					LockMutex;

	// General purpose locks

	GENERAL_PURPOSE_LOCK	GPLocks[4];

} RAM_DATA_OLD, *PRAM_DATA_OLD;
