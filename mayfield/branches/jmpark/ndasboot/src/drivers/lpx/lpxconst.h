/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

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

#define LPX_DEBUG_SENDENG       0x00000001      // sendeng.c debugging.
#define LPX_DEBUG_RCVENG        0x00000002      // rcveng.c debugging.
#define LPX_DEBUG_ADDRESS       0x00000020      // address.c debugging.
#define LPX_DEBUG_CONNECT       0x00000040      // connect.c debugging.
#define LPX_DEBUG_DEVCTX        0x00000100      // devctx.c debugging.

#define LPX_DEBUG_PNP           0x00000800      // used in debugging PnP functions

#define LPX_DEBUG_DYNAMIC       0x00004000      // dynamic allocation debugging.

#define LPX_DEBUG_RESOURCE      0x00010000      // resource allocation debugging.
#define LPX_DEBUG_DISPATCH      0x00020000      // IRP request dispatching.

#define LPX_DEBUG_REQUEST       0x00080000      // request.c debugging.

#define LPX_DEBUG_DATAGRAMS     0x00200000      // datagram send/receive
#define LPX_DEBUG_REGISTRY      0x00400000      // registry access.
#define LPX_DEBUG_NDIS          0x00800000      // NDIS related information

#define LPX_DEBUG_TEARDOWN      0x02000000      // link/connection teardown info
#define LPX_DEBUG_REFCOUNTS     0x04000000      // link/connection ref/deref information

extern ULONG LpxDebug;                          // in LPXDRVR.C.
   
#endif


//
// MAJOR PROTOCOL IDENTIFIERS THAT CHARACTERIZE THIS DRIVER.
//

#define LPX_DEVICE_NAME         L"\\Device\\Lpx"// name of our driver.
#define LPX_NAME                L"Lpx"          // name for protocol chars.
 
#define LPX_FILE_TYPE_CONTROL   (ULONG)0x1919   // file is type control


//
// GENERAL CAPABILITIES STATEMENTS THAT CANNOT CHANGE.
//
 
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

// Number of TDI resources that we report.
//

#define LPX_TDI_RESOURCES      0


//
// More debugging stuff
//
 
#define LPX_CONNECTION_SIGNATURE     ((CSHORT)0x4704)
#define LPX_ADDRESSFILE_SIGNATURE    ((CSHORT)0x4705)
#define LPX_ADDRESS_SIGNATURE        ((CSHORT)0x4706)
#define LPX_DEVICE_CONTEXT_SIGNATURE ((CSHORT)0x4707)
#define LPX_CONTROL_CONTEXT_SIGNATURE ((CSHORT)0x4709)

//
// Tags used in Memory Debugging
//
#define LPX_MEM_TAG_TP_ADDRESS          'aXPL'
#define LPX_MEM_TAG_TP_CONNECTION       'cXPL'
#define LPX_MEM_TAG_DEVICE_EXPORT       'eXPL'
#define LPX_MEM_TAG_TP_ADDRESS_FILE     'fXPL'
#define LPX_MEM_TAG_REGISTRY_PATH       'gXPL'

#define LPX_MEM_TAG_LPX_ADDRESS_NAME    'nXPL'
#define LPX_MEM_TAG_CONFIG_DATA         'oXPL'
#define LPX_MEM_TAG_TDI_QUERY_BUFFER    'qXPL'

#define LPX_MEM_TAG_WORK_ITEM           'wXPL'

#define LPX_MEM_TAG_DEVICE_PDO          'zXPL'
#define LPX_MEM_TAG_DGRAM_DATA        'dXPL'

#endif // _LPXCONST_

