#ifndef __AFDTCPTDIPROC_H__
#define __AFDTCPTDIPROC_H__

#include <ndis.h>
#include <tdi.h>
#include <tdiinfo.h>
#include <tdikrnl.h>


typedef struct _WSABUF {
    ULONG len;
    PCHAR buf;
} WSABUF, *LPWSABUF;


#define TCP_STREAM		0
#define TCP_DATAGRAM	1

#define FSCTL_AFD_BASE                  FILE_DEVICE_NETWORK
#define _AFD_CONTROL_CODE(request,method) \
                ((FSCTL_AFD_BASE)<<12 | (request<<2) | method)
#define _AFD_REQUEST(ioctl) \
                ((((ULONG)(ioctl)) >> 2) & 0x03FF)

#define _AFD_BASE(ioctl) \
                ((((ULONG)(ioctl)) >> 12) & 0xFFFFF)

#define AFD_BIND                    0
#define AFD_CONNECT                 1
#define AFD_START_LISTEN            2
#define AFD_WAIT_FOR_LISTEN         3
#define AFD_ACCEPT                  4
#define AFD_RECEIVE                 5
#define AFD_RECEIVE_DATAGRAM        6
#define AFD_SEND                    7
#define AFD_SEND_DATAGRAM           8
#define AFD_POLL                    9
#define AFD_PARTIAL_DISCONNECT      10

#define AFD_GET_ADDRESS             11
#define AFD_QUERY_RECEIVE_INFO      12
#define AFD_QUERY_HANDLES           13
#define AFD_SET_INFORMATION         14
#define AFD_GET_REMOTE_ADDRESS      15
#define AFD_GET_CONTEXT             16
#define AFD_SET_CONTEXT             17

#define AFD_SET_CONNECT_DATA        18
#define AFD_SET_CONNECT_OPTIONS     19
#define AFD_SET_DISCONNECT_DATA     20
#define AFD_SET_DISCONNECT_OPTIONS  21

#define AFD_GET_CONNECT_DATA        22
#define AFD_GET_CONNECT_OPTIONS     23
#define AFD_GET_DISCONNECT_DATA     24
#define AFD_GET_DISCONNECT_OPTIONS  25

#define AFD_SIZE_CONNECT_DATA       26
#define AFD_SIZE_CONNECT_OPTIONS    27
#define AFD_SIZE_DISCONNECT_DATA    28
#define AFD_SIZE_DISCONNECT_OPTIONS 29

#define AFD_GET_INFORMATION         30
#define AFD_TRANSMIT_FILE           31
#define AFD_SUPER_ACCEPT            32

#define AFD_EVENT_SELECT            33
#define AFD_ENUM_NETWORK_EVENTS     34

#define AFD_DEFER_ACCEPT            35
#define AFD_WAIT_FOR_LISTEN_LIFO    36
#define AFD_SET_QOS                 37
#define AFD_GET_QOS                 38
#define AFD_NO_OPERATION            39
#define AFD_VALIDATE_GROUP          40
#define AFD_GET_UNACCEPTED_CONNECT_DATA 41

#define AFD_ROUTING_INTERFACE_QUERY  42
#define AFD_ROUTING_INTERFACE_CHANGE 43
#define AFD_ADDRESS_LIST_QUERY      44
#define AFD_ADDRESS_LIST_CHANGE     45
#define AFD_JOIN_LEAF               46
#define AFD_TRANSPORT_IOCTL         47
#define AFD_TRANSMIT_PACKETS        48
#define AFD_SUPER_CONNECT           49
#define AFD_SUPER_DISCONNECT        50
#define AFD_RECEIVE_MESSAGE         51

