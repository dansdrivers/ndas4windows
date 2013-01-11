/*++

Copyright (c) 2002-2007 XIMETA, Inc. All rights reserved.

    This module contains necessary routines for the Windows Sockets
    Helper DLL.  This DLL provides the transport-specific support necessary
    for the Windows Sockets DLL to use TCP/IP as a transport.

--*/

#define UNICODE
#include "wshsmple.h"
#include <stdio.h>
#include <ctype.h>
#include <wchar.h>
#include <tdi.h>

#include <winsock2.h>
#include "wsahelp.h"

#include <tdiinfo.h>
#include <socketlpx.h>

#define LPXTCP_DEVICE		SOCKETLPX_DEVICE_NAME
#define LPXUDP_DEVICE		SOCKETLPX_DEVICE_NAME
#define LPXTCP_DOSDEVICE	SOCKETLPX_DOSDEVICE_NAME
#define LPXUDP_DOSDEVICE	SOCKETLPX_DOSDEVICE_NAME

#include <basetyps.h>
#include <nspapi.h>
#include <nspapip.h>

///////////////////////////////////////////////////
#define LPX_TCP_NAME L"TCP/LPX"
#define LPX_UDP_NAME L"UDP/LPX"

#define IS_DGRAM_SOCK(type)  (((type) == SOCK_DGRAM) || ((type) == SOCK_RAW))

//
// Define valid flags for WSHOpenSocket2().
//

#define VALID_TCP_FLAGS         (WSA_FLAG_OVERLAPPED)

#define VALID_UDP_FLAGS         (WSA_FLAG_OVERLAPPED |          \
                                 WSA_FLAG_MULTIPOINT_C_LEAF |   \
                                 WSA_FLAG_MULTIPOINT_D_LEAF)

//
// Structure and variables to define the triples supported by TCP/IP. The
// first entry of each array is considered the canonical triple for
// that socket type; the other entries are synonyms for the first.
//

typedef struct _MAPPING_TRIPLE {
    INT AddressFamily;
    INT SocketType;
    INT Protocol;
} MAPPING_TRIPLE, *PMAPPING_TRIPLE;

MAPPING_TRIPLE TcpMappingTriples[] = { 
	AF_LPX,    SOCK_STREAM, LPXPROTO_STREAM,
	AF_LPX,    SOCK_STREAM, 0,
	AF_LPX,    0,           LPXPROTO_STREAM,
	AF_UNSPEC, SOCK_STREAM, LPXPROTO_STREAM,
	AF_UNSPEC, SOCK_STREAM, 0,
	AF_UNSPEC, 0,           LPXPROTO_STREAM,
};

MAPPING_TRIPLE UdpMappingTriples[] = { 
	AF_LPX,    SOCK_DGRAM, LPXPROTO_DGRAM,
	AF_LPX,    SOCK_DGRAM, 0,
	AF_LPX,    0,          LPXPROTO_DGRAM,
	AF_UNSPEC, SOCK_DGRAM, LPXPROTO_DGRAM,
	AF_UNSPEC, SOCK_DGRAM, 0,
	AF_UNSPEC, 0,          LPXPROTO_DGRAM,
};

MAPPING_TRIPLE RawMappingTriples[] = {
	AF_LPX,    SOCK_RAW, 0, 
	AF_UNSPEC, SOCK_RAW, 0,
};

//
// Winsock 2 WSAPROTOCOL_INFO structures for all supported protocols.
//

#define WINSOCK_SPI_VERSION 2
#define UDP_MESSAGE_SIZE  (65535-68)

WSAPROTOCOL_INFOW Winsock2Protocols[] =
    {
        //
        // TCP
        //

        {
            XP1_GUARANTEED_DELIVERY                 // dwServiceFlags1
                | XP1_GUARANTEED_ORDER
                | XP1_GRACEFUL_CLOSE
//                | XP1_EXPEDITED_DATA
                | XP1_IFS_HANDLES,
            0,                                      // dwServiceFlags2
            0,                                      // dwServiceFlags3
            0,                                      // dwServiceFlags4
            PFL_MATCHES_PROTOCOL_ZERO,              // dwProviderFlags
            {                                       // gProviderId
                0, 0, 0,
                { 0, 0, 0, 0, 0, 0, 0, 0 }
            },
            0,                                      // dwCatalogEntryId
            {                                       // ProtocolChain
                BASE_PROTOCOL,                          // ChainLen
                { 0, 0, 0, 0, 0, 0, 0 }                 // ChainEntries
            },
            WINSOCK_SPI_VERSION,                    // iVersion
            AF_LPX,                                // iAddressFamily
            sizeof(SOCKADDR_LPX),                    // iMaxSockAddr
            sizeof(SOCKADDR_LPX),                    // iMinSockAddr
            SOCK_STREAM,                            // iSocketType
            LPXPROTO_STREAM,                            // iProtocol
            0,                                      // iProtocolMaxOffset
            BIGENDIAN,                              // iNetworkByteOrder
            SECURITY_PROTOCOL_NONE,                 // iSecurityScheme
            0,                                      // dwMessageSize
            0,                                      // dwProviderReserved
            L"MSAFD Lpx [TCP/LPX]"                 // szProtocol
        },

        //
        // UDP
        //

        {
            XP1_CONNECTIONLESS                      // dwServiceFlags1
                | XP1_MESSAGE_ORIENTED
                | XP1_SUPPORT_BROADCAST
                | XP1_SUPPORT_MULTIPOINT
                | XP1_IFS_HANDLES,
            0,                                      // dwServiceFlags2
            0,                                      // dwServiceFlags3
            0,                                      // dwServiceFlags4
            PFL_MATCHES_PROTOCOL_ZERO,              // dwProviderFlags
            {                                       // gProviderId
                0, 0, 0,
                { 0, 0, 0, 0, 0, 0, 0, 0 }
            },
            0,                                      // dwCatalogEntryId
            {                                       // ProtocolChain
                BASE_PROTOCOL,                          // ChainLen
                { 0, 0, 0, 0, 0, 0, 0 }                 // ChainEntries
            },
            WINSOCK_SPI_VERSION,                    // iVersion
            AF_LPX,                                // iAddressFamily
            sizeof(SOCKADDR_LPX),                    // iMaxSockAddr
            sizeof(SOCKADDR_LPX),                    // iMinSockAddr
            SOCK_DGRAM,                             // iSocketType
            LPXPROTO_DGRAM,                            // iProtocol
            0,                                      // iProtocolMaxOffset
            BIGENDIAN,                              // iNetworkByteOrder
            SECURITY_PROTOCOL_NONE,                 // iSecurityScheme
            UDP_MESSAGE_SIZE,                       // dwMessageSize
            0,                                      // dwProviderReserved
            L"MSAFD Lpx [UDP/LPX]"                 // szProtocol
        }

    };

#define NUM_WINSOCK2_PROTOCOLS  \
            ( sizeof(Winsock2Protocols) / sizeof(Winsock2Protocols[0]) )

//
// The GUID identifying this provider.
//

