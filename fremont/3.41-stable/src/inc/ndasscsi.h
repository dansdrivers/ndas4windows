#ifndef __NDAS_SCSI_H__
#define __NDAS_SCSI_H__

#include "socketlpx.h"

typedef struct _TDI_ADDRESS_NDAS {

	union {
		
//		TDI_ADDRESS_IP	Ip;
		TDI_ADDRESS_LPX	Lpx;
	};

} TDI_ADDRESS_NDAS, *PTDI_ADDRESS_NDAS;

#define TDI_ADDRESS_LENGTH_NDAS sizeof (TDI_ADDRESS_NDAS)

C_ASSERT( TDI_ADDRESS_LENGTH_NDAS == 18 );

typedef struct _TA_ADDRESS_NDAS {

	LONG TAAddressCount;

	struct _AddrNdas {

		USHORT				AddressLength;       // length in bytes of this address == 18
        USHORT				AddressType;         // this will == TDI_ADDRESS_TYPE_NETBIOS
        TDI_ADDRESS_NDAS	NdasAddress;
 
	} Address[1];

} TA_NDAS_ADDRESS, *PTA_NDAS_ADDRESS;

#if __NDAS_KLIB__
C_ASSERT( sizeof(struct _AddrNdas) == 22 );
#endif

//	NDAS content encrypt

#define	NDAS_CONTENTENCRYPT_KEY_LENGTH		64		// 64 bytes. 512bits.
#define	NDAS_CONTENTENCRYPT_METHOD_NONE		0
#define	NDAS_CONTENTENCRYPT_METHOD_SIMPLE	1
#define	NDAS_CONTENTENCRYPT_METHOD_AES		2

#include "lurdesc.h"
#include "ndasscsiioctl.h"

#endif
