/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

This module defines private data structures and types for the NT
LPX transport provider.

--*/

#ifndef _LPXTYPES_
#define _LPXTYPES_


#if __LPX__

//
// the number of LPX packets that the LPX driver can hold at a time.
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
// Pool tag for LPX's memory allocations
//


#define LpxAllocateMemoryWithLpxTag(a,b) NdisAllocateMemoryWithTag(a,b,'-xpL')

#define LpxFreeMemoryWithLpxTag(a) NdisFreeMemory(a, 0, 0)

//
// Convert a sequence number to a unsinged short integer
//

#define SHORT_SEQNUM(SEQNUM) ((USHORT)(SEQNUM))

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

extern PCHAR LpxStateName[];


#define LPX_MAX_DATAGRAM_SIZE	1024


//
// SMP( stream connection ) states
//

typedef enum _SMP_STATE {

	SMP_STATE_ESTABLISHED = 1,
	SMP_STATE_SYN_SENT,
	SMP_STATE_SYN_RECV,
	SMP_STATE_FIN_WAIT1,
	SMP_STATE_FIN_WAIT2,
	SMP_STATE_TIME_WAIT,
	SMP_STATE_CLOSE,
	SMP_STATE_CLOSE_WAIT,
	SMP_STATE_LAST_ACK,
	SMP_STATE_LISTEN,
	SMP_STATE_CLOSING,     /* now a valid state */
	SMP_STATE_LAST

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



#define LPX_DEFAULT_SMP_TIMER_INTERVAL			(150*NANO100_PER_MSEC)

#define LPX_DEFAULT_MAX_STALL_INTERVAL			(8*NANO100_PER_SEC)
#define LPX_DEFALUT_MAX_CONNECT_WAIT_INTERVAL	(2*NANO100_PER_SEC)

#define LPX_DEFAULT_RETRANSMIT_INTERVAL			(200*NANO100_PER_MSEC)
#define LPX_DEFAULT_ALIVE_INTERVAL				(1*NANO100_PER_SEC)

#define LPX_DEFAULT_TIME_WAIT_INTERVAL			(1*NANO100_PER_SEC)
#define LPX_DEFAULT_LAST_ACK_INTERVAL			(4*NANO100_PER_SEC)


#define DEFAULT_MAXIMUM_TRANSFER_UNIT	1500	// Bytes 

//
// Service point
// represents a stream between two LPX node.
//

#define LPX_MAX_PACKET_ARRAY	128

typedef struct _LPX_SMP {
	
	SMP_STATE		SmpState;	// Guarded by  SpSpinLock. To do: Remove this. modify to use access function instead.
	UCHAR			Shutdown;	// SMP_xx_SHUTDOWN
	BOOLEAN			CanceledByUser;

	PIRP			DisconnectIrp;
	PIRP			ConnectIrp;
	PIRP			ListenIrp;

	LPX_ADDRESS		DestinationAddress;		// Address of peer
	BOOLEAN			DestinationSmallLengthDelayedAck;

	USHORT			PcbWindow;

	LONG			lDisconnectHandlerCalled;

	USHORT			Sequence;
	USHORT          RemoteAckSequence;
	USHORT          FinSequence;

	UCHAR           ServerTag;
	USHORT          RemoteSequence;

	LONG            MaxFlights;

	KSPIN_LOCK      TransmitSpinLock;
	LIST_ENTRY      TramsmitQueue;
	USHORT			NextTransmitSequece;

	KSPIN_LOCK		ReceiveIrpQSpinLock;
	LIST_ENTRY		ReceiveIrpQueue;

	KSPIN_LOCK		ReceiveQSpinLock;
	LIST_ENTRY		ReceiveQueue;

	KSPIN_LOCK      RecvDataQSpinLock;
	LIST_ENTRY      RecvDataQueue;

	KSPIN_LOCK		ReceiveReorderQSpinLock;
	LIST_ENTRY		ReceiveReorderQueue;

	KSPIN_LOCK		TimeCounterSpinLock;
	KTIMER          SmpTimer;
	KDPC            SmpTimerDpc;

	ULONG			SmpTimerFlag;
#define SMP_TIMER_DELAYED_ACK		0x00000001

	LARGE_INTEGER	MaxStallTimeOut;		// no response from remote...

	LARGE_INTEGER	RetransmitTimeOut;

	LARGE_INTEGER	AliveTimeOut;

	LARGE_INTEGER	TimeWaitTimeOut;
	LARGE_INTEGER	LastAckTimeOut;

	LARGE_INTEGER	SmpTimerInterval;
	
	LARGE_INTEGER	MaxStallInterval;
	LARGE_INTEGER	MaxConnectWaitInterval;
	
	LARGE_INTEGER	RetransmitInterval;

	LARGE_INTEGER	AliveInterval;

	LARGE_INTEGER	TimeWaitInterval;
	LARGE_INTEGER	LastAckInterval;

	LONG            RetransmitCount;

	UCHAR			Option;

	BOOLEAN			PacketArrayUsed;
	PNDIS_PACKET	PacketArray[LPX_MAX_PACKET_ARRAY];

	struct {

		ULONG			NumberofLargeSendRequests;
		LARGE_INTEGER	ResponseTimeOfLargeSendRequests;
		LARGE_INTEGER	BytesOfLargeSendRequests;

		ULONG			NumberofSmallSendRequests;
		LARGE_INTEGER	ResponseTimeOfSmallSendRequests;
		LARGE_INTEGER	BytesOfSmallSendRequests;

		ULONG			NumberOfSendRetransmission;
		ULONG			DropOfReceivePacket;

		TRANS_STAT		TransportStats;

#if __LPX_STATISTICS__

#define LPX_DEFAULT_STATSTICS_INTERVAL	(60*NANO100_PER_SEC)

		LARGE_INTEGER	StatisticsTimeOut;
		LARGE_INTEGER	StatisticsInterval;


		ULONG			NumberofLargeRecvRequests;
		LARGE_INTEGER	ResponseTimeOfLargeRecvRequests;
		LARGE_INTEGER	BytesOfLargeRecvRequests;

		ULONG			NumberofSmallRecvRequests;
		LARGE_INTEGER	ResponseTimeOfSmallRecvRequests;
		LARGE_INTEGER	BytesOfSmallRecvRequests;

		ULONG			NumberOfSendPackets;
		LARGE_INTEGER	ResponseTimeOfSendPackets;
		LARGE_INTEGER	BytesOfSendPackets;

		ULONG			NumberofSmallSendPackets;
		LARGE_INTEGER	ResponseTimeOfSmallSendPackets;
		LARGE_INTEGER	BytesOfSmallSendPackets;

		ULONG			NumberofLargeSendPackets;
		LARGE_INTEGER	ResponseTimeOfLargeSendPackets;
		LARGE_INTEGER	BytesOfLargeSendPackets;

		ULONG			NumberofRecvPackets;
		LARGE_INTEGER	BytesOfRecvPackets;

		ULONG			NumberofLargeRecvPackets;
		LARGE_INTEGER	BytesOfLargeRecvPackets;

		ULONG			NumberofSmallRecvPackets;
		LARGE_INTEGER	BytesOfSmallRecvPackets;

#else

		ULONG			NumberOfSendPackets;
		ULONG			NumberofRecvPackets;

#endif

	};

} LPX_SMP, *PLPX_SMP;


//
// Ethernet header structure for LPX packet context
//

typedef UNALIGNED struct __ETHERNET_HEADER {

    UCHAR    DestinationAddress[ETHERNET_ADDRESS_LENGTH];
    UCHAR    SourceAddress[ETHERNET_ADDRESS_LENGTH];
    USHORT   Type;

} ETHERNET_HEADER, *PETHERNET_HEADER;


//
// LPX packet context for each packet
//

#include <pshpack1.h>

typedef struct _LPX_RESERVED {

	// NDIS miniport allocate only 4*sizeof(PVOID) bytes for protocol reserved memory.
	// If ExternalReserved field is not NULL, do not refer fields below the field.

	struct _LPX_RESERVED	*ExternalReserved;

#define LPX_PACKET_TYPE_SEND		0
#define LPX_PACKET_TYPE_RECEIVE		1

	UCHAR			Type;
	UCHAR			HeaderCopied; // Used by LPX_PACKET_TYPE_RECEIVE
	UCHAR			Reserved[2];

	LIST_ENTRY		ListEntry;

	PNDIS_PACKET	Packet;

	union {

		// for Send

		struct {

			NDIS_STATUS			NdisStatus;
			LARGE_INTEGER		SendTime;
			LONG				Cloned;
			PIO_STACK_LOCATION	IrpSp;
			LONG				Retransmits;    // Count 
		};

		// for Receive

		struct {

			UINT			PacketRawDataOffset;
			ULONG			PacketRawDataLength;
			LARGE_INTEGER   RecvTime;			// Used for detecting retransmission from netdisk.
			UINT            ReorderCount;		// Increase when handled as reordered packet. Drop if larger than certain value

#if __LPX_STATISTICS__
			LARGE_INTEGER	RecvTime2;			// Used for detecting retransmission from netdisk.
#endif

#define		LPX_RESERVED_RECVFLAG_ALLOC_MINIPORT	0x00001
			UINT			RecvFlags;
		};

	};

#if __LPX_STATISTICS__
	struct _DEVICE_CONTEXT	*DeviceContext;
#endif


	ETHERNET_HEADER	EthernetHeader;
	LPX_HEADER		LpxHeader;
	UCHAR			OptionDestinationAddress[6];
	UCHAR			OptionSourceAddress[6];
	
}  LPX_RESERVED, *PLPX_RESERVED;

#define PROTOCOL_RESERVED_OFFSET	(sizeof(PVOID))

static UCHAR ZeroProtocolReserved[PROTOCOL_RESERVED_OFFSET];

#include <poppack.h>

// Convert NDIS protocol reserved field to LPX reserved field

__inline
PLPX_RESERVED
LpxGetReserved(PNDIS_PACKET Packet) {

	PLPX_RESERVED reserved = (PLPX_RESERVED)(&Packet->ProtocolReserved[PROTOCOL_RESERVED_OFFSET]);

	if (reserved->ExternalReserved == NULL) {

		return (PLPX_RESERVED)(&Packet->ProtocolReserved[PROTOCOL_RESERVED_OFFSET]);

	} else {

		return (PLPX_RESERVED)reserved->ExternalReserved;
	}
}

//
// LPX packet type enumeration
// for internal function call use
//

typedef enum _LPX_SMP_PACKET_TYPE {
    
	SMP_TYPE_DATA,
    SMP_TYPE_ACK,
    SMP_TYPE_CONREQ,
    SMP_TYPE_DISCON,
    SMP_TYPE_RETRANS,
    SMP_TYPE_ACKREQ

} LPX_SMP_PACKET_TYPE, *PLPX_SMP_PACKET_TYPE;

#endif

//
// This structure defines a NETBIOS name as a character array for use when
// passing preformatted NETBIOS names between internal routines.  It is
// not a part of the external interface to the transport provider.
//

#define NETBIOS_NAME_SIZE 16

typedef UCHAR NAME;
typedef NAME UNALIGNED *PNAME;


//
// This structure defines things associated with a TP_REQUEST, or outstanding
// TDI request, maintained on a queue somewhere in the transport.  All
// requests other than open/close require that a TP_REQUEST block be built.
//

#if DBG
#define REQUEST_HISTORY_LENGTH 20
extern KSPIN_LOCK LpxGlobalInterlock;
#endif


//typedef
//NTSTATUS
//(*PTDI_TIMEOUT_ACTION)(
//    IN PTP_REQUEST Request
//    );

//
// The request itself
//

#if DBG
#define RREF_CREATION   0
#define RREF_PACKET     1
#define RREF_TIMER      2
#define RREF_RECEIVE    3
#define RREF_FIND_NAME  4
#define RREF_STATUS     5

#define NUMBER_OF_RREFS 8
#endif

//
// in nbfdrvr.c
//

extern UNICODE_STRING LpxRegistryPath;

//
// We need the driver object to create device context structures.
//

extern PDRIVER_OBJECT LpxDriverObject;

//
// This is a list of all the device contexts that LPX owns,
// used while unloading.
//

extern LIST_ENTRY LpxDeviceList;

//
// And a lock that protects the global list of LPX devices
//
extern FAST_MUTEX LpxDevicesLock;

#define INITIALIZE_DEVICES_LIST_LOCK()                                  \
    ExInitializeFastMutex(&LpxDevicesLock)

#define ACQUIRE_DEVICES_LIST_LOCK()                                     \
    ACQUIRE_FAST_MUTEX_UNSAFE(&LpxDevicesLock)

#define RELEASE_DEVICES_LIST_LOCK()                                     \
    RELEASE_FAST_MUTEX_UNSAFE(&LpxDevicesLock)

//
// A handle to be used in all provider notifications to TDI layer
// 
extern HANDLE LpxProviderHandle;

//
// Global Configuration block for the driver ( no lock required )
// 
extern PCONFIG_DATA   LpxConfig;

#if DBG
extern KSPIN_LOCK LpxGlobalHistoryLock;
extern LIST_ENTRY LpxGlobalRequestList;
#define StoreRequestHistory(_req,_ref) {                                \
    KIRQL oldIrql;                                                      \
    KeAcquireSpinLock (&LpxGlobalHistoryLock, &oldIrql);                \
    if ((_req)->Destroyed) {                                            \
        DbgPrint ("request touched after being destroyed 0x%p\n",      \
                    (_req));                                            \
        DbgBreakPoint();                                                \
    }                                                                   \
    RtlGetCallersAddress(                                               \
        &(_req)->History[(_req)->NextRefLoc].Caller,                    \
        &(_req)->History[(_req)->NextRefLoc].CallersCaller              \
        );                                                              \
    if ((_ref)) {                                                       \
        (_req)->TotalReferences++;                                      \
    } else {                                                            \
        (_req)->TotalDereferences++;                                    \
        (_req)->History[(_req)->NextRefLoc].Caller =                    \
         (PVOID)((ULONG_PTR)(_req)->History[(_req)->NextRefLoc].Caller | 1); \
    }                                                                   \
    if (++(_req)->NextRefLoc == REQUEST_HISTORY_LENGTH) {               \
        (_req)->NextRefLoc = 0;                                         \
    }                                                                   \
    KeReleaseSpinLock (&LpxGlobalHistoryLock, oldIrql);                 \
}
#endif

