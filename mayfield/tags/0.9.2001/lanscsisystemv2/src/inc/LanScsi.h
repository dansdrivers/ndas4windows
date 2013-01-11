#ifndef __LANSCSI_H__
#define __LANSCSI_H__

#include "basetsdex.h"

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi transport address
//
#define LSTRANS_ADDRESS_LENGTH	18	// must be larger than any address type

typedef struct _TA_ADDRESS_LSTRNAS {

	LONG  TAAddressCount;
	struct  _AddrLstrans {
		USHORT					AddressLength;
		USHORT					AddressType;
		UCHAR					Address[LSTRANS_ADDRESS_LENGTH];
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

#define LANSCSIIDE_CURRENT_VERSION		LANSCSIIDE_VERSION_2_0


//////////////////////////////////////////////////////////////////////////
//
//	Protocol packet structures.
//
#include "LSProtoSpec.h"
#include "LSProtoIdeSpec.h"


#endif
