/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPX_H_
#define _LPX_H_

//
// Enable these warnings in the code.
//
#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


//
// the number of LPX packets that the LPX driver can hold per network interface.
//

#define PACKET_BUFFER_POOL_SIZE		1024


//
// LPX driver assign a port number from this number
//

#define LPX_PORTASSIGN_BEGIN	0x4000


//
// Do not allow to assign reserved ports due to historical reason.
//

#define RESERVEDPORT_NDASPNP_BCAST	10002


//
// Length of ethernet field of 802.2 LLC SNAP
//

#define LENGTH_8022LLCSNAP		8


//
// Maximum difference of sequence number from the last recevied stream packet
// If the difference is over this value, discards it.
//

#define MAX_ALLOWED_SEQ        1024


//
// Ethernet packet type for LPX
//

#define ETH_P_LPX				0x88AD        // 0x8200


//
// Pool tag for LPX's memory allocations
//

#define LpxAllocateMemoryWithLpxTag(a,b) NdisAllocateMemoryWithTag(a,b,'-xpL')

#define LpxFreeMemoryWithLpxTag(a) NdisFreeMemory(a, 0, 0)


//
// Convert a sequence number to a unsinged short integer
//

#define SHORT_SEQNUM(SEQNUM) ((USHORT)(SEQNUM))


//
// SMP( stream connection ) states
//

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
	SMP_CLOSING,     /* now a valid state */
	SMP_LAST

} SMP_STATE, *PSMP_STATE;


//
// Maximum number of packets that is not ACKed.
//

#define	SMP_MAX_FLIGHT			2048


//
// Indicate shutdown in progress
//

#define SMP_RECEIVE_SHUTDOWN	0x01
#define SMP_SEND_SHUTDOWN		0x02
#define SMP_SHUTDOWN_MASK		0x03

struct _LPX_STATE;

//
// Service point
// represents a stream between two LPX node.
//

typedef struct _SERVICE_POINT {
	KSPIN_LOCK			SpSpinLock;
	SMP_STATE			SmpState;	// Guarded by  SpSpinLock. To do: Remove this. modify to use access function instead.
	UCHAR				Shutdown;	// SMP_xx_SHUTDOWN


	//
	// ServicePointListEntry is protected by address->SpSpinLock
	// Linked to TP_ADDRESS->ConnectionServicePointList
	//

	LIST_ENTRY		ServicePointListEntry;

	//
	// Save IRPs to be complete asynchronously
	//

	PIRP			DisconnectIrp;
	PIRP			ConnectIrp;
	PIRP			ListenIrp;

	LIST_ENTRY		ReceiveIrpList;
	BOOLEAN			ReserveCanceling;   
	KSPIN_LOCK		ReceiveIrpQSpinLock;


	//
	// Addresses of each end point
	//

	LPX_ADDRESS		SourceAddress;			// remove this. Address of my network interface
	LPX_ADDRESS		DestinationAddress;		// Address of peer


	//
	//	pointers to TDI address and connection structure
	//

	struct _TP_ADDRESS		*Address;		// remove this
	struct _TP_CONNECTION	*Connection;	// remove this


	//
	// Indicate if Disconnection hanlder is called
	//

	LONG			lDisconnectHandlerCalled;
 

	//
	// Acked sequence number
	//

	LONG            Sequence;


	//
	//
	//

	LONG            FinSequence;


	//
	// Acked remote sequence number
	//

	LONG            RemoteSequence;

	
	//
	//
	//

	LONG            RemoteAck;			// used only in SmpDoReceive() and SendTest()


	//
	// Timer object
	//

	KTIMER          SmpTimer;


	//
	// DPC object for the timer object
	//

	KDPC            SmpTimerDpc;


	//
	// Timer call reason
	//

	LONG            TimerReason;
#define    SMP_SENDIBLE            0x0001
#define    SMP_RETRANSMIT_ACKED    0x0002


	//
    // protect following timeout counters.
	//

	KSPIN_LOCK		TimeCounterSpinLock ;


	//
	// Timeouts
	//

	LARGE_INTEGER	TimeWaitTimeOut;
	LARGE_INTEGER	AliveTimeOut;
	LARGE_INTEGER	RetransmitTimeOut;
	LARGE_INTEGER	ConnectTimeOut;
	LARGE_INTEGER	SmpTimerExpire;

	LARGE_INTEGER    RetransmitEndTime;    // End of Retransmit...


	//
	// Last retransmitted packet's sequence
	//

	LONG            LastRetransmitSequence;


	//
	// Retransmission counters
	//

	LONG            AliveRetries;
	LONG            Retransmits;
    

	//
	// Maximum number of packets that is not ACKed.
	//

	LONG            MaxFlights;


	//
	// Packet queues
	//

	KSPIN_LOCK        RcvDataQSpinLock;
	LIST_ENTRY        RcvDataQueue;

	KSPIN_LOCK        RetransmitQSpinLock;
	LIST_ENTRY        RetransmitQueue;

	KSPIN_LOCK        WriteQSpinLock;
	LIST_ENTRY        WriteQueue;

	UCHAR            ServerTag;


	//
    //    DPC to do actual jobs instead of ReceiveCompeletion & Timer
	//

	KDPC			SmpWorkDpc;
    LONG			RequestCnt;
    KSPIN_LOCK		SmpWorkDpcLock;

    KSPIN_LOCK		ReceiveQSpinLock;
    LIST_ENTRY		ReceiveQueue;

	KSPIN_LOCK		ReceiveReorderQSpinLock;
	LIST_ENTRY		ReceiveReorderQueue;


	//
	// To check retransmission
	//

	LARGE_INTEGER	LastRecvDataArrived;

} SERVICE_POINT, *PSERVICE_POINT;