#define LPX_ALLOCATION_TYPE_REQUEST 1

#define REQUEST_FLAGS_TIMER      0x0001 // a timer is active for this request.
#define REQUEST_FLAGS_TIMED_OUT  0x0002 // a timer expiration occured on this request.
#define REQUEST_FLAGS_ADDRESS    0x0004 // request is attached to a TP_ADDRESS.
#define REQUEST_FLAGS_CONNECTION 0x0008 // request is attached to a TP_CONNECTION.
#define REQUEST_FLAGS_STOPPING   0x0010 // request is being killed.
#define REQUEST_FLAGS_EOR        0x0020 // TdiSend request has END_OF_RECORD mark.
#define REQUEST_FLAGS_PIGGYBACK  0x0040 // TdiSend that can be piggyback ack'ed.
#define REQUEST_FLAGS_DC         0x0080 // request is attached to a TP_DEVICE_CONTEXT

//
// This defines the TP_SEND_IRP_PARAMETERS, which is masked onto the
// Parameters section of a send IRP's stack location.
//

typedef struct _TP_SEND_IRP_PARAMETERS {
	
	union {

		TDI_REQUEST_KERNEL_SEND		SendReqeust;
		TDI_REQUEST_KERNEL_SENDDG	SendDgramReqeust;
	};
    
	LONG ReferenceCount;
    PVOID Irp;

} TP_SEND_IRP_PARAMETERS, *PTP_SEND_IRP_PARAMETERS;

