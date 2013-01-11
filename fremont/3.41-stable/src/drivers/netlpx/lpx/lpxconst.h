/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

This header file defines manifest constants for the NT LPX transport
provider.

--*/

#ifndef _LPXCONST_
#define _LPXCONST_



//
// DEBUGGING SUPPORT.  DBG is a macro that is turned on at compile time
// to enable debugging code in the system.  If this is turned on, then
// you can use the IF_LPXDBG(flags) macro in the LPX code to selectively
// enable a piece of debugging code in the transport.  This macro tests
// LpxDebug, a global ULONG defined in LPXDRVR.C.
//

#if DBG

#if __LPX__

#define LPX_DEBUG_SENDENG       0x00000001      // sendeng.c debugging.
#define LPX_DEBUG_RCVENG        0x00000002      // rcveng.c debugging.
#define LPX_DEBUG_IFRAMES       0x00000004      // displays sent/rec'd iframes.
#define LPX_DEBUG_UFRAMES       0x00000008      // displays sent/rec'd uframes.
#define LPX_DEBUG_DLCFRAMES     0x00000010      // displays sent/rec'd dlc frames.
#define LPX_DEBUG_ADDRESS       0x00000020      // address.c debugging.
#define LPX_DEBUG_CONNECT       0x00000040      // connect.c debugging.
#define LPX_DEBUG_CONNOBJ       0x00000080      // connobj.c debugging.
#define LPX_DEBUG_DEVCTX        0x00000100      // devctx.c debugging.
#define LPX_DEBUG_DLC           0x00000200      // dlc.c data link engine debugging.
#define LPX_DEBUG_PKTLOG        0x00000400      // used to debug packet logging
#define LPX_DEBUG_PNP           0x00000800      // used in debugging PnP functions
#define LPX_DEBUG_FRAMECON      0x00001000      // framecon.c debugging.
#define LPX_DEBUG_FRAMESND      0x00002000      // framesnd.c debugging.
#define LPX_DEBUG_DYNAMIC       0x00004000      // dynamic allocation debugging.
#define LPX_DEBUG_LINK          0x00008000      // link.c debugging.
#define LPX_DEBUG_RESOURCE      0x00010000      // resource allocation debugging.
#define LPX_DEBUG_DISPATCH      0x00020000      // IRP request dispatching.
#define LPX_DEBUG_PACKET        0x00040000      // packet.c debugging.
#define LPX_DEBUG_REQUEST       0x00080000      // request.c debugging.
#define LPX_DEBUG_TIMER         0x00100000      // timer.c debugging.
#define LPX_DEBUG_DATAGRAMS     0x00200000      // datagram send/receive
#define LPX_DEBUG_REGISTRY      0x00400000      // registry access.
#define LPX_DEBUG_NDIS          0x00800000      // NDIS related information
#define LPX_DEBUG_LINKTREE      0x01000000      // Link splay tree debugging
#define LPX_DEBUG_TEARDOWN      0x02000000      // link/connection teardown info
#define LPX_DEBUG_REFCOUNTS     0x04000000      // link/connection ref/deref information
#define LPX_DEBUG_IRP           0x08000000      // irp completion debugging
#define LPX_DEBUG_SETUP         0x10000000      // debug session setup

//
// past here are debug things that are really frequent; don't use them
// unless you want LOTS of output
//
#define LPX_DEBUG_TIMERDPC      0x20000000      // the timer DPC
#define LPX_DEBUG_PKTCONTENTS   0x40000000      // dump packet contents in dbg
#define LPX_DEBUG_TRACKTDI      0x80000000      // store tdi info when set

#define LPX_DEBUG_CURRENT_IRQL	0x0000000100000000
#define LPX_DEBUG_ERROR			0x0000000200000000
#define LPX_DEBUG_TEMP			0x0000000400000000
#define LPX_DEBUG_TEST			0x8000000000000000

extern ULONGLONG LpxDebug;

extern BOOLEAN LpxDisconnectDebug;              // in LPXDRVR.C.

#endif

#endif

//
// some convenient constants used for timing. All values are in clock ticks.
//

#define MICROSECONDS 10
#define MILLISECONDS 10000              // MICROSECONDS*1000
#define SECONDS 10000000                // MILLISECONDS*1000


//
// MAJOR PROTOCOL IDENTIFIERS THAT CHARACTERIZE THIS DRIVER.
//

#if __LPX__
#define LPX_DEVICE_NAME         L"\\Device\\Lpx"// name of our driver.
#define LPX_NAME                L"Lpx"          // name for protocol chars.
#endif

