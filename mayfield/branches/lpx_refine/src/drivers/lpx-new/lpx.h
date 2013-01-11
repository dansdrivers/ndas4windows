/*++

Copyright (c) 2000-2005    XIMETA Corp.

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


#define  TRANSMIT_PACKETS    1024
#define LPX_PORTASSIGN_BEGIN   0x4000
#define PNPMOUDLEPORT        10002
#define LENGTH_8022LLCSNAP  8
#define MAX_ALLOWED_SEQ        1024
#define ETH_P_LPX            0x88AD        // 0x8200    

// Mod by jgahn.
#undef    NdisAllocateMemory
#define    NdisAllocateMemory(a,b) NdisAllocateMemoryWithTag(a,b,'-xpL')

#undef    NdisFreeMemory
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
  SMP_CLOSING,     /* now a valid state */
  SMP_LAST
} SMP_STATE, *PSMP_STATE;


#define SMP_MAX_FLIGHT 2048

#define    SMP_RECEIVE_SHUTDOWN    0x01
#define    SMP_SEND_SHUTDOWN        0x02
#define    SMP_SHUTDOWN_MASK        0x03

struct _LPX_STATE;

typedef struct _SERVICE_POINT {
    KSPIN_LOCK                SpSpinLock;    // added by hootch 09072003
    SMP_STATE                SmpState;  // Guarded by  SpSpinLock. To do: Remove this. modify to use access function instead.
    struct _LPX_STATE*  State;      // Guarded by  SpSpinLock
    UCHAR                    Shutdown;  // SMP_xx_SHUTDOWN

    //
    //    ServicePointListEntry is protected by address->SpSpinLock
    //      Linked to TP_ADDRESS->ConnectionServicePointList
    //
    LIST_ENTRY                ServicePointListEntry;

    PIRP                    DisconnectIrp;
    PIRP                    ConnectIrp;
    PIRP                    ListenIrp;

    LIST_ENTRY                ReceiveIrpList;
    KSPIN_LOCK                ReceiveIrpQSpinLock;

    LPX_ADDRESS                SourceAddress;   // remove this. Address of my network interface
    LPX_ADDRESS                DestinationAddress; // Address of peer

    struct _TP_ADDRESS        *Address;     // remove this
    struct _TP_CONNECTION    *Connection; // remove this

    // 052303 jgahn
    LONG                    lDisconnectHandlerCalled;

 
    //    use ULONG type instead of USHORT to call InterlockedXXXX() with
    LONG            Sequence;
    LONG            FinSequence;
    LONG            RemoteSequence;
    LONG            RemoteAck;        // used only in SmpDoReceive() and SendTest()

    LONG            EarlySequence; // For debugging. use for early packet loss detection
    
    KTIMER            SmpTimer;
    KDPC            SmpTimerDpc;

    LONG            TimerReason;
#define    SMP_SENDIBLE            0x0001
#define    SMP_RETRANSMIT_ACKED    0x0002

    //
    //    timeout counters
    //

    //    protect following timeout counters.
    // added by hootch    09062003
    KSPIN_LOCK        TimeCounterSpinLock ;
    LARGE_INTEGER    TimeWaitTimeOut;
    LARGE_INTEGER    AliveTimeOut;
    LARGE_INTEGER    RetransmitTimeOut;
    LARGE_INTEGER    ConnectTimeOut;
    LARGE_INTEGER    SmpTimerExpire;
    // Added by jgahn.
    LARGE_INTEGER    RetransmitEndTime;    // End of Retransmit...

    LONG            LastRetransmitSequence;

    LONG            AliveRetries;
    LONG            Retransmits;
    
    LONG            MaxFlights;

    //
    //    queues
    //
    KSPIN_LOCK        RcvDataQSpinLock;        // added by hootch    09062003
    LIST_ENTRY        RcvDataQueue;

    KSPIN_LOCK        RetransmitQSpinLock;    // added by hootch    09062003
    LIST_ENTRY        RetransmitQueue;

    KSPIN_LOCK        WriteQSpinLock;        // added by hootch    09062003
    LIST_ENTRY        WriteQueue;

    UCHAR            ServerTag;

    //
    //    DPC to do actual jobs instead of ReceiveCompeletion & Timer
    //
    //    added by hootch 09072003 to build a DPC worker
    KDPC            SmpWorkDpc;
    LONG            RequestCnt;
    KSPIN_LOCK        SmpWorkDpcLock;

//        KSPIN_LOCK        SendQSpinLock;
//        LIST_ENTRY        SendQueue;

    KSPIN_LOCK        ReceiveQSpinLock;
    LIST_ENTRY        ReceiveQueue;

    KSPIN_LOCK        ReceiveReorderQSpinLock;
    LIST_ENTRY        ReceiveReorderQueue;

} SERVICE_POINT, *PSERVICE_POINT;