#define IRP_SEND_LENGTH(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->SendReqeust.SendLength)

#define IRP_SEND_FLAGS(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.SendFlags)

#define IRP_SEND_REFCOUNT(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_SEND_IRP(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Irp)

#define IRP_SEND_CONNECTION(_IrpSp) \
    ((PTP_CONNECTION)((_IrpSp)->FileObject->FsContext))

#define IRP_DEVICE_CONTEXT(_IrpSp) \
    ((PDEVICE_CONTEXT)((_IrpSp)->DeviceObject))


//
// This defines the TP_RECEIVE_IRP_PARAMETERS, which is masked onto the
// Parameters section of a receive IRP's stack location.
//

typedef struct _TP_RECEIVE_IRP_PARAMETERS {
    TDI_REQUEST_KERNEL_RECEIVE Request;
    LONG ReferenceCount;
    PIRP Irp;
} TP_RECEIVE_IRP_PARAMETERS, *PTP_RECEIVE_IRP_PARAMETERS;

#define IRP_RECEIVE_LENGTH(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.ReceiveLength)

#define IRP_RECEIVE_FLAGS(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.ReceiveFlags)

#define IRP_RECEIVE_REFCOUNT(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_RECEIVE_IRP(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Irp)

#define IRP_RECEIVE_CONNECTION(_IrpSp) \
    ((PTP_CONNECTION)((_IrpSp)->FileObject->FsContext))


//
// This structure defines a TP_CONNECTION, or active transport connection,
// maintained on a transport address.
//

#if DBG
#define CONNECTION_HISTORY_LENGTH 50

#define CREF_SPECIAL_CREATION 0
#define CREF_SPECIAL_TEMP 1
#define CREF_COMPLETE_SEND 2
#define CREF_SEND_IRP 3
#define CREF_ADM_SESS 4
#define CREF_TRANSFER_DATA 5
#define CREF_FRAME_SEND 6
#define CREF_TIMER 7
#define CREF_BY_ID 8
#define CREF_LINK 9
#define CREF_SESSION_END 10
#define CREF_LISTENING 11
#define CREF_P_LINK 12
#define CREF_P_CONNECT 13
#define CREF_PACKETIZE 14
#define CREF_RECEIVE_IRP 15
#define CREF_PROCESS_DATA 16
#define CREF_REQUEST 17
#define CREF_TEMP 18
#define CREF_DATA_ACK_QUEUE 19
#define CREF_ASSOCIATE 20
#define CREF_STOP_ADDRESS 21
#define CREF_PACKETIZE_QUEUE 22
#define CREF_STALLED 23

#if __LPX__
#define CREF_LPX_TIMER		24
#define CREF_LPX_RECEIVE	25
#define CREF_LPX_PRIVATE	26
#define NUMBER_OF_CREFS		27
#else
#define NUMBER_OF_CREFS 24
#endif

#endif

typedef struct _TP_CONNECTION {

#if __LPX__
	LPX_SMP	LpxSmp;
#endif

#if DBG
    ULONG RefTypes[NUMBER_OF_CREFS];
#endif

#if DBG
    ULONG LockAcquired;
    UCHAR LastAcquireFile[8];
    ULONG LastAcquireLine;
    ULONG Padding;
    UCHAR LastReleaseFile[8];
    ULONG LastReleaseLine;
#endif

    CSHORT Type;
    USHORT Size;

    LIST_ENTRY LinkList;                // used for link thread or for free
                                        // resource list
    KSPIN_LOCK SpinLock;                // spinlock for connection protection.

    LONG ReferenceCount;                // number of references to this object.
    LONG SpecialRefCount;               // controls freeing of connection.

    //
    // The following lists are used to associate this connection with a
    // particular address.
    //

    LIST_ENTRY AddressList;             // list of connections for given address
    LIST_ENTRY AddressFileList;         // list for connections bound to a
                                        // given address reference
    //
    // The following field points to the TP_LINK object that describes the
    // (active) data link connection for this transport connection.  To be
    // valid, this field is non-NULL.
    //

    struct _TP_ADDRESS_FILE *AddressFile;   // pointer to owning Address.
    struct _DEVICE_CONTEXT *Provider;       // device context to which we are attached.
    PKSPIN_LOCK ProviderInterlock;          // &Provider->Interlock
    PFILE_OBJECT FileObject;                // easy backlink to file object.

    //
    // The following field is specified by the user at connection open time.
    // It is the context that the user associates with the connection so that
    // indications to and from the client can be associated with a particular
    // connection.
    //

    CONNECTION_CONTEXT Context;         // client-specified value.

    //
    // If the connection is being closed, this will hold
    // the IRP passed to TdiCloseConnection. It is needed
    // when the request is completed.
    //

    PIRP CloseIrp;

    //
    // The following fields are used for connection housekeeping.
    //

    ULONG Flags2;                       // attributes guarded by SpinLock
    NTSTATUS Status;                    // status code for connection rundown.
    LPX_ADDRESS CalledAddress;  // TdiConnect request's T.A.
    USHORT MaximumDataSize;             // maximum I-frame data size for LPX.

    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

#if DBG
    LIST_ENTRY GlobalLinkage;
    ULONG TotalReferences;
    ULONG TotalDereferences;
    ULONG NextRefLoc;
    struct {
        PVOID Caller;
        PVOID CallersCaller;
    } History[CONNECTION_HISTORY_LENGTH];
    BOOLEAN Destroyed;
#endif

} TP_CONNECTION, *PTP_CONNECTION;