// {4E27AE72-4DA8-41ee-98F8-3E1ED5F3FD24}
static const GUID LpxProviderGuid = 
{ 0x4e27ae72, 0x4da8, 0x41ee, { 0x98, 0xf8, 0x3e, 0x1e, 0xd5, 0xf3, 0xfd, 0x24 } };


#define TL_INSTANCE 0

//
// Forward declarations of internal routines.
//

BOOLEAN
IsTripleInList (
    IN PMAPPING_TRIPLE List,
    IN ULONG ListLength,
    IN INT AddressFamily,
    IN INT SocketType,
    IN INT Protocol
    );

//
// The socket context structure for this DLL.  Each open TCP/IP socket
// will have one of these context structures, which is used to maintain
// information about the socket.
//

typedef struct _WSHLPX_SOCKET_CONTEXT {
    INT     AddressFamily;
    INT     SocketType;
    INT     Protocol;
    INT     ReceiveBufferSize;
    DWORD   Flags;
//    INT     MulticastTtl;
//    UCHAR   IpTtl;
//    UCHAR   IpTos;
//    UCHAR   IpDontFragment;
//    UCHAR   IpOptionsLength;
//    UCHAR  *IpOptions;
//    ULONG   MulticastInterface;
//    BOOLEAN MulticastLoopback;
//    BOOLEAN KeepAlive;
//    BOOLEAN DontRoute;
//    BOOLEAN NoDelay;
//    BOOLEAN BsdUrgent;
//    BOOLEAN MultipointLeaf;
//    BOOLEAN UdpNoChecksum;
//    BOOLEAN Reserved3;
//    IN_ADDR MultipointTarget;
//    HANDLE MultipointRootTdiAddressHandle;

} WSHLPX_SOCKET_CONTEXT, *PWSHLPX_SOCKET_CONTEXT;

#define DEFAULT_RECEIVE_BUFFER_SIZE 8192
//#define DEFAULT_MULTICAST_TTL 1
//#define DEFAULT_MULTICAST_INTERFACE INADDR_ANY
//#define DEFAULT_MULTICAST_LOOPBACK TRUE

//#define DEFAULT_IP_TTL 32
//#define DEFAULT_IP_TOS 0


#if DBG

#undef DebugPrint
#define DebugPrint(x) WSHDebugPrint x

#ifndef WSHLPX_DEBUG_LEVEL
#define WSHLPX_DEBUG_LEVEL 1
#endif

ULONG	DebugLevel = WSHLPX_DEBUG_LEVEL;

#define DEBUG_BUFFER_LENGTH	256
CHAR	MiniBuffer[DEBUG_BUFFER_LENGTH + 1];

VOID
WSHDebugPrint(
			  IN ULONG		DebugPrintLevel,
			  IN LPSTR		DebugMessage,
			  ...
			  )
{
    va_list ap;
	
    va_start(ap, DebugMessage);
	
    if (DebugPrintLevel <= DebugLevel) {
		
        _vsnprintf(&MiniBuffer[0], DEBUG_BUFFER_LENGTH, DebugMessage, ap);

        OutputDebugStringA((LPCSTR)&MiniBuffer[0]);
    }
	
    va_end(ap);
}

#else

#define DebugPrint(x)

#endif


BOOL
APIENTRY
DllMain (
    IN PVOID DllHandle,
    IN ULONG Reason,
    IN PVOID Context OPTIONAL
    )
{

	DebugPrint((2, "[WshLpx]DllMain: Entered. Reason = %x Compiled %s %s\n", Reason, __DATE__, __TIME__));

    switch ( Reason ) {

    case DLL_PROCESS_ATTACH:

        //
        // We don't need to receive thread attach and detach
        // notifications, so disable them to help application
        // performance.
        //

        DisableThreadLibraryCalls( DllHandle );

        return TRUE;

    case DLL_THREAD_ATTACH:

        break;

    case DLL_PROCESS_DETACH:

        break;

    case DLL_THREAD_DETACH:

        break;
    }

    return TRUE;

} // SockInitialize

INT
WSHGetSockaddrType (
    IN PSOCKADDR Sockaddr,
    IN DWORD SockaddrLength,
    OUT PSOCKADDR_INFO SockaddrInfo
    )

