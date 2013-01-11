#ifndef LFS_DGSVRCLI_H
#define LFS_DGSVRCLI_H


#include "NdftProtocolHeader.h"


//
//	WAITUNIT : 0.1 second
//
#define OBTAINOWN_TIMEOUT	( 20 * WAITUNIT )		// 20	waitunits.
#define MAX_RETRYDELAY			100					// 1 ~ 100 waitunits.
#define OBTAINOWN_MAX_RETRY		10					// 10 times.
#define OBTAINOWN_BCAST_MAX		2					// 2 times
#define DGNOTIFICATION_FREQ		( 1 * TIMERUNIT_SEC)	// 1 seconds.


//
//	Datagram server context
//
typedef struct	_LFSDATAGRAMSVR_CTX	{
	HANDLE			hSvrThread ;
	KEVENT			ShutdownEvent ;
	KEVENT			DatagramRecvEvent ;
	KEVENT			NetworkEvent ;
	ULONG			ConnCount ;
	PLFS_TABLE		LfsTable ;

	LIST_ENTRY		RecvDGPktQueue ;
	KSPIN_LOCK		RecvDGPktQSpinLock ;
} LFSDGRAMSVR_CTX, *PLFSDGRAMSVR_CTX ;


//
//	Datagram client context
//
typedef struct {
	LPX_ADDRESS		LpxAddr ;
	KEVENT			RecvEvent ;
	KEVENT			ShutdownEvent ;
	ULONG			Flags ;
	ULONG			RandomSeed ;
} LFSDGRAMCLI_CTX, *PLFSDGRAMCLI_CTX ;


//
//	Datagram broadcaster context
//
typedef struct	_LFSDATAGRAMNTC_CTX	{
	HANDLE			hThread ;
	KEVENT			ShutdownEvent ;
	LONG			NetEvt ;
	LIST_ENTRY		SendPkts ;
	PLFS_TABLE		LfsTable ;

} LFSDGRAMNTC_CTX, *PLFSDGRAMNTC_CTX ;


VOID
LfsDGInitCliCtx(
		PLFSDGRAMCLI_CTX DatagramCtx,
		PKEVENT			ShutdownEvent
	) ;

#endif