//
// Ethernet header structure for LPX packet context
//

typedef UNALIGNED struct __ETHERNET_HEADER
{
    UCHAR    DestinationAddress[ETHERNET_ADDRESS_LENGTH];
    UCHAR    SourceAddress[ETHERNET_ADDRESS_LENGTH];
    USHORT    Type;

} ETHERNET_HEADER, *PETHERNET_HEADER;


//
// LPX packet context for each packet
//

typedef struct _LPX_RESERVED 
{
#define SEND_TYPE		0
#define RECEIVE_TYPE	1
	UCHAR        Type;
	LIST_ENTRY    ListElement;

	PVOID        LpxSmpHeader;
	USHORT        DataLength;

       LARGE_INTEGER RecvTime; // Used for detecting retransmission from netdisk.

	union {

		//
		// for ReceivePacketHandler
		//

        struct {
            UINT            PacketDataOffset;
            UINT            ReorderCount;   // Increase when handled as reordered packet. Drop if larger than certain value
		};


		//
		// for Packet Send and ReceiveHandler
		//

        struct {
			LONG				Cloned;
			PIO_STACK_LOCATION	IrpSp;
			ETHERNET_HEADER		EthernetHeader;
			LONG				Retransmits;    // Count 
		};
	};
}  LPX_RESERVED, *PLPX_RESERVED;


//
// Convert NDIS protocol reserved field to LPX reserved field
//

#define  RESERVED(_p) ((PLPX_RESERVED)((_p)->ProtocolReserved))


//
// LPX packet type enumeration
// for internal function call use
//

typedef enum _PACKET_TYPE {
    DATA,
    ACK,
    CONREQ,
    DISCON,
    RETRAN,
    ACKREQ
} PACKET_TYPE;

#define HZ                (LONGLONG)(10 * 1000 * 1000)
#define MSEC_TO_HZ(ms)   (((LONGLONG)(10* 1000)) * ms)


//
// Time units are in mili-seconds
//

#define DEFAULT_CONNECTION_TIMEOUT		2000

#define DEFAULT_SMP_TIMEOUT				50
#define DEFAULT_TIME_WAIT_INTERVAL		1000
#define DEFAULT_ALIVE_INTERVAL			1000

#define DEFAULT_RETRANSMIT_DELAY		200
#define DEFAULT_MAX_RETRANSMIT_DELAY	1000

#define DEFAULT_MAX_ALIVE_COUNT			8
#define DEFAULT_MAX_RETRANSMIT_TIME		8000

#define DEFAULT_MAXIMUM_TRANSFER_UNIT	1500	// Bytes 

//
// These values time units are in 100ns(1/HZ). Values assigned in lpxcnfg.c
//

extern LONGLONG	LpxConnectionTimeout;
extern LONGLONG	LpxSmpTimeout;
extern LONGLONG	LpxWaitInterval;
extern LONGLONG	LpxAliveInterval;
extern LONGLONG	LpxRetransmitDelay;
extern LONGLONG	LpxMaxRetransmitDelay;
extern LONG		LpxMaxAliveCount;
extern LONGLONG	LpxMaxRetransmitTime;
extern LONG		LpxMaximumTransferUnit;

#endif