/*++

Routine Description:

    This routine parses a sockaddr to determine the type of the
    machine address and endpoint address portions of the sockaddr.
    This is called by the winsock DLL whenever it needs to interpret
    a sockaddr.

Arguments:

    Sockaddr - a pointer to the sockaddr structure to evaluate.

    SockaddrLength - the number of bytes in the sockaddr structure.

    SockaddrInfo - a pointer to a structure that will receive information
        about the specified sockaddr.


Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    SOCKADDR_LPX *sockaddr = (PSOCKADDR_LPX)Sockaddr;
    UCHAR ZeroNode[6] = {0};
    UCHAR BroadcastNode[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    
    DebugPrint((2, "[WshLpx]WSHGetSockaddrType: Entered SockaddrLength = %x\n", SockaddrLength));

    //
    // Make sure that the address family is correct.
    //

    if ( sockaddr->sin_family != AF_LPX ) 
	{
        return WSAEAFNOSUPPORT;
    }

    //
    // Make sure that the length is correct.
    //

    if ( SockaddrLength < sizeof(SOCKADDR_LPX) ) 
	{
        return WSAEFAULT;
    }

    if (RtlCompareMemory(sockaddr->LpxAddress.Node, ZeroNode, 6)==6) 
	{
        SockaddrInfo->AddressInfo = SockaddrAddressInfoWildcard;
    }
	else if (RtlCompareMemory(sockaddr->LpxAddress.Node, BroadcastNode, 6)==6) 
	{
        SockaddrInfo->AddressInfo = SockaddrAddressInfoBroadcast;
    }
	else 
	{
        SockaddrInfo->AddressInfo = SockaddrAddressInfoNormal;
    }

    if(sockaddr->LpxAddress.Port == 0 ) 
	{
        SockaddrInfo->EndpointInfo = SockaddrEndpointInfoWildcard;
    }
	else 
	{
        SockaddrInfo->EndpointInfo = SockaddrEndpointInfoNormal;
    }

    return NO_ERROR;

} // WSHGetSockaddrType


INT
WSHGetSocketInformation (
    IN PVOID HelperDllSocketContext,
    IN SOCKET SocketHandle,
    IN HANDLE TdiAddressObjectHandle,
    IN HANDLE TdiConnectionObjectHandle,
    IN INT Level,
    IN INT OptionName,
    OUT PCHAR OptionValue,
    OUT PINT OptionLength
    )

/*++

Routine Description:

    This routine retrieves information about a socket for those socket
    options supported in this helper DLL.  The options supported here
    are SO_KEEPALIVE, SO_DONTROUTE, and TCP_EXPEDITED_1122.  This routine is
    called by the winsock DLL when a level/option name combination is
    passed to getsockopt() that the winsock DLL does not understand.

Arguments:

    HelperDllSocketContext - the context pointer returned from
        WSHOpenSocket().

    SocketHandle - the handle of the socket for which we're getting
        information.

    TdiAddressObjectHandle - the TDI address object of the socket, if
        any.  If the socket is not yet bound to an address, then
        it does not have a TDI address object and this parameter
        will be NULL.

    TdiConnectionObjectHandle - the TDI connection object of the socket,
        if any.  If the socket is not yet connected, then it does not
        have a TDI connection object and this parameter will be NULL.

    Level - the level parameter passed to getsockopt().

    OptionName - the optname parameter passed to getsockopt().

    OptionValue - the optval parameter passed to getsockopt().

    OptionLength - the optlen parameter passed to getsockopt().

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    PWSHLPX_SOCKET_CONTEXT context = HelperDllSocketContext;

    UNREFERENCED_PARAMETER( SocketHandle );
    UNREFERENCED_PARAMETER( TdiAddressObjectHandle );
    UNREFERENCED_PARAMETER( TdiConnectionObjectHandle );

	DebugPrint((2, "[WshLpx]WSHGetSocketInformation: Entered Level = %x OptionName = %d\n", 
		Level, OptionName));

    //
    // Check if this is an internal request for context information.
    //

    if ( Level == SOL_INTERNAL && OptionName == SO_CONTEXT ) {

		DebugPrint((2, "[WshLpx]WSHGetSocketInformation: OptionValue = %p OptionLength = %p\n", 
				OptionValue, OptionLength));
        
		if(OptionValue != NULL)
			DebugPrint((2, "[WshLpx]WSHGetSocketInformation: OptionValue = %p, OptionLength = %d, sizeof(*context) = %d\n", 
				OptionValue, *OptionLength, sizeof(*context)));
		//
        // The Windows Sockets DLL is requesting context information
        // from us.  If an output buffer was not supplied, the Windows
        // Sockets DLL is just requesting the size of our context
        // information.
        //

        if ( OptionValue != NULL ) {

            //
            // Make sure that the buffer is sufficient to hold all the
            // context information.
            //

            if ( *OptionLength < sizeof(*context) ) {
                return WSAEFAULT;
            }

            //
            // Copy in the context information.
            //

            CopyMemory( OptionValue, context, sizeof(*context) );
        }

        *OptionLength = sizeof(*context);

        return NO_ERROR;
    }

    //
    // The only other levels we support here are SOL_SOCKET,
    // IPPROTO_TCP, IPPROTO_UDP, and IPPROTO_IP.
    //

    if ( Level != SOL_SOCKET &&
         Level != LPXPROTO_STREAM &&
         Level != LPXPROTO_DGRAM
         ) {
        return WSAEINVAL;
    }

    //
    // Make sure that the output buffer is sufficiently large.
    //

    if ( *OptionLength < sizeof(int) ) {
        return WSAEFAULT;
    }

    //
    // Handle TCP-level options.
    //

    if ( Level == LPXPROTO_STREAM ) {

        if ( IS_DGRAM_SOCK(context->SocketType) ) {
            return WSAENOPROTOOPT;
        }

        switch ( OptionName ) {
#if 0
        case TCP_NODELAY:

            ZeroMemory( OptionValue, *OptionLength );

            *OptionValue = context->NoDelay;
            *OptionLength = sizeof(int);
            break;

        case TCP_EXPEDITED_1122:

            ZeroMemory( OptionValue, *OptionLength );

            *OptionValue = !context->BsdUrgent;
            *OptionLength = sizeof(int);
            break;
#endif
        default:

            return WSAEINVAL;
        }

        return NO_ERROR;
    }

    //
    // Handle UDP-level options.
    //

    if ( Level == LPXPROTO_DGRAM ) {

        switch ( OptionName ) {
#if 0
        case UDP_NOCHECKSUM :

            //
            // This option is only valid for datagram sockets.
            //
            if ( !IS_DGRAM_SOCK(context->SocketType) ) {
                return WSAENOPROTOOPT;
            }

            ZeroMemory( OptionValue, *OptionLength );

            *OptionValue = context->UdpNoChecksum;
            *OptionLength = sizeof(int);
            break;
#endif
        default :

            return WSAEINVAL;
        }

        return NO_ERROR;
    }


    //
    // Handle socket-level options.
    //

    switch ( OptionName ) {
        // to do: Get retransmit timeout, keep alive timeout etc.
#if 0
    case SO_KEEPALIVE:

        if ( IS_DGRAM_SOCK(context->SocketType) ) {
            return WSAENOPROTOOPT;
        }

        ZeroMemory( OptionValue, *OptionLength );

        *OptionValue = context->KeepAlive;
        *OptionLength = sizeof(int);

        break;

    case SO_DONTROUTE:

        ZeroMemory( OptionValue, *OptionLength );

        *OptionValue = context->DontRoute;
        *OptionLength = sizeof(int);

        break;
#endif
    default:

        return WSAENOPROTOOPT;
    }

    return NO_ERROR;

} // WSHGetSocketInformation


INT
WSHGetWildcardSockaddr (
    IN PVOID HelperDllSocketContext,
    OUT PSOCKADDR Sockaddr,
    OUT PINT SockaddrLength
    )

/*++

Routine Description:

    This routine returns a wildcard socket address.  A wildcard address
    is one which will bind the socket to an endpoint of the transport's
    choosing.  For TCP/IP, a wildcard address has IP address ==
    0.0.0.0 and port = 0.

Arguments:

    HelperDllSocketContext - the context pointer returned from
        WSHOpenSocket() for the socket for which we need a wildcard
        address.

    Sockaddr - points to a buffer which will receive the wildcard socket
        address.

    SockaddrLength - receives the length of the wioldcard sockaddr.

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
	DebugPrint((2, "[WshLpx]WSHGetWildcardSockaddr: Entered	SockaddrLength = %d, sizeof(SOCKADDR_LPX) = %d \n", *SockaddrLength, sizeof(SOCKADDR_LPX)));

    if ( *SockaddrLength < sizeof(SOCKADDR_LPX) ) {
        return WSAEFAULT;
    }

    *SockaddrLength = sizeof(SOCKADDR_LPX);

    //
    // Just zero out the address and set the family to AF_INET--this is
    // a wildcard address for TCP/IP.
    //

    ZeroMemory( Sockaddr, sizeof(SOCKADDR_LPX) );

    Sockaddr->sa_family = AF_LPX;

    return NO_ERROR;

} // WSAGetWildcardSockaddr


DWORD
WSHGetWinsockMapping (
    OUT PWINSOCK_MAPPING Mapping,
    IN DWORD MappingLength
    )

/*++

Routine Description:

    Returns the list of address family/socket type/protocol triples
    supported by this helper DLL.

Arguments:

    Mapping - receives a pointer to a WINSOCK_MAPPING structure that
        describes the triples supported here.

    MappingLength - the length, in bytes, of the passed-in Mapping buffer.

Return Value:

    DWORD - the length, in bytes, of a WINSOCK_MAPPING structure for this
        helper DLL.  If the passed-in buffer is too small, the return
        value will indicate the size of a buffer needed to contain
        the WINSOCK_MAPPING structure.

--*/