//
// SAN switch specific AFD function numbers
//
#define AFD_SWITCH_CEMENT_SAN       52
#define AFD_SWITCH_SET_EVENTS       53
#define AFD_SWITCH_RESET_EVENTS     54
#define AFD_SWITCH_CONNECT_IND      55
#define AFD_SWITCH_CMPL_ACCEPT      56
#define AFD_SWITCH_CMPL_REQUEST     57
#define AFD_SWITCH_CMPL_IO          58
#define AFD_SWITCH_REFRESH_ENDP     59
#define AFD_SWITCH_GET_PHYSICAL_ADDR 60
#define AFD_SWITCH_ACQUIRE_CTX      61
#define AFD_SWITCH_TRANSFER_CTX     62
#define AFD_SWITCH_GET_SERVICE_PID  63
#define AFD_SWITCH_SET_SERVICE_PROCESS  64
#define AFD_SWITCH_PROVIDER_CHANGE  65
#define AFD_SWITCH_ADDRLIST_CHANGE	66
#define AFD_NUM_IOCTLS				67



#define IOCTL_AFD_BIND                    _AFD_CONTROL_CODE( AFD_BIND, METHOD_NEITHER )
#define IOCTL_AFD_CONNECT                 _AFD_CONTROL_CODE( AFD_CONNECT, METHOD_NEITHER )
#define IOCTL_AFD_START_LISTEN            _AFD_CONTROL_CODE( AFD_START_LISTEN, METHOD_NEITHER )
#define IOCTL_AFD_WAIT_FOR_LISTEN         _AFD_CONTROL_CODE( AFD_WAIT_FOR_LISTEN, METHOD_BUFFERED )
#define IOCTL_AFD_ACCEPT                  _AFD_CONTROL_CODE( AFD_ACCEPT, METHOD_BUFFERED )
#define IOCTL_AFD_RECEIVE                 _AFD_CONTROL_CODE( AFD_RECEIVE, METHOD_NEITHER )
#define IOCTL_AFD_RECEIVE_DATAGRAM        _AFD_CONTROL_CODE( AFD_RECEIVE_DATAGRAM, METHOD_NEITHER )
#define IOCTL_AFD_SEND                    _AFD_CONTROL_CODE( AFD_SEND, METHOD_NEITHER )
#define IOCTL_AFD_SEND_DATAGRAM           _AFD_CONTROL_CODE( AFD_SEND_DATAGRAM, METHOD_NEITHER )
#define IOCTL_AFD_POLL                    _AFD_CONTROL_CODE( AFD_POLL, METHOD_BUFFERED )
#define IOCTL_AFD_PARTIAL_DISCONNECT      _AFD_CONTROL_CODE( AFD_PARTIAL_DISCONNECT, METHOD_NEITHER )

#define IOCTL_AFD_GET_ADDRESS             _AFD_CONTROL_CODE( AFD_GET_ADDRESS, METHOD_NEITHER )
#define IOCTL_AFD_QUERY_RECEIVE_INFO      _AFD_CONTROL_CODE( AFD_QUERY_RECEIVE_INFO, METHOD_NEITHER )
#define IOCTL_AFD_QUERY_HANDLES           _AFD_CONTROL_CODE( AFD_QUERY_HANDLES, METHOD_NEITHER )
#define IOCTL_AFD_SET_INFORMATION         _AFD_CONTROL_CODE( AFD_SET_INFORMATION, METHOD_NEITHER )
#define IOCTL_AFD_GET_REMOTE_ADDRESS      _AFD_CONTROL_CODE( AFD_GET_REMOTE_ADDRESS, METHOD_NEITHER )
#define IOCTL_AFD_GET_CONTEXT             _AFD_CONTROL_CODE( AFD_GET_CONTEXT, METHOD_NEITHER )
#define IOCTL_AFD_SET_CONTEXT             _AFD_CONTROL_CODE( AFD_SET_CONTEXT, METHOD_NEITHER )

#define IOCTL_AFD_SET_CONNECT_DATA        _AFD_CONTROL_CODE( AFD_SET_CONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_SET_CONNECT_OPTIONS     _AFD_CONTROL_CODE( AFD_SET_CONNECT_OPTIONS, METHOD_NEITHER )
#define IOCTL_AFD_SET_DISCONNECT_DATA     _AFD_CONTROL_CODE( AFD_SET_DISCONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_SET_DISCONNECT_OPTIONS  _AFD_CONTROL_CODE( AFD_SET_DISCONNECT_OPTIONS, METHOD_NEITHER )