#if __LPX__
typedef struct __CONNECTION_PRIVATE {

	LARGE_INTEGER	CurrentTime;
	PTP_CONNECTION	Connection;

} CONNECTION_PRIVATE, *PCONNECTION_PRIVATE;

#endif


#if DBG
extern KSPIN_LOCK LpxGlobalHistoryLock;
extern LIST_ENTRY LpxGlobalConnectionList;
#define StoreConnectionHistory(_conn,_ref) {                                \
    KIRQL oldIrql;                                                          \
    KeAcquireSpinLock (&LpxGlobalHistoryLock, &oldIrql);                    \
    if ((_conn)->Destroyed) {                                               \
        DbgPrint ("connection touched after being destroyed 0x%p\n",        \
                    (_conn));                                               \
        DbgBreakPoint();                                                    \
    }                                                                       \
    RtlGetCallersAddress(                                                   \
        &(_conn)->History[(_conn)->NextRefLoc].Caller,                      \
        &(_conn)->History[(_conn)->NextRefLoc].CallersCaller                \
        );                                                                  \
    if ((_ref)) {                                                           \
        (_conn)->TotalReferences++;                                         \
    } else {                                                                \
        (_conn)->TotalDereferences++;                                       \
        (_conn)->History[(_conn)->NextRefLoc].Caller =                      \
         (PVOID)((ULONG_PTR)(_conn)->History[(_conn)->NextRefLoc].Caller | 1); \
    }                                                                       \
    if (++(_conn)->NextRefLoc == CONNECTION_HISTORY_LENGTH) {               \
        (_conn)->NextRefLoc = 0;                                            \
    }                                                                       \
    KeReleaseSpinLock (&LpxGlobalHistoryLock, oldIrql);                     \
}
#endif

#define CONNECTION_FLAGS2_STOPPING      0x00000001 // connection is running down.
#define CONNECTION_FLAGS2_WAIT_NR       0x00000002 // waiting for NAME_RECOGNIZED.
#define CONNECTION_FLAGS2_WAIT_NQ       0x00000004 // waiting for NAME_QUERY.
#define CONNECTION_FLAGS2_WAIT_NR_FN    0x00000008 // waiting for FIND NAME response.
#define CONNECTION_FLAGS2_CLOSING       0x00000010 // connection is closing
#define CONNECTION_FLAGS2_ASSOCIATED    0x00000020 // associated with address
#define CONNECTION_FLAGS2_DISCONNECT    0x00000040 // disconnect done on connection
#define CONNECTION_FLAGS2_ACCEPTED      0x00000080 // accept done on connection
#define CONNECTION_FLAGS2_REQ_COMPLETED 0x00000100 // Listen/Connect request completed.
#define CONNECTION_FLAGS2_DISASSOCIATED 0x00000200 // associate CRef has been removed
#define CONNECTION_FLAGS2_DISCONNECTED  0x00000400 // disconnect has been indicated
#define CONNECTION_FLAGS2_NO_LISTEN     0x00000800 // no_listen received during setup
#define CONNECTION_FLAGS2_REMOTE_VALID  0x00001000 // Connection->RemoteName is valid
#define CONNECTION_FLAGS2_GROUP_LSN     0x00002000 // connection LSN is globally assigned
#define CONNECTION_FLAGS2_W_ADDRESS     0x00004000 // waiting for address reregistration.
#define CONNECTION_FLAGS2_PRE_ACCEPT    0x00008000 // no TdiAccept after listen completes
#define CONNECTION_FLAGS2_ABORT         0x00010000 // abort this connection.
#define CONNECTION_FLAGS2_ORDREL        0x00020000 // we're in orderly release.
#define CONNECTION_FLAGS2_DESTROY       0x00040000 // destroy this connection.
#define CONNECTION_FLAGS2_LISTENER      0x00100000 // we were the passive listener.
#define CONNECTION_FLAGS2_CONNECTOR     0x00200000 // we were the active connector.
#define CONNECTION_FLAGS2_WAITING_SC    0x00400000 // the connection is waiting for
                                                   // and accept to send the
                                                   // session confirm
#define CONNECTION_FLAGS2_INDICATING    0x00800000 // connection was manipulated while
                                                   // indication was in progress

#define CONNECTION_FLAGS2_LDISC         0x01000000 // Local disconnect req.


//
// This structure is pointed to by the FsContext field in the FILE_OBJECT
// for this Address.  This structure is the base for all activities on
// the open file object within the transport provider.  All active connections
// on the address point to this structure, although no queues exist here to do
// work from. This structure also maintains a reference to a TP_ADDRESS
// structure, which describes the address that it is bound to. Thus, a
// connection will point to this structure, which describes the address the
// connection was associated with. When the address file closes, all connections
// opened on this address file get closed, too. Note that this may leave an
// address hanging around, with other references.
//