{
    DWORD mappingLength;

	DebugPrint((2, "[WshLpx]WSHGetWinsockMapping: Entered\n"));

    mappingLength = sizeof(WINSOCK_MAPPING) - sizeof(MAPPING_TRIPLE) +
                        sizeof(TcpMappingTriples) + sizeof(UdpMappingTriples)
                        + sizeof(RawMappingTriples);

    //
    // If the passed-in buffer is too small, return the length needed
    // now without writing to the buffer.  The caller should allocate
    // enough memory and call this routine again.
    //

    if ( mappingLength > MappingLength ) {
        return mappingLength;
    }

    //
    // Fill in the output mapping buffer with the list of triples
    // supported in this helper DLL.
    //

    Mapping->Rows = sizeof(TcpMappingTriples) / sizeof(TcpMappingTriples[0])
                     + sizeof(UdpMappingTriples) / sizeof(UdpMappingTriples[0])
                     + sizeof(RawMappingTriples) / sizeof(RawMappingTriples[0]);
    Mapping->Columns = sizeof(MAPPING_TRIPLE) / sizeof(DWORD);
    MoveMemory(
        Mapping->Mapping,
        TcpMappingTriples,
        sizeof(TcpMappingTriples)
        );
    MoveMemory(
        (PCHAR)Mapping->Mapping + sizeof(TcpMappingTriples),
        UdpMappingTriples,
        sizeof(UdpMappingTriples)
        );
    MoveMemory(
        (PCHAR)Mapping->Mapping + sizeof(TcpMappingTriples)
                                + sizeof(UdpMappingTriples),
        RawMappingTriples,
        sizeof(RawMappingTriples)
        );

    //
    // Return the number of bytes we wrote.
    //

    return mappingLength;

} // WSHGetWinsockMapping


INT
WSHOpenSocket (
    IN OUT PINT AddressFamily,
    IN OUT PINT SocketType,
    IN OUT PINT Protocol,
    OUT PUNICODE_STRING TransportDeviceName,
    OUT PVOID *HelperDllSocketContext,
    OUT PDWORD NotificationEvents
    )
{
    return WSHOpenSocket2(
               AddressFamily,
               SocketType,
               Protocol,
               0,           // Group
               0,           // Flags
               TransportDeviceName,
               HelperDllSocketContext,
               NotificationEvents
               );

} // WSHOpenSocket


INT
WSHOpenSocket2 (
    IN OUT PINT AddressFamily,
    IN OUT PINT SocketType,
    IN OUT PINT Protocol,
    IN GROUP Group,
    IN DWORD Flags,
    OUT PUNICODE_STRING TransportDeviceName,
    OUT PVOID *HelperDllSocketContext,
    OUT PDWORD NotificationEvents
    )