#define DSAP_NETBIOS_OVER_LLC   0xf0            // NETBEUI always has DSAP 0xf0.
#define PSAP_LLC                0               // LLC always runs over PSAP 0.
#define MAX_SOURCE_ROUTE_LENGTH 32              // max. bytes of SR. info.
#define MAX_NETWORK_NAME_LENGTH 128             // # bytes in netname in TP_ADDRESS.
#define MAX_USER_PACKET_DATA    1500            // max. user bytes per DFM/DOL.

#define LPX_FILE_TYPE_CONTROL   (ULONG)0x4701   // file is type control


//
// MAJOR CONFIGURATION PARAMETERS THAT WILL BE MOVED TO THE INIT-LARGE_INTEGER
// CONFIGURATION MANAGER.
//

#define MAX_CONNECTIONS        10
#define MAX_ADDRESSFILES       10
#define MAX_ADDRESSES          10


#define ETHERNET_HEADER_SIZE      14    // used for current NDIS compliance
#define ETHERNET_PACKET_SIZE    1514

#define MAX_DEFERRED_TRAVERSES     6    // number of times we can go through
                                        // the deferred operations queue and
                                        // not do anything without causing an
                                        // error indication


//
// NETBIOS PROTOCOL CONSTANTS.
//

#define NETBIOS_NAME_LENGTH     16

//
// DATA LINK PROTOCOL CONSTANTS.
//
// There are two timers, short and long. T1, T2, and the purge
// timer are run off of the short timer, Ti and the adaptive timer
// is run off of the long one.
//

#define LONG_TIMER_DELTA         (1*SECONDS)

#define DLC_TIMER_ACCURACY       8    // << between BaseT1Timeout and CurrentT1Timeout

#if __LPX__

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

#endif

//
// Number of TDI resources that we report.
//

#define LPX_TDI_RESOURCES      9

//
// Resource IDs for query and error logging.
//

#define ADDRESS_RESOURCE_ID              12
#define ADDRESS_FILE_RESOURCE_ID         13
#define CONNECTION_RESOURCE_ID           14

//
// memory management additions
//

//
// Fake IOCTLs used for kernel mode testing.
//

#define IOCTL_LPX_BASE FILE_DEVICE_TRANSPORT

#define _LPX_CONTROL_CODE(request,method) \
                ((IOCTL_LPX_BASE)<<16 | (request<<2) | method)

#define IOCTL_TDI_SEND_TEST      _LPX_CONTROL_CODE(26,0)
#define IOCTL_TDI_RECEIVE_TEST   _LPX_CONTROL_CODE(27,0)
#define IOCTL_TDI_SERVER_TEST    _LPX_CONTROL_CODE(28,0)

//
// More debugging stuff
//

#define LPX_CONNECTION_SIGNATURE     ((CSHORT)0x4704)
#define LPX_ADDRESSFILE_SIGNATURE    ((CSHORT)0x4705)
#define LPX_ADDRESS_SIGNATURE        ((CSHORT)0x4706)
#define LPX_DEVICE_CONTEXT_SIGNATURE ((CSHORT)0x4707)

#if DBG
extern PVOID * LpxConnectionTable;
extern PVOID * LpxAddressFileTable;
extern PVOID * LpxAddressTable;
#endif

//
// Tags used in Memory Debugging
//

#define LPX_MEM_TAG_GENERAL_USE         ' XPL'

#define LPX_MEM_TAG_TP_ADDRESS          'aXPL'
#define LPX_MEM_TAG_TP_CONNECTION       'cXPL'
#define LPX_MEM_TAG_DEVICE_EXPORT       'eXPL'
#define LPX_MEM_TAG_TP_ADDRESS_FILE     'fXPL'
#define LPX_MEM_TAG_REGISTRY_PATH       'gXPL'

#define LPX_MEM_TAG_TDI_CONNECTION_INFO 'iXPL'

#define LPX_MEM_TAG_NETBIOS_NAME        'nXPL'
#define LPX_MEM_TAG_CONFIG_DATA         'oXPL'
#define LPX_MEM_TAG_TDI_QUERY_BUFFER    'qXPL'
#define LPX_MEM_TAG_TDI_PROVIDER_STATS  'sXPL'
#define LPX_MEM_TAG_CONNECTION_TABLE    'tXPL'

#define LPX_MEM_TAG_WORK_ITEM           'wXPL'

#define LPX_MEM_TAG_DEVICE_PDO          'zXPL'

#define LPX_MEM_TAG_DGRAM_DATA          'dXPL'
#define LPX_MEM_TAG_PRIVATE				'pRPL'

#define LPX_MEM_TAG_RXCOMPDPC_CTX		'crPL'

#define LPX_MEM_TAG_EXTERNAL_RESERVED	'rxPL'

#endif // _LPXCONST_
