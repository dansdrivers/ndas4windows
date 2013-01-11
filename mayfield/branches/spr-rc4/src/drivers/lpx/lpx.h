/*++

Copyright (c) 2000-2003	XIMETA Corp.

Module Name:

    Lpx.h

Abstract:

    About LPX protocol.

Revision History:

--*/

#ifndef _LPX_H_
#define _LPX_H_

//
//  Enable these warnings in the code.
//
#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


#define  TRANSMIT_PACKETS	1024
#define LPX_PORTASSIGN_BEGIN   0x4000
#define PNPMOUDLEPORT		10002
#define LENGTH_8022LLCSNAP  8
#define MAX_ALLOWED_SEQ		1024
// Mod by jgahn. 01/29/2003
// Since IEEE assigned our type #, Change Ether Type from 0x8200 to 0x88AD.
#define ETH_P_LPX			0x88AD		// 0x8200	




// Mod by jgahn.
#undef	NdisAllocateMemory
#define	NdisAllocateMemory(a,b) NdisAllocateMemoryWithTag(a,b,'-xpL')

#undef	NdisFreeMemory
#define NdisFreeMemory(a) NdisFreeMemory(a, 0, 0)

#define SHORT_SEQNUM(SEQNUM) ((USHORT)(SEQNUM))

typedef enum _SMP_STATE {
  SMP_ESTABLISHED = 1,
  SMP_SYN_SENT,
  SMP_SYN_RECV,
  SMP_FIN_WAIT1,
  SMP_FIN_WAIT2,
  SMP_TIME_WAIT,
  SMP_CLOSE,
  SMP_CLOSE_WAIT,
  SMP_LAST_ACK,
  SMP_LISTEN,
  SMP_CLOSING	 /* now a valid state */
} SMP_STATE, *PSMP_STATE;


typedef struct _SMP_CONTEXT {
	struct _SERVICE_POINT	*ServicePoint;

//
//	use ULONG type instead of USHORT to call InterlockedXXXX() with
//	hootch 09062003
//
/*
	USHORT			Sequence;
	USHORT			FinSequence;

	USHORT			RemoteSequence;
	USHORT			RemoteAck;

	USHORT			Allocate;
	USHORT			RemoteAllocate;
*/
	LONG			Sequence;
	LONG			FinSequence;
	LONG			RemoteSequence;
	LONG			RemoteAck;		// used only in SmpDoReceive() and SendTest()

//
//	OBSOLUTE
//	removed by hootch 09062003
//	LONG			Allocate;
//	LONG			RemoteAllocate;
//	USHORT			SourceId;
//	USHORT			DestionationId;

//	BOOLEAN			SmpTimerSet;
	KTIMER			SmpTimer;
	KDPC			SmpTimerDpc;

	LONG			TimerReason;
#define	SMP_SENDIBLE			0x0001
#define	SMP_RETRANSMIT_ACKED	0x0002
#define	SMP_DELAYED_ACK			0x0004

	//
	//	timeout counters
	//

	//	protect following timeout counters.
	// added by hootch	09062003
	KSPIN_LOCK		TimeCounterSpinLock ;
	// ILGU for Debugging
	LARGE_INTEGER	Lsmptimercall;
	LARGE_INTEGER	WorkDpcCall;
	LONG			TimeOutCount;
	LARGE_INTEGER	TimeWaitTimeOut;
	LARGE_INTEGER	AliveTimeOut;
	LARGE_INTEGER	RetransmitTimeOut;
	LARGE_INTEGER	ConnectTimeOut;
	LARGE_INTEGER	SmpTimerExpire;
	// Added by jgahn.
	LARGE_INTEGER	RetransmitEndTime;	// End of Retransmit...

	LONG			LastRetransmitSequence;

	LONG			AliveRetries;
	LONG			Retransmits;
	
	LONG			MaxFlights;
#define SMP_MAX_FLIGHT 2048

	//
	//	queues
	//
	KSPIN_LOCK		RcvDataQSpinLock;		// added by hootch	09062003
	LIST_ENTRY		RcvDataQueue;

	KSPIN_LOCK		RetransmitQSpinLock;	// added by hootch	09062003
	LIST_ENTRY		RetransmitQueue;

	KSPIN_LOCK		WriteQSpinLock;		// added by hootch	09062003
	LIST_ENTRY		WriteQueue;

//
//	OBSOULTE
//	not use now.
//	removed by hootch 09062003
//
//	LARGE_INTEGER	LatestSendTime;
//	LARGE_INTEGER	IntervalTime;
//#define INITIAL_INTERVAL_TIME			(HZ/1)

	UCHAR			ServerTag;

	//
	//	DPC to do actual jobs instead of ReceiveCompeletion & Timer
	//
	//	added by hootch 09072003 to build a DPC worker
	KDPC			SmpWorkDpc;
	LONG			RequestCnt ;

	KSPIN_LOCK		SendQSpinLock;
	LIST_ENTRY		SendQueue;

	KSPIN_LOCK		ReceiveQSpinLock;
	LIST_ENTRY		ReceiveQueue;

} SMP_CONTEXT, *PSMP_CONTEXT;