/*++

Routine Description:

    Does the necessary work for this helper DLL to open a socket and is
    called by the winsock DLL in the socket() routine.  This routine
    verifies that the specified triple is valid, determines the NT
    device name of the TDI provider that will support that triple,
    allocates space to hold the socket's context block, and
    canonicalizes the triple.

Arguments:

    AddressFamily - on input, the address family specified in the
        socket() call.  On output, the canonicalized value for the
        address family.

    SocketType - on input, the socket type specified in the socket()
        call.  On output, the canonicalized value for the socket type.

    Protocol - on input, the protocol specified in the socket() call.
        On output, the canonicalized value for the protocol.

    Group - Identifies the group for the new socket.

    Flags - Zero or more WSA_FLAG_* flags as passed into WSASocket().

    TransportDeviceName - receives the name of the TDI provider that
        will support the specified triple.

    HelperDllSocketContext - receives a context pointer that the winsock
        DLL will return to this helper DLL on future calls involving
        this socket.

    NotificationEvents - receives a bitmask of those state transitions
        this helper DLL should be notified on.

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    PWSHLPX_SOCKET_CONTEXT context;

    DebugPrint((2, "[WshLpx]WSHOpenSocket2: Entered\n"));

    //
    // Determine whether this is to be a TCP, UDP, or RAW socket.
    //

    if ( IsTripleInList(
             TcpMappingTriples,
             sizeof(TcpMappingTriples) / sizeof(TcpMappingTriples[0]),
             *AddressFamily,
             *SocketType,
             *Protocol ) ) {

        //
        // It's a TCP socket. Check the flags.
        //

        if( ( Flags & ~VALID_TCP_FLAGS ) != 0 ) {

            return WSAEINVAL;

        }

        //
        // Return the canonical form of a TCP socket triple.
        //

        *AddressFamily = TcpMappingTriples[0].AddressFamily;
        *SocketType = TcpMappingTriples[0].SocketType;
        *Protocol = TcpMappingTriples[0].Protocol;

        //
        // Indicate the name of the TDI device that will service
        // SOCK_STREAM sockets in the internet address family.
        //

        RtlInitUnicodeString( TransportDeviceName, LPXTCP_DEVICE );

    } else if ( IsTripleInList(
                    UdpMappingTriples,
                    sizeof(UdpMappingTriples) / sizeof(UdpMappingTriples[0]),
                    *AddressFamily,
                    *SocketType,
                    *Protocol ) ) {

        //
        // It's a UDP socket. Check the flags & group ID.
        //

        if( ( Flags & ~VALID_UDP_FLAGS ) != 0 ||
            Group == SG_CONSTRAINED_GROUP ) {

            return WSAEINVAL;

        }

        //
        // Return the canonical form of a UDP socket triple.
        //

        *AddressFamily = UdpMappingTriples[0].AddressFamily;
        *SocketType = UdpMappingTriples[0].SocketType;
        *Protocol = UdpMappingTriples[0].Protocol;

        //
        // Indicate the name of the TDI device that will service
        // SOCK_DGRAM sockets in the internet address family.
        //

        RtlInitUnicodeString( TransportDeviceName, LPXUDP_DEVICE );

    } else if ( IsTripleInList(
                    RawMappingTriples,
                    sizeof(RawMappingTriples) / sizeof(RawMappingTriples[0]),
                    *AddressFamily,
                    *SocketType,
                    *Protocol ) )
    {
        return(WSAEINVAL); // RAW packet is not supported 
    } else {

        //
        // This should never happen if the registry information about this
        // helper DLL is correct.  If somehow this did happen, just return
        // an error.
        //

        return WSAEINVAL;
    }

    //
    // Allocate context for this socket.  The Windows Sockets DLL will
    // return this value to us when it asks us to get/set socket options.
    //

    context = HeapAlloc(GetProcessHeap(), 0, sizeof(*context) );
    if ( context == NULL ) {
		DebugPrint((1, "[WshLpx]WSHOpenSocket2: context == NULL\n"));

        return WSAENOBUFS;
    }

    //
    // Initialize the context for the socket.
    //

    context->AddressFamily = *AddressFamily;
    context->SocketType = *SocketType;
    context->Protocol = *Protocol;
    context->ReceiveBufferSize = DEFAULT_RECEIVE_BUFFER_SIZE;
    context->Flags = Flags;

    //
    // Tell the Windows Sockets DLL which state transitions we're
    // interested in being notified of.  The only times we need to be
    // called is after a connect has completed so that we can turn on
    // the sending of keepalives if SO_KEEPALIVE was set before the
    // socket was connected, when the socket is closed so that we can
    // free context information, and when a connect fails so that we
    // can, if appropriate, dial in to the network that will support the
    // connect attempt.
    //

    *NotificationEvents =
        WSH_NOTIFY_CONNECT | WSH_NOTIFY_CLOSE | WSH_NOTIFY_CONNECT_ERROR;

    if (*SocketType == SOCK_RAW) {
        *NotificationEvents |= WSH_NOTIFY_BIND;
    }

    //
    // Everything worked, return success.
    //

    *HelperDllSocketContext = context;
    return NO_ERROR;

} // WSHOpenSocket


INT
WSHNotify (
    IN PVOID HelperDllSocketContext,
    IN SOCKET SocketHandle,
    IN HANDLE TdiAddressObjectHandle,
    IN HANDLE TdiConnectionObjectHandle,
    IN DWORD NotifyEvent
    )

/*++

Routine Description:

    This routine is called by the winsock DLL after a state transition
    of the socket.  Only state transitions returned in the
    NotificationEvents parameter of WSHOpenSocket() are notified here.
    This routine allows a winsock helper DLL to track the state of
    socket and perform necessary actions corresponding to state
    transitions.

Arguments:

    HelperDllSocketContext - the context pointer given to the winsock
        DLL by WSHOpenSocket().

    SocketHandle - the handle for the socket.

    TdiAddressObjectHandle - the TDI address object of the socket, if
        any.  If the socket is not yet bound to an address, then
        it does not have a TDI address object and this parameter
        will be NULL.

    TdiConnectionObjectHandle - the TDI connection object of the socket,
        if any.  If the socket is not yet connected, then it does not
        have a TDI connection object and this parameter will be NULL.

    NotifyEvent - indicates the state transition for which we're being
        called.

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    PWSHLPX_SOCKET_CONTEXT context = HelperDllSocketContext;

    DebugPrint((2, "[WshLpx]WSHNotify: Entered NotifyEvent = %x\n", NotifyEvent));

    //
    // We should only be called after a connect() completes or when the
    // socket is being closed.
    //

    if ( NotifyEvent == WSH_NOTIFY_CONNECT ) {

    } else if ( NotifyEvent == WSH_NOTIFY_CLOSE ) {
        HeapFree(GetProcessHeap(), 0, context );
    } else if ( NotifyEvent == WSH_NOTIFY_CONNECT_ERROR ) {

    } else if ( NotifyEvent == WSH_NOTIFY_BIND ) {

    } else {
        return WSAEINVAL;
    }

    return NO_ERROR;

} // WSHNotify


INT
WSHSetSocketInformation (
    IN PVOID HelperDllSocketContext,
    IN SOCKET SocketHandle,
    IN HANDLE TdiAddressObjectHandle,
    IN HANDLE TdiConnectionObjectHandle,
    IN INT Level,
    IN INT OptionName,
    IN PCHAR OptionValue,
    IN INT OptionLength
    )

/*++

Routine Description:

    This routine sets information about a socket for those socket
    options supported in this helper DLL.  The options supported here
    are SO_KEEPALIVE, SO_DONTROUTE, and TCP_EXPEDITED_1122.  This routine is
    called by the winsock DLL when a level/option name combination is
    passed to setsockopt() that the winsock DLL does not understand.

Arguments:

    HelperDllSocketContext - the context pointer returned from
        WSHOpenSocket().

    SocketHandle - the handle of the socket for which we're getting
        information.

    TdiAddressObjectHandle - the TDI address object of the socket, if
        any.  If the socket is not yet bound to an address, then
        it does not have a TDI address object and this parameter
        will be NULL.

    TdiConnectionObjectHandle - the TDI connection object of the socket,
        if any.  If the socket is not yet connected, then it does not
        have a TDI connection object and this parameter will be NULL.

    Level - the level parameter passed to setsockopt().

    OptionName - the optname parameter passed to setsockopt().

    OptionValue - the optval parameter passed to setsockopt().

    OptionLength - the optlen parameter passed to setsockopt().

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    PWSHLPX_SOCKET_CONTEXT context = HelperDllSocketContext;
    INT error;
    INT optionValue;

    UNREFERENCED_PARAMETER( SocketHandle );
    UNREFERENCED_PARAMETER( TdiAddressObjectHandle );
    UNREFERENCED_PARAMETER( TdiConnectionObjectHandle );

	DebugPrint((2, "[WshLpx]WSHSetSocketInformation: Entered Level = %x, \
		OptionName = %x, OptionValue = %x\n", Level, OptionName, *OptionValue));

    //
    // Check if this is an internal request for context information.
    //

    if ( Level == SOL_INTERNAL && OptionName == SO_CONTEXT ) {

        //
        // The Windows Sockets DLL is requesting that we set context
        // information for a new socket.  If the new socket was
        // accept()'ed, then we have already been notified of the socket
        // and HelperDllSocketContext will be valid.  If the new socket
        // was inherited or duped into this process, then this is our
        // first notification of the socket and HelperDllSocketContext
        // will be equal to NULL.
        //
        // Insure that the context information being passed to us is
        // sufficiently large.
        //

        if ( OptionLength < sizeof(*context) ) {
            return WSAEINVAL;
        }

        if ( HelperDllSocketContext == NULL ) {

            //
            // This is our notification that a socket handle was
            // inherited or duped into this process.  Allocate a context
            // structure for the new socket.
            //

            context = HeapAlloc(GetProcessHeap(), 0, sizeof(*context) );
            if ( context == NULL ) {
                DebugPrint((1, "[WshLpx]WSHSetSocketInformation: context == NULL\n"));
                return WSAENOBUFS;
            }

            //
            // Copy over information into the context block.
            //

            CopyMemory( context, OptionValue, sizeof(*context) );

            //
            // Tell the Windows Sockets DLL where our context information is
            // stored so that it can return the context pointer in future
            // calls.
            //

            *(PWSHLPX_SOCKET_CONTEXT *)OptionValue = context;

            return NO_ERROR;

        } else {
            return NO_ERROR;
        }
    }

    //
    // The only other levels we support here are SOL_SOCKET,
    // IPPROTO_TCP, IPPROTO_UDP
    //

    if ( Level != SOL_SOCKET &&
         Level != LPXPROTO_STREAM &&
         Level != LPXPROTO_DGRAM) {
        return WSAEINVAL;
    }

    //
    // Make sure that the option length is sufficient.
    //

    if ( OptionLength < sizeof(int) ) {
        return WSAEFAULT;
    }

    optionValue = *(INT UNALIGNED *)OptionValue;

    //
    // Handle UDP-level options.
    //

    if ( Level == LPXPROTO_DGRAM ) {
        switch ( OptionName ) {

        default :

            return WSAEINVAL;
        }

        return NO_ERROR;
    }

    //
    // Handle socket-level options.
    //

    switch ( OptionName ) {

    default:

        return WSAENOPROTOOPT;
    }

    return NO_ERROR;

} // WSHSetSocketInformation


INT
WSHEnumProtocols (
    IN LPINT lpiProtocols,
    IN LPWSTR lpTransportKeyName,
    IN OUT LPVOID lpProtocolBuffer,
    IN OUT LPDWORD lpdwBufferLength
    )

/*++

Routine Description:

    Enumerates the protocols supported by this helper.

Arguments:

    lpiProtocols - Pointer to a NULL-terminated array of protocol
        identifiers. Only protocols specified in this array will
        be returned by this function. If this pointer is NULL,
        all protocols are returned.

    lpTransportKeyName -

    lpProtocolBuffer - Pointer to a buffer to fill with PROTOCOL_INFO
        structures.

    lpdwBufferLength - Pointer to a variable that, on input, contains
        the size of lpProtocolBuffer. On output, this value will be
        updated with the size of the data actually written to the buffer.

Return Value:

    INT - The number of protocols returned if successful, -1 if not.

--*/

