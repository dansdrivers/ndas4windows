#ifndef __LANSCSI_H__
#define __LANSCSI_H__

#include "basetsdex.h"

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi transport address
//
#define LSTRANS_ADDRESS_LENGTH	18	// must be larger than any address type

typedef	struct {
	UCHAR	Address[LSTRANS_ADDRESS_LENGTH];
} TDI_LSTRANS_ADDRESS, *PTDI_LSTRANS_ADDRESS;

typedef struct _TA_LSTRANS_ADDRESS {

	LONG  TAAddressCount;
	struct  _AddrLstrans {
		USHORT					AddressLength;
		USHORT					AddressType;
		TDI_LSTRANS_ADDRESS		Address;
	} Address [1];

} TA_LSTRANS_ADDRESS, *PTA_LSTRANS_ADDRESS;


//
//	NDAS hardware types
//
//#define	NDASHW_NETDISK	0x00000000
//
//	NDAS Hardware versions
//
#define LANSCSITYPE_IDE					0

#define LANSCSIIDE_VERSION_1_0			0
#define LANSCSIIDE_VERSION_1_1			1
#define LANSCSIIDE_VERSION_2_0			2

#define LANSCSIIDE_MAX_LOCK_VER11		4

#define LANSCSIIDE_MAX_CONNECTION_VER11	64

#define LANSCSIIDE_CURRENT_VERSION		LANSCSIIDE_VERSION_2_0

//
//	NDAS content encrypt
//
#define	NDAS_CONTENTENCRYPT_KEY_LENGTH		64		// 64 bytes. 512bits.
#define	NDAS_CONTENTENCRYPT_METHOD_NONE		0
#define	NDAS_CONTENTENCRYPT_METHOD_SIMPLE	1
#define	NDAS_CONTENTENCRYPT_METHOD_AES		2

//
//	Lock reservation for NDAS ver 1.1 ~ 2.0
//
//	Write lock
//	Prevents buffer corruption in NDAS chip when commands embedding data
//	are sent to the NDAS chip simultaneously.
//	Each unit device has different Write lock by adding unit device number
//	to the base lock index below.
//

#define SCSISTRG_WRITE_LOCKIDX_BASE			0


//////////////////////////////////////////////////////////////////////////
//
//	Protocol packet structures.
//
#include "LSProtoSpec.h"
#include "LSProtoIdeSpec.h"


#endif