typedef struct _SERVICE_POINT {
	KSPIN_LOCK				SpinLock ;	// added by hootch 09072003

	SMP_STATE				SmpState;

#define	SMP_RECEIVE_SHUTDOWN	0x01
#define	SMP_SEND_SHUTDOWN		0x02
#define	SMP_SHUTDOWN_MASK		0x03
	UCHAR					Shutdown;

	//
	//	ServicePointListEntry is protected by address->SpinLock
	//
	LIST_ENTRY				ServicePointListEntry;

//	removed by hootch 0906
//    KSPIN_LOCK				ServicePointQSpinLock;
//	
	PIRP					DisconnectIrp;
	PIRP					ConnectIrp;
	PIRP					ListenIrp;

	LIST_ENTRY				ReceiveIrpList;
    KSPIN_LOCK				ReceiveIrpQSpinLock;

	LPX_ADDRESS				SourceAddress;
	LPX_ADDRESS				DestinationAddress;

	struct _TP_ADDRESS		*Address;

	struct _SMP_CONTEXT		SmpContext;
	struct _TP_CONNECTION	*Connection;

	// 052303 jgahn
	LONG					lDisconnectHandlerCalled;
} SERVICE_POINT, *PSERVICE_POINT;

typedef UNALIGNED struct __ETHERNET_HEADER
{
	UCHAR	DestinationAddress[ETHERNET_ADDRESS_LENGTH];
	UCHAR	SourceAddress[ETHERNET_ADDRESS_LENGTH];
	USHORT	Type;

} ETHERNET_HEADER, *PETHERNET_HEADER;

typedef /*UNALIGNED*/ struct _LPX_RESERVED 
{
#define SEND_TYPE				0
#define	RECEIVE_TYPE			1
//#define	DIRECT_RECEIVE_TYPE		2
	UCHAR		Type;
	LIST_ENTRY	ListElement;
	LIST_ENTRY	LoopbackLinkage;

	union {
//		PVOID		LpxHeader;
		PVOID		LpxSmpHeader;
	};
	USHORT		DataLength;
	
	union {
		struct {
			#define	EXPORTED	0x1
			#define RECEIVING	0x2
			#define CONSUMED	0x4
			UCHAR			Exported;
			UINT			PacketDataOffset;
		};// for ReceivePacketHandler;
		struct {
//			UCHAR			Cloned;
			LONG			Cloned;	// mod by hootch 09052003
//			PSERVICE_POINT	ServicePoint;
		    PIO_STACK_LOCATION	IrpSp;
			ETHERNET_HEADER		EthernetHeader;
			LONG			Retransmits;

			// Added by jaghn.
			PVOID				pSmpContext;	// For Send Packet.
		}; // for Packet Send and ReceiveHandler
	};
}  LPX_RESERVED, *PLPX_RESERVED;