{
    DWORD bytesRequired;
    PPROTOCOL_INFO tcpProtocolInfo;
    PPROTOCOL_INFO udpProtocolInfo;
    BOOL useTcp = FALSE;
    BOOL useUdp = FALSE;
    DWORD i;

    lpTransportKeyName;         // Avoid compiler warnings.

    DebugPrint((2, "[WshLpx]WSHEnumProtocols: Entered\n"));

    //
    // Make sure that the caller cares about TCP and/or UDP.
    //

    if ( ARGUMENT_PRESENT( lpiProtocols ) ) {

        for ( i = 0; lpiProtocols[i] != 0; i++ ) {
            if ( lpiProtocols[i] == LPXPROTO_STREAM ) {
                useTcp = TRUE;
            }
            if ( lpiProtocols[i] == LPXPROTO_DGRAM ) {
                useUdp = TRUE;
            }
        }

    } else {

        useTcp = TRUE;
        useUdp = TRUE;
    }

    if ( !useTcp && !useUdp ) {
        *lpdwBufferLength = 0;
        return 0;
    }

    //
    // Make sure that the caller has specified a sufficiently large
    // buffer.
    //

    bytesRequired = (sizeof(PROTOCOL_INFO) * 2) +
                        ( (wcslen( LPX_TCP_NAME ) + 1) * sizeof(WCHAR)) +
                        ( (wcslen( LPX_UDP_NAME ) + 1) * sizeof(WCHAR));

    if ( bytesRequired > *lpdwBufferLength ) {
        *lpdwBufferLength = bytesRequired;
        return -1;
    }

    //
    // Fill in TCP info, if requested.
    //

    if ( useTcp ) {

        tcpProtocolInfo = lpProtocolBuffer;

        tcpProtocolInfo->dwServiceFlags = XP_GUARANTEED_DELIVERY |
                                              XP_GUARANTEED_ORDER |
                                              XP_GRACEFUL_CLOSE |
//                                              XP_EXPEDITED_DATA |
                                              XP_FRAGMENTATION;
        tcpProtocolInfo->iAddressFamily = AF_LPX;
        tcpProtocolInfo->iMaxSockAddr = sizeof(SOCKADDR_LPX);
        tcpProtocolInfo->iMinSockAddr = sizeof(SOCKADDR_LPX);
        tcpProtocolInfo->iSocketType = SOCK_STREAM;
        tcpProtocolInfo->iProtocol = LPXPROTO_STREAM;
        tcpProtocolInfo->dwMessageSize = 0;
        tcpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)lpProtocolBuffer + *lpdwBufferLength -
                ( (wcslen( LPX_TCP_NAME ) + 1) * sizeof(WCHAR) ) );
        wcscpy( tcpProtocolInfo->lpProtocol, LPX_TCP_NAME );

        udpProtocolInfo = tcpProtocolInfo + 1;
        udpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)tcpProtocolInfo->lpProtocol -
                ( (wcslen( LPX_UDP_NAME ) + 1) * sizeof(WCHAR) ) );

    } else {

        udpProtocolInfo = lpProtocolBuffer;
        udpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)lpProtocolBuffer + *lpdwBufferLength -
                ( (wcslen( LPX_UDP_NAME ) + 1) * sizeof(WCHAR) ) );
    }

    //
    // Fill in UDP info, if requested.
    //

    if ( useUdp ) {

        udpProtocolInfo->dwServiceFlags = XP_CONNECTIONLESS |
                                              XP_MESSAGE_ORIENTED |
                                              XP_SUPPORTS_BROADCAST |
                                              XP_SUPPORTS_MULTICAST |
                                              XP_FRAGMENTATION;
        udpProtocolInfo->iAddressFamily = AF_LPX;
        udpProtocolInfo->iMaxSockAddr = sizeof(SOCKADDR_LPX);
        udpProtocolInfo->iMinSockAddr = sizeof(SOCKADDR_LPX);
        udpProtocolInfo->iSocketType = SOCK_DGRAM;
        udpProtocolInfo->iProtocol = LPXPROTO_DGRAM;
        udpProtocolInfo->dwMessageSize = UDP_MESSAGE_SIZE;
        wcscpy( udpProtocolInfo->lpProtocol, LPX_UDP_NAME );
    }

    *lpdwBufferLength = bytesRequired;

    return (useTcp && useUdp) ? 2 : 1;

} // WSHEnumProtocols



BOOLEAN
IsTripleInList (
    IN PMAPPING_TRIPLE List,
    IN ULONG ListLength,
    IN INT AddressFamily,
    IN INT SocketType,
    IN INT Protocol
    )

/*++

Routine Description:

    Determines whether the specified triple has an exact match in the
    list of triples.

Arguments:

    List - a list of triples (address family/socket type/protocol) to
        search.

    ListLength - the number of triples in the list.

    AddressFamily - the address family to look for in the list.

    SocketType - the socket type to look for in the list.

    Protocol - the protocol to look for in the list.

Return Value:

    BOOLEAN - TRUE if the triple was found in the list, false if not.

--*/

{
    ULONG i;

    //
    // Walk through the list searching for an exact match.
    //

	DebugPrint((2, "[WshLpx]IsTripleInList: Entered\n"));

    for ( i = 0; i < ListLength; i++ ) 
	{
        //
        // If all three elements of the triple match, return indicating
        // that the triple did exist in the list.
        //

        if ( AddressFamily == List[i].AddressFamily &&
             SocketType == List[i].SocketType &&
             ( (Protocol == List[i].Protocol) || (SocketType == SOCK_RAW) )) 
		{
            return TRUE;
        }
    }

    //
    // The triple was not found in the list.
    //

    return FALSE;

} // IsTripleInList



INT
WINAPI
WSHJoinLeaf (
    IN PVOID HelperDllSocketContext,
    IN SOCKET SocketHandle,
    IN HANDLE TdiAddressObjectHandle,
    IN HANDLE TdiConnectionObjectHandle,
    IN PVOID LeafHelperDllSocketContext,
    IN SOCKET LeafSocketHandle,
    IN PSOCKADDR Sockaddr,
    IN DWORD SockaddrLength,
    IN LPWSABUF CallerData,
    IN LPWSABUF CalleeData,
    IN LPQOS SocketQOS,
    IN LPQOS GroupQOS,
    IN DWORD Flags
    )