typedef struct _TP_ADDRESS_FILE {

    CSHORT Type;
    CSHORT Size;

    LIST_ENTRY Linkage;                 // next address file on this address.
                                        // also used for linkage in the
                                        // look-aside list

    LONG ReferenceCount;                // number of references to this object.

    //
    // This structure is edited after taking the Address spinlock for the
    // owning address. This ensures that the address and this structure
    // will never get out of syncronization with each other.
    //

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per connection open on this address.  This list of connections
    // is used to help the cleanup process if a process closes an address
    // before disassociating all connections on it. By design, connections
    // will stay around until they are explicitly
    // closed; we use this database to ensure that we clean up properly.
    //

    LIST_ENTRY ConnectionDatabase;      // list of defined transport connections.

    //
    // the current state of the address file structure; this is either open or
    // closing
    //

    UCHAR State;

    //
    // The following fields are kept for housekeeping purposes.
    //

    PIRP Irp;                           // the irp used for open or close
    struct _TP_ADDRESS *Address;        // address to which we are bound.
    PFILE_OBJECT FileObject;            // easy backlink to file object.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following queue is used to queue receive datagram requests
    // on this address file. Send datagram requests are queued on the
    // address itself. These queues are managed by the EXECUTIVE interlocked
    // list management routines. The actual objects which get queued to this
    // structure are request control blocks (RCBs).
    //

    LIST_ENTRY ReceiveDatagramQueue;    // FIFO of outstanding TdiReceiveDatagrams.

    //
    // This holds the Irp used to close this address file,
    // for pended completion.
    //

    PIRP CloseIrp;

    //
    // is this address file currently indicating a connection request? if yes, we
    // need to mark connections that are manipulated during this time.
    //

    BOOLEAN ConnectIndicationInProgress;

    //
    // handler for kernel event actions. First we have a set of booleans that
    // indicate whether or not this address has an event handler of the given
    // type registered.
    //

    BOOLEAN RegisteredConnectionHandler;
    BOOLEAN RegisteredDisconnectHandler;
    BOOLEAN RegisteredReceiveHandler;
    BOOLEAN RegisteredReceiveDatagramHandler;
    BOOLEAN RegisteredExpeditedDataHandler;
    BOOLEAN RegisteredErrorHandler;

    //
    // This function pointer points to a connection indication handler for this
    // Address. Any time a connect request is received on the address, this
    // routine is invoked.
    //
    //

    PTDI_IND_CONNECT ConnectionHandler;
    PVOID ConnectionHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_DISCONNECT
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_DISCONNECT DisconnectHandler;
    PVOID DisconnectHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE
    // event handler for connections on this address.  If the NULL handler
    // is specified in a TdiSetEventHandler, then this points to an internal
    // routine which does not accept the incoming data.
    //

    PTDI_IND_RECEIVE ReceiveHandler;
    PVOID ReceiveHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE_DATAGRAM
    // event handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which does
    // not accept the incoming data.
    //

    PTDI_IND_RECEIVE_DATAGRAM ReceiveDatagramHandler;
    PVOID ReceiveDatagramHandlerContext;

    //
    // An expedited data handler. This handler is used if expedited data is
    // expected; it never is in LPX, thus this handler should always point to
    // the default handler.
    //

    PTDI_IND_RECEIVE_EXPEDITED ExpeditedDataHandler;
    PVOID ExpeditedDataHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_ERROR
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_ERROR ErrorHandler;
    PVOID ErrorHandlerContext;
    PVOID ErrorHandlerOwner;


} TP_ADDRESS_FILE, *PTP_ADDRESS_FILE;

#define ADDRESSFILE_STATE_OPENING   0x00    // not yet open for business
#define ADDRESSFILE_STATE_OPEN      0x01    // open for business
#define ADDRESSFILE_STATE_CLOSING   0x02    // closing


//
// This structure defines a TP_ADDRESS, or active transport address,
// maintained by the transport provider.  It contains all the visible
// components of the address (such as the TSAP and network name components),
// and it also contains other maintenance parts, such as a reference count,
// ACL, and so on. All outstanding connection-oriented and connectionless
// data transfer requests are queued here.
//

#if DBG
#define AREF_TIMER              0
#define AREF_TEMP_CREATE        1
#define AREF_OPEN               2
#define AREF_VERIFY             3
#define AREF_LOOKUP             4
#define AREF_FRAME_SEND         5
#define AREF_CONNECTION         6
#define AREF_TEMP_STOP          7
#define AREF_REQUEST            8
#define AREF_PROCESS_UI         9
#define AREF_PROCESS_DATAGRAM  10
#define AREF_TIMER_SCAN        11

#define NUMBER_OF_AREFS        12
#endif

typedef struct _TP_ADDRESS {

#if DBG
    ULONG RefTypes[NUMBER_OF_AREFS];
#endif

    USHORT Size;
    CSHORT Type;

    LIST_ENTRY Linkage;                 // next address/this device object.
    LONG ReferenceCount;                // number of references to this object.

    //
    // The following spin lock is acquired to edit this TP_ADDRESS structure
    // or to scan down or edit the list of address files.
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this structure.

    //
    // The following fields comprise the actual address itself.
    //

    PIRP Irp;                           // pointer to address creation IRP.
    PLPX_ADDRESS NetworkName;    // this address
#if __LPX__
	BOOLEAN	PortAssignedByLpx;
#endif

    //
    // The following fields are used to maintain state about this address.
    //

    ULONG Flags;                        // attributes of the address.
    ULONG SendFlags;				   // State of the datagram current send
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following queues is used to hold send datagrams for this
    // address. Receive datagrams are queued to the address file. Requests are
    // processed in a first-in, first-out manner, so that the very next request
    // to be serviced is always at the head of its respective queue.  These
    // queues are managed by the EXECUTIVE interlocked list management routines.
    // The actual objects which get queued to this structure are request control
    // blocks (RCBs).
    //

    LIST_ENTRY SendDatagramQueue;       // FIFO of outstanding TdiSendDatagrams.

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per active, connecting, or disconnecting connections on this
    // address.  By definition, if a connection is on this list, then
    // it is visible to the client in terms of receiving events and being
    // able to post requests by naming the ConnectionId.  If the connection
    // is not on this list, then it is not valid, and it is guaranteed that
    // no indications to the client will be made with reference to it, and
    // no requests specifying its ConnectionId will be accepted by the transport.
    //

    LIST_ENTRY ConnectionDatabase;  // list of defined transport connections.
    LIST_ENTRY AddressFileDatabase; // list of defined address file objects

    //
    // The following fields are used to register this address on the network.
    //

    ULONG Retries;                      // retries of ADD_NAME_QUERY left to go.
    //
    // These two can be a union because they are not used
    // concurrently.
    //

    union {

        //
        // This structure is used for checking share access.
        //

        SHARE_ACCESS ShareAccess;

        //
        // Used for delaying LpxDestroyAddress to a thread so
        // we can access the security descriptor.
        //

        WORK_QUEUE_ITEM DestroyAddressQueueItem;

    } u;

    //
    // This structure is used to hold ACLs on the address.

    PSECURITY_DESCRIPTOR SecurityDescriptor;

    //
    // If we get an ADD_NAME_RESPONSE frame, this holds the address
    // of the remote we got it from (used to check for duplicate names).
    //

    UCHAR UniqueResponseAddress[6];

    //
    // Set to TRUE once we send a name in conflict frame, so that
    // we don't flood the network with them on every response.
    //

    BOOLEAN NameInConflictSent;

} TP_ADDRESS, *PTP_ADDRESS;