#define  RESERVED(_p) ((PLPX_RESERVED)((_p)->ProtocolReserved))

typedef enum _PACKET_TYPE {
	DATA,
	ACK,
	CONREQ,
	DISCON,
	RETRAN,
	ACKREQ
} PACKET_TYPE;

#define HZ				(LONGLONG)(10 * 1000 * 1000)
#define MAX_RECONNECTION_INTERVAL	(HZ/1)
#define MAX_CONNECT_TIME		(2*HZ)

#define SMP_TIMEOUT				(HZ/20)	// 50 ms
#define SMP_DESTROY_TIMEOUT		(2*HZ)	// 2 sec
#define TIME_WAIT_INTERVAL		(HZ)	// 1 sec
#define ALIVE_INTERVAL  		(HZ)	// 1 sec

#define RETRANSMIT_TIME			(HZ/5)	// 200 ms
#define MAX_RETRANSMIT_DELAY	(HZ/1)	// 1 sec

#define MAX_ALIVE_COUNT     	(8 * HZ/ALIVE_INTERVAL)		
#define MAX_RETRANSMIT_TIME		(8 * HZ)	
#define MAX_RETRANSMIT_COUNT    (MAX_RETRANSMIT_TIME/MAX_RETRANSMIT_DELAY)

//#define MAX_RECVIRP_TIMEOUT		(MAX_RETRANSMIT_TIME)

#define LPX_HOST_DGMSG_ID		0

/*
//
// For 860board.
//
#define MAX_CONNECT_TIME		(5*HZ)

#define SMP_TIMEOUT				(HZ/5)
#define SMP_DESTROY_TIMEOUT		(2*HZ)
#define TIME_WAIT_INTERVAL		(HZ)
#define ALIVE_INTERVAL  		(HZ/2)

//#define RETRANSMIT_TIME		(HZ/1)
#define MAX_RETRANSMIT_DELAY	(2*HZ)

#define MAX_ALIVE_COUNT     	(20*HZ/ALIVE_INTERVAL)
#define MAX_RETRANSMIT_COUNT    (10*HZ/MAX_RETRANSMIT_DELAY)
*/

#include <pshpack1.h>

typedef UNALIGNED struct _LPX_HEADER2 
{
	union {
		struct {
			UCHAR	PacketSizeMsb:6;
			UCHAR	LpxType:2;
#define LPX_TYPE_RAW		0
#define LPX_TYPE_DATAGRAM	2
#define LPX_TYPE_STREAM		3
#define LPX_TYPE_MASK		(USHORT)0x00C0
			UCHAR	PacketSizeLsb;
		};
		USHORT		PacketSize;
	};

	USHORT	DestinationPort;
	USHORT	SourcePort;

//	LPX_ADDRESS	SourceAddress;
//	LPX_ADDRESS	DestinationAddress;

	union {
		USHORT	Reserved[5];
		struct {
			USHORT	Lsctl;
#define LSCTL_CONNREQ		(USHORT)0x0001
#define LSCTL_DATA			(USHORT)0x0002
#define LSCTL_DISCONNREQ	(USHORT)0x0004
#define LSCTL_ACKREQ		(USHORT)0x0008
#define LSCTL_ACK			(USHORT)0x1000
			USHORT	Sequence;
			USHORT	AckSequence;
			UCHAR	ServerTag;
			UCHAR	ReservedS1[3];
		};

		struct {
			USHORT	MessageId;
			USHORT	MessageLength;
			USHORT	FragmentId;
			USHORT	FragmentLength;
			USHORT	ResevedU1;
		};
	};

} LPX_HEADER2, *PLPX_HEADER2;

extern ULONG				ulPacketDropRate;

#include <poppack.h>

#define	SMP_SYS_PKT_LEN	(sizeof(LPX_HEADER2))

#endif