/*++

Routine Description:

    Performs the protocol-dependent portion of creating a multicast
    socket.

Arguments:

    The following four parameters correspond to the socket passed into
    the WSAJoinLeaf() API:

    HelperDllSocketContext - The context pointer returned from
        WSHOpenSocket().

    SocketHandle - The handle of the socket used to establish the
        multicast "session".

    TdiAddressObjectHandle - The TDI address object of the socket, if
        any.  If the socket is not yet bound to an address, then
        it does not have a TDI address object and this parameter
        will be NULL.

    TdiConnectionObjectHandle - The TDI connection object of the socket,
        if any.  If the socket is not yet connected, then it does not
        have a TDI connection object and this parameter will be NULL.

    The next two parameters correspond to the newly created socket that
    identifies the multicast "session":

    LeafHelperDllSocketContext - The context pointer returned from
        WSHOpenSocket().

    LeafSocketHandle - The handle of the socket that identifies the
        multicast "session".

    Sockaddr - The name of the peer to which the socket is to be joined.

    SockaddrLength - The length of Sockaddr.

    CallerData - Pointer to user data to be transferred to the peer
        during multipoint session establishment.

    CalleeData - Pointer to user data to be transferred back from
        the peer during multipoint session establishment.

    SocketQOS - Pointer to the flowspecs for SocketHandle, one in each
        direction.

    GroupQOS - Pointer to the flowspecs for the socket group, if any.

    Flags - Flags to indicate if the socket is acting as sender,
        receiver, or both.

Return Value:

    INT - 0 if successful, a WinSock error code if not.

--*/

{
    return 0;
} // WSHJoinLeaf


INT
WINAPI
WSHGetBroadcastSockaddr (
    IN PVOID HelperDllSocketContext,
    OUT PSOCKADDR Sockaddr,
    OUT PINT SockaddrLength
    )

/*++

Routine Description:

    This routine returns a broadcast socket address.  A broadcast address
    may be used as a destination for the sendto() API to send a datagram
    to all interested clients.

Arguments:

    HelperDllSocketContext - the context pointer returned from
        WSHOpenSocket() for the socket for which we need a broadcast
        address.

    Sockaddr - points to a buffer which will receive the broadcast socket
        address.

    SockaddrLength - receives the length of the broadcast sockaddr.

Return Value:

    INT - a winsock error code indicating the status of the operation, or
        NO_ERROR if the operation succeeded.

--*/

{
    PSOCKADDR_LPX sockAddrLpx;

	DebugPrint((2, "[WshLpx]WSHGetBroadcastSockaddr: Entered\n"));

    if( *SockaddrLength < sizeof(SOCKADDR_LPX) ) 
	{
        return WSAEFAULT;
    }

    *SockaddrLength = sizeof(SOCKADDR_LPX);

    //
    // Build the broadcast address.
    //

    sockAddrLpx = (PSOCKADDR_LPX) Sockaddr;

    ZeroMemory(
        sockAddrLpx,
        sizeof(*sockAddrLpx));

	sockAddrLpx->sin_family = AF_LPX;

	FillMemory(
		&sockAddrLpx->LpxAddress.Node, 
		sizeof(sockAddrLpx->LpxAddress.Node),
		0xFF);

    return NO_ERROR;

} // WSAGetBroadcastSockaddr


INT
WINAPI
WSHGetWSAProtocolInfo (
    IN LPWSTR ProviderName,
    OUT LPWSAPROTOCOL_INFOW * ProtocolInfo,
    OUT LPDWORD ProtocolInfoEntries
    )

/*++

Routine Description:

    Retrieves a pointer to the WSAPROTOCOL_INFOW structure(s) describing
    the protocol(s) supported by this helper.

Arguments:

    ProviderName - Contains the name of the provider, such as "TcpIp".

    ProtocolInfo - Receives a pointer to the WSAPROTOCOL_INFOW array.

    ProtocolInfoEntries - Receives the number of entries in the array.

Return Value:

    INT - 0 if successful, WinSock error code if not.

--*/

{

	DebugPrint((2, "[WshLpx]WSHGetWSAProtocolInfo: Entered\n"));

    if( ProviderName == NULL ||
        ProtocolInfo == NULL ||
        ProtocolInfoEntries == NULL ) 
	{
        return WSAEFAULT;
    }

    if( _wcsicmp( ProviderName, L"Lpx" ) == 0 ) 
	{
        *ProtocolInfo = Winsock2Protocols;
        *ProtocolInfoEntries = NUM_WINSOCK2_PROTOCOLS;
        return NO_ERROR;

    }

    return WSAEINVAL;

} // WSHGetWSAProtocolInfo


INT
WINAPI
WSHAddressToString (
    IN LPSOCKADDR Address,
    IN INT AddressLength,
    IN LPWSAPROTOCOL_INFOW ProtocolInfo,
    OUT LPWSTR AddressString,
    IN OUT LPDWORD AddressStringLength
    )

/*++

Routine Description:

    Converts a SOCKADDR to a human-readable form.

Arguments:

    Address - The SOCKADDR to convert.

    AddressLength - The length of Address.

    ProtocolInfo - The WSAPROTOCOL_INFOW for a particular provider.

    AddressString - Receives the formatted address string.

    AddressStringLength - On input, contains the length of AddressString.
        On output, contains the number of characters actually written
        to AddressString.

Return Value:

    INT - 0 if successful, WinSock error code if not.

--*/

{

    WCHAR string[32];
    INT length;
    PSOCKADDR_LPX addr;

	DebugPrint((2, "[WshLpx]WSHGetWSAProtocolInfo: Entered\n"));

    //
    // Quick sanity checks.
    //

    if( Address == NULL ||
        AddressLength < sizeof(SOCKADDR_LPX) ||
        AddressString == NULL ||
        AddressStringLength == NULL ) {

        return WSAEFAULT;

    }

    addr = (PSOCKADDR_LPX)Address;

    if( addr->sin_family != AF_LPX )
	{
        return WSA_INVALID_PARAMETER;
    }

    //
    // Do the converstion.
    //

    length = wsprintfW(
                 string,
                 L"%02x:%02x:%02x:%02x:%02x:%02x",
                 ( addr->LpxAddress.Node[0] ) & 0xFF,
                 ( addr->LpxAddress.Node[1] ) & 0xFF,
                 ( addr->LpxAddress.Node[2] ) & 0xFF,
                 ( addr->LpxAddress.Node[3] ) & 0xFF,
                 ( addr->LpxAddress.Node[4] ) & 0xFF,
                 ( addr->LpxAddress.Node[5] ) & 0xFF
                 );

    if( addr->LpxAddress.Port != 0 ) {

        length += wsprintfW(
                      string + length,
                      L":%u",
                      ntohs( addr->LpxAddress.Port )
                      );
    }

    length++;   // account for terminator

    if( *AddressStringLength < (DWORD)length ) {

        return WSAEFAULT;

    }

    *AddressStringLength = (DWORD)length;

    CopyMemory(
        AddressString,
        string,
        length * sizeof(WCHAR)
        );

    return NO_ERROR;

} // WSHAddressToString