typedef UNALIGNED struct __ETHERNET_HEADER
{
    UCHAR    DestinationAddress[ETHERNET_ADDRESS_LENGTH];
    UCHAR    SourceAddress[ETHERNET_ADDRESS_LENGTH];
    USHORT    Type;

} ETHERNET_HEADER, *PETHERNET_HEADER;

typedef /*UNALIGNED*/ struct _LPX_RESERVED 
{
#define SEND_TYPE                0
#define    RECEIVE_TYPE            1
    UCHAR        Type;
    LIST_ENTRY    ListElement;

    PVOID        LpxSmpHeader;
    USHORT        DataLength;
    
    union {
        struct {
            UINT            PacketDataOffset;
            UINT            ReorderCount;   // Increase when handled as reordered packet. Drop if larger than certain value
        };// for ReceivePacketHandler;
        struct {
            LONG            Cloned;    // mod by hootch 09052003
            PIO_STACK_LOCATION    IrpSp;
            ETHERNET_HEADER        EthernetHeader;
            LONG            Retransmits;
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

#define HZ                (LONGLONG)(10 * 1000 * 1000)
#define MSEC_TO_HZ(ms)   (((LONGLONG)(10* 1000)) * ms)

/* Time units are in mili-seconds */
#define DEFAULT_CONNECTION_TIMEOUT        (2000)

#define DEFAULT_SMP_TIMEOUT                (50)
#define DEFAULT_TIME_WAIT_INTERVAL        (1000)
#define DEFAULT_ALIVE_INTERVAL          (1000)

#define DEFAULT_RETRANSMIT_DELAY            (200)
#define DEFAULT_MAX_RETRANSMIT_DELAY    (1000)

#define DEFAULT_MAX_ALIVE_COUNT         (8)
#define DEFAULT_MAX_RETRANSMIT_TIME        (8000)

/* These values time units are in 100ns(1/HZ). Pre-calcurated in lpxcnfg.c */
extern LONGLONG   LpxConnectionTimeout;
extern LONGLONG   LpxSmpTimeout;
extern LONGLONG   LpxWaitInterval;
extern LONGLONG   LpxAliveInterval;
extern LONGLONG   LpxRetransmitDelay;
extern LONGLONG   LpxMaxRetransmitDelay;
extern LONG   LpxMaxAliveCount;
extern LONGLONG   LpxMaxRetransmitTime;


#define LPX_HOST_DGMSG_ID        0

#include <pshpack1.h>

typedef UNALIGNED struct _LPX_HEADER2 
{
    union {
        struct {
            UCHAR    PacketSizeMsb:6;
            UCHAR    LpxType:2;
#define LPX_TYPE_RAW        0
#define LPX_TYPE_DATAGRAM    2
#define LPX_TYPE_STREAM        3
#define LPX_TYPE_MASK        (USHORT)0x00C0
            UCHAR    PacketSizeLsb;
        };
        USHORT        PacketSize;
    };

    USHORT    DestinationPort;
    USHORT    SourcePort;

//    LPX_ADDRESS    SourceAddress;
//    LPX_ADDRESS    DestinationAddress;

    union {
        USHORT    Reserved[5];
        struct {
            USHORT    Lsctl;
#define LSCTL_CONNREQ        (USHORT)0x0001
#define LSCTL_DATA            (USHORT)0x0002
#define LSCTL_DISCONNREQ    (USHORT)0x0004
#define LSCTL_ACKREQ        (USHORT)0x0008
#define LSCTL_ACK            (USHORT)0x1000
            USHORT    Sequence;
            USHORT    AckSequence;
            UCHAR    ServerTag;
            UCHAR    ReservedS1[3];
        };

        struct {
            USHORT    MessageId;
            USHORT    MessageLength;
            USHORT    FragmentId;
            USHORT    FragmentLength;
            USHORT    ResevedU1;
        };
    };

} LPX_HEADER2, *PLPX_HEADER2;

extern ULONG                ulPacketDropRate;   // number of packet to drop over 1000 packets.

#include <poppack.h>

#define    SMP_SYS_PKT_LEN    (sizeof(LPX_HEADER2))

#endif

