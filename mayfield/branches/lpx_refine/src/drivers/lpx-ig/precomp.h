/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    nbf.h

Abstract:

    Private include file for the NBF (NetBIOS Frames Protocol) transport
    provider subcomponent of the NTOS project.

Author:

    Stephen E. Jones (stevej) 25-Oct-1989

Revision History:

    David Beaver (dbeaver) 24-Sep-1990
        Remove PDI and PC586-specific support; add NDIS support

--*/

#include <ntddk.h>
#include <windef.h>
#include <nb30.h>
#include <stdio.h>
#include <ndis.h>                       // Physical Driver Interface.
#include <tdikrnl.h>                        // Transport Driver Interface.


#include "..\inc\socketlpx.h"

#undef TDI_ADDRESS_TYPE_NETBIOS
#define TDI_ADDRESS_TYPE_NETBIOS	TDI_ADDRESS_TYPE_LPX

#undef	TDI_ADDRESS_LENGTH_NETBIOS	
#define	TDI_ADDRESS_LENGTH_NETBIOS	TDI_ADDRESS_LENGTH_LPX



#define ETHERNET_ADDRESS_LENGTH        6
#define ETHERNET_PACKET_LENGTH      1514  // max size of an ethernet packet
#define ETHERNET_HEADER_LENGTH        14  // size of the ethernet MAC header
#define ETHERNET_DATA_LENGTH_OFFSET   12
#define ETHERNET_DESTINATION_OFFSET    0
#define ETHERNET_SOURCE_OFFSET         6

#define TR_ADDRESS_LENGTH        6
#define TR_ADDRESS_OFFSET        2
#define NETBIOS_NAME_LENGTH     16


typedef struct _NBF_NDIS_IDENTIFICATION {
  NDIS_MEDIUM MediumType;
  BOOLEAN SourceRouting;
  BOOLEAN MediumAsync;
  BOOLEAN QueryWithoutSourceRouting;
  BOOLEAN AllRoutesNameRecognized;
  ULONG DestinationOffset;
  ULONG SourceOffset;
  ULONG AddressLength;
  ULONG TransferDataOffset;
  ULONG MaxHeaderLength;
  BOOLEAN CopyLookahead;
  BOOLEAN ReceiveSerialized;
  BOOLEAN TransferSynchronous;
  BOOLEAN SingleReceive;
} NBF_NDIS_IDENTIFICATION, *PNBF_NDIS_IDENTIFICATION;

#define NETBIOS_NAME_SIZE 16

typedef struct _NBF_NETBIOS_ADDRESS {
	UNALIGNED union {
	UNALIGNED struct {
			TDI_ADDRESS_LPX	LpxAddress;
			UCHAR			Reseved[10];
		};
	UNALIGNED struct {
			USHORT  NetbiosNameType;
			UCHAR	NetbiosName[NETBIOS_NAME_SIZE];
		};
	};
} NBF_NETBIOS_ADDRESS, *PNBF_NETBIOS_ADDRESS;



#define LPX_SERVICE_FLAGS  (                            \
                TDI_SERVICE_FORCE_ACCESS_CHECK |        \
                TDI_SERVICE_CONNECTION_MODE |           \
                TDI_SERVICE_CONNECTIONLESS_MODE |       \
                TDI_SERVICE_MESSAGE_MODE |              \
                TDI_SERVICE_ERROR_FREE_DELIVERY |       \
                TDI_SERVICE_BROADCAST_SUPPORTED |       \
                TDI_SERVICE_MULTICAST_SUPPORTED |       \
				TDI_SERVICE_INTERNAL_BUFFERING	|		\
                TDI_SERVICE_DELAYED_ACCEPTANCE  )


#define LPX_TDI_RESOURCES      9

              
#include "LpxCnst.h"
#include "LpxCfg.h" 
#include "LpxDev.h"                   
#include "LpxDrv.h"
#include "Lpx.h"     
#include "LpxAddr.h"
#include "LpxConn.h"
#include "LpxNdis.h"
#include "LpxProtocol.h"





#define ACQUIRE_RESOURCE_EXCLUSIVE(Resource, Wait) \
    KeEnterCriticalRegion(); ExAcquireResourceExclusive(Resource, Wait);
    
#define RELEASE_RESOURCE(Resource) \
    ExReleaseResource(Resource); KeLeaveCriticalRegion();

#define ACQUIRE_FAST_MUTEX_UNSAFE(Mutex) \
    KeEnterCriticalRegion(); ExAcquireFastMutexUnsafe(Mutex);

#define RELEASE_FAST_MUTEX_UNSAFE(Mutex) \
    ExReleaseFastMutexUnsafe(Mutex); KeLeaveCriticalRegion();




#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock) KeAcquireSpinLockAtDpcLevel(lock)
#define RELEASE_DPC_SPIN_LOCK(lock) KeReleaseSpinLockFromDpcLevel(lock)




#define ACQUIRE_C_SPIN_LOCK(lock,irql) ACQUIRE_SPIN_LOCK(lock,irql)
#define RELEASE_C_SPIN_LOCK(lock,irql) RELEASE_SPIN_LOCK(lock,irql)
#define ACQUIRE_DPC_C_SPIN_LOCK(lock) ACQUIRE_DPC_SPIN_LOCK(lock)
#define RELEASE_DPC_C_SPIN_LOCK(lock) RELEASE_DPC_SPIN_LOCK(lock)