INT
WINAPI
WSHStringToAddress (
    IN LPWSTR AddressString,
    IN DWORD AddressFamily,
    IN LPWSAPROTOCOL_INFOW ProtocolInfo,
    OUT LPSOCKADDR Address,
    IN OUT LPINT AddressLength
    )

/*++

Routine Description:

    Fills in a SOCKADDR structure by parsing a human-readable string.

Arguments:

    AddressString - Points to the zero-terminated human-readable string.

    AddressFamily - The address family to which the string belongs.

    ProtocolInfo - The WSAPROTOCOL_INFOW for a particular provider.

    Address - Receives the SOCKADDR structure.

    AddressLength - On input, contains the length of Address. On output,
        contains the number of bytes actually written to Address.

Return Value:

    INT - 0 if successful, WinSock error code if not.

--*/

{
    LPWSTR terminator;
    PSOCKADDR_LPX addr;
    int i;
    PWCHAR token;
    PWCHAR endptr;
    long val;
    DebugPrint((2, "[WshLpx]WSHAddressToString: Entered\n"));

    //
    // Quick sanity checks.
    //

    if( AddressString == NULL ||
        Address == NULL ||
        AddressLength == NULL ||
        *AddressLength < sizeof(SOCKADDR_LPX) ) 
	{
        return WSAEFAULT;

    }

    if (AddressFamily != AF_LPX) 
	{
        return WSA_INVALID_PARAMETER;
    }


    //
    // Build the address.
    //
    addr = (PSOCKADDR_LPX)Address;

    ZeroMemory(
        addr,
        sizeof(SOCKADDR_LPX));

    *AddressLength = sizeof(SOCKADDR_LPX);
    addr->sin_family = AF_LPX;

    // Convert 
    // Only accept mm:mm:mm:mm:mm:mm or mm:mm:mm:mm:mm:mm:pppp

    i=0;

   /* Establish string and get the first token: */
   token = wcstok( AddressString, L":" );
   while( token != NULL )
   {
        if (i<6) {
            val = wcstol(token, &endptr, 16);
            if (val>0xff || val<0)
                break;
            addr->LpxAddress.Node[i] = (UCHAR) val;
        } else if (i==6) {
            val = wcstol(token, &endptr, 16);
            if (val>0xffffff || val<0)
                break;
            addr->LpxAddress.Port = (USHORT)val;
        } else {
            break;
        }
        i++;
          /* Get next token: */
        token = wcstok( NULL,  L":");
   }
    return NO_ERROR;
} // WSHStringToAddress


INT
WINAPI
WSHGetProviderGuid (
    IN LPWSTR ProviderName,
    OUT LPGUID ProviderGuid
    )

/*++

Routine Description:

    Returns the GUID identifying the protocols supported by this helper.

Arguments:

    ProviderName - Contains the name of the provider, such as "TcpIp".

    ProviderGuid - Points to a buffer that receives the provider's GUID.

Return Value:

    INT - 0 if successful, WinSock error code if not.

--*/

{

	DebugPrint((2, "[WshLpx]WSHGetProviderGuid: Entered\n"));

    if( ProviderName == NULL ||
        ProviderGuid == NULL ) 
	{
        return WSAEFAULT;
    }

    if( _wcsicmp( ProviderName, L"Lpx" ) == 0 ) 
	{
        CopyMemory(
            ProviderGuid,
            &LpxProviderGuid,
            sizeof(GUID));
        return NO_ERROR;

    }

    return WSAEINVAL;

} // WSHGetProviderGuid

INT
WINAPI
WSHIoctl (
    IN PVOID HelperDllSocketContext,
    IN SOCKET SocketHandle,
    IN HANDLE TdiAddressObjectHandle,
    IN HANDLE TdiConnectionObjectHandle,
    IN DWORD IoControlCode,
    IN LPVOID InputBuffer,
    IN DWORD InputBufferLength,
    IN LPVOID OutputBuffer,
    IN DWORD OutputBufferLength,
    OUT LPDWORD NumberOfBytesReturned,
    IN LPWSAOVERLAPPED Overlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine,
    OUT LPBOOL NeedsCompletion
    )

/*++

Routine Description:

    Performs queries & controls on the socket. This is basically an
    "escape hatch" for IOCTLs not supported by MSAFD.DLL. Any unknown
    IOCTLs are routed to the socket's helper DLL for protocol-specific
    processing.

Arguments:

    HelperDllSocketContext - the context pointer returned from
        WSHOpenSocket().

    SocketHandle - the handle of the socket for which we're controlling.

    TdiAddressObjectHandle - the TDI address object of the socket, if
        any.  If the socket is not yet bound to an address, then
        it does not have a TDI address object and this parameter
        will be NULL.

    TdiConnectionObjectHandle - the TDI connection object of the socket,
        if any.  If the socket is not yet connected, then it does not
        have a TDI connection object and this parameter will be NULL.

    IoControlCode - Control code of the operation to perform.

    InputBuffer - Address of the input buffer.

    InputBufferLength - The length of InputBuffer.

    OutputBuffer - Address of the output buffer.

    OutputBufferLength - The length of OutputBuffer.

    NumberOfBytesReturned - Receives the number of bytes actually written
        to the output buffer.

    Overlapped - Pointer to a WSAOVERLAPPED structure for overlapped
        operations.

    CompletionRoutine - Pointer to a completion routine to call when
        the operation is completed.

    NeedsCompletion - WSAIoctl() can be overlapped, with all the gory
        details that involves, such as setting events, queuing completion
        routines, and posting to IO completion ports. Since the majority
        of the IOCTL codes can be completed quickly "in-line", MSAFD.DLL
        can optionally perform the overlapped completion of the operation.

        Setting *NeedsCompletion to TRUE (the default) causes MSAFD.DLL
        to handle all of the IO completion details iff this is an
        overlapped operation on an overlapped socket.

        Setting *NeedsCompletion to FALSE tells MSAFD.DLL to take no
        further action because the helper DLL will perform any necessary
        IO completion.

        Note that if a helper performs its own IO completion, the helper
        is responsible for maintaining the "overlapped" mode of the socket
        at socket creation time and NOT performing overlapped IO completion
        on non-overlapped sockets.

Return Value:

    INT - 0 if successful, WinSock error code if not.

--*/

{

    INT err;
    NTSTATUS status;

 	DebugPrint((2, "[WshLpx]WSHIoctl: Entered\n"));

	//
    // Quick sanity checks.
    //

    if( HelperDllSocketContext == NULL ||
        SocketHandle == INVALID_SOCKET ||
        NumberOfBytesReturned == NULL ||
        NeedsCompletion == NULL ) 
	{
        return WSAEINVAL;
    }

    *NeedsCompletion = TRUE;

    switch( IoControlCode ) 
	{
    default :
        err = WSAEINVAL;
        break;
    }

    return err;

}   // WSHIoctl