#define IOCTL_AFD_GET_CONNECT_DATA        _AFD_CONTROL_CODE( AFD_GET_CONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_GET_CONNECT_OPTIONS     _AFD_CONTROL_CODE( AFD_GET_CONNECT_OPTIONS, METHOD_NEITHER )
#define IOCTL_AFD_GET_DISCONNECT_DATA     _AFD_CONTROL_CODE( AFD_GET_DISCONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_GET_DISCONNECT_OPTIONS  _AFD_CONTROL_CODE( AFD_GET_DISCONNECT_OPTIONS, METHOD_NEITHER )

#define IOCTL_AFD_SIZE_CONNECT_DATA       _AFD_CONTROL_CODE( AFD_SIZE_CONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_SIZE_CONNECT_OPTIONS    _AFD_CONTROL_CODE( AFD_SIZE_CONNECT_OPTIONS, METHOD_NEITHER )
#define IOCTL_AFD_SIZE_DISCONNECT_DATA    _AFD_CONTROL_CODE( AFD_SIZE_DISCONNECT_DATA, METHOD_NEITHER )
#define IOCTL_AFD_SIZE_DISCONNECT_OPTIONS _AFD_CONTROL_CODE( AFD_SIZE_DISCONNECT_OPTIONS, METHOD_NEITHER )

#define IOCTL_AFD_GET_INFORMATION         _AFD_CONTROL_CODE( AFD_GET_INFORMATION, METHOD_NEITHER )
#define IOCTL_AFD_TRANSMIT_FILE           _AFD_CONTROL_CODE( AFD_TRANSMIT_FILE, METHOD_NEITHER )
#define IOCTL_AFD_SUPER_ACCEPT            _AFD_CONTROL_CODE( AFD_SUPER_ACCEPT, METHOD_NEITHER )

#define IOCTL_AFD_EVENT_SELECT            _AFD_CONTROL_CODE( AFD_EVENT_SELECT, METHOD_NEITHER )
#define IOCTL_AFD_ENUM_NETWORK_EVENTS     _AFD_CONTROL_CODE( AFD_ENUM_NETWORK_EVENTS, METHOD_NEITHER )

#define IOCTL_AFD_DEFER_ACCEPT            _AFD_CONTROL_CODE( AFD_DEFER_ACCEPT, METHOD_BUFFERED )
#define IOCTL_AFD_WAIT_FOR_LISTEN_LIFO    _AFD_CONTROL_CODE( AFD_WAIT_FOR_LISTEN_LIFO, METHOD_BUFFERED )
#define IOCTL_AFD_SET_QOS                 _AFD_CONTROL_CODE( AFD_SET_QOS, METHOD_BUFFERED )
#define IOCTL_AFD_GET_QOS                 _AFD_CONTROL_CODE( AFD_GET_QOS, METHOD_BUFFERED )
#define IOCTL_AFD_NO_OPERATION            _AFD_CONTROL_CODE( AFD_NO_OPERATION, METHOD_NEITHER )
#define IOCTL_AFD_VALIDATE_GROUP          _AFD_CONTROL_CODE( AFD_VALIDATE_GROUP, METHOD_BUFFERED )
#define IOCTL_AFD_GET_UNACCEPTED_CONNECT_DATA _AFD_CONTROL_CODE( AFD_GET_UNACCEPTED_CONNECT_DATA, METHOD_NEITHER )

