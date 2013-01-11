#ifndef __SOCKETNBFPROC_H__
#define __SOCKETNBFPROC_H__

#if DBG
extern LONG	DebugLevel;
#define DebugPrint(_l_, _x_)			\
		do{								\
			if(_l_ < DebugLevel)		\
				DbgPrint _x_;			\
		}	while(0)					\
		
#else	
#define DebugPrint(_l_, _x_)			\
		do{								\
		} while(0)
#endif

extern PDEVICE_CONTEXT  SocketLpxDeviceContext;
extern PDEVICE_CONTEXT  SocketLpxPrimaryDeviceContext;


VOID
SocketNbfProtocolBindAdapter(
	OUT PNDIS_STATUS	NdisStatus,
    IN  NDIS_HANDLE     BindContext,
    IN  PNDIS_STRING    DeviceName,
    IN	PVOID           SystemSpecific1,
    IN	PVOID           SystemSpecific2
    ); 

VOID
SocketNbfProtocolUnbindAdapter(
	OUT PNDIS_STATUS	NdisStatus,
    IN	NDIS_HANDLE		ProtocolBindContext,
    IN	PNDIS_HANDLE	UnbindContext
    );

PDEVICE_CONTEXT
SocketLpxFindDeviceContext(
    PNBF_NETBIOS_ADDRESS networkName
	);

#if DBG
	
extern ULONG	PacketTxDropRate;
extern ULONG	PacketTxCountForDrop;

extern ULONG	PacketRxDropRate;
extern ULONG	PacketRxCountForDrop;

#endif


#endif //__SOCKETNBF_H__