// ndascomm.cpp : Defines the entry point for the DLL application.
//

#include <windows.h>
#include <tchar.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <crtdbg.h>
#include <map>
#include <set>
#include <list>
// #include <socketlpx.h>
#include <winsock2.h>
#include <ndas/ndasid.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndascomm.h>
#include <ndas/ndashixnotify.h>
#include <lsp/lsp.h>
#include <lsp/lsp_util.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>
#include "ndascomm_type_internal.h"

#define ASSERT_PARAM_ERROR NDASCOMM_ERROR_INVALID_PARAMETER
#include "ctrace.hxx"
#include "lock.hxx"
#include "init.h"

#ifdef RUN_WPP
#include "ndascomm.tmh"
#endif

// remove this line
#define NDASCOMM_ERROR_NOT_INITIALIZED	0xFFFFFFFF

static const NDAS_OEM_CODE
NDAS_PRIVILEGED_OEM_CODE_DEFAULT = {
	0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E };

#if 0
static const INT64 NDASCOMM_PW_USER	= 0x1F4A50731530EABB;
static const INT64 NDASCOMM_PW_SAMPLE = 0x0001020304050607;
//static const INT64 NDASCOMM_PW_DLINK = 0xCE00983088A66118;
static const INT64 NDASCOMM_PW_RUTTER = 0x1F4A50731530EABB;
//static const INT64 NDASCOMM_PW_SUPER1 = 0x0F0E0D0304050607;
//static const INT64 NDASCOMM_PW_SUPER_V1= 0x3e2b321a4750131e;
//static const INT64 NDASCOMM_PW_SUPER = NDASCOMM_PW_USER;
static const INT64 NDASCOMM_PW_SEAGATE = 0x99A26EBC46274152;
#endif

const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE  = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER  = NDAS_OEM_CODE_DEFAULT;
const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE = { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };
const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE = { 0xCD, NDAS_VID_SEAGATE, 0xFF, 0xFF };

#define AUTOLOCK_CONTEXT(CONTEXT) 	CAutoCritSecLock _auto_context_lock_((CONTEXT)->lock)
#define UNLOCK_CONTEXT(CONTEXT) _auto_context_lock_.Release()

const static UINT32 NDASCOMM_BLOCK_SIZE = (0x0001 << 9);

/*
Protocol / OS dependent codes.
Should be taken out
*/

// parts in lsp_proc.h
// parts in lsp_proc.cpp

struct lpx_addr {
	u_char node[6];
	char _reserved_[10];
};

struct sockaddr_lpx {
	short           sin_family;
	u_short	        port;
	struct lpx_addr slpx_addr;
};

//typedef struct sockaddr_lpx SOCKADDR_LPX;
//typedef struct sockaddr_lpx *PSOCKADDR_LPX;
//typedef struct sockaddr_lpx FAR *LPSOCKADDR_LPX;

struct lsp_proc_context {
	SOCKET s;
};

struct lsp_wait_context {
	DWORD LastTransferredBytes;
	DWORD Timeout;
	WSABUF DataBuffer;
	WSAOVERLAPPED AcceptOverlapped;
};

typedef struct _NDASCOMM_HANDLE_CONTEXT {
	lsp_handle hLSP;
	lsp_proc_context *proc_context;
	lsp_uint8 write_access;
	lsp_uint8 device_node[6];
	lsp_uint8 host_node[6];
	lsp_uint8 vendor_id;
	CCritSecLock *lock;
	lsp_uint32 target_id;
	lsp_uint8 use_dma;
	lsp_uint8 use_48;
	lsp_uint64_ll capacity;
	lsp_uint8 use_lba;
	lsp_uint8 packet_device;
	lsp_uint8 packet_device_type;
	lsp_uint32 address_type;
	lsp_uint32 protocol;
	DWORD LockCount[4]; /* To support nested lock */
	DWORD Flags; /* Connection Flags */
} NDASCOMM_HANDLE_CONTEXT, *PNDASCOMM_HANDLE_CONTEXT;

typedef struct _LPX_REMOTE_NODE {
	u_char node[6];
} LPX_REMOTE_NODE, *PLPX_REMOTE_NODE;

template<>
struct std::less <LPX_REMOTE_NODE> : 
	public std::binary_function <LPX_REMOTE_NODE, LPX_REMOTE_NODE, bool> 
{
	bool operator()(const LPX_REMOTE_NODE& lhs, const LPX_REMOTE_NODE& rhs) const
	{
		for (DWORD i = 0; i < 6; ++i) {
			if (lhs.node[i] < rhs.node[i]) return true;	// less
			if (lhs.node[i] > rhs.node[i]) return false; // greater
		}
		return false; // equal
	}
};

typedef struct _LPX_HOST_NODE {
	u_char node[6];
} LPX_HOST_NODE, *PLPX_HOST_NODE;

template<>
struct std::less <LPX_HOST_NODE> : 
	public std::binary_function <LPX_HOST_NODE, LPX_HOST_NODE, bool> 
{
	bool operator()(const LPX_HOST_NODE& lhs, const LPX_HOST_NODE& rhs) const
	{
		for (DWORD i = 0; i < 6; ++i) {
			if (lhs.node[i] < rhs.node[i]) return true;	// less
			if (lhs.node[i] > rhs.node[i]) return false; // greater
		}
		return false; // equal
	}
};

typedef std::map<LPX_REMOTE_NODE, LPX_HOST_NODE> MAP_REMOTE_HOST;
typedef std::set<LPX_HOST_NODE> SET_HOST_NODE;

// anonymous namespace for local functions
namespace 
{

// Assertion Helper Functions
__forceinline
BOOL 
IsValidWritePtr(
	LPVOID lp,
	UINT_PTR ucb)
{ 
	return !IsBadWritePtr(lp, ucb); 
}

__forceinline 
BOOL 
IsValidReadPtr(
	CONST VOID* lp,
	UINT_PTR ucb) 
{
	return !IsBadReadPtr(lp, ucb); 
}

inline
BOOL
IsValidSocketAddressList(
	const SOCKET_ADDRESS_LIST* SocketAddressList)
{
	ASSERT_PARAM( IsValidReadPtr(SocketAddressList, sizeof(SOCKET_ADDRESS_LIST)) );
	ASSERT_PARAM( SocketAddressList->iAddressCount > 0 );
	ASSERT_PARAM( IsValidReadPtr(SocketAddressList, 
		sizeof(SOCKET_ADDRESS_LIST) + 
		sizeof(SOCKET_ADDRESS) * (SocketAddressList->iAddressCount - 1)) );

	for (int i = 0; i < SocketAddressList->iAddressCount; ++i)
	{
		ASSERT_PARAM( IsValidReadPtr(
			SocketAddressList->Address[i].lpSockaddr,
			SocketAddressList->Address[i].iSockaddrLength) );
	}
	return TRUE;
}

__forceinline
PNDASCOMM_HANDLE_CONTEXT
NdasHandleToContext(
	HNDAS hNdasDevice)
{
	return reinterpret_cast<PNDASCOMM_HANDLE_CONTEXT>(hNdasDevice);
}

__forceinline
void
CopyLpxSockAddrToRemoteNode(
	const sockaddr_lpx* LpxSockAddr,
	LPX_REMOTE_NODE* RemoteNode)
{
	::CopyMemory(
		RemoteNode->node,
		LpxSockAddr->slpx_addr.node,
		sizeof(LpxSockAddr->slpx_addr.node));
}

__forceinline
void
CopyLpxSockAddrToHostNode(
	const sockaddr_lpx* LpxSockAddr,
	LPX_HOST_NODE* HostNode)
{
	::CopyMemory(
		HostNode->node,
		LpxSockAddr->slpx_addr.node,
		sizeof(LpxSockAddr->slpx_addr.node));
}

__forceinline
SOCKET
SocketFromNdasHandleContext(
	PNDASCOMM_HANDLE_CONTEXT context)
{
	lsp_proc_context *lspProcContext = NULL;
	lsp_error_t err = lsp_get_proc_context(context->hLSP, (void **)&lspProcContext);
	if (LSP_ERR_SUCCESS != err)
	{
		_ASSERTE(FALSE && "lsp_get_proc_context cannot be failed!");
		return INVALID_SOCKET;
	}
	return lspProcContext->s;
}

__forceinline
SOCKET 
SocketFromNdasHandle(
	HNDAS hNdasDevice)
{
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	return SocketFromNdasHandleContext(context);
}

BOOL
GetSocketTimeout(
	SOCKET sock,
	int optname, 
	LPDWORD lpdwTimeout)
{
	_ASSERTE(SO_SNDTIMEO == optname || SO_RCVTIMEO == optname);
	int timeo = 0;
	int len = sizeof(timeo);
	int sockerr = ::getsockopt(sock, SOL_SOCKET, optname, (char*) &timeo, &len);
	if (SOCKET_ERROR == sockerr)
	{
		return FALSE;
	}
	*lpdwTimeout = static_cast<DWORD>(timeo);
	return TRUE;
}

inline
BOOL
GetSocketTimeout(
	PNDASCOMM_HANDLE_CONTEXT context,
	int optname,
	LPDWORD lpdwTimeout)
{
	SOCKET sock = SocketFromNdasHandleContext(context);
	return GetSocketTimeout(sock, SO_SNDTIMEO, lpdwTimeout);
}

BOOL
SetSocketTimeout(
	SOCKET sock,
	int optname,
	DWORD timeout)
{
	_ASSERTE(SO_SNDTIMEO == optname || SO_RCVTIMEO == optname);
	int timeo = static_cast<int>(timeout);
	int len = sizeof(timeo);
	int sockerr = ::setsockopt(sock, SOL_SOCKET, optname, (const char*) &timeo, len);
	if (SOCKET_ERROR == sockerr)
	{
		return FALSE;
	}
	return TRUE;
}

inline
BOOL
SetSocketTimeout(
	PNDASCOMM_HANDLE_CONTEXT context,
	int optname,
	DWORD timeout)
{
	SOCKET sock = SocketFromNdasHandleContext(context);
	return SetSocketTimeout(sock, SO_SNDTIMEO, timeout);
}

} // end of anonymous namespace

// #define LPXPROTO_STREAM 214
// #define LPXPROTO_DGRAM  215
#define	NDAS_CTRL_REMOTE_PORT 10000
#define AF_LPX AF_UNSPEC