#define IOCTL_AFD_ROUTING_INTERFACE_QUERY  _AFD_CONTROL_CODE( AFD_ROUTING_INTERFACE_QUERY, METHOD_NEITHER ) 
#define IOCTL_AFD_ROUTING_INTERFACE_CHANGE _AFD_CONTROL_CODE( AFD_ROUTING_INTERFACE_CHANGE, METHOD_BUFFERED )
#define IOCTL_AFD_ADDRESS_LIST_QUERY       _AFD_CONTROL_CODE( AFD_ADDRESS_LIST_QUERY, METHOD_NEITHER ) 
#define IOCTL_AFD_ADDRESS_LIST_CHANGE      _AFD_CONTROL_CODE( AFD_ADDRESS_LIST_CHANGE, METHOD_BUFFERED )
#define IOCTL_AFD_JOIN_LEAF                _AFD_CONTROL_CODE( AFD_JOIN_LEAF, METHOD_NEITHER )
#define IOCTL_AFD_TRANSPORT_IOCTL          _AFD_CONTROL_CODE( AFD_TRANSPORT_IOCTL, METHOD_NEITHER )
#define IOCTL_AFD_TRANSMIT_PACKETS         _AFD_CONTROL_CODE( AFD_TRANSMIT_PACKETS, METHOD_NEITHER )
#define IOCTL_AFD_SUPER_CONNECT            _AFD_CONTROL_CODE( AFD_SUPER_CONNECT, METHOD_NEITHER )
#define IOCTL_AFD_SUPER_DISCONNECT         _AFD_CONTROL_CODE( AFD_SUPER_DISCONNECT, METHOD_NEITHER )
#define IOCTL_AFD_RECEIVE_MESSAGE          _AFD_CONTROL_CODE( AFD_RECEIVE_MESSAGE, METHOD_NEITHER )




//
// New multipoint semantics
//
#define AFD_ENDPOINT_FLAG_MULTIPOINT	    0x00001000
#define AFD_ENDPOINT_FLAG_CROOT			    0x00010000
#define AFD_ENDPOINT_FLAG_DROOT			    0x00100000

#define AFD_ENDPOINT_VALID_FLAGS		    0x00111111
typedef struct _AFD_ENDPOINT_FLAGS {
    union {
        struct {
            BOOLEAN     ConnectionLess :1;
            BOOLEAN     :3;                 // This spacing makes strcutures
                                            // much more readable (hex) in the 
                                            // debugger and has no effect
                                            // on the generated code as long
                                            // as number of flags is less than
                                            // 8 (we still take up full 32 bits
                                            // because of aligment requiremens
                                            // of most other fields)
            BOOLEAN     MessageMode :1;
            BOOLEAN     :3;
            BOOLEAN     Raw :1;
            BOOLEAN     :3;
            BOOLEAN     Multipoint :1;
            BOOLEAN     :3;
            BOOLEAN     C_Root :1;
            BOOLEAN     :3;
            BOOLEAN     D_Root :1;
            BOOLEAN     :3;
        };
        ULONG           EndpointFlags;      // Flags are as fine as bit fields,
                                            // but create problems when we need
                                            // to cast them to boolean.
    };
#define AFD_ENDPOINT_FLAG_CONNECTIONLESS	0x00000001
#define AFD_ENDPOINT_FLAG_MESSAGEMODE		0x00000010
#define AFD_ENDPOINT_FLAG_RAW			    0x00000100

//
// Old AFD_ENDPOINT_TYPE mappings. Flags make things clearer at
// at the TDI level and after all Winsock2 switched to provider flags
// instead of socket type anyway (ATM for example needs connection oriented
// raw sockets, which can only be reflected by SOCK_RAW+SOCK_STREAM combination
// which does not exists).
//
#define AfdEndpointTypeStream			0
#define AfdEndpointTypeDatagram			(AFD_ENDPOINT_FLAG_CONNECTIONLESS|\
                                            AFD_ENDPOINT_FLAG_MESSAGEMODE)
#define AfdEndpointTypeRaw				(AFD_ENDPOINT_FLAG_CONNECTIONLESS|\
                                            AFD_ENDPOINT_FLAG_MESSAGEMODE|\
                                            AFD_ENDPOINT_FLAG_RAW)
#define AfdEndpointTypeSequencedPacket	(AFD_ENDPOINT_FLAG_MESSAGEMODE)
#define AfdEndpointTypeReliableMessage	(AFD_ENDPOINT_FLAG_MESSAGEMODE)