#define ADDRESS_FLAGS_GROUP             0x00000001 // set if group, otherwise unique.
#define ADDRESS_FLAGS_CONFLICT          0x00000002 // address in conflict detected.
#define ADDRESS_FLAGS_REGISTERING       0x00000004 // registration in progress.
#define ADDRESS_FLAGS_DEREGISTERING     0x00000008 // deregistration in progress.
#define ADDRESS_FLAGS_DUPLICATE_NAME    0x00000010 // duplicate name was found on net.
#define ADDRESS_FLAGS_NEEDS_REG         0x00000020 // address must be registered.
#define ADDRESS_FLAGS_STOPPING          0x00000040 // TpStopAddress is in progress.
#define ADDRESS_FLAGS_BAD_ADDRESS       0x00000080 // name in conflict on associated address.
#define ADDRESS_FLAGS_SEND_IN_PROGRESS  0x00000100 // send datagram process active.
#define ADDRESS_FLAGS_CLOSED            0x00000200 // address has been closed;
                                                   // existing activity can
                                                   // complete, nothing new can start
#define ADDRESS_FLAGS_NEED_REREGISTER   0x00000400 // quick-reregister on next connect.
#define ADDRESS_FLAGS_QUICK_REREGISTER  0x00000800 // address is quick-reregistering.

#ifndef NO_STRESS_BUG
#define ADDRESS_FLAGS_SENT_TO_NDIS		 0x00010000	// Packet sent to the NDIS layer
#define ADDRESS_FLAGS_RETD_BY_NDIS		 0x00020000	// Packet returned by the NDIS layer
#endif


//
// This structure defines a TP_LINK, or established data link object,
// maintained by the transport provider.  Each data link connection with
// a remote machine is represented by this object.  Zero, one, or several
// transport connections can be multiplexed over the same data link connection.
// This object is managed by routines in LINK.C.
//

#if DBG
#define LREF_SPECIAL_CONN 0
#define LREF_SPECIAL_TEMP 1
#define LREF_CONNECTION 2
#define LREF_STOPPING 3
#define LREF_START_T1 4
#define LREF_TREE 5
#define LREF_NOT_ADM 6
#define LREF_NDIS_SEND 7

#define NUMBER_OF_LREFS 8
#endif

#if DBG
#define LINK_HISTORY_LENGTH 20
#endif


#if DBG
extern KSPIN_LOCK LpxGlobalHistoryLock;
extern LIST_ENTRY LpxGlobalLinkList;
#define StoreLinkHistory(_link,_ref) {                                      \
    KIRQL oldIrql;                                                          \
    KeAcquireSpinLock (&LpxGlobalHistoryLock, &oldIrql);                    \
    if ((_link)->Destroyed) {                                               \
        DbgPrint ("link touched after being destroyed 0x%p\n", (_link));   \
        DbgBreakPoint();                                                    \
    }                                                                       \
    RtlGetCallersAddress(                                                   \
        &(_link)->History[(_link)->NextRefLoc].Caller,                      \
        &(_link)->History[(_link)->NextRefLoc].CallersCaller                \
        );                                                                  \
    if ((_ref)) {                                                           \
        (_link)->TotalReferences++;                                         \
    } else {                                                                \
        (_link)->TotalDereferences++;                                       \
        (_link)->History[(_link)->NextRefLoc].Caller =                      \
           (PVOID)((ULONG_PTR)(_link)->History[(_link)->NextRefLoc].Caller | 1);\
    }                                                                       \
    if (++(_link)->NextRefLoc == LINK_HISTORY_LENGTH) {                     \
        (_link)->NextRefLoc = 0;                                            \
    }                                                                       \
    KeReleaseSpinLock (&LpxGlobalHistoryLock, oldIrql);                     \
}
#endif


//
// This structure defines the DEVICE_OBJECT and its extension allocated at
// the time the transport provider creates its device object.
//

#if DBG
#define DCREF_CREATION    0
#define DCREF_ADDRESS     1
#define DCREF_CONNECTION  2
#define DCREF_LINK        3
#define DCREF_QUERY_INFO  4
#define DCREF_SCAN_TIMER  5
#define DCREF_REQUEST     6
#define DCREF_TEMP_USE    7

#define NUMBER_OF_DCREFS 8
#endif


typedef struct _LPX_POOL_LIST_DESC {
    NDIS_HANDLE PoolHandle;
    USHORT   NumElements;
    USHORT   TotalElements;
    struct _LPX_POOL_LIST_DESC *Next;
} LPX_POOL_LIST_DESC, *PLPX_POOL_LIST_DESC;

#if __LPX__

#if 0

#define INCREASE_SENDING_THREAD_COUNT(_DeviceContext) {							\
																				\
	KIRQL	_oldIrql;															\
																				\
	ACQUIRE_SPIN_LOCK( &_DeviceContext->PacketInProgressQSpinLock, &_oldIrql );	\
	_DeviceContext->SendingThreadCount++;										\
	RELEASE_SPIN_LOCK( &_DeviceContext->PacketInProgressQSpinLock, _oldIrql );	\
}

#define DECREASE_SENDING_THREAD_COUNT(_DeviceContext) {						\
																				\
	KIRQL	_oldIrql;															\
																				\
	ACQUIRE_SPIN_LOCK( &_DeviceContext->PacketInProgressQSpinLock, &_oldIrql );	\
	_DeviceContext->SendingThreadCount--;										\
	RELEASE_SPIN_LOCK( &_DeviceContext->PacketInProgressQSpinLock, _oldIrql );	\
																				\
	KeRaiseIrql( DISPATCH_LEVEL, &_oldIrql );									\
	LpxReceiveComplete( (NDIS_HANDLE)_DeviceContext );							\
	KeLowerIrql( _oldIrql );													\
}

#else

#define INCREASE_SENDING_THREAD_COUNT(_DeviceContext)
#define DECREASE_SENDING_THREAD_COUNT(_DeviceContext)

#endif

#endif

typedef struct _DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.

#if __LPX__

#if DBG
	ULONG LockAcquired;
	UCHAR LastAcquireFile[8];
	ULONG LastAcquireLine;
	ULONG Padding;
	UCHAR LastReleaseFile[8];
	ULONG LastReleaseLine;
	UCHAR LockType;
#endif

	USHORT				LastPortNum;
	NDIS_HANDLE         LpxPacketPool;

	// Mod by jgahn.
	NDIS_HANDLE			LpxBufferPool;

	// Received packet.
	KSPIN_LOCK				PacketInProgressQSpinLock;
	LIST_ENTRY				PacketInProgressList;
	
#if 0	
	NPAGED_LOOKASIDE_LIST  DeferredRxCompContext;
#else
	KDPC				DeferredLpxReceiveCompleteDpc;
	LIST_ENTRY			DeferredLpxReceiveCompleteDpcPacketList;
	BOOLEAN				DeferredLpxReceiveCompleteDpcRun;
#endif

	ULONG				MaxUserData;

	NDIS_MEDIA_STATE	NdisMediaState;


#define LPX_DEVICE_CONTEXT_START					0x00000001
#define LPX_DEVICE_CONTEXT_STOP						0x00000002