PTCHAR 
lpx_addr_node_str(
	unsigned char* nodes)
{
	static TCHAR buf[30] = {0};
	StringCchPrintf(buf, 30, _T("%02X:%02X:%02X:%02X:%02X:%02X"),
		nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
	return buf;
}

PTCHAR 
lpx_addr_str(
	struct sockaddr_lpx* lpx_addr)
{
	return lpx_addr_node_str(lpx_addr->slpx_addr.node);
}

void* 
lsp_proc_call 
lsp_proc_mem_alloc(
	void* context, 
	size_t size) 
{
	return ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void 
lsp_proc_call 
lsp_proc_mem_free(
	void* context, 
	void* pblk) 
{
	HeapFree(::GetProcessHeap(), NULL, pblk);
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_send(
	void* context, 
	const void* buf, 
	size_t len, 
	size_t* sent, 
	void **wait_handle_ptr)
{
	lsp_proc_context* lpc = reinterpret_cast<lsp_proc_context*>(context);
	SOCKET s = lpc->s;

	if (NULL == wait_handle_ptr)
	{
		// synchronous send
		int sent_bytes = send(s, static_cast<const char*>(buf), len, 0);
		if (sent_bytes == SOCKET_ERROR)
		{
			return LSP_TRANS_ERROR;
		}
		*sent = sent_bytes;
	}
	else
	{
		LPVOID heapBuffer = ::HeapAlloc(
			::GetProcessHeap(), 
			HEAP_ZERO_MEMORY, 
			sizeof(lsp_wait_context));

		if (NULL == heapBuffer)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return LSP_TRANS_ERROR;
		}

		// Detach aBuffer on successful return
		XTL::AutoProcessHeap aBuffer = heapBuffer;

		// Wait Context Pointer to the allocated heap buffer
		lsp_wait_context *pWaitContext = 
			reinterpret_cast<lsp_wait_context*>(heapBuffer);

		// Data Event
		WSAEVENT hEvent = WSACreateEvent();
		if (NULL == hEvent)
		{
			return LSP_TRANS_ERROR;
		}

		// Transfer Timeout (Receive or Send)
		DWORD dwTimeout;
		if (!GetSocketTimeout(s, SO_SNDTIMEO, &dwTimeout))
		{
			return LSP_TRANS_ERROR;
		}

		// WSABUF must remain valid for the duration of the operation
		pWaitContext->DataBuffer.buf = const_cast<char*>(reinterpret_cast<const char*>(buf));
		pWaitContext->DataBuffer.len = len;
		pWaitContext->AcceptOverlapped.hEvent = hEvent;
		pWaitContext->Timeout = dwTimeout;
	
		DWORD cbSent;
		int sockerr = WSASend(
			s,
			&pWaitContext->DataBuffer,
			1,
			&cbSent,
			0,
			&pWaitContext->AcceptOverlapped, 
			NULL);

		if(SOCKET_ERROR == sockerr)
		{
			if(WSA_IO_PENDING != WSAGetLastError())
			{
				return LSP_TRANS_ERROR;
			}
		}

		*wait_handle_ptr = pWaitContext;

		// Detach the auto buffer on success
		(void) aBuffer.Detach();
	}
	
	return LSP_TRANS_SUCCESS;
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_recv(
	void* context, 
	void* buf, 
	size_t len, 
	size_t* recvd, 
	void **wait_handle_ptr)
{
	lsp_proc_context* lpc = reinterpret_cast<lsp_proc_context*>(context);
	SOCKET s = lpc->s;

	if(NULL == wait_handle_ptr)
	{
		// synchronous receive
		int recv_bytes = recv(s, static_cast<char*>(buf), len, 0);
		if (recv_bytes == SOCKET_ERROR)
		{
			return LSP_TRANS_ERROR;
		}
		*recvd = recv_bytes;
	}
	else
	{
		LPVOID heapBuffer = ::HeapAlloc(
			::GetProcessHeap(), 
			HEAP_ZERO_MEMORY, 
			sizeof(lsp_wait_context));

		if (NULL == heapBuffer)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return LSP_TRANS_ERROR;
		}

		// Detach aBuffer on successful return
		XTL::AutoProcessHeap aBuffer = heapBuffer;

		// Wait Context Pointer to the allocated heap buffer
		lsp_wait_context *pWaitContext = 
			reinterpret_cast<lsp_wait_context*>(heapBuffer);

		// Data Event
		WSAEVENT hEvent = WSACreateEvent();
		if (NULL == hEvent)
		{
			return LSP_TRANS_ERROR;
		}

		// Transfer Timeout (Receive or Send)
		DWORD dwTimeout;
		if (!GetSocketTimeout(s, SO_RCVTIMEO, &dwTimeout))
		{
			return LSP_TRANS_ERROR;
		}

		// WSABUF must remain valid for the duration of the operation
		pWaitContext->DataBuffer.buf = reinterpret_cast<char*>(buf);
		pWaitContext->DataBuffer.len = len;
		pWaitContext->AcceptOverlapped.hEvent = hEvent;
		pWaitContext->Timeout = dwTimeout;

		DWORD cbRecvd;
		int sockerr = WSARecv(
			s,
			&pWaitContext->DataBuffer, 
			1,
			&cbRecvd,
			NULL,
			&pWaitContext->AcceptOverlapped, 
			NULL);

		if (SOCKET_ERROR == sockerr)
		{
			if (WSA_IO_PENDING != WSAGetLastError())
			{
				return LSP_TRANS_ERROR;
			}
		}

		*wait_handle_ptr = pWaitContext;

		// Detach auto buffer on successful return
		(void) aBuffer.Detach();
	}

	return LSP_TRANS_SUCCESS;
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_wait(
	void* context, 
	size_t *bytes_transferred, 
	void *wait_handle)
{
	// Pseudo wait handle
	if (NULL == wait_handle)
	{
		*bytes_transferred = 0;
		return LSP_TRANS_SUCCESS;
	}

	lsp_proc_context* lpc = reinterpret_cast<lsp_proc_context*>(context);
	lsp_wait_context* lpwc = reinterpret_cast<lsp_wait_context*>(wait_handle);

	// Wait until timeout
	DWORD timeout = lpwc->Timeout;
	DWORD waitResult = WSAWaitForMultipleEvents(
		1, 
		&lpwc->AcceptOverlapped.hEvent,
		TRUE,
		timeout,
		FALSE);

	if(WSA_WAIT_EVENT_0 != waitResult)
	{
		return LSP_TRANS_ERROR;
	}

	SOCKET s = lpc->s;
	DWORD transFlags = 0;
	DWORD cbTransferred = 0;

	// Retrieve the result
	BOOL fSuccess = WSAGetOverlappedResult(
		s, 
		&lpwc->AcceptOverlapped,
		&cbTransferred, 
		TRUE, 
		&transFlags);

	if (!fSuccess)
	{
		return LSP_TRANS_ERROR;
	}

	*bytes_transferred = cbTransferred;

	// Free Wait Handle
	(void) ::WSACloseEvent(lpwc->AcceptOverlapped.hEvent);
	(void) ::HeapFree(::GetProcessHeap(), 0, lpwc);

	return LSP_TRANS_SUCCESS;
}


lsp_trans_error_t 
lsp_proc_call
lsp_proc_initialize(
	void *context)
{
	int iResults;
	WSADATA wsaData;

	context = context;

	// make connection
	iResults = ::WSAStartup( MAKEWORD(2, 2), &wsaData );

	switch(iResults)
	{
	case 0:
		break;
	case WSASYSNOTREADY:
	case WSAVERNOTSUPPORTED:
	case WSAEINPROGRESS:
	case WSAEPROCLIM:
	case WSAEFAULT:
	default:
		::SetLastError(iResults);
		return iResults;
	}

	return 0;
}

lsp_trans_error_t 
lsp_proc_call
lsp_proc_uninitialize(
	void *context)
{
	int iResults;
	WSADATA				wsaData;

	context = context;

	// make connection
	iResults = ::WSACleanup();

	switch(iResults)
	{
	case 0:
		break;
	case WSANOTINITIALISED:
	case WSAENETDOWN:
	case WSAEINPROGRESS:
	default:
		::SetLastError(iResults);
		return iResults;
	}

	return 0;
}

LPSOCKET_ADDRESS_LIST
NdasCommImpGetHostAddressList();

SOCKET
NdasCommImpInitializeLpxConnection(
	IN const LPX_REMOTE_NODE* RemoteNode,
	IN const SOCKET_ADDRESS_LIST* AddressListHint OPTIONAL,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL,
	OUT LPX_HOST_NODE* ConnectedHostNode);


/*++

NdasCommGetPassword function ...

Parameters:

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

inline
UINT64 
NdasCommImpGetPassword(
	__in CONST BYTE* pAddress,
	__in CONST NDASID_EXT_DATA* NdasIdExtensionData);

/*++

NdasCommIsHandleValidForRW function tests the NDAS handle if it is good for read/write block device or not
Because NdasCommIsHandleValidForRW set last error, caller function does not need to set last error.

Parameters:

hNdasDevice
[in] NDAS HANDLE which is LANSCSI_PATH pointer type

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

inline
BOOL 
IsValidNdasHandleForRW(
	HNDAS hNdasDevice);

inline
BOOL 
IsValidNdasHandle(
	HNDAS hNdasDevice);

inline
void
NdasCommImpCleanupInitialLocks(HNDAS hNdasDevice);

void 
NdasCommImpDisconnect(
	PNDASCOMM_HANDLE_CONTEXT context,
	DWORD DisconnectFlags);

//BOOL 
//NdasCommConnectionInfoToDeviceID(
//	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
//	PNDAS_UNITDEVICE_ID pUnitDeviceID);

void
NdasCommImpCloseLpxConnection(
	lsp_proc_context* context);

SOCKET 
NdasCommImpCreateLpxConnection(
	IN const sockaddr_lpx* HostSockAddr,
	IN const sockaddr_lpx* RemoteSockAddr,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL);

SOCKET
NdasCommImpCreateLpxConnection(
	IN const LPX_HOST_NODE* HostNode,
	IN const LPX_REMOTE_NODE* RemoteNode,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL);

void
NdasCommImpCloseLpxConnection(
	lsp_proc_context* context)
{
	if(context->s != INVALID_SOCKET)
	{
		int sockret = ::closesocket(context->s);
		_ASSERTE(0 == sockret);
		context->s = INVALID_SOCKET;
	}
}

SOCKET 
NdasCommImpCreateLpxConnection(
	IN const sockaddr_lpx* HostSockAddr,
	IN const sockaddr_lpx* RemoteSockAddr,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL)
{
	XTL::AutoSocket atSocket = socket(
		AF_UNSPEC, 
		SOCK_STREAM, 
		LPXPROTO_STREAM);

	if (INVALID_SOCKET == (SOCKET) atSocket)
	{
		return INVALID_SOCKET;
	}

	if (0 == SendTimeout) SendTimeout = NDASCOMM_SEND_TIMEOUT_DEFAULT;
	if (!SetSocketTimeout(atSocket, SO_SNDTIMEO, SendTimeout))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"SetSocketTimeout(Send:%d) failed, error=0x%X\n",
			SendTimeout, GetLastError());
		return INVALID_SOCKET;
	}

	if (0 == ReceiveTimeout) ReceiveTimeout = NDASCOMM_RECEIVE_TIMEOUT_DEFAULT;
	if (!SetSocketTimeout(atSocket, SO_RCVTIMEO, ReceiveTimeout))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"SetSocketTimeout(Recv:%d) failed, error=0x%X\n",
			ReceiveTimeout, GetLastError());
		return INVALID_SOCKET;
	}

	// binding to lpx_addr_str(&host_addr)
	int sockret = bind(
		atSocket, 
		(const sockaddr*) HostSockAddr, 
		sizeof(sockaddr_lpx));
	if (SOCKET_ERROR == sockret)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"bind failed, socket=%p, error=0x%X\n",
			(HANDLE) (SOCKET) atSocket, GetLastError());

		return INVALID_SOCKET;
	}


	// binded to lpx_addr_str(&host_addr)
	// connecting to lpx_addr_str(&remote_addr) at lpx_addr_str(&host_addr)
	sockret = connect(
		atSocket, 
		(const sockaddr*) RemoteSockAddr, 
		sizeof(sockaddr_lpx));

	if (SOCKET_ERROR == sockret)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"connect failed, socket=%p, error=0x%X\n",
			(HANDLE)(SOCKET)atSocket, GetLastError());
		return INVALID_SOCKET;
	}

	return atSocket.Detach();
}

SOCKET
NdasCommImpCreateLpxConnection(
	IN const LPX_HOST_NODE* HostNode,
	IN const LPX_REMOTE_NODE* RemoteNode,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL)
{

	struct sockaddr_lpx hostSockAddr;
	hostSockAddr.port = 0;
	hostSockAddr.sin_family = AF_LPX;
	::CopyMemory(
		&hostSockAddr.slpx_addr.node, 
		HostNode, 
		sizeof(hostSockAddr.slpx_addr.node));

	struct sockaddr_lpx remoteSockAddr;
	remoteSockAddr.port = htons( NDAS_CTRL_REMOTE_PORT );
	remoteSockAddr.sin_family = AF_LPX;
	::CopyMemory(
		&remoteSockAddr.slpx_addr.node, 
		RemoteNode, 
		sizeof(remoteSockAddr.slpx_addr.node));

	return NdasCommImpCreateLpxConnection(
		&hostSockAddr, 
		&remoteSockAddr, 
		SendTimeout, 
		ReceiveTimeout);
}

LPSOCKET_ADDRESS_LIST
NdasCommImpGetHostAddressList(
	SOCKET sock)
{
	//
	// Query Buffer length should not affect last error
	//
	DWORD cbSockAddrList = 0;
	
	//
	// Even if the requested size is 0, ::HeapAlloc does not return NULL
	// on success.
	//
	LPVOID heapBuffer;
	APITRACE(
	heapBuffer = ::HeapAlloc(::GetProcessHeap(), 0, cbSockAddrList) );

	if (NULL == heapBuffer)
	{
		::SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	LPSOCKET_ADDRESS_LIST pSockAddrList = (LPSOCKET_ADDRESS_LIST) heapBuffer;

	while (TRUE)
	{
		DWORD savedError = ::GetLastError();

		int sockret;

		// SOCKTRACE(
		sockret = ::WSAIoctl(
			sock, 
			SIO_ADDRESS_LIST_QUERY, 
			0, 0, 
			pSockAddrList, cbSockAddrList, &cbSockAddrList, 
			NULL, NULL); // ); 

		if (sockret != SOCKET_ERROR)
		{
			::SetLastError(savedError);
			return pSockAddrList;
		}

		if (WSAEFAULT != ::WSAGetLastError())
		{
			if (NULL != pSockAddrList)
			{
				APITRACE( 
				::HeapFree(::GetProcessHeap(), 0, pSockAddrList) );
			}
			return NULL;
		}

		::SetLastError(savedError);

		APITRACE(
		heapBuffer = ::HeapReAlloc(
			::GetProcessHeap(), 
			0, 
			pSockAddrList, 
			cbSockAddrList) );

		if (NULL == heapBuffer)
		{
			if (NULL != pSockAddrList)
			{
				APITRACE(
				::HeapFree(::GetProcessHeap(), 0, pSockAddrList) );
			}
			::SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
			return NULL;
		}

		pSockAddrList = (LPSOCKET_ADDRESS_LIST) heapBuffer;
	}
}

LPSOCKET_ADDRESS_LIST
NdasCommImpGetHostAddressList()
{
	XTL::AutoSocket atSocket = ::socket(
		AF_UNSPEC, SOCK_STREAM, LPXPROTO_STREAM);
	if (INVALID_SOCKET == (SOCKET) atSocket)
	{
		return NULL;
	}

	return NdasCommImpGetHostAddressList(atSocket);
}

SOCKET
NdasCommImpInitializeLpxConnection(
	IN const LPX_REMOTE_NODE* RemoteNode,
	IN const SOCKET_ADDRESS_LIST* BindingSocketAddressList OPTIONAL,
	IN DWORD SendTimeout OPTIONAL,
	IN DWORD ReceiveTimeout OPTIONAL,
	OUT LPX_HOST_NODE* ConnectedHostNode)
{
	_ASSERTE(IsValidReadPtr(RemoteNode, sizeof(LPX_REMOTE_NODE)));
	_ASSERTE(IsValidWritePtr(ConnectedHostNode, sizeof(LPX_HOST_NODE)));

	static MAP_REMOTE_HOST HostRemoteNodeCache; // caches succeeded host address
	SET_HOST_NODE failedHostNodeSet; // store all the tried host addresses

	//
	// try connecting with AddressListHint
	//
	if (BindingSocketAddressList)
	{
		for (int i = 0; i < BindingSocketAddressList->iAddressCount; ++i)
		{
			LPX_HOST_NODE hostNode;
			sockaddr_lpx* salpx = (sockaddr_lpx*)
				BindingSocketAddressList->Address[i].lpSockaddr;
			CopyLpxSockAddrToHostNode(salpx, &hostNode);
			SOCKET sock = NdasCommImpCreateLpxConnection(
				&hostNode, 
				RemoteNode, 
				SendTimeout, 
				ReceiveTimeout);
			if (INVALID_SOCKET != sock)
			{
				*ConnectedHostNode = hostNode;
				HostRemoteNodeCache[*RemoteNode] = *ConnectedHostNode;
				return sock;
			}
			failedHostNodeSet.insert(hostNode);
		}
		//
		// LocalAddressList is specified, only those will be tried.
		//
		return INVALID_SOCKET;
	}

	//
	// try connecting with cached host address
	//
	MAP_REMOTE_HOST::const_iterator itr = HostRemoteNodeCache.find(*RemoteNode);
	if (HostRemoteNodeCache.end() != itr)
	{
		const LPX_HOST_NODE& hostNode = itr->second;
		if (failedHostNodeSet.end() != failedHostNodeSet.find(hostNode))
		{
			// if failed already, do not attempt
		}
		else
		{
			SOCKET sock = NdasCommImpCreateLpxConnection(
				&hostNode, 
				RemoteNode,
				SendTimeout,
				ReceiveTimeout);
			if (INVALID_SOCKET != sock)
			{
				*ConnectedHostNode = hostNode;
				HostRemoteNodeCache[*RemoteNode] = *ConnectedHostNode;
				return sock;
			}
			failedHostNodeSet.insert(hostNode);
		}
	}

	//
	// retrieve host address list
	//
	{
		LPSOCKET_ADDRESS_LIST hostAddrList = NdasCommImpGetHostAddressList();
		if (NULL == hostAddrList)
		{
			return INVALID_SOCKET;
		}
		// addr_list should be freed on return
		XTL::AutoProcessHeap atAddrList = hostAddrList;
		for (int i = 0; i < hostAddrList->iAddressCount; ++i)
		{
			LPX_HOST_NODE hostNode;
			sockaddr_lpx* salpx = (sockaddr_lpx*)
				hostAddrList->Address[i].lpSockaddr;
			CopyLpxSockAddrToHostNode(salpx, &hostNode);
			if (failedHostNodeSet.end() != failedHostNodeSet.find(hostNode))
			{
				// if failed already, do not attempt
				continue;
			}
			SOCKET sock = NdasCommImpCreateLpxConnection(
				&hostNode, 
				RemoteNode,
				SendTimeout,
				ReceiveTimeout);
			if (INVALID_SOCKET != sock)
			{
				*ConnectedHostNode = hostNode;
				HostRemoteNodeCache[*RemoteNode] = *ConnectedHostNode;
				return sock;
			}
			//
			// We don't use failedHostNodeSet anymore.
			// If the code will be added, the following line should be added.
			// failedHostNodeSet.insert(hostNode);
		}
	}

	return INVALID_SOCKET;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Communication API Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

static BOOL g_bInitialized = FALSE;
static volatile LONG g_InitCount = 0;

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommInitialize()
{
	DllEnterInitSync();

	LONG refCount = ::InterlockedIncrement(&g_InitCount);

	_ASSERTE(refCount >= 1);

	if (refCount > 1)
	{
		return DllLeaveInitSync(), TRUE;
	}

	_ASSERTE(1 == refCount);

	lsp_trans_error_t err_trans = lsp_proc_initialize(NULL);
	if(LSP_SUCCESS != err_trans)
	{
		LONG ref = ::InterlockedDecrement(&g_InitCount);
		_ASSERTE(0 == ref);
		DllLeaveInitSync();
		return DllLeaveInitSync(), FALSE;
	}

	g_bInitialized = TRUE;

	return DllLeaveInitSync(), TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUninitialize()
{
	DllEnterInitSync();

	LONG refCount = ::InterlockedDecrement(&g_InitCount);

	_ASSERTE(refCount >= 0);

	if (refCount > 0)
	{
		return DllLeaveInitSync(), TRUE;
	}

	_ASSERTE(0 == refCount);

	lsp_trans_error_t err_trans = lsp_proc_uninitialize(NULL);
	if(LSP_SUCCESS != err_trans)
	{
		refCount = ::InterlockedIncrement(&g_InitCount);
		_ASSERTE(1 == refCount);
		return DllLeaveInitSync(), FALSE;
	}

	g_bInitialized = FALSE;

	return DllLeaveInitSync(), TRUE;
}

NDASCOMM_API
DWORD
NDASAPICALL
NdasCommGetAPIVersion()
{
	return (DWORD)MAKELONG(
		NDASCOMM_API_VERSION_MAJOR, 
		NDASCOMM_API_VERSION_MINOR);
}

BOOL
NdasCommImpConnectionInfoToDeviceID(
	__in CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	__out PNDAS_UNITDEVICE_ID pUnitDeviceID,
	__out_opt NDASID_EXT_DATA* NdasIdExtentionData)
{
	BOOL bResults;

	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM( sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size );
	ASSERT_PARAM( 
		NDASCOMM_CIT_SA_LPX == pConnectionInfo->AddressType ||
//		NDASCOMM_CIT_SA_IN == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_NDAS_IDA == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_NDAS_IDW == pConnectionInfo->AddressType  ||
		NDASCOMM_CIT_DEVICE_ID == pConnectionInfo->AddressType);
	ASSERT_PARAM(0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pConnectionInfo->Protocol);
	ASSERT_PARAM( IsValidWritePtr(pUnitDeviceID, sizeof(NDAS_UNITDEVICE_ID)) );

	switch(pConnectionInfo->AddressType)
	{
	case NDASCOMM_CIT_DEVICE_ID:
		{
			C_ASSERT(sizeof(pUnitDeviceID->DeviceId.Node) ==
				sizeof(pConnectionInfo->Address.DeviceId.Node));

			::CopyMemory(
				pUnitDeviceID->DeviceId.Node, 
				pConnectionInfo->Address.DeviceId.Node, 
				sizeof(pUnitDeviceID->DeviceId.Node));
			NdasIdExtentionData->VID = pConnectionInfo->Address.DeviceId.VID;
		}
		break;
	case NDASCOMM_CIT_NDAS_IDW:
		{
			WCHAR wszID[20 +1] = {0};
			WCHAR wszKey[5 +1] = {0};

			::CopyMemory(wszID, pConnectionInfo->Address.NdasIdW.Id, sizeof(WCHAR) * 20);
			::CopyMemory(wszKey, pConnectionInfo->Address.NdasIdW.Key, sizeof(WCHAR) * 5);

			if(pConnectionInfo->WriteAccess)
			{
				// ***** is a magic write key
				if (0 == lstrcmpW(wszKey, L"*****"))
				{
					API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
						::NdasIdValidateW(wszID, NULL) );
				}
				else
				{
					API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
						::NdasIdValidateW(wszID, wszKey) );
				}
			}
			else
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateW(wszID, NULL) );
			}
		
			API_CALLEX( NDASCOMM_ERROR_INVALID_PARAMETER,
				::NdasIdStringToDeviceExW(
					wszID, 
					&pUnitDeviceID->DeviceId, 
					NULL, 
					NdasIdExtentionData) );
		}
		break;
	case NDASCOMM_CIT_NDAS_IDA:
		{
			CHAR szID[20 +1] = {0};
			CHAR szKey[5 +1] = {0};

			::CopyMemory(szID, pConnectionInfo->Address.NdasIdA.Id, 20);
			::CopyMemory(szKey, pConnectionInfo->Address.NdasIdA.Key, 5);

			if(pConnectionInfo->WriteAccess)
			{
				// Workaround:
				// There is no way to obtain the write key from the 
				// NDAS Service, neither there is a public API to generate
				// a write key. To assist the RW connection only
				// with the NDAS ID, we would use a magic write key
				// which can be a write key for all connections,
				// only for using NdasComm API.
				// "*****" will be a magic write key for the time being.
				// This behavior should be changed in the future releases.
				//
				if (0 == lstrcmpA(szKey, "*****"))
				{
					API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
						::NdasIdValidateA(szID, NULL) );
				}
				else
				{
					API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
						::NdasIdValidateA(szID, szKey) );
				}
			}
			else
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateA(szID, NULL) );
			}

			API_CALLEX( NDASCOMM_ERROR_INVALID_PARAMETER,
				::NdasIdStringToDeviceExA(
					szID, 
					&pUnitDeviceID->DeviceId, 
					NULL, 
					NdasIdExtentionData) );
		}
		break;
	default:
		ASSERT_PARAM(FALSE);
		break;
	}

	pUnitDeviceID->UnitNo = pConnectionInfo->UnitNo;

	return TRUE;
}

PNDASCOMM_HANDLE_CONTEXT
NdasCommImpCreateContext()
{
	const DWORD cbContextBlock = 
		sizeof(NDASCOMM_HANDLE_CONTEXT) + sizeof(lsp_proc_context);

	LPVOID heapBuffer = ::HeapAlloc(
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY,
		cbContextBlock);

	if (NULL == heapBuffer)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"HeapAlloc failed, bytes=%d\n", cbContextBlock);
		::SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	PNDASCOMM_HANDLE_CONTEXT context = 
		reinterpret_cast<PNDASCOMM_HANDLE_CONTEXT>(heapBuffer);

	context->proc_context = 
		reinterpret_cast<lsp_proc_context*>(
		reinterpret_cast<LPBYTE>(heapBuffer) + 
		sizeof(NDASCOMM_HANDLE_CONTEXT));

	context->proc_context->s = INVALID_SOCKET;

	context->lock = new CCritSecLock;
	if (NULL == context->lock || 
		! context->lock->Initialize())
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Context lock init failed, error=0x%X\n", GetLastError());
		XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, heapBuffer));
		::SetLastError(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	return context;
}

inline
BOOL
IsValidNdasConnectionInfo(
	CONST NDASCOMM_CONNECTION_INFO* pci)
{
	// process parameter
	ASSERT_PARAM( IsValidReadPtr(pci, sizeof(NDASCOMM_CONNECTION_INFO)));
	ASSERT_PARAM( sizeof(NDASCOMM_CONNECTION_INFO) == pci->Size );
	ASSERT_PARAM(
		NDASCOMM_CIT_NDAS_IDA == pci->AddressType ||		
		NDASCOMM_CIT_NDAS_IDW == pci->AddressType ||		
		NDASCOMM_CIT_DEVICE_ID == pci->AddressType ||
		NDASCOMM_CIT_SA_LPX == pci->AddressType ||
		NDASCOMM_CIT_SA_IN == pci->AddressType
		);

	ASSERT_PARAM(
		NDASCOMM_LOGIN_TYPE_NORMAL == pci->LoginType ||
		NDASCOMM_LOGIN_TYPE_DISCOVER == pci->LoginType);

	ASSERT_PARAM(0 == pci->UnitNo || 1 == pci->UnitNo);

	// Only LPX is available at this time
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pci->Protocol);

	// A pointer to a socket address list is very prone to corrupting the memory
	const SOCKET_ADDRESS_LIST* pSockAddrList = 
		reinterpret_cast<const SOCKET_ADDRESS_LIST*>(pci->BindingSocketAddressList);
	if (NULL != pSockAddrList && !IsValidSocketAddressList(pSockAddrList))
	{
		// Last Error is set by IsValidSocketAddressList
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Invalid Binding Socket Address List, error=0x%X\n",
			GetLastError());
		return FALSE;
	}
	return TRUE;
}

NDASCOMM_API
HNDAS
NdasCommConnectEx(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo,
	__in LPOVERLAPPED Overlapped)
{
	SetLastError(NDASCOMM_ERROR_NOT_IMPLEMENTED);
	return NULL;
}

NDASCOMM_API
HNDAS
NDASAPICALL
NdasCommConnect(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo)
{
	lsp_error_t err;
	lsp_trans_error_t err_trans;
	int result;
	lsp_transport_proc proc;
	lsp_login_info login_info;
		
	if (!g_bInitialized) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NDASCOMM_ERROR_NOT_INITIALIZED!\n");
		::SetLastError(NDASCOMM_ERROR_NOT_INITIALIZED);
		return NULL;
	}

	if (!IsValidNdasConnectionInfo(pConnectionInfo))
	{
		// Last Error is set by IsValidNdasConnectionInfo
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Invalid Binding Socket Address List, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	NDASID_EXT_DATA ndasIdExtension;
	NDAS_UNITDEVICE_ID unitDeviceID;
	
	BOOL fSuccess = NdasCommImpConnectionInfoToDeviceID(
		pConnectionInfo, 
		&unitDeviceID,
		&ndasIdExtension);

	if (!fSuccess)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"DeviceID is invalid, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	unitDeviceID.UnitNo = pConnectionInfo->UnitNo;

	PNDASCOMM_HANDLE_CONTEXT context = NdasCommImpCreateContext();
	if (NULL == context)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Handle.CreateContext failed, error=0x%X\n",
			GetLastError());
		return NULL;
	}

	// store connection flags
	context->Flags = pConnectionInfo->Flags;

	LPX_REMOTE_NODE remoteNode;

	::CopyMemory(
		remoteNode.node,
		unitDeviceID.DeviceId.Node,
		sizeof(remoteNode.node));

	C_ASSERT(sizeof(remoteNode.node) == sizeof(unitDeviceID.DeviceId.Node));

	const SOCKET_ADDRESS_LIST* pBindingSockAddrList = 
		reinterpret_cast<const SOCKET_ADDRESS_LIST*>(
			pConnectionInfo->BindingSocketAddressList);

	LPX_HOST_NODE hostNode;

	SOCKET sock = NdasCommImpInitializeLpxConnection(
		&remoteNode, 
		pBindingSockAddrList, 
		pConnectionInfo->SendTimeout,
		pConnectionInfo->ReceiveTimeout,
		&hostNode);
	
	if (INVALID_SOCKET == sock)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"InitializeLpxConnection failed, error=0x%X\n",
			GetLastError());
		goto fail;
	}

	context->proc_context->s = sock;

	::CopyMemory(
		context->host_node,
		hostNode.node,
		sizeof(context->host_node));

	C_ASSERT(sizeof(context->host_node) == sizeof(hostNode.node));

	proc.mem_alloc = lsp_proc_mem_alloc;
	proc.mem_free = lsp_proc_mem_free;
	proc.send = lsp_proc_send;
	proc.recv = lsp_proc_recv;
	proc.wait = lsp_proc_wait;

	login_info.login_type = 
		(NDASCOMM_LOGIN_TYPE_NORMAL == pConnectionInfo->LoginType ) ?
		LSP_LOGIN_TYPE_NORMAL : LSP_LOGIN_TYPE_DISCOVER;

	login_info.password = 
		(pConnectionInfo->OEMCode.UI64Value) ? 
			pConnectionInfo->OEMCode.UI64Value :
	        NdasCommImpGetPassword(unitDeviceID.DeviceId.Node, &ndasIdExtension);

	login_info.unit_no = (lsp_uint8)unitDeviceID.UnitNo;
	login_info.write_access = (lsp_uint8)(pConnectionInfo->WriteAccess) ? 1 : 0;
	login_info.supervisor_password = 
		(pConnectionInfo->Flags & NDASCOMM_CNF_ENABLE_DEFAULT_PRIVILEGED_MODE) ?
		NDAS_PRIVILEGED_OEM_CODE_DEFAULT.UI64Value : 
		pConnectionInfo->PrivilegedOEMCode.UI64Value;

	// create session
	context->hLSP = lsp_create_session(&proc, context->proc_context);
	API_CALLEX_JMP(NDASCOMM_ERROR_LOGIN_COMMUNICATION, fail, context->hLSP);

	LSP_CALL_JMP(fail, lsp_login(context->hLSP, &login_info));

	context->write_access = (lsp_uint8)(pConnectionInfo->WriteAccess) ? 1 : 0;

	::CopyMemory(
		context->device_node, 
		unitDeviceID.DeviceId.Node, 
		sizeof(context->device_node));

	context->target_id = pConnectionInfo->UnitNo;
	context->vendor_id = ndasIdExtension.VID;
	context->address_type = pConnectionInfo->AddressType;
	context->protocol = pConnectionInfo->Protocol;

	UINT32 protocolVersion;

	LSP_CALL_JMP(fail, 
		lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_proto_version, 
		&protocolVersion, 
		sizeof(protocolVersion)) );

	// Lock cleanup routine
	if (context->Flags & NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT)
	{
		// disabled by the flag
	}
	else
	{
		// use lock cleanup routine
		UINT32 hardwareVersion = 0;
		UINT16 hardwareRevision = 0;

		LSP_CALL_JMP(fail,
			lsp_get_handle_info(
			context->hLSP,
			lsp_handle_info_hw_version,
			&hardwareVersion,
			sizeof(hardwareVersion)));

		LSP_CALL_JMP(fail,
			lsp_get_handle_info(
			context->hLSP,
			lsp_handle_info_hw_revision,
			&hardwareRevision,
			sizeof(hardwareRevision)));

		// only 2.0R0 requires this
		if (LSP_LOGIN_TYPE_NORMAL == login_info.login_type &&
			((2 == hardwareVersion && 0 == hardwareRevision)))
		{
			NdasCommImpCleanupInitialLocks(reinterpret_cast<HNDAS>(context));
		}
	}


	if (LSP_LOGIN_TYPE_NORMAL == login_info.login_type && !login_info.supervisor_password)
	{
		LSP_CALL_JMP(fail,
		lsp_ide_handshake(
			context->hLSP,
			context->target_id, 0, 0,
			&context->use_dma,
			&context->use_48,
			&context->use_lba,
			&context->capacity,
			&context->packet_device,
			&context->packet_device_type) );
	}
	else
	{
		// Proto V1.0 does not support ide command in discover mode
		context->use_dma = 0;
		context->use_48 = 0;
		context->use_lba = 0;
		context->capacity.low = 0;
		context->capacity.high = 0;
		context->packet_device = 0;
		context->packet_device_type = 0;
	}

	return reinterpret_cast<HNDAS>(context);

fail:

	{
		DWORD dwLastErr = ::GetLastError();
		if(context)
		{
			(void) NdasCommImpDisconnect(context, NDASCOMM_DF_NONE);
		}
		::SetLastError(dwLastErr);
	}

	return NULL;
}

void 
NdasCommImpDisconnect(
	PNDASCOMM_HANDLE_CONTEXT context,
	DWORD DisconnectFlags)
{
	DWORD dwLastErr = ::GetLastError();

	CCritSecLock* lock = context->lock;

	if (lock)
	{
		lock->Lock();
	}

	if (context->hLSP)
	{
		if (!(DisconnectFlags & NDASCOMM_DF_DONT_LOGOUT))
		{
			lsp_error_t lsp_error = lsp_logout(context->hLSP);
			if (LSP_SUCCESS != lsp_error)
			{
				XTLTRACE1(TRACE_LEVEL_WARNING,
					"lsp_logout failed, lsp_error=0x%X\n", lsp_error);
			}
		}
	}

	if (context->hLSP)
	{
		lsp_destroy_session(context->hLSP);
		context->hLSP = NULL;
	}

	if (context->proc_context)
	{
		NdasCommImpCloseLpxConnection(context->proc_context);
		context->proc_context = NULL;
	}

	if (context->lock)
	{
//		delete context->lock;
		context->lock = NULL;
	}

	XTLVERIFY(HeapFree(GetProcessHeap(), NULL, context));

	if(lock)
	{
		lock->Unlock();
		delete lock;
	}

	::SetLastError(dwLastErr);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnect(
	HNDAS hNdasDevice)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	NdasCommImpDisconnect(context, NDASCOMM_DF_NONE);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnectEx(
	__in HNDAS NdasHandle, 
	__in DWORD DisconnectFlags)
{
	ASSERT_PARAM( IsValidNdasHandle(NdasHandle) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(NdasHandle);

	NdasCommImpDisconnect(context, DisconnectFlags);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceRead(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	OUT PBYTE	pData)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidReadPtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	lsp_error_t err;
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	INT64	l_i64LocationAbsolute;
	INT64	l_i64Capacity;
	UINT16	l_usSectorCount;
	UINT64	l_ui64SectorCountLeft;
	PBYTE	l_pData;

	// lsp variables
	lsp_uint32 max_request_blocks;
	lsp_uint64_ll location;
	lsp_uint16 sectors;
	void *data;
	size_t len;

	AUTOLOCK_CONTEXT(context);

	// set variables
	LSP_CALL( lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_max_blocks, 
		&max_request_blocks, 
		sizeof(max_request_blocks)) );

	l_i64Capacity = (INT64)((UINT64)context->capacity.low + (((UINT64)context->capacity.high) << 32));
	l_i64LocationAbsolute = (i64Location < 0) ? l_i64Capacity + i64Location : i64Location;
	ASSERT_PARAM(l_i64LocationAbsolute >= 0 && l_i64LocationAbsolute < l_i64Capacity);

	l_pData = pData;

	l_ui64SectorCountLeft = ui64SectorCount;
	while(l_ui64SectorCountLeft > 0)
	{
		l_usSectorCount = (UINT16)min(l_ui64SectorCountLeft, max_request_blocks);
	
		sectors = (lsp_uint16)l_usSectorCount;
		location.low = (lsp_uint32)(l_i64LocationAbsolute & 0x00000000FFFFFFFF);
		location.high = (lsp_uint32)((l_i64LocationAbsolute & 0xFFFFFFFF00000000) >> 32);
		data = l_pData;
		len = sectors * NDASCOMM_BLOCK_SIZE;

		LSP_CALL( lsp_ide_read(
			context->hLSP, 
			context->target_id, 0, 0, 
			context->use_dma, context->use_48, 
			&location, sectors, 
			data, len) );

		l_ui64SectorCountLeft -= l_usSectorCount;
		l_i64LocationAbsolute += l_usSectorCount;
		l_pData += l_usSectorCount * NDASCOMM_BLOCK_SIZE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceLockedWrite(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN OUT PBYTE pData)
{
	ASSERT_PARAM( IsValidNdasHandleForRW(hNdasDevice) );

	BOOL fSuccess = NdasCommLockDevice(hNdasDevice, 0, NULL);
	if (!fSuccess)
	{
		return FALSE;
	}

	__try
	{
		fSuccess = NdasCommBlockDeviceWrite(hNdasDevice, i64Location, ui64SectorCount, pData);
	}
	__finally
	{
		DWORD dwLastError = ::GetLastError();
		{
			(void) NdasCommUnlockDevice(hNdasDevice, 0, NULL);
		}
		::SetLastError(dwLastError);
	}

	return fSuccess;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceLockedWriteSafeBuffer(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN CONST BYTE* pData)
{
	ASSERT_PARAM( IsValidNdasHandleForRW(hNdasDevice) );

	BOOL fSuccess = NdasCommLockDevice(hNdasDevice, 0, NULL);
	if (!fSuccess)
	{
		return FALSE;
	}

	NdasCommBlockDeviceWriteSafeBuffer(hNdasDevice, i64Location, ui64SectorCount, pData);

	DWORD dwLastError = ::GetLastError();
	{
		(void) NdasCommUnlockDevice(hNdasDevice, 0, NULL);
	}
	::SetLastError(dwLastError);

	return fSuccess;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceWrite(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN OUT PBYTE pData)
{
	ASSERT_PARAM( IsValidNdasHandleForRW(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	INT64	l_i64LocationAbsolute;
	INT64	l_i64Capacity;
	UINT16	l_usSectorCount;
	UINT64	l_ui64SectorCountLeft;
	PBYTE l_pData;

	// lsp variables
	lsp_uint32 max_request_blocks;
	lsp_uint64_ll location;
	lsp_uint16 sectors;
	void *data;
	size_t len;

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	// set variables
	LSP_CALL( lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_max_blocks, 
		&max_request_blocks, sizeof(max_request_blocks)) );

	l_i64Capacity = (INT64)((UINT64)context->capacity.low + (((UINT64)context->capacity.high) << 32));
	l_i64LocationAbsolute = (i64Location < 0) ? l_i64Capacity + i64Location : i64Location;
	ASSERT_PARAM(l_i64LocationAbsolute >= 0 && l_i64LocationAbsolute < l_i64Capacity);
	l_pData = pData;
	l_ui64SectorCountLeft = ui64SectorCount;

	while(l_ui64SectorCountLeft > 0)
	{
		l_usSectorCount = (UINT16)min(l_ui64SectorCountLeft, max_request_blocks);

		sectors = (lsp_uint16)l_usSectorCount;
		location.low = (lsp_uint32)(l_i64LocationAbsolute & 0x00000000FFFFFFFF);
		location.high = (lsp_uint32)((l_i64LocationAbsolute & 0xFFFFFFFF00000000) >> 32);
		data = l_pData;
		len = sectors * NDASCOMM_BLOCK_SIZE;

		LSP_CALL( lsp_ide_write(
			context->hLSP, context->target_id, 
			0, 0, 
			context->use_dma, context->use_48, 
			&location, sectors, 
			data, len) );

		l_ui64SectorCountLeft -= l_usSectorCount;
		l_i64LocationAbsolute += l_usSectorCount;
		l_pData += l_usSectorCount * NDASCOMM_BLOCK_SIZE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceWriteSafeBuffer(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN CONST BYTE* pData)
{
	ASSERT_PARAM( IsValidNdasHandleForRW(hNdasDevice) );
	ASSERT_PARAM(IsValidReadPtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	lsp_uint16 data_encrypt_algo;

	// do not lock this function, NdasCommBlockDeviceWrite will.
//	AUTOLOCK_CONTEXT(context);

	LSP_CALL( lsp_get_handle_info(
		context->hLSP,
		lsp_handle_info_hw_data_encrypt_algo,
		&data_encrypt_algo,
		sizeof(data_encrypt_algo)) );

	if(data_encrypt_algo)
	{
		PBYTE l_data = (PBYTE) ::HeapAlloc(
			::GetProcessHeap(), 0, 
			NDASCOMM_BLOCK_SIZE * (UINT32)ui64SectorCount);
		API_CALLEX(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY, NULL != l_data);

		::CopyMemory(l_data, pData, NDASCOMM_BLOCK_SIZE * (UINT32)ui64SectorCount);

		BOOL fSuccess = ::NdasCommBlockDeviceWrite(
			hNdasDevice, 
			i64Location, ui64SectorCount, 
			l_data);

		(VOID) ::HeapFree(::GetProcessHeap(), 0, l_data);

		return fSuccess;
	}
	else
	{
		return NdasCommBlockDeviceWrite(hNdasDevice, i64Location, ui64SectorCount, (PBYTE)pData);
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceVerify(
	IN HNDAS	hNdasDevice,
	IN INT64	i64Location,
	IN UINT64 ui64SectorCount)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	// lsp variables
	lsp_uint32 max_request_blocks;
	lsp_uint64_ll location;
	lsp_uint16 sectors;

	AUTOLOCK_CONTEXT(context);

	// set variables
	LSP_CALL( lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_max_blocks, 
		&max_request_blocks, sizeof(max_request_blocks)) );

	INT64 l_i64Capacity = (INT64)((UINT64)context->capacity.low + (((UINT64)context->capacity.high) << 32));
	INT64 l_i64LocationAbsolute = (i64Location < 0) ? l_i64Capacity + i64Location : i64Location;
	ASSERT_PARAM(l_i64LocationAbsolute >= 0 && l_i64LocationAbsolute < l_i64Capacity);

	UINT64 l_ui64SectorCountLeft = ui64SectorCount;
	while(l_ui64SectorCountLeft > 0)
	{
		UINT16 l_usSectorCount = (UINT16)min(l_ui64SectorCountLeft, max_request_blocks);

		sectors = (lsp_uint16)l_usSectorCount;
		location.low = (lsp_uint32)(l_i64LocationAbsolute & 0x00000000FFFFFFFF);
		location.high = (lsp_uint32)((l_i64LocationAbsolute & 0xFFFFFFFF00000000) >> 32);
		LSP_CALL( lsp_ide_verify(
			context->hLSP, context->target_id, 0, 0, 
			context->use_48, 
			&location, sectors) );

		l_ui64SectorCountLeft -= l_usSectorCount;
		l_i64LocationAbsolute += l_usSectorCount;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetFeatures(
	IN HNDAS hNdasDevice,
	IN BYTE feature,
	IN BYTE param0,
	IN BYTE param1,
	IN BYTE param2,
	IN BYTE param3
	)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	LSP_CALL( lsp_ide_setfeatures(
		context->hLSP, context->target_id, 0, 0, 
		feature, param0, param1, param2, param3) );

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommVendorCommand(
	IN HNDAS hNdasDevice,
	IN NDASCOMM_VCMD_COMMAND vop_code,
	IN OUT PNDASCOMM_VCMD_PARAM param,
	IN OUT PBYTE pWriteData,
	IN UINT32 uiWriteDataLen,
	IN OUT PBYTE pReadData,
	IN UINT32 uiReadDataLen)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	// Write is IN and Read is OUT!. However, both are mutable
	ASSERT_PARAM( IsValidWritePtr(pWriteData, uiWriteDataLen) );
	ASSERT_PARAM( IsValidWritePtr(pReadData, uiReadDataLen) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	UINT64 param_8; // 8 bytes if HwVersion <= 2
	lsp_uint8 param_length = sizeof(param_8);

	AUTOLOCK_CONTEXT(context);

	lsp_io_data_buffer data_buf;

	data_buf.send_buffer = (lsp_uint8 *)pWriteData;
	data_buf.send_size = (lsp_uint32)uiWriteDataLen;
	data_buf.recv_buffer = (lsp_uint8 *)pReadData;
	data_buf.recv_size = (lsp_uint32)uiReadDataLen;

	lsp_error_t err;
	BYTE HwVersion;

	LSP_CALL(lsp_get_handle_info(context->hLSP, lsp_handle_info_hw_version, &HwVersion, sizeof(HwVersion)));

	// Version check

	// all version
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_ret_time || TRUE);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_max_conn_time || TRUE);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_supervisor_pw || TRUE);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_user_pw || TRUE);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_reset|| TRUE);

	// V1.1
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_enc_opt || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_standby_timer|| 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_sema || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_free_sema || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_sema || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_owner_sema || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_dynamic_ret_time || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_dynamic_ret_time || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_ret_time || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_max_conn_time || 1 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_standby_timer || 1 <= HwVersion);

	// V2.0
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_delay || 2 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_delay || 2 <= HwVersion);
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_lpx_address || 2 <= HwVersion);

	// Higher version
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_d_enc_opt || FALSE);	
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_get_d_enc_opt || FALSE);	

	// Obsolete
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_dynamic_max_conn_time || FALSE);

	param_8 = 0;

	switch(vop_code)
	{
	case ndascomm_vcmd_set_ret_time:
		API_CALLEX(NDASCOMM_ERROR_INVALID_PARAMETER, 0 < param->SET_RET_TIME.RetTime);
		
		param_8 = (UINT64)param->SET_RET_TIME.RetTime -1;
		break;
	case ndascomm_vcmd_set_max_conn_time:
		API_CALLEX(NDASCOMM_ERROR_INVALID_PARAMETER, 0 < param->SET_RET_TIME.RetTime);

		if(2 <= HwVersion)
			param_8 = (UINT64)(param->SET_RET_TIME.RetTime -1);
		else
			param_8 = (UINT64)((param->SET_RET_TIME.RetTime -1) * 1000 * 1000); // micro second
		break;
	case ndascomm_vcmd_set_supervisor_pw:
		::CopyMemory(&param_8, param->SET_SUPERVISOR_PW.SupervisorPassword, sizeof(param_8));
		break;
	case ndascomm_vcmd_set_user_pw:
		::CopyMemory(&param_8, param->SET_USER_PW.UserPassword, sizeof(param_8));
		break;
	case ndascomm_vcmd_set_enc_opt:
		param_8 |= (param->SET_ENC_OPT.EncryptHeader) ? 0x02 : 0x00;
		param_8 |= (param->SET_ENC_OPT.EncryptData) ? 0x01 : 0x00;
		break;
	case ndascomm_vcmd_set_standby_timer:
		param_8 |= (param->SET_STANDBY_TIMER.EnableTimer) ? 0x80000000 : 0x00000000;
		param_8 |= param->SET_STANDBY_TIMER.TimeValue & 0x7FFFFFFF;
		break;
	case ndascomm_vcmd_reset:
		break;
	case ndascomm_vcmd_set_sema:
		param_8 |= (UINT64)param->SET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_free_sema:
		param_8 |= (UINT64)param->FREE_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_sema:
		param_8 |= (UINT64)param->GET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_owner_sema:
		param_8 |= (UINT64)param->GET_OWNER_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_set_delay:
		API_CALLEX(NDASCOMM_ERROR_INVALID_PARAMETER, 8 <= param->SET_DELAY.Delay);

		param_8 = (param->SET_DELAY.Delay)/ 8 -1;
		break;
	case ndascomm_vcmd_get_delay:
		break;
	case ndascomm_vcmd_get_dynamic_ret_time:
		break;
	case ndascomm_vcmd_get_ret_time:
		break;
	case ndascomm_vcmd_get_max_conn_time:
		break;
	case ndascomm_vcmd_get_standby_timer:
		break;
	case ndascomm_vcmd_set_lpx_address:
		param_8 = 
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[0] << 40) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[1] << 32) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[2] << 24) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[3] << 16) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[4] << 8) +
			((UINT64)param->SET_LPX_ADDRESS.AddressLPX[5] << 0);
		break;
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_d_enc_opt:
	case ndascomm_vcmd_get_d_enc_opt:
	default:
		API_CALLEX(NDASCOMM_ERROR_INVALID_PARAMETER, FALSE);
	}

	LSP_CALL( lsp_vendor_command(
		context->hLSP, 
		NDASCOMM_VENDOR_ID, NDASCOMM_OP_VERSION, 
		vop_code, 
		(lsp_uint8 *)&param_8, param_length, 
		&data_buf) );

	switch(vop_code)
	{
	case ndascomm_vcmd_set_ret_time:
		break;
	case ndascomm_vcmd_set_max_conn_time:
		break;
	case ndascomm_vcmd_set_supervisor_pw:
		break;
	case ndascomm_vcmd_set_user_pw:
		break;
	case ndascomm_vcmd_set_enc_opt:
		break;
	case ndascomm_vcmd_set_standby_timer:
		break;
	case ndascomm_vcmd_reset:
		break;
	case ndascomm_vcmd_set_sema:
		param->SET_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_free_sema:
		param->FREE_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_get_sema:
		param->GET_SEMA.SemaCounter = (UINT32)param_8;
		break;
	case ndascomm_vcmd_get_owner_sema:
		::CopyMemory(param->GET_OWNER_SEMA.AddressLPX, &param_8, sizeof(param->GET_OWNER_SEMA.AddressLPX));
		break;
	case ndascomm_vcmd_set_delay:
		break;
	case ndascomm_vcmd_get_delay:
		param->GET_DELAY.TimeValue = (UINT32)((param_8 +1)* 8);
		break;
	case ndascomm_vcmd_get_dynamic_ret_time:
		param->GET_DYNAMIC_RET_TIME.RetTime = (UINT32)(param_8 +1);
		break;
	case ndascomm_vcmd_get_ret_time:
		param->GET_RET_TIME.RetTime = (UINT32)param_8 ;
		break;
	case ndascomm_vcmd_get_max_conn_time:
		if(2 <= HwVersion)
			param->GET_MAX_CONN_TIME.MaxConnTime = (UINT32)param_8 +1;
		else
			param->GET_MAX_CONN_TIME.MaxConnTime = ((UINT32)param_8 / 1000 / 1000) +1;
		break;
	case ndascomm_vcmd_get_standby_timer:
		param->GET_STANDBY_TIMER.EnableTimer = (param_8 & 0x80000000) ? TRUE : FALSE;
		param->GET_STANDBY_TIMER.TimeValue = (UINT32)param_8 & 0x7FFFFFFF;
		break;
	case ndascomm_vcmd_set_lpx_address:
		break;
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_d_enc_opt:
	case ndascomm_vcmd_get_d_enc_opt:
	default:
		API_CALLEX(NDASCOMM_ERROR_INVALID_PARAMETER, FALSE);
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommIdeCommand(
   __in HNDAS NdasHandle,
   __inout PNDASCOMM_IDE_REGISTER pIdeRegister,
   __inout_bcount(uiWriteDataLen) PBYTE pWriteData,
   __in UINT32 uiWriteDataLen,
   __out_bcount(uiReadDataLen) PBYTE pReadData,
   __in UINT32 uiReadDataLen)
{
	ASSERT_PARAM( IsValidWritePtr(pIdeRegister, sizeof(NDASCOMM_IDE_REGISTER)) );
	//
	// Explicit enforcement of READ/WRITE access
	//
	switch (pIdeRegister->command.command)
	{
	case WIN_NOP:

	case WIN_READ:
	case WIN_READ_ONCE:
	case WIN_READ_LONG:
	case WIN_READ_LONG_ONCE:
	case WIN_READ_EXT:
	case WIN_READDMA_EXT:
	case WIN_READDMA_QUEUED_EXT:
	case WIN_READ_NATIVE_MAX_EXT:
	case WIN_MULTREAD_EXT:

	case WIN_VERIFY:
	case WIN_VERIFY_ONCE:
	case WIN_VERIFY_EXT:

	case WIN_PIDENTIFY:
	case WIN_MULTREAD:
	case WIN_READDMA_QUEUED:
	case WIN_READDMA:
	case WIN_READDMA_ONCE:
	case WIN_IDENTIFY:
	case WIN_IDENTIFY_DMA:
	case WIN_READ_NATIVE_MAX:

		// Only these commands are available for explicit IdeCommands
		ASSERT_PARAM( IsValidNdasHandle(NdasHandle) );
		break;
	default:
		// Otherwise, IdeCommand requires RW access
		ASSERT_PARAM( IsValidNdasHandleForRW(NdasHandle) );
		break;
	}

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(NdasHandle);

	AUTOLOCK_CONTEXT(context);

	lsp_ide_register_param p;
	lsp_io_data_buffer data_buf;

	p.use_dma = pIdeRegister->use_dma;
	p.use_48 = pIdeRegister->use_48;
	p.device.device = pIdeRegister->device.device;
	p.command.command = pIdeRegister->command.command;

	if(p.use_48)
	{
		p.reg.basic_48.reg_prev[0] = pIdeRegister->reg.basic_48.reg_prev[0];
		p.reg.basic_48.reg_prev[1] = pIdeRegister->reg.basic_48.reg_prev[1];
		p.reg.basic_48.reg_prev[2] = pIdeRegister->reg.basic_48.reg_prev[2];
		p.reg.basic_48.reg_prev[3] = pIdeRegister->reg.basic_48.reg_prev[3];
		p.reg.basic_48.reg_prev[4] = pIdeRegister->reg.basic_48.reg_prev[4];
		p.reg.basic_48.reg_cur[0] = pIdeRegister->reg.basic_48.reg_cur[0];
		p.reg.basic_48.reg_cur[1] = pIdeRegister->reg.basic_48.reg_cur[1];
		p.reg.basic_48.reg_cur[2] = pIdeRegister->reg.basic_48.reg_cur[2];
		p.reg.basic_48.reg_cur[3] = pIdeRegister->reg.basic_48.reg_cur[3];
		p.reg.basic_48.reg_cur[4] = pIdeRegister->reg.basic_48.reg_cur[4];
	}
	else
	{
		p.reg.basic.reg[0] = pIdeRegister->reg.basic.reg[0];
		p.reg.basic.reg[1] = pIdeRegister->reg.basic.reg[1];
		p.reg.basic.reg[2] = pIdeRegister->reg.basic.reg[2];
		p.reg.basic.reg[3] = pIdeRegister->reg.basic.reg[3];
		p.reg.basic.reg[4] = pIdeRegister->reg.basic.reg[4];
	}

	data_buf.send_buffer = (lsp_uint8 *)pWriteData;
	data_buf.send_size = (lsp_uint32)uiWriteDataLen;
	data_buf.recv_buffer = (lsp_uint8 *)pReadData;
	data_buf.recv_size = (lsp_uint32)uiReadDataLen;

	LSP_CALL( lsp_ide_command(context->hLSP, context->target_id, 0, 0, &p, &data_buf, NULL) );

	pIdeRegister->use_dma = p.use_dma;
	pIdeRegister->use_48 = p.use_48;
	pIdeRegister->device.device = p.device.device;
	pIdeRegister->command.command = p.command.command;

	if(pIdeRegister->use_48)
	{
		pIdeRegister->reg.basic_48.reg_prev[0] = p.reg.basic_48.reg_prev[0];
		pIdeRegister->reg.basic_48.reg_prev[1] = p.reg.basic_48.reg_prev[1];
		pIdeRegister->reg.basic_48.reg_prev[2] = p.reg.basic_48.reg_prev[2];
		pIdeRegister->reg.basic_48.reg_prev[3] = p.reg.basic_48.reg_prev[3];
		pIdeRegister->reg.basic_48.reg_prev[4] = p.reg.basic_48.reg_prev[4];
		pIdeRegister->reg.basic_48.reg_cur[0] = p.reg.basic_48.reg_cur[0];
		pIdeRegister->reg.basic_48.reg_cur[1] = p.reg.basic_48.reg_cur[1];
		pIdeRegister->reg.basic_48.reg_cur[2] = p.reg.basic_48.reg_cur[2];
		pIdeRegister->reg.basic_48.reg_cur[3] = p.reg.basic_48.reg_cur[3];
		pIdeRegister->reg.basic_48.reg_cur[4] = p.reg.basic_48.reg_cur[4];
	}
	else
	{
		pIdeRegister->reg.basic.reg[0] = p.reg.basic.reg[0];
		pIdeRegister->reg.basic.reg[1] = p.reg.basic.reg[1];
		pIdeRegister->reg.basic.reg[2] = p.reg.basic.reg[2];
		pIdeRegister->reg.basic.reg[3] = p.reg.basic.reg[3];
		pIdeRegister->reg.basic.reg[4] = p.reg.basic.reg[4];
	}	

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceHardwareInfo(
	IN HNDAS hNdasDevice,
	IN OUT PNDAS_DEVICE_HARDWARE_INFO HardwareInfo)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(HardwareInfo, sizeof(NDAS_DEVICE_HARDWARE_INFO)) );
	ASSERT_PARAM( HardwareInfo->Size == sizeof(NDAS_DEVICE_HARDWARE_INFO) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	lsp_handle h = context->hLSP;

	UINT8 uint8data;
	UINT32 uint32data;
	UINT16 uint16data;

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_type, &uint8data, sizeof(uint8data)) );
	HardwareInfo->HardwareType = static_cast<DWORD>(uint8data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_version, &uint8data, sizeof(uint8data)) );
	HardwareInfo->HardwareVersion = static_cast<DWORD>(uint8data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_revision, &uint16data, sizeof(uint16data)) );
	HardwareInfo->HardwareRevision = static_cast<DWORD>(uint16data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_proto_type, &uint8data, sizeof(uint8data)) );
	HardwareInfo->ProtocolType = static_cast<DWORD>(uint8data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_proto_version, &uint8data, sizeof(uint8data)) );
	HardwareInfo->ProtocolVersion = static_cast<DWORD>(uint8data);


	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_num_slot, &uint32data, sizeof(uint32data)) );
	HardwareInfo->NumberOfCommandProcessingSlots = static_cast<DWORD>(uint32data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_max_blocks, &uint32data, sizeof(uint32data)) );
	HardwareInfo->MaximumTransferBlocks = static_cast<DWORD>(uint32data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_max_targets, &uint32data, sizeof(uint32data)) );
	HardwareInfo->MaximumNumberOfTargets = static_cast<DWORD>(uint32data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_max_lus, &uint32data, sizeof(uint32data)) );
	HardwareInfo->MaximumNumberOfLUs = static_cast<DWORD>(uint32data);


	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_header_encrypt_algo, &uint16data, sizeof(uint16data)) );
	HardwareInfo->HeaderEncryptionMode = static_cast<DWORD>(uint16data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_header_digest_algo, &uint16data, sizeof(uint16data)) );
	HardwareInfo->HeaderDigestMode = static_cast<DWORD>(uint16data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_data_encrypt_algo, &uint16data, sizeof(uint16data)) );
	HardwareInfo->DataEncryptionMode = static_cast<DWORD>(uint16data);

	LSP_CALL( lsp_get_handle_info(h, lsp_handle_info_hw_data_digest_algo, &uint16data, sizeof(uint16data)) );
	HardwareInfo->DataDigestMode = static_cast<DWORD>(uint16data);

	::ZeroMemory(
		&HardwareInfo->NdasDeviceId,
		sizeof(NDAS_DEVICE_ID));

	C_ASSERT(sizeof(HardwareInfo->NdasDeviceId.Node) == sizeof(context->device_node));

	::CopyMemory(
		HardwareInfo->NdasDeviceId.Node,
		context->device_node,
		sizeof(HardwareInfo->NdasDeviceId.Node));

	return TRUE;
}

template <typename CharT, typename StructT>
BOOL
NdasCommGetUnitDeviceInfoT(HNDAS hNdasDevice, StructT* pUnitInfo)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(pUnitInfo, sizeof(StructT)) );
	ASSERT_PARAM( pUnitInfo->Size == sizeof(StructT) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	pUnitInfo->LBA = context->use_lba;
	pUnitInfo->LBA48 = context->use_48;
	pUnitInfo->PIO = (context->use_dma) ? 0 : 1;
	pUnitInfo->DMA = context->use_dma;

	pUnitInfo->SectorCount.HighPart = context->capacity.high;
	pUnitInfo->SectorCount.LowPart = context->capacity.low;

	if(context->packet_device)
	{
		switch(context->packet_device_type)
		{
		case 0x05: // CD-ROM device
			pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE;
			break;
		case 0x07: // Optical memory device
			pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE;
			break;
		case 0x00: // Direct-access device
		case 0x01: // Sequential-access device
		case 0x02: // Printer device
		case 0x03: // Processor device
		case 0x04: // Write-once device
		case 0x06: // Scanner device
		case 0x08: // Medium changer device
		case 0x09: // Communications device
		case 0x0A: 
		case 0x0B: // Reserved for ACS IT8 (Graphic arts pre-press devices)
		case 0x0C: // Array controller device
		case 0x0D: // Enclosure services device
		case 0x0E: // Reduced block command devices
		case 0x0F: // Optical card reader/writer device
		case 0x1F: // Unknown or no device type
		default:
			pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE;
			break;
		}
	}
	else
	{
		pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE;
	}

	UINT8 packet_device;
	struct hd_driveid hid;

	LSP_CALL( lsp_ide_identify(
		context->hLSP, context->target_id, 0, 0, 
		&packet_device, &hid) );

	pUnitInfo->UDMA = (hid.dma_ultra & 0x7f) ? 1 : 0;

	CHAR buffer[40]; // Maximum of Model, FirmwareRevision and Serial Number

	// Model
	for (int i = 0; i < sizeof(hid.model) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = NTOHS(((WORD *)hid.model)[i]);
	}
	
	::ZeroMemory(pUnitInfo->Model, sizeof(pUnitInfo->Model));
	for (int i = 0; i < sizeof(hid.model); ++i)
	{
		pUnitInfo->Model[i] = static_cast<CharT>(buffer[i]);
	}

	// FirmwareRevision
	for (int i = 0; i < sizeof(hid.fw_rev) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = NTOHS(((WORD *)hid.fw_rev)[i]);
	}

	::ZeroMemory(pUnitInfo->FirmwareRevision, sizeof(pUnitInfo->FirmwareRevision));
	for (int i = 0; i < sizeof(hid.fw_rev); ++i)
	{
		pUnitInfo->FirmwareRevision[i] = static_cast<CharT>(buffer[i]);
	}

	// Serial Number
	for (int i = 0; i < sizeof(hid.serial_no) / sizeof(WORD); ++i)
	{
		((WORD *)buffer)[i] = NTOHS(((WORD *)hid.serial_no)[i]);
	}

	::ZeroMemory(pUnitInfo->SerialNumber, sizeof(pUnitInfo->SerialNumber));
	for (int i = 0; i < sizeof(hid.serial_no); ++i)
	{
		pUnitInfo->SerialNumber[i] = static_cast<CharT>(buffer[i]);
	}

	return TRUE;
};

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoW(
	IN HNDAS hNdasDevice,
	IN OUT PNDAS_UNITDEVICE_HARDWARE_INFOW pUnitInfo)
{
	return NdasCommGetUnitDeviceInfoT<WCHAR,NDAS_UNITDEVICE_HARDWARE_INFOW>(hNdasDevice, pUnitInfo);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoA(
	IN HNDAS hNdasDevice,
	IN OUT PNDAS_UNITDEVICE_HARDWARE_INFOA pUnitInfo)
{
	return NdasCommGetUnitDeviceInfoT<CHAR,NDAS_UNITDEVICE_HARDWARE_INFOA>(hNdasDevice, pUnitInfo);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceStat(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_DEVICE_STAT pDeviceStat)
{
	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM(sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size);

	ASSERT_PARAM( IsValidWritePtr(pDeviceStat, sizeof(NDAS_UNITDEVICE_STAT)) );
	ASSERT_PARAM(sizeof(NDAS_DEVICE_STAT) == pDeviceStat->Size);

	ASSERT_PARAM( NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->LoginType);

	HNDAS hNdasDevice;

	API_CALL( NULL != (hNdasDevice = ::NdasCommConnect(pConnectionInfo)) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	lsp_uint16 data_reply;

	NDASCOMM_BIN_PARAM_TARGET_LIST target_list = {0};
	target_list.ParamType = NDASCOMM_BINPARM_TYPE_TARGET_LIST;

	LSP_CALL(
	lsp_text_command(
		context->hLSP,
		NDASCOMM_TEXT_TYPE_BINARY,
		NDASCOMM_TEXT_VERSION,
		(lsp_uint8 *)&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
		&(data_reply = NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY)) );

	pDeviceStat->NumberOfUnitDevices = target_list.NRTarget;

	BYTE hardwareVersion;
	LSP_CALL(
		lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_version, 
		&hardwareVersion, 
		sizeof(hardwareVersion)) );

	UINT16 hardwareRevision;
	LSP_CALL(
		lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_revision, 
		&hardwareRevision, 
		sizeof(hardwareRevision)) );


	for (int i = 0; i < NDAS_MAX_UNITDEVICES; ++i)
	{
		PNDAS_UNITDEVICE_STAT pUnitDeviceStat = &pDeviceStat->UnitDevices[i];
		pUnitDeviceStat->Size = sizeof(NDAS_UNITDEVICE_STAT);
		pUnitDeviceStat->IsPresent = (i < target_list.NRTarget) ? TRUE : FALSE;

		pUnitDeviceStat->RWHostCount = 
			target_list.PerTarget[i].NRRWHost;

		// chip bug : Read only host count is invalid at V 2.0 rev.0
		pUnitDeviceStat->ROHostCount = 
			(i < target_list.NRTarget) ? 
				(2 == hardwareVersion && 0 == hardwareRevision) ? 
					NDAS_HOST_COUNT_UNKNOWN : 
					target_list.PerTarget[i].NRROHost :
				0;

		::CopyMemory(
			pUnitDeviceStat->TargetData, 
			&target_list.PerTarget[i].TargetData0,
			sizeof(UINT32));

		::CopyMemory(
			&pUnitDeviceStat->TargetData[0] + sizeof(UINT32),
			&target_list.PerTarget[i].TargetData1,
			sizeof(UINT32));
	}

	UNLOCK_CONTEXT(context);

	(void) NdasCommDisconnect(hNdasDevice);
	
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceStat(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat)
{
	ASSERT_PARAM( IsValidReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM(sizeof(NDASCOMM_CONNECTION_INFO) == pConnectionInfo->Size);

	ASSERT_PARAM( IsValidWritePtr(pUnitDeviceStat, sizeof(NDAS_UNITDEVICE_STAT)) );
	ASSERT_PARAM(sizeof(NDAS_UNITDEVICE_STAT) == pUnitDeviceStat->Size);

	ASSERT_PARAM( NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->LoginType);

	HNDAS hNdasDevice;

	API_CALL( NULL != (hNdasDevice = ::NdasCommConnect(pConnectionInfo)) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	lsp_uint16 data_reply;

	NDASCOMM_BIN_PARAM_TARGET_LIST target_list = {0};
	target_list.ParamType = NDASCOMM_BINPARM_TYPE_TARGET_LIST;

	LSP_CALL(
	lsp_text_command(
		context->hLSP,
		NDASCOMM_TEXT_TYPE_BINARY,
		NDASCOMM_TEXT_VERSION,
		(lsp_uint8 *)&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
		&(data_reply = NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY)) );

	// pUnitDeviceStat->NumberOfTargets = target_list.NRTarget;
	pUnitDeviceStat->IsPresent = (context->target_id < target_list.NRTarget) ? TRUE : FALSE;
	pUnitDeviceStat->RWHostCount = target_list.PerTarget[context->target_id].NRRWHost;

	BYTE hardwareVersion;
	UINT16 hwRevision;

	LSP_CALL(
	lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_version, 
		&hardwareVersion, 
		sizeof(hardwareVersion)) );

	LSP_CALL(
	lsp_get_handle_info(
		context->hLSP, 
		lsp_handle_info_hw_revision, 
		&hwRevision, 
		sizeof(hwRevision)) );

	if(2 == hardwareVersion && 0 == hwRevision)
	{
		// chip bug : Read only host count is invalid at V 2.0 original.
		pUnitDeviceStat->ROHostCount = NDAS_HOST_COUNT_UNKNOWN;
	}
	else
	{
		pUnitDeviceStat->ROHostCount = target_list.PerTarget[context->target_id].NRROHost;
	}

	::CopyMemory(
		pUnitDeviceStat->TargetData, 
		&target_list.PerTarget[context->target_id].TargetData0,
		sizeof(UINT32));

	::CopyMemory(
		&pUnitDeviceStat->TargetData[0] + sizeof(UINT32),
		&target_list.PerTarget[context->target_id].TargetData1,
		sizeof(UINT32));

	UNLOCK_CONTEXT(context);

	(void) NdasCommDisconnect(hNdasDevice);
	
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetReceiveTimeout(
	IN HNDAS hNdasDevice, 
	IN DWORD dwTimeout)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	AUTOLOCK_CONTEXT(context);
	return SetSocketTimeout(context, SO_RCVTIMEO, dwTimeout);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetReceiveTimeout(
	IN HNDAS hNdasDevice, 
	OUT LPDWORD lpdwTimeout)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(lpdwTimeout, sizeof(DWORD)) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	AUTOLOCK_CONTEXT(context);
	return GetSocketTimeout(context, SO_RCVTIMEO, lpdwTimeout);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetSendTimeout(
	IN HNDAS hNdasDevice,
	IN DWORD dwTimeout)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	AUTOLOCK_CONTEXT(context);
	return SetSocketTimeout(context, SO_SNDTIMEO, dwTimeout);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetSendTimeout(
	IN HNDAS hNdasDevice, 
	OUT LPDWORD lpdwTimeout)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(lpdwTimeout, sizeof(DWORD)) );
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);
	AUTOLOCK_CONTEXT(context);
	return GetSocketTimeout(context, SO_SNDTIMEO, lpdwTimeout);
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetHostAddress(
	IN HNDAS hNdasDevice,
	OUT PBYTE Buffer,
	IN OUT LPDWORD lpBufferLen)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( IsValidWritePtr(lpBufferLen, sizeof(DWORD)) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	if(NDASCOMM_TRANSPORT_LPX == context->protocol)
	{
		if(Buffer && LPXADDR_NODE_LENGTH <= *lpBufferLen)
		{
			CopyMemory(Buffer, context->host_node, LPXADDR_NODE_LENGTH);
		}

		*lpBufferLen = LPXADDR_NODE_LENGTH;
	}
	else
	{
		API_CALLEX(NDASUSER_ERROR_NOT_IMPLEMENTED, FALSE);
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceID(
	__in_opt HNDAS hNdasDevice,
	__in_opt CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	__out PBYTE pDeviceId,
	__out LPDWORD pUnitNo,
	__out LPBYTE VID)
{
	ASSERT_PARAM( (hNdasDevice && IsValidNdasHandle(hNdasDevice)) || (pConnectionInfo && IsValidNdasConnectionInfo(pConnectionInfo)));
	ASSERT_PARAM( !(NULL != hNdasDevice && NULL != pConnectionInfo) );
	ASSERT_PARAM( IsValidWritePtr(pDeviceId, RTL_FIELD_SIZE(NDASCOMM_HANDLE_CONTEXT, device_node)) );
	ASSERT_PARAM( IsValidWritePtr(pUnitNo, sizeof(DWORD)) );
	ASSERT_PARAM( NULL == VID || IsValidWritePtr(VID, sizeof(BYTE)) );

	if(hNdasDevice)
	{
		PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

		AUTOLOCK_CONTEXT(context);

		::CopyMemory(pDeviceId, context->device_node, sizeof(context->device_node));
		*pUnitNo = (DWORD) context->target_id;
		if (VID) *VID = context->vendor_id;
	}
	else
	{
		NDAS_UNITDEVICE_ID unitDeviceID;
		NDASID_EXT_DATA ndasIdExtension;

		BOOL fSuccess = NdasCommImpConnectionInfoToDeviceID(
			pConnectionInfo, 
			&unitDeviceID,
			&ndasIdExtension);

		::CopyMemory(pDeviceId, unitDeviceID.DeviceId.Node, LPXADDR_NODE_LENGTH);
		*pUnitNo = unitDeviceID.UnitNo;
		if (VID) *VID = ndasIdExtension.VID;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

inline
UINT64 
NdasCommImpGetPassword(
	__in CONST BYTE* pAddress, 
	__in CONST NDASID_EXT_DATA* NdasIdExtension)
{
	_ASSERTE(IsValidReadPtr(pAddress, 6));

	if (NdasIdExtension && NDAS_VID_SEAGATE == NdasIdExtension->VID)
	{
		return NDAS_OEM_CODE_SEAGATE.UI64Value;
	}

	// password
	// if it's sample's address, use its password
	if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0xf0 &&
		pAddress[2] == 0x0f)
	{
		return NDAS_OEM_CODE_SAMPLE.UI64Value;
	}
#ifdef OEM_RUTTER
	else if(pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0 &&
		pAddress[3] & 0xFE == 0x20)
	{
		return NDAS_OEM_CODE_RUTTER.UI64Value;
	}
#endif // OEM_RUTTER
	else if(pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0)
	{
		return NDAS_OEM_CODE_DEFAULT.UI64Value;
	}
	else
	{
		//	default to XIMETA
		return NDAS_OEM_CODE_DEFAULT.UI64Value;
	}
}

inline
BOOL
IsValidNdasHandle(
	HNDAS hNdasDevice)
{
	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	if (::IsBadWritePtr(context, sizeof(NDASCOMM_HANDLE_CONTEXT)))
	{
		return FALSE;
	}
	if (!context->hLSP)
	{
		return FALSE;
	}
	return TRUE;
}

inline
BOOL
IsValidNdasHandleForRW(
	HNDAS hNdasDevice)
{
	if (!IsValidNdasHandle(hNdasDevice))
	{
		return FALSE;
	}

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	if (context->packet_device)
	{
		return FALSE;
	}

	if (!context->write_access)
	{
		return FALSE;
	}

	return TRUE;
}

inline
void
NdasCommImpCleanupInitialLocks(HNDAS hNdasDevice)
{
	for (DWORD i = 0; i < NDAS_DEVICE_LOCK_COUNT; ++i)
	{
#if 0
		(void) NdasCommLockDevice(hNdasDevice, i, NULL);
#endif
		(void) NdasCommUnlockDevice(hNdasDevice, i, NULL);
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommLockDevice(
	IN HNDAS hNdasDevice, 
	IN DWORD dwIndex,
	OUT OPTIONAL LPDWORD lpdwCounter)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( dwIndex < 4 );
	ASSERT_PARAM( NULL == lpdwCounter || IsValidWritePtr(lpdwCounter, sizeof(DWORD)) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	++(context->LockCount[dwIndex]);

	NDASCOMM_VCMD_PARAM param = {0};
	param.SET_SEMA.Index = static_cast<UINT8>(dwIndex);
	
	BOOL fSuccess = ::NdasCommVendorCommand(
		hNdasDevice, 
		ndascomm_vcmd_set_sema,
		&param,
		NULL, 0,
		NULL, 0);

	if (NULL != lpdwCounter)
	{
		*lpdwCounter = static_cast<DWORD>(param.SET_SEMA.SemaCounter);
	}

	return fSuccess;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUnlockDevice(
	IN HNDAS hNdasDevice, 
	IN DWORD dwIndex,
	OUT OPTIONAL LPDWORD lpdwCounter)
{
	ASSERT_PARAM( IsValidNdasHandle(hNdasDevice) );
	ASSERT_PARAM( dwIndex < 4 );
	ASSERT_PARAM( NULL == lpdwCounter || IsValidWritePtr(lpdwCounter, sizeof(DWORD)) );

	PNDASCOMM_HANDLE_CONTEXT context = NdasHandleToContext(hNdasDevice);

	AUTOLOCK_CONTEXT(context);

	if (0 == context->LockCount[dwIndex])
	{
		::SetLastError(NDASCOMM_ERROR_INVALID_OPERATION);
		return FALSE;
	}

	--(context->LockCount[dwIndex]);

	if (FALSE && context->LockCount[dwIndex] > 0)
	{
		if (NULL != lpdwCounter)
		{
			NDASCOMM_VCMD_PARAM param = {0};
			param.GET_SEMA.Index = static_cast<UINT8>(dwIndex);
			BOOL fSuccess = ::NdasCommVendorCommand(
				hNdasDevice, 
				ndascomm_vcmd_get_sema, 
				&param,
				NULL, 0,
				NULL, 0);
			if (fSuccess)
			{
				*lpdwCounter = static_cast<DWORD>(param.GET_SEMA.SemaCounter);
			}
			else
			{
				*lpdwCounter = 0;
			}
		}
		return TRUE;
	}
	else
	{
		NDASCOMM_VCMD_PARAM param = {0};
		param.FREE_SEMA.Index = static_cast<UINT8>(dwIndex);

		BOOL fSuccess = ::NdasCommVendorCommand(
			hNdasDevice, 
			ndascomm_vcmd_free_sema,
			&param,
			NULL, 0,
			NULL, 0);

		if (NULL != lpdwCounter)
		{
			*lpdwCounter = static_cast<DWORD>(param.FREE_SEMA.SemaCounter);
		}

		return fSuccess;
	}
}

#define NDAS_MAX_CONNECTION_V11 64

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommCleanupDeviceLocks(
	IN CONST NDASCOMM_CONNECTION_INFO* ConnInfo)
{
	HNDAS hNdasDevices[NDAS_MAX_CONNECTION_V11];

	NDASCOMM_CONNECTION_INFO ci;
	::CopyMemory(&ci, ConnInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ci.WriteAccess = FALSE;
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	// at least first connection should succeed
	APITRACE( hNdasDevices[0] = NdasCommConnect(&ci) );
	if (NULL == hNdasDevices[0])
	{
		return FALSE;
	}

	// subsequent connection may fail
	for (DWORD i = 1; i < NDAS_MAX_CONNECTION_V11; ++i)
	{
		APITRACE( hNdasDevices[i] = NdasCommConnect(&ci) );
	}

	for (DWORD i = 0; i < NDAS_MAX_CONNECTION_V11; ++i)
	{
		// only connected handles will be attempted
		if (NULL != hNdasDevices[i])
		{
			NdasCommImpCleanupInitialLocks(hNdasDevices[i]);
			APITRACE( NdasCommDisconnect(hNdasDevices[i]) );
		}
	}

	return TRUE;
}

static
BOOL
GetNdasDeviceId(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__out NDAS_DEVICE_ID* DeviceId)
{
	BOOL success;

	switch (Class)
	{
	case NDAS_DIC_NDAS_IDA:
		success = NdasIdStringToDeviceExA(
			static_cast<LPCSTR>(Identifier), DeviceId, NULL, NULL);
		break;
	case NDAS_DIC_NDAS_IDW:
		success = NdasIdStringToDeviceExW(
			static_cast<LPCWSTR>(Identifier), DeviceId, NULL, NULL);
		break;
	case NDAS_DIC_DEVICE_ID:
		if (IsBadReadPtr(Identifier, sizeof(NDAS_DEVICE_ID)))
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		CopyMemory(DeviceId, Identifier, sizeof(NDAS_DEVICE_ID));
		success = TRUE;
		break;
	default:
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	return success;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyDeviceChange(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__in_opt LPCGUID HostIdentifier)
{
	NDAS_DEVICE_ID deviceId;

	BOOL success = GetNdasDeviceId(Class, Identifier, &deviceId);

	if (!success)
	{
		return FALSE;
	}

	CNdasHIXChangeNotify HixChangeNotify(HostIdentifier);

	success = HixChangeNotify.Initialize();
	if (!success)
	{
		return FALSE;
	}

	success = HixChangeNotify.Notify(deviceId);
	if (!success)
	{
		return FALSE;
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyUnitDeviceChange(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__in DWORD UnitNumber,
	__in_opt LPCGUID HostIdentifier)
{
	NDAS_DEVICE_ID deviceId;

	BOOL success = GetNdasDeviceId(Class, Identifier, &deviceId);
	if (!success)
	{
		return FALSE;
	}

	NDAS_UNITDEVICE_ID unitDeviceId = { deviceId, UnitNumber };

	CNdasHIXChangeNotify HixChangeNotify(HostIdentifier);

	success = HixChangeNotify.Initialize();
	if (!success)
	{
		return FALSE;
	}

	success = HixChangeNotify.Notify(unitDeviceId);
	if (!success)
	{
		return FALSE;
	}

	return TRUE;
}