//
// New multipoint semantics
//
#define AFD_ENDPOINT_FLAG_MULTIPOINT	    0x00001000
#define AFD_ENDPOINT_FLAG_CROOT			    0x00010000
#define AFD_ENDPOINT_FLAG_DROOT			    0x00100000

#define AFD_ENDPOINT_VALID_FLAGS		    0x00111111

} AFD_ENDPOINT_FLAGS;


typedef struct _AFD_OPEN_PACKET {
	AFD_ENDPOINT_FLAGS __f;
#define afdConnectionLess  __f.ConnectionLess
#define afdMessageMode     __f.MessageMode
#define afdRaw             __f.Raw
#define afdMultipoint      __f.Multipoint
#define afdC_Root          __f.C_Root
#define afdD_Root          __f.D_Root
#define afdEndpointFlags   __f.EndpointFlags
    LONG  GroupID;
    ULONG TransportDeviceNameLength;
    WCHAR TransportDeviceName[1];
} AFD_OPEN_PACKET, *PAFD_OPEN_PACKET;



typedef struct _AFD_BIND_INFO {
    ULONG                       ShareAccess;
#define AFD_NORMALADDRUSE		0	// Do not reuse address if
									// already in use but allow
									// subsequent reuse by others
									// (this is a default)
#define AFD_REUSEADDRESS		1	// Reuse address if necessary
#define AFD_WILDCARDADDRESS     2   // Address is a wildcard, no checking
                                    // can be performed by winsock layer.
#define AFD_EXCLUSIVEADDRUSE	3	// Do not allow reuse of this
									// address (admin only).
	TRANSPORT_ADDRESS			Address;
} AFD_BIND_INFO, *PAFD_BIND_INFO;




typedef struct _AFD_CONNECT_JOIN_INFO {
    BOOLEAN     SanActive;
    HANDLE  RootEndpoint;       // Root endpoint for joins
    HANDLE  ConnectEndpoint;    // Connect/leaf endpoint for async connects
    TRANSPORT_ADDRESS   RemoteAddress; // Remote address
} AFD_CONNECT_JOIN_INFO, *PAFD_CONNECT_JOIN_INFO;



#define AFD_PARTIAL_DISCONNECT_SEND 0x01
#define AFD_PARTIAL_DISCONNECT_RECEIVE 0x02
#define AFD_ABORTIVE_DISCONNECT 0x4
#define AFD_UNCONNECT_DATAGRAM 0x08

typedef struct _AFD_PARTIAL_DISCONNECT_INFO {
    ULONG DisconnectMode;
    LARGE_INTEGER Timeout;
} AFD_PARTIAL_DISCONNECT_INFO, *PAFD_PARTIAL_DISCONNECT_INFO;



typedef struct _AFD_LISTEN_RESPONSE_INFO {
    LONG Sequence;
    TRANSPORT_ADDRESS RemoteAddress;
} AFD_LISTEN_RESPONSE_INFO, *PAFD_LISTEN_RESPONSE_INFO;


typedef struct _AFD_LISTEN_INFO {
    BOOLEAN     SanActive;
    ULONG MaximumConnectionQueue;
    BOOLEAN UseDelayedAcceptance;
} AFD_LISTEN_INFO, *PAFD_LISTEN_INFO;


typedef struct _AFD_ACCEPT_INFO {
    BOOLEAN     SanActive;
    LONG		Sequence;
    HANDLE		AcceptHandle;
} AFD_ACCEPT_INFO, *PAFD_ACCEPT_INFO;

typedef struct _AFD_RECV_INFO {
    LPWSABUF BufferArray;
    ULONG BufferCount;
    ULONG AfdFlags;
    ULONG TdiFlags;
} AFD_RECV_INFO, *PAFD_RECV_INFO;

typedef struct _AFD_SEND_INFO {
    LPWSABUF BufferArray;
    ULONG BufferCount;
    ULONG AfdFlags;
    ULONG TdiFlags;
} AFD_SEND_INFO, *PAFD_SEND_INFO;