//#define LPX_DEVICE_CONTEXT_FLAGS_RECEIVE_COMPLETING	0x00000100

	ULONG			LpxFlags;

#define LPX_DEVICE_CONTEXT_MEDIA_CONNECTED			0x00000001
#define LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED		0x00000002

	KSPIN_LOCK		LpxMediaFlagSpinLock;
	ULONG			LpxMediaFlags;

	LONG			SendingThreadCount;


	//
	// Force one ack packet per N packets.
	// Currently N = 2.
	// Maybe some NIC drivers is optimized for one ack per 2 data packets
	// because TCP/IP send an ack packet per 2 packets.
	// Broadcom Giga Ethernet 3788 works best with N = 2 on Windows VISTA x86
	// There was no difference between 1 and 2.
	//

#define DEFAULT_FORCED_ACK_PER_PACKETS 2

	LONG			ForcedAckPerPackets;


#if __LPX_STATISTICS__

	LARGE_INTEGER	StatisticsTimeOut;
	LARGE_INTEGER	StatisticsInterval;

	ULONG			NumberOfSendPackets;
	LARGE_INTEGER	ResponseTimeOfSendPackets;
	LARGE_INTEGER	BytesOfSendPackets;
		
	ULONG			NumberofLaregeSendRequests;
	LARGE_INTEGER	ResponseTimeOfLargeSendRequests;
	LARGE_INTEGER	BytesOfLargeSendRequests;

	ULONG			NumberofSmallSendRequests;
	LARGE_INTEGER	ResponseTimeOfSmallSendRequests;
	LARGE_INTEGER	BytesOfSmallSendRequests;

	ULONG			NumberOfCompleteSendPackets;
	LARGE_INTEGER	CompleteTimeOfSendPackets;
	LARGE_INTEGER	BytesOfCompleteSendPackets;

	ULONG			NumberOfCompleteSmallSendPackets;
	LARGE_INTEGER	CompleteTimeOfSmallSendPackets;
	LARGE_INTEGER	BytesOfCompleteSmallSendPackets;

	ULONG			NumberOfCompleteLargeSendPackets;
	LARGE_INTEGER	CompleteTimeOfLargeSendPackets;
	LARGE_INTEGER	BytesOfCompleteLargeSendPackets;

	ULONG			NumberOfRecvPackets;
	LARGE_INTEGER	FreeTimeOfRecvPackets;
	LARGE_INTEGER	BytesOfRecvPackets;

	ULONG			NumberOfLargeRecvPackets;
	LARGE_INTEGER	FreeTimeOfLargeRecvPackets;
	LARGE_INTEGER	BytesOfLargeRecvPackets;

	ULONG			NumberOfSmallRecvPackets;
	LARGE_INTEGER	FreeTimeOfSmallRecvPackets;
	LARGE_INTEGER	BytesOfSmallRecvPackets;

#endif

#endif

#if DBG
    ULONG RefTypes[NUMBER_OF_DCREFS];
#endif

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    LIST_ENTRY Linkage;                   // links them on LpxDeviceList;

    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)
                                        
    LONG ReferenceCount;                // activity count/this provider.
    LONG CreateRefRemoved;              // has unload or unbind been called ?

    //
    // The queue of (currently receive only) IRPs waiting to complete.
    //

    LIST_ENTRY IrpCompletionQueue;

    //
    // This boolean is TRUE if either of the above two have ever
    // had anything on them.
    //

    BOOLEAN IndicationQueuesInUse;

    //
    // Following are protected by Global Device Context SpinLock
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this object.
                                        //  (used in KeAcquireSpinLock calls)

    //
    // the device context state, among open, closing
    //

    UCHAR State;

    //
    // Used when processing a STATUS_CLOSING indication.
    //

    WORK_QUEUE_ITEM StatusClosingQueueItem;

    //
    // The following queue holds free TP_ADDRESS objects available for allocation.
    //

    LIST_ENTRY AddressPool;

    //
    // These counters keep track of resources uses by TP_ADDRESS objects.
    //

    ULONG AddressAllocated;
    ULONG AddressInitAllocated;
    ULONG AddressMaxAllocated;
    ULONG AddressInUse;
    ULONG AddressMaxInUse;
    ULONG AddressExhausted;
    ULONG AddressTotal;
    ULONG AddressSamples;


    //
    // The following queue holds free TP_ADDRESS_FILE objects available for allocation.
    //

    LIST_ENTRY AddressFilePool;

    //
    // These counters keep track of resources uses by TP_ADDRESS_FILE objects.
    //

    ULONG AddressFileAllocated;
    ULONG AddressFileInitAllocated;
    ULONG AddressFileMaxAllocated;
    ULONG AddressFileInUse;
    ULONG AddressFileMaxInUse;
    ULONG AddressFileExhausted;
    ULONG AddressFileTotal;
    ULONG AddressFileSamples;


    //
    // The following queue holds free TP_CONNECTION objects available for allocation.
    //

    LIST_ENTRY ConnectionPool;

    //
    // These counters keep track of resources uses by TP_CONNECTION objects.
    //

    ULONG ConnectionAllocated;
    ULONG ConnectionInitAllocated;
    ULONG ConnectionMaxAllocated;
    ULONG ConnectionInUse;
    ULONG ConnectionMaxInUse;
    ULONG ConnectionExhausted;
    ULONG ConnectionTotal;
    ULONG ConnectionSamples;

    //
    // This holds the total memory allocated for the above structures.
    //

    ULONG MemoryUsage;
    ULONG MemoryLimit;


    //
    // The following field is a head of a list of TP_ADDRESS objects that
    // are defined for this transport provider.  To edit the list, you must
    // hold the spinlock of the device context object.
    //

    LIST_ENTRY AddressDatabase;        // list of defined transport addresses.

    //
    // When this hits thirty seconds we checked for stalled connections.
    //

    USHORT StalledConnectionCount;

    //
    // This queue contains receives that are in progress
    //

    LIST_ENTRY ReceiveInProgress;

    //
    // NDIS fields
    //

    //
    // following is used to keep adapter information.
    //

    NDIS_HANDLE NdisBindingHandle;

    //
    // The following fields are used for talking to NDIS. They keep information
    // for the NDIS wrapper to use when determining what pool to use for
    // allocating storage.
    //

    KSPIN_LOCK SendPoolListLock;            // protects these values
    PLPX_POOL_LIST_DESC SendPacketPoolDesc;
    KSPIN_LOCK RcvPoolListLock;            // protects these values
    PLPX_POOL_LIST_DESC ReceivePacketPoolDesc;
    NDIS_HANDLE NdisBufferPool;

	//
    // These are kept around for error logging.
    //

    ULONG SendPacketPoolSize;
    ULONG ReceivePacketPoolSize;
    ULONG MaxRequests;
    ULONG MaxLinks;
    ULONG MaxConnections;
    ULONG MaxAddressFiles;
    ULONG MaxAddresses;
    PWCHAR DeviceName;
    ULONG DeviceNameLength;

    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //

    LPX_NDIS_IDENTIFICATION MacInfo;    // MAC type and other info
    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header
    ULONG CurSendPacketSize;            // may be smaller for async
    USHORT RecommendedSendWindow;       // used for Async lines
    BOOLEAN EasilyDisconnected;         // TRUE over wireless nets.

    //
    // some MAC addresses we use in the transport
    //

    HARDWARE_ADDRESS LocalAddress;      // our local hardware address.
    HARDWARE_ADDRESS NetBIOSAddress;    // NetBIOS functional address, used for TR

    //
    // The reserved Netbios address; consists of 10 zeroes
    // followed by LocalAddress;
    //

    UCHAR ReservedNetBIOSAddress[NETBIOS_NAME_LENGTH];
    HANDLE TdiDeviceHandle;
    HANDLE ReservedAddressHandle;

    //
    // These are used while initializing the MAC driver.
    //

    KEVENT NdisRequestEvent;            // used for pended requests.
    NDIS_STATUS NdisRequestStatus;      // records request status.

    //
    // This next field maintains a unique number which can next be assigned
    // as a connection identifier.  It is incremented by one each time a
    // value is allocated.
    //

    USHORT UniqueIdentifier;            // starts at 0, wraps around 2^16-1.

    //
    // This contains the next unique indentified to use as
    // the FsContext in the file object associated with an
    // open of the control channel.
    //

    USHORT ControlChannelIdentifier;

    //
    // The following fields are used to implement the lightweight timer
    // system in the protocol provider.  Each TP_LINK object in the device
    // context's LinkDatabase contains three lightweight timers that are
    // serviced by a DPC routine, which receives control by kernel functions.
    // There is one kernel timer for this transport that is set
    // to go off at regular intervals.  This timer increments the Absolute time,
    // which is then used to compare against the timer queues. The timer queues
    // are ordered, so whenever the first element is not expired, the rest of
    // the queue is not expired. This allows us to have hundreds of timers
    // running with very low system overhead.
    // A value of -1 indicates that the timer is not active.
    //

    ULONG TimerState;                   // See the timer Macros in nbfprocs.h

    LARGE_INTEGER ShortTimerStart;      // when the short timer was set.
    KDPC ShortTimerSystemDpc;           // kernel DPC object, short timer.
    KTIMER ShortSystemTimer;            // kernel timer object, short timer.
    ULONG ShortAbsoluteTime;            // up-count timer ticks, short timer.
    KDPC LongTimerSystemDpc;            // kernel DPC object, long timer.
    KTIMER LongSystemTimer;             // kernel timer object, long timer.
    ULONG LongAbsoluteTime;             // up-count timer ticks, long timer.
    union _DC_ACTIVE {
      struct _DC_INDIVIDUAL {
        BOOLEAN ShortListActive;        // ShortList is not empty.
        BOOLEAN DataAckQueueActive;     // DataAckQueue is not empty.
        BOOLEAN LinkDeferredActive;     // LinkDeferred is not empty.
      } i;
      ULONG AnyActive;                  // used to check all four at once.
    } a;
    BOOLEAN ProcessingShortTimer;       // TRUE if we are in ScanShortTimer.
    KSPIN_LOCK TimerSpinLock;           // lock for following timer queues
    LIST_ENTRY ShortList;               // list of links waiting T1 or T2
    LIST_ENTRY LongList;                // list of links waiting Ti expire
    LIST_ENTRY PurgeList;               // list of links waiting LAT expire

    //
    // These fields are used on "easily disconnected" adapters.
    // Every time the long timer expires, it notes if there has
    // been any multicast traffic received. If there has not been,
    // it increments LongTimeoutsWithoutMulticast. Activity is
    // recorded by incrementing MulticastPacket when MC
    // packets are received, and zeroing it when the long timer
    // expires.
    //

    ULONG LongTimeoutsWithoutMulticast; // LongTimer timeouts since traffic.
    ULONG MulticastPacketCount;         // How many MC packets rcved, this timeout.

    //
    // This information is used to keep track of the speed of
    // the underlying medium.
    //

    ULONG MediumSpeed;                    // in units of 100 bytes/sec
    BOOLEAN MediumSpeedAccurate;          // if FALSE, can't use the link.

#if __LPX__

	//
	//	General Mac options supplied by underlying NIC drivers
	//

	ULONG	MacOptions;

#endif

    //
    // This is TRUE if we are on a UP system.
    //

    BOOLEAN UniProcessor;

    //
    // Counters for most of the statistics that LPX maintains;
    // some of these are kept elsewhere. Including the structure
    // itself wastes a little space but ensures that the alignment
    // inside the structure is correct.
    //

    TDI_PROVIDER_STATISTICS Statistics;

    //
    // Counters for "active" time.
    //

    LARGE_INTEGER LpxStartTime;

    //
    // This resource guards access to the ShareAccess
    // and SecurityDescriptor fields in addresses.
    //

    ERESOURCE AddressResource;

    //
    // This array is used to quickly dismiss UI frames that
    // are not destined for us. The count is the number
    // of addresses with that first letter that are registered
    // on this device.
    //

    UCHAR AddressCounts[256];

    //
    // This is to hold the underlying PDO of the device so
    // that we can answer DEVICE_RELATION IRPs from above
    //

    PVOID PnPContext;

    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

    TDI_PROVIDER_INFO Information;      // information about this provider.

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// device context state definitions
//

#define DEVICECONTEXT_STATE_OPENING  0x00
#define DEVICECONTEXT_STATE_OPEN     0x01
#define DEVICECONTEXT_STATE_DOWN     0x02
#define DEVICECONTEXT_STATE_STOPPING 0x03

//
// device context PnP Flags
//

// #define DEVICECONTEXT_FLAGS_REMOVING     0x01
// #define DEVICECONTEXT_FLAGS_POWERING_OFF 0x02
// #define DEVICECONTEXT_FLAGS_POWERED_DOWN 0x04


#define MAGIC_BULLET_FOOD 0x04


//
// Structure used to interpret the TransportReserved part in the NET_PNP_EVENT
//

typedef struct _NET_PNP_EVENT_RESERVED {
    PWORK_QUEUE_ITEM PnPWorkItem;
    PDEVICE_CONTEXT DeviceContext;
} NET_PNP_EVENT_RESERVED, *PNET_PNP_EVENT_RESERVED;

#if 0

//
// Deferred receive complete context
//

typedef	struct _RX_COMP_DPC_CTX {

	KDPC	Dpc;
	PDEVICE_CONTEXT	DeviceContext;
	LIST_ENTRY		ReceivedPackets;

} RX_COMP_DPC_CTX, *PRX_COMP_DPC_CTX;

#endif

#endif // def _LPXTYPES_