typedef struct _AFD_RECV_DATAGRAM_INFO {
    LPWSABUF BufferArray;
    ULONG BufferCount;
    ULONG AfdFlags;
    ULONG TdiFlags;
    PVOID Address;				//PTRANSPORT_ADDRESS tdiAddress; Remote address
    PULONG AddressLength;
} AFD_RECV_DATAGRAM_INFO, *PAFD_RECV_DATAGRAM_INFO;

typedef struct _AFD_SEND_DATAGRAM_INFO {
    LPWSABUF BufferArray;
    ULONG BufferCount;
    ULONG AfdFlags;
    TDI_REQUEST_SEND_DATAGRAM   TdiRequest;
    TDI_CONNECTION_INFORMATION  TdiConnInfo;  //PTRANSPROT_ADDRESS tdiAddress ; Remote address
} AFD_SEND_DATAGRAM_INFO, *PAFD_SEND_DATAGRAM_INFO;

#define AFD_INLINE_MODE          0x01
#define AFD_NONBLOCKING_MODE     0x02
#define AFD_MAX_SEND_SIZE        0x03
#define AFD_SENDS_PENDING        0x04
#define AFD_MAX_PATH_SEND_SIZE   0x05
#define AFD_RECEIVE_WINDOW_SIZE  0x06
#define AFD_SEND_WINDOW_SIZE     0x07
#define AFD_CONNECT_TIME         0x08
#define AFD_CIRCULAR_QUEUEING    0x09
#define AFD_GROUP_ID_AND_TYPE    0x0A
#define AFD_GROUP_ID_AND_TYPE    0x0A
#define AFD_REPORT_PORT_UNREACHABLE 0x0B

typedef struct _AFD_INFORMATION {
    ULONG InformationType;
    union {
        BOOLEAN Boolean;
        ULONG Ulong;
        LARGE_INTEGER LargeInteger;
    } Information;
} AFD_INFORMATION, *PAFD_INFORMATION;


#define AFD_MAX_FAST_TRANSPORT_ADDRESS  32
#define AFD_DEVICE_NAME L"\\Device\\Afd"
#define AfdOpenPacket "AfdOpenPacketXX"
#define AFD_OPEN_PACKET_NAME_LENGTH (sizeof(AfdOpenPacket) - 1)
#define DD_TCP_DEVICE_NAME			L"\\Device\\Tcp"
#define TCPTRANSPORT_NAME			DD_TCP_DEVICE_NAME
#define TCPTRANSPORT_NAME_LENGTH	(sizeof(TCPTRANSPORT_NAME) -2)


// Create Entry point
#define	AFDTCPCREATE_EA_BUFFER_LENGTH_IN (									\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)			\
					+ AFD_OPEN_PACKET_NAME_LENGTH + 1 						\
					+ FIELD_OFFSET(AFD_OPEN_PACKET, TransportDeviceName)	\
					+ TCPTRANSPORT_NAME_LENGTH + 2							\
					)


// Bind local Address
#define AFDTCPBIND_INFO_LENGTH_IN	(										\
					FIELD_OFFSET(AFD_BIND_INFO, Address)					\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPBIND_INFO_LENGTH_OUT	(										\
					sizeof(TDI_ADDRESS_INFO)								\
					+ TDI_TRANSPORT_ADDRESS_LENGTH 							\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)


// Connect 					
#define AFDTCPCONNECT_INFO_LENGTH_IN (										\
					FIELD_OFFSET(AFD_CONNECT_JOIN_INFO, RemoteAddress)		\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPCONNECT_INFO_LENGTH_OUT (										\
					sizeof(IO_STATUS_BLOCK)									\
					)


// Disconnect
#define AFDTCPDISCONNECT_INFO_LENGTH_IN (									\
					sizeof(AFD_PARTIAL_DISCONNECT_INFO)						\
					)


// Wait for listen			
#define AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT (						\
					FIELD_OFFSET(AFD_LISTEN_RESPONSE_INFO, RemoteAddress)	\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT (								\
					(AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT) * 5       \
					)


// Stat for listen
#define AFDTCPLISTEN_START_LISTEN_INFO_IN	(								\
					sizeof(AFD_LISTEN_INFO)									\
					)


// Accept
#define AFDTCPACCEPT_IN	(													\
					sizeof(AFD_ACCEPT_INFO)									\
					)


#define AFD_NO_FAST_IO      0x0001      // Always fail Fast IO on this request.
#define AFD_OVERLAPPED      0x0002      // Overlapped operation.

// Send
#define AFDTCPWRITE_IN  (													\
					sizeof(AFD_SEND_INFO)									\
					)
// Recv
#define AFDTCPREAD_IN	(													\
					sizeof(AFD_RECV_INFO)									\
					)


#define AFDTCP_REMOTEADDRESS	(											\
					FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)
// SendDataGram
#define AFDTCPWRITEDATAGRAM_IN	(											\
					sizeof(AFD_SEND_DATAGRAM_INFO)							\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)


// RecvDataGram
#define AFDTCPRECVDATAGRAM_IN	(											\
					sizeof(AFD_RECV_DATAGRAM_INFO)							\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)
	
// SetInformation
#define AFDTCPSETINFORMATION_IN	(											\
					sizeof(AFD_INFORMATION)									\
					)

// QueryInformation
#define AFDTCPQUERYINFORMATION_IN (											\
					sizeof(AFD_INFORMATION)									\
					)



NTSTATUS
AfdTcpCreateSocket(
		OUT PHANDLE			SocketFileHandle,
		OUT PFILE_OBJECT	*SocketFileObject,
		IN	ULONG			SocketType
		);

NTSTATUS
AfdTcpCloseSocket(
	IN HANDLE			SocketFileHandle,
	IN PFILE_OBJECT		SocketFileObject
	);

NTSTATUS
AfdTcpIoCallDriver(
    IN		PDEVICE_OBJECT		DeviceObject,
    IN OUT	PIRP				Irp,
	IN		PIO_STATUS_BLOCK	IoStatusBlock,
	IN		PKEVENT				Event
    );

NTSTATUS
AfdTcpBindSocket(
	IN 	PFILE_OBJECT		SocketFileObject,
	IN	PTDI_ADDRESS_IP		Address,
	IN  ULONG				AddressType,
	OUT PTDI_ADDRESS_IP		RetAddress
	);

NTSTATUS
AfdTcpConnectSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN	PTDI_ADDRESS_IP		Address					
	);

NTSTATUS
AfdTcpDisConnectSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  ULONG				SocketType		
	);

NTSTATUS
AfdTcpListenStartSocket(
	IN	PFILE_OBJECT		SocketFileObject				
	);

NTSTATUS
AfdTcpListenWaitSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	OUT	PTDI_ADDRESS_IP		Address,
	OUT PULONG				pSequence
	);

NTSTATUS
AfdTcpAcceptSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN	HANDLE				AcceptFileHanlde,
	IN  ULONG				Sequence
	);

NTSTATUS
AfdTcpRecvSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	OUT PULONG				TotalRecved
	);

NTSTATUS
AfdTcpSendSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	OUT PULONG				TotalSent
	);

NTSTATUS
AfdTcpRecvDataGramSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	IN	PTDI_ADDRESS_IP		Address,	
	OUT PULONG				TotalRecved
	);

NTSTATUS
AfdTcpSendDataGramSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	IN	PTDI_ADDRESS_IP		Address,	
	OUT PULONG				TotalSent
	);

NTSTATUS
AfdTcpSetInformationSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  PAFD_INFORMATION	AfdInformation
	);

NTSTATUS
AfdTcpGetInformationSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN OUT  PAFD_INFORMATION	AfdInformation
	);



#if DBG
extern LONG	AfdDebugLevel;
#define AfdTcpDebugPrint(l, x)			\
		if(l <= AfdDebugLevel) {		\
			DbgPrint x; 			\
		}
#else	
#define AfdTcpDebugPrint(l, x) 
#endif


							
#endif