// ndascomm.cpp : Defines the entry point for the DLL application.
//

#include <windows.h>
#include <tchar.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <crtdbg.h>
#include <map>
#include <list>
#include <socketlpx.h>
#include <winsock2.h>
#include <ndas/ndasid.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndascomm.h>
#include <lsp/lsp.h>
#include <lsp/lsp_util.h>
#include "ndascomm_type_internal.h"
#include "xdebug.h"

#define ASSERT_PARAM_ERROR NDASCOMM_ERROR_INVALID_PARAMETER
#include "ctrace.hxx"


struct ILock
{
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
};

class CCritSecLock : public ILock
{
protected:

	CRITICAL_SECTION m_cs;

public:

	CCritSecLock()
	{
		::InitializeCriticalSection(&m_cs);
		// if (!::InitializeCriticalSectionAndSpinCount(&m_cs,0x80004000)) throw; 
	}

	virtual ~CCritSecLock() 
	{
		::DeleteCriticalSection(&m_cs); 
	}

	void Lock() { ::EnterCriticalSection(&m_cs); }
	void Unlock() {	::LeaveCriticalSection(&m_cs); }
};

class CAutoLock
{
protected:

	ILock* m_pLock;

public:

	CAutoLock(ILock* plock) : m_pLock(plock) { _ASSERTE(m_pLock); m_pLock->Lock(); }
	~CAutoLock() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }
	void Release() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }

};

// remove this line
#define NDASCOMM_ERROR_UNKNOWN 0xFFFFFFFF
#define NDASCOMM_ERROR_NOT_INITIALIZED	0xFFFFFFFF

static const INT64 NDASCOMM_PW_USER	= 0x1F4A50731530EABB;
static const INT64 NDASCOMM_PW_SAMPLE = 0x0001020304050607;
static const INT64 NDASCOMM_PW_DLINK = 0xCE00983088A66118;
static const INT64 NDASCOMM_PW_RUTTER = 0x1F4A50731530EABB;
static const INT64 NDASCOMM_PW_SUPER1 = 0x0F0E0D0304050607;
static const INT64 NDASCOMM_PW_SUPER_V1= 0x3e2b321a4750131e;
static const INT64 NDASCOMM_PW_SUPER = NDASCOMM_PW_USER;

typedef struct _NDASCOMM_HANDLE_CONTEXT {
	lsp_handle hLSP;
	void *proc_context;
	lsp_uint8 write_access;
	lsp_uint8 device_node[6];
	lsp_uint8 host_node[6];
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
} NDASCOMM_HANDLE_CONTEXT, *PNDASCOMM_HANDLE_CONTEXT;

#define LOCK_CONTEXT(CONTEXT) 	CAutoLock autolock((CONTEXT)->lock)

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

struct lsp_proc_context {
	SOCKET s;
	DWORD dwTimeout;
};

struct lsp_wait_context {
	WSAOVERLAPPED AcceptOverlapped;
};

#define IPPROTO_LPXTCP 214
#define	NDAS_CTRL_REMOTE_PORT 10000
#define AF_LPX AF_UNSPEC

#define SOCK_CALL(stmt) \
	do { int err = stmt; if (0 != err) { printf("err %u from " #stmt "\n", WSAGetLastError()); return err; } } while(0)

PTCHAR lpx_addr_node_str(unsigned char* nodes)
{
	static TCHAR buf[30] = {0};
	StringCchPrintf(buf, 30, _T("%02X:%02X:%02X:%02X:%02X:%02X"),
		nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
	return buf;
}

PTCHAR lpx_addr_str(struct sockaddr_lpx* lpx_addr)
{
	return lpx_addr_node_str(lpx_addr->slpx_addr.node);
}

void* 
lsp_proc_call 
lsp_proc_mem_alloc(void* context, size_t size) 
{
	return ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void 
lsp_proc_call 
lsp_proc_mem_free(void* context, void* pblk) 
{
	HeapFree(::GetProcessHeap(), NULL, pblk);
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_send(void* context, const void* buf, size_t len, size_t* sent, void **wait_handle_ptr)
{
	SOCKET s = ((lsp_proc_context *)context)->s;
	DWORD dwTimeout = ((lsp_proc_context *)context)->dwTimeout;

	if(0 == dwTimeout || !wait_handle_ptr)
	{
		int sent_bytes = send(s, (const char*) buf, len, 0);
		if (sent_bytes == SOCKET_ERROR)
			return LSP_TRANS_ERROR;
		*sent = sent_bytes;
		if(wait_handle_ptr)
			*wait_handle_ptr = NULL;
	}
	else
	{
		if(!wait_handle_ptr)
			return LSP_TRANS_ERROR;

		WSABUF DataBuf;
		DataBuf.buf = (char *)buf;
		DataBuf.len = len;

		lsp_wait_context *pWaitContext = 
			(lsp_wait_context *)::HeapAlloc(::GetProcessHeap(), 
			HEAP_ZERO_MEMORY, sizeof(lsp_wait_context));

		DWORD dwNumberOfBytesSent;

		pWaitContext->AcceptOverlapped.hEvent = WSACreateEvent();
		
		int iResult = WSASend(
			s,
			&DataBuf,
			1,
			&dwNumberOfBytesSent,
			NULL,
			&pWaitContext->AcceptOverlapped, 
			NULL);
		if(SOCKET_ERROR == iResult)
		{
			if(WSA_IO_PENDING != WSAGetLastError())
			{
				::HeapFree(::GetProcessHeap(), NULL, pWaitContext);
				return LSP_TRANS_ERROR;
			}
		}

		*wait_handle_ptr = pWaitContext;
	}
	
	return LSP_TRANS_SUCCESS;
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_recv(void* context, void* buf, size_t len, size_t* recvd, void **wait_handle_ptr)
{
	SOCKET s = ((lsp_proc_context *)context)->s;
	DWORD dwTimeout = ((lsp_proc_context *)context)->dwTimeout;

	if(0 == dwTimeout || !wait_handle_ptr)
	{
		int recv_bytes = recv(s, (char*) buf, len, 0);
		if (recv_bytes == SOCKET_ERROR)
			return LSP_TRANS_ERROR;
		*recvd = recv_bytes;
	}
	else
	{
		if(!wait_handle_ptr)
			return LSP_TRANS_ERROR;

		WSABUF DataBuf;
		DataBuf.buf = (char *)buf;
		DataBuf.len = len;

		lsp_wait_context *pWaitContext = 
			(lsp_wait_context *)::HeapAlloc(::GetProcessHeap(), 
			HEAP_ZERO_MEMORY, sizeof(lsp_wait_context));

		DWORD dwNumberOfBytesRecvd;

		pWaitContext->AcceptOverlapped.hEvent = WSACreateEvent();

		int iResult = WSARecv(
			s,
			&DataBuf,
			1,
			&dwNumberOfBytesRecvd,
			NULL,
			&pWaitContext->AcceptOverlapped, 
			NULL);
		if(SOCKET_ERROR == iResult)
		{
			if(WSA_IO_PENDING != WSAGetLastError())
			{
				::HeapFree(::GetProcessHeap(), NULL, pWaitContext);
				return LSP_TRANS_ERROR;
			}
		}

		*wait_handle_ptr = pWaitContext;
	}

	return LSP_TRANS_SUCCESS;
}

lsp_trans_error_t 
lsp_proc_call 
lsp_proc_wait(void* context, size_t *bytes_transferred, void *wait_handle)
{
	SOCKET s = ((lsp_proc_context *)context)->s;
	DWORD dwTimeout = ((lsp_proc_context *)context)->dwTimeout;
	WSAEVENT hEvent = (WSAEVENT)wait_handle;
	DWORD Flags = 0;
	DWORD BytesTransferred;

	DWORD Index;

	if(!wait_handle)
		return LSP_TRANS_SUCCESS;

	lsp_wait_context *pWaitContext = (lsp_wait_context *)wait_handle;

	Index = WSAWaitForMultipleEvents(
		1, 
		&pWaitContext->AcceptOverlapped.hEvent,
		TRUE,
		dwTimeout,
		FALSE);

	if(WSA_WAIT_EVENT_0 != Index)
		return LSP_TRANS_ERROR;

	BOOL bResult = WSAGetOverlappedResult(s, 
		&pWaitContext->AcceptOverlapped, &BytesTransferred, FALSE, &Flags );
	if(TRUE != bResult)
		return LSP_TRANS_ERROR;

	*bytes_transferred = BytesTransferred;

	WSACloseEvent(hEvent);

	::HeapFree(::GetProcessHeap(), NULL, pWaitContext);

	return LSP_TRANS_SUCCESS;
}


lsp_trans_error_t lsp_proc_call
lsp_proc_initialize(void *context)
{
	int iResults;
	WSADATA				wsaData;

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

lsp_trans_error_t lsp_proc_call
lsp_proc_uninitialize(void *context)
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

void
lpx_disconnect(void* context)
{
	SOCKET s = ((lsp_proc_context *)context)->s;
	DWORD dwTimeout = ((lsp_proc_context *)context)->dwTimeout;

	if(!s)
		return;
	closesocket(s);
}

static SOCKET _lpx_connect(u_char *host_node, u_char* remote_node)
{
	SOCKET s;
	struct sockaddr_lpx remote_addr, host_addr;

	remote_addr.port = htons( NDAS_CTRL_REMOTE_PORT );
	remote_addr.sin_family = AF_LPX;
	::CopyMemory(&remote_addr.slpx_addr.node, 
		remote_node, sizeof(remote_addr.slpx_addr.node));

	host_addr.port = 0;
	host_addr.sin_family = AF_UNSPEC;
	::CopyMemory(&host_addr.slpx_addr.node, 
		host_node, sizeof(host_addr.slpx_addr.node));

	s = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);

	// binding    to lpx_addr_str(&host_addr)
	if(!bind(s, (const struct sockaddr*) &host_addr, sizeof(struct sockaddr_lpx)))
	{
		// binded     to lpx_addr_str(&host_addr)
		// connecting to lpx_addr_str(&remote_addr) at lpx_addr_str(&host_addr)
		if(!connect(s, (const struct sockaddr*) &remote_addr, sizeof(remote_addr)))
		{
			return s;
		}
	}

	closesocket(s);

	return NULL;
}

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
typedef std::map<LPX_HOST_NODE, int> MAP_HOST_TRIED;

void *
lpx_connect(u_char *remote_node, u_char *host_node, LPSOCKET_ADDRESS_LIST addr_list_hint)
{
	LPSOCKET_ADDRESS_LIST addr_list = 0;
	SOCKET s = NULL;
	size_t list_size;
	int result;
	int i;

	static MAP_REMOTE_HOST s_addr_hint_cache; // caches succeeded host address
	std::map<LPX_HOST_NODE, int> addr_tried; // store all the tried host addresses

	if(!remote_node || !host_node)
		return 0;

	// try connecting with addr_list_hint
	if(addr_list_hint)
	{
		for (i = 0; i < addr_list_hint->iAddressCount; ++i)
		{
			
			::CopyMemory(host_node, 
				((struct sockaddr_lpx *)addr_list_hint->Address[i].lpSockaddr)->slpx_addr.node,
				sizeof(LPX_REMOTE_NODE));
			addr_tried[*(LPX_HOST_NODE *)host_node] = 1;

			if(s = _lpx_connect(host_node, remote_node))
			{
				goto out;
			}
		}
	}

	// try connecting with cached host address
	if(s_addr_hint_cache.find(*(LPX_REMOTE_NODE *)remote_node) != s_addr_hint_cache.end())
	{
		::CopyMemory(host_node, 
			s_addr_hint_cache[*(LPX_REMOTE_NODE *)remote_node].node,
			sizeof(LPX_REMOTE_NODE));
		if(addr_tried.find(*(LPX_HOST_NODE *)host_node) == addr_tried.end())
		{
			addr_tried[*(LPX_HOST_NODE *)host_node] = 1;

			if(s = _lpx_connect(host_node, remote_node))
			{
				goto out;
			}
		}
	}

	// retrieve host address list
	{
		SOCKET s_for_list;
		// init socket
		s_for_list = socket(AF_UNSPEC, SOCK_STREAM, LPXPROTO_STREAM);

		// query list_size // -1 == result , cause addr_list
		API_CALL_JMP(out, -1 == WSAIoctl(s_for_list, SIO_ADDRESS_LIST_QUERY, 
			0, 0, addr_list, 0, (LPDWORD)&list_size, 0, 0));
		

		// query addr_list
		addr_list = (LPSOCKET_ADDRESS_LIST)::HeapAlloc(::GetProcessHeap(),
			HEAP_ZERO_MEMORY, list_size);
		API_CALLEX_JMP(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY, out, NULL != addr_list);

		API_CALL_JMP(out, ERROR_SUCCESS == WSAIoctl(s_for_list, 
			SIO_ADDRESS_LIST_QUERY, 0, 0, addr_list, list_size, (LPDWORD)&list_size, 0, 0));

		closesocket(s_for_list);
	}


	// try connecting with retrieved host address list
	for (int i = 0; i < addr_list->iAddressCount; ++i)
	{
		::CopyMemory(host_node, 
			((struct sockaddr_lpx*) addr_list->Address[i].lpSockaddr)->slpx_addr.node,
			sizeof(LPX_REMOTE_NODE));
		if(addr_tried.find(*(LPX_HOST_NODE *)host_node) != addr_tried.end())
		{
			continue;
		}
		addr_tried[*(LPX_HOST_NODE *)host_node] = 1;

		if(s = _lpx_connect(host_node, remote_node))
		{
			goto out;
		}
	}

out:
	if(addr_list)
	{
		::HeapFree(GetProcessHeap(), NULL, addr_list);
		addr_list = 0;

	}

	// if connection succeeded, store the host address to the cache.
	if(s)
	{
		s_addr_hint_cache[*(LPX_REMOTE_NODE *)remote_node] = *(LPX_HOST_NODE *)host_node;
	}

	return (void *)s;
}

/*++

NdasCommGetPassword function ...

Parameters:

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

static UINT64 pNdasCommGetPassword(CONST BYTE* pAddress);

/*++

NdasCommIsHandleValidForRW function tests the NDAS handle if it is good for read/write block device or not
Because NdasCommIsHandleValidForRW set last error, caller function does not need to set last error.

Parameters:

hNDASDevice
[in] NDAS HANDLE which is LANSCSI_PATH pointer type

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

static BOOL pNdasCommIsHandleValidForRW(HNDAS hNDASDevice);
static BOOL pNdasCommIsHandleValid(HNDAS hNDASDevice);
static VOID pNdasCommDisconnect(PNDASCOMM_HANDLE_CONTEXT context);

static BOOL 
NdasCommConnectionInfoToDeviceID(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_UNITDEVICE_ID pUnitDeviceID);


////////////////////////////////////////////////////////////////////////////////////////////////
//
// NDAS Communication API Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////

static BOOL g_bInitialized = FALSE;

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommInitialize()
{
	lsp_trans_error_t err_trans;

	if(g_bInitialized)
		return TRUE;

	err_trans = lsp_proc_initialize(NULL);
	if(LSP_SUCCESS != err_trans)
		return FALSE;

	g_bInitialized = TRUE;

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUninitialize()
{
	lsp_trans_error_t err_trans;

	if(!g_bInitialized)
		return TRUE;

	err_trans = lsp_proc_uninitialize(NULL);
	if(LSP_SUCCESS != err_trans)
		return FALSE;

	g_bInitialized = FALSE;

	return TRUE;
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
NdasCommConnectionInfoToDeviceID(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDAS_UNITDEVICE_ID pUnitDeviceID)
{
	BOOL bResults;

	ASSERT_PARAM( ! ::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM( 
		NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX == pConnectionInfo->address_type ||
//		NDASCOMM_CONNECTION_INFO_TYPE_ADDR_IP == pConnectionInfo->address_type ||
		NDASCOMM_CONNECTION_INFO_TYPE_ID_DEVICE == pConnectionInfo->address_type ||
		NDASCOMM_CONNECTION_INFO_TYPE_ID_W == pConnectionInfo->address_type ||
		NDASCOMM_CONNECTION_INFO_TYPE_ID_A == pConnectionInfo->address_type);
	ASSERT_PARAM(0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pConnectionInfo->protocol);
	ASSERT_PARAM( ! ::IsBadWritePtr(pUnitDeviceID, sizeof(NDAS_UNITDEVICE_ID)) );

	switch(pConnectionInfo->address_type)
	{
	case NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX:
		{
			::CopyMemory(
				pUnitDeviceID->DeviceId.Node, 
				pConnectionInfo->AddressLPX, 
				LPXADDR_NODE_LENGTH);
		}
		break;
	case NDASCOMM_CONNECTION_INFO_TYPE_ID_DEVICE:
		{
			::CopyMemory(
				pUnitDeviceID->DeviceId.Node, 
				pConnectionInfo->DeviceID, 
				LPXADDR_NODE_LENGTH);
		}
		break;
	case NDASCOMM_CONNECTION_INFO_TYPE_ID_W:
		{
			WCHAR wszID[20 +1] = {0};
			WCHAR wszKey[5 +1] = {0};

			::CopyMemory(wszID, pConnectionInfo->DeviceIDW.wszDeviceStringId, sizeof(WCHAR) * 20);
			::CopyMemory(wszKey, pConnectionInfo->DeviceIDW.wszDeviceStringKey, sizeof(WCHAR) * 5);

			if(pConnectionInfo->bWriteAccess)
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateW(wszID, wszKey) );
			}
			else
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateW(wszID, NULL) );
			}
		
			API_CALLEX( NDASCOMM_ERROR_INVALID_PARAMETER,
				::NdasIdStringToDeviceW(wszID, &pUnitDeviceID->DeviceId) );
		}
		break;
	case NDASCOMM_CONNECTION_INFO_TYPE_ID_A:
		{
			CHAR szID[20 +1] = {0};
			CHAR szKey[5 +1] = {0};

			::CopyMemory(szID, pConnectionInfo->DeviceIDA.szDeviceStringId, 20);
			::CopyMemory(szKey, pConnectionInfo->DeviceIDA.szDeviceStringKey, 5);

			if(pConnectionInfo->bWriteAccess)
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateA(szID, szKey) );
			}
			else
			{
				API_CALLEX(	NDASCOMM_ERROR_INVALID_PARAMETER,
					::NdasIdValidateA(szID, NULL) );
			}

			API_CALLEX( NDASCOMM_ERROR_INVALID_PARAMETER,
				::NdasIdStringToDeviceA(szID, &pUnitDeviceID->DeviceId) );
		}
		break;
	default:
		ASSERT_PARAM(FALSE);
		break;
	}

	pUnitDeviceID->UnitNo = pConnectionInfo->UnitNo;

	return TRUE;
}

NDASCOMM_API
HNDAS
NDASAPICALL
NdasCommConnect(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo, 
	CONST DWORD dwTimeout, 
	CONST VOID* hint)
{
	NDAS_UNITDEVICE_ID UnitDeviceID;
	PNDASCOMM_HANDLE_CONTEXT context = NULL;
//	SOCKET s;
	lsp_error_t err;
	lsp_trans_error_t err_trans;
	int result;
	lsp_transport_proc proc;
	lsp_login_info login_info;
	UINT32 ProtoVer;
		
	if (!g_bInitialized) 
	{
		::SetLastError(NDASCOMM_ERROR_NOT_INITIALIZED);
		return NULL;
	}

	// process parameter
	ASSERT_PARAM(!::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)));
	ASSERT_PARAM(
		NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX == pConnectionInfo->address_type ||
		NDASCOMM_CONNECTION_INFO_TYPE_ID_W == pConnectionInfo->address_type ||
		NDASCOMM_CONNECTION_INFO_TYPE_ID_A == pConnectionInfo->address_type);
	ASSERT_PARAM(
		NDASCOMM_LOGIN_TYPE_NORMAL == pConnectionInfo->login_type ||
		NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->login_type);
	ASSERT_PARAM(0 == pConnectionInfo->UnitNo || 1 == pConnectionInfo->UnitNo);
	// Only LPX is available at this time
	ASSERT_PARAM(NDASCOMM_TRANSPORT_LPX == pConnectionInfo->protocol);

	API_CALL( ::NdasCommConnectionInfoToDeviceID(pConnectionInfo, &UnitDeviceID) );
	
	UnitDeviceID.UnitNo = pConnectionInfo->UnitNo;

	context = (PNDASCOMM_HANDLE_CONTEXT)::HeapAlloc(
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		sizeof(NDASCOMM_HANDLE_CONTEXT));
	API_CALLEX_JMP(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY, fail, NULL != context );

	context->proc_context = (lsp_proc_context *)::HeapAlloc(
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		sizeof(lsp_proc_context));
	API_CALLEX_JMP(NDASCOMM_ERROR_NOT_ENOUGH_MEMORY, fail, NULL != context );

	context->lock = new CCritSecLock;

	API_CALL_JMP(fail, NULL != (((lsp_proc_context *)context->proc_context)->s = (SOCKET)
		lpx_connect((u_char *)UnitDeviceID.DeviceId.Node, context->host_node, (LPSOCKET_ADDRESS_LIST)hint) ));
	((lsp_proc_context *)context->proc_context)->dwTimeout = dwTimeout;

	proc.mem_alloc = lsp_proc_mem_alloc;
	proc.mem_free = lsp_proc_mem_free;
	proc.send = lsp_proc_send;
	proc.recv = lsp_proc_recv;
	proc.wait = lsp_proc_wait;

	login_info.login_type = 
		(NDASCOMM_LOGIN_TYPE_NORMAL == pConnectionInfo->login_type) ?
		LSP_LOGIN_TYPE_NORMAL : LSP_LOGIN_TYPE_DISCOVER;

	login_info.password = 
		(pConnectionInfo->ui64OEMCode) ? 
			pConnectionInfo->ui64OEMCode :
	        pNdasCommGetPassword(UnitDeviceID.DeviceId.Node);

	login_info.unit_no = (lsp_uint8)UnitDeviceID.UnitNo;
	login_info.write_access = (lsp_uint8)(pConnectionInfo->bWriteAccess) ? 1 : 0;
	login_info.supervisor = (lsp_uint8)(pConnectionInfo->bSupervisor) ? 1 : 0;

	// create session
	context->hLSP = lsp_create_session(&proc, context->proc_context);
	API_CALLEX_JMP(NDASCOMM_ERROR_LOGIN_COMMUNICATION, fail, context->hLSP);

	LSP_CALL_JMP(fail, lsp_login(context->hLSP, &login_info));

	context->write_access = (lsp_uint8)(pConnectionInfo->bWriteAccess) ? 1 : 0;

	::CopyMemory(
		context->device_node, 
		UnitDeviceID.DeviceId.Node, 
		sizeof(context->device_node));

	context->target_id = pConnectionInfo->UnitNo;
	context->address_type = pConnectionInfo->address_type;
	context->protocol = pConnectionInfo->protocol;

	LSP_CALL_JMP(fail, 
		lsp_get_handle_info(
			context->hLSP, 
			lsp_handle_info_hw_proto_version, 
			&ProtoVer, 
			sizeof(ProtoVer)) );

	if(LSP_LOGIN_TYPE_NORMAL == login_info.login_type && !login_info.supervisor)
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

	return (HNDAS)context;

fail:

	{
		DWORD dwLastErr = ::GetLastError();

		if(context)
		{
			pNdasCommDisconnect(context);
			context = NULL;
		}
		::SetLastError(dwLastErr);
	}

	return NULL;
}

VOID pNdasCommDisconnect(PNDASCOMM_HANDLE_CONTEXT context)
{
	CCritSecLock* lock = context->lock;

	if(lock)
		lock->Lock();

	if (context->hLSP)
	{
		(VOID) lsp_logout(context->hLSP);
	}

	if(context->hLSP)
	{
		lsp_destroy_session(context->hLSP);
		context->hLSP = NULL;
	}

	if(context->proc_context)
	{
		lpx_disconnect(context->proc_context);
		::HeapFree(GetProcessHeap(), NULL, context->proc_context);
		context->proc_context = NULL;
	}

	if(context->lock)
	{
//		delete context->lock;
		context->lock = NULL;
	}

	::HeapFree(GetProcessHeap(), NULL, context);

	if(lock)
	{
		lock->Unlock();
		delete lock;
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnect(
  HNDAS hNDASDevice)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	// pNdasCommDisconnect does process lock
//	LOCK_CONTEXT(context);

	pNdasCommDisconnect(context);

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceRead(
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	OUT PBYTE	pData)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	ASSERT_PARAM(!::IsBadReadPtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	lsp_error_t err;
	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

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

	LOCK_CONTEXT(context);

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
NdasCommBlockDeviceWrite(
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN OUT PBYTE pData)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValidForRW(hNDASDevice) );
	ASSERT_PARAM(!::IsBadWritePtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

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

	LOCK_CONTEXT(context);

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
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN CONST BYTE* pData)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValidForRW(hNDASDevice) );
	ASSERT_PARAM(!::IsBadReadPtr(pData, (UINT32)ui64SectorCount * NDASCOMM_BLOCK_SIZE));

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;
	lsp_uint16 data_encrypt_algo;

	// do not lock this function, NdasCommBlockDeviceWrite will.
//	LOCK_CONTEXT(context);

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
			hNDASDevice, 
			i64Location, ui64SectorCount, 
			l_data);

		(VOID) ::HeapFree(::GetProcessHeap(), 0, l_data);

		return fSuccess;
	}
	else
	{
		return NdasCommBlockDeviceWrite(hNDASDevice, i64Location, ui64SectorCount, (PBYTE)pData);
	}
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceVerify(
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64 ui64SectorCount)
{
	ASSERT_PARAM( pNdasCommIsHandleValid(hNDASDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	// lsp variables
	lsp_uint32 max_request_blocks;
	lsp_uint64_ll location;
	lsp_uint16 sectors;

	LOCK_CONTEXT(context);

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
	IN HNDAS hNDASDevice,
	IN BYTE feature,
	IN BYTE param0,
	IN BYTE param1,
	IN BYTE param2,
	IN BYTE param3
	)
{
	ASSERT_PARAM( pNdasCommIsHandleValid(hNDASDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	LSP_CALL( lsp_ide_setfeatures(
		context->hLSP, context->target_id, 0, 0, 
		feature, param0, param1, param2, param3) );

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommVendorCommand(
	IN HNDAS hNDASDevice,
	IN NDASCOMM_VCMD_COMMAND vop_code,
	IN OUT PNDASCOMM_VCMD_PARAM param,
	IN OUT PBYTE pWriteData,
	IN UINT32 uiWriteDataLen,
	IN OUT PBYTE pReadData,
	IN UINT32 uiReadDataLen)
{
	ASSERT_PARAM( pNdasCommIsHandleValid(hNDASDevice) );
	// Write is IN and Read is OUT!. However, both are mutable
	ASSERT_PARAM( !::IsBadWritePtr(pWriteData, uiWriteDataLen) );
	ASSERT_PARAM( !::IsBadWritePtr(pReadData, uiReadDataLen) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	UINT64 param_8; // 8 bytes if HwVersion <= 2
	lsp_uint8 param_length = sizeof(param_8);

	LOCK_CONTEXT(context);

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

	// Higher version
	API_CALLEX(NDASCOMM_ERROR_HARDWARE_UNSUPPORTED, vop_code != ndascomm_vcmd_set_lpx_address || FALSE);	
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
		param_8 |= param->SET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_free_sema:
		param_8 |= param->FREE_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_sema:
		param_8 |= param->GET_SEMA.Index << 32;
		break;
	case ndascomm_vcmd_get_owner_sema:
		param_8 |= param->GET_OWNER_SEMA.Index << 32;
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
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_lpx_address:
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
	case ndascomm_vcmd_set_dynamic_max_conn_time:
	case ndascomm_vcmd_set_lpx_address:
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
   IN HNDAS hNDASDevice,
   IN OUT PNDASCOMM_IDE_REGISTER pIdeRegister,
   IN OUT PBYTE pWriteData,
   IN UINT32 uiWriteDataLen,
   OUT PBYTE pReadData,
   IN UINT32 uiReadDataLen )
{
	ASSERT_PARAM( pNdasCommIsHandleValidForRW(hNDASDevice) );
	ASSERT_PARAM( !::IsBadWritePtr(pIdeRegister, sizeof(NDASCOMM_IDE_REGISTER)) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

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

	LSP_CALL( lsp_ide_command(context->hLSP, context->target_id, 0, 0, &p, &data_buf) );

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
NdasCommGetDeviceInfo(
	IN HNDAS hNDASDevice,
	IN NDASCOMM_HANDLE_INFO_TYPE info_type,
	OUT PBYTE data,
	IN UINT32 data_len)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	ASSERT_PARAM( !::IsBadWritePtr(data, data_len) );
	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	switch(info_type)
	{

#define NDASCOMM_SET_HANDLE_INFO(INFO_TYPE, LSP_INFO_TYPE, LSP_HANDLE, DATA_PTR, DATA_LENGTH) \
		case (INFO_TYPE) : \
			LSP_CALL( lsp_get_handle_info(LSP_HANDLE, LSP_INFO_TYPE, DATA_PTR, DATA_LENGTH) ); \
			break;

	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_type, lsp_handle_info_hw_type, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_version, lsp_handle_info_hw_version, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_proto_type, lsp_handle_info_hw_proto_type, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_proto_version, lsp_handle_info_hw_proto_version, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_num_slot, lsp_handle_info_hw_num_slot, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_max_blocks, lsp_handle_info_hw_max_blocks, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_max_targets, lsp_handle_info_hw_max_targets, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_max_lus, lsp_handle_info_hw_max_lus, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_header_encrypt_algo, lsp_handle_info_hw_header_encrypt_algo, context->hLSP, data, data_len)
	NDASCOMM_SET_HANDLE_INFO(ndascomm_handle_info_hw_data_encrypt_algo, lsp_handle_info_hw_data_encrypt_algo, context->hLSP, data, data_len)

#undef NDASCOMM_SET_HANDLE_INFO

	default:
		ASSERT_PARAM(FALSE);
	}

	return TRUE;
}


NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceInfo(
	IN HNDAS hNDASDevice,
	OUT PNDASCOMM_UNIT_DEVICE_INFO pUnitInfo)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	ASSERT_PARAM( !::IsBadWritePtr(pUnitInfo, sizeof(NDASCOMM_UNIT_DEVICE_INFO)) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	pUnitInfo->SectorCount = context->capacity.high;
	pUnitInfo->SectorCount <<= 32;
	pUnitInfo->SectorCount |= context->capacity.low;
	pUnitInfo->bLBA = context->use_lba;
	pUnitInfo->bLBA48 = context->use_48;
	pUnitInfo->bPIO = (context->use_dma) ? 0 : 1;
	pUnitInfo->bDma = context->use_dma;
	
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
				pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE;
				break;
		}
	}
	else
	{
		pUnitInfo->MediaType = NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE;
	}

	UINT8 packet_device;
	struct hd_driveid info;

	LSP_CALL( lsp_ide_identify(
		context->hLSP, context->target_id, 0, 0, 
		&packet_device, &info) );

	pUnitInfo->bUDma = (info.dma_ultra & 0x7f) ? 1 : 0;
	
	int i;
//	::CopyMemory(pUnitInfo->Model, info.model, sizeof(pUnitInfo->Model));
	for(i = 0; i < sizeof(pUnitInfo->Model) / sizeof(WORD); i++)
	{
		((WORD *)pUnitInfo->Model)[i] = NTOHS(((WORD *)info.model)[i]);
	}
//	::CopyMemory(pUnitInfo->FwRev, info.fw_rev, sizeof(pUnitInfo->FwRev));
	for(i = 0; i < sizeof(pUnitInfo->FwRev) / sizeof(WORD); i++)
	{
		((WORD *)pUnitInfo->FwRev)[i] = NTOHS(((WORD *)info.fw_rev)[i]);
	}
//	::CopyMemory(pUnitInfo->SerialNo, info.serial_no, sizeof(pUnitInfo->SerialNo));
	for(i = 0; i < sizeof(pUnitInfo->SerialNo) / sizeof(WORD); i++)
	{
		((WORD *)pUnitInfo->SerialNo)[i] = NTOHS(((WORD *)info.serial_no)[i]);
	}

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceStat(
	CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	PNDASCOMM_UNIT_DEVICE_STAT pUnitDynInfo,
	CONST DWORD dwTimeout,
	CONST VOID* hint)
{
	ASSERT_PARAM( !::IsBadReadPtr(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO)) );
	ASSERT_PARAM( !::IsBadWritePtr(pUnitDynInfo, sizeof(PNDASCOMM_UNIT_DEVICE_STAT)) );
	ASSERT_PARAM( NDASCOMM_LOGIN_TYPE_DISCOVER == pConnectionInfo->login_type );

	HNDAS hNDASDevice = ::NdasCommConnect(pConnectionInfo, dwTimeout, hint);
	API_CALL( NULL != hNDASDevice );
	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	lsp_uint16 data_reply;

	NDASCOMM_BIN_PARAM_TARGET_LIST target_list = {0};
	target_list.ParamType = NDASCOMM_BINPARM_TYPE_TARGET_LIST;

	LSP_CALL( lsp_text_command(
		context->hLSP,
		NDASCOMM_TEXT_TYPE_BINARY,
		NDASCOMM_TEXT_VERSION,
		(lsp_uint8 *)&target_list,
		NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
		&(data_reply = NDASCOMM_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY)) );

	pUnitDynInfo->iNRTargets = target_list.NRTarget;
	pUnitDynInfo->bPresent = (context->target_id < target_list.NRTarget) ? 1 : 0;
	pUnitDynInfo->NRRWHost = target_list.PerTarget[context->target_id].NRRWHost;

	BYTE HwVersion;

	LSP_CALL(lsp_get_handle_info(context->hLSP, lsp_handle_info_hw_version, &HwVersion, sizeof(HwVersion)));
	if(2 == HwVersion)
	{
		// chip bug : Read only host count is invalid at V 2.0
		pUnitDynInfo->NRROHost = 0xFFFFFFFF;
	}
	else
	{
		pUnitDynInfo->NRROHost = target_list.PerTarget[context->target_id].NRROHost;
	}
	pUnitDynInfo->TargetData = target_list.PerTarget[context->target_id].TargetData0;
	pUnitDynInfo->TargetData <<= 32;
	pUnitDynInfo->TargetData |= target_list.PerTarget[context->target_id].TargetData1;

	autolock.Release();
	(VOID)NdasCommDisconnect(context);
	
	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetTransmitTimeout(
	IN HNDAS hNDASDevice,
	OUT LPDWORD dwTimeout)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	lsp_proc_context *proc_context;
	lsp_get_proc_context(context->hLSP, (void **)&proc_context);

	*dwTimeout = proc_context->dwTimeout;

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetTransmitTimeout(
	IN HNDAS hNDASDevice,
	CONST DWORD dwTimeout)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	lsp_proc_context *proc_context;
	lsp_get_proc_context(context->hLSP, (void **)&proc_context);

	proc_context->dwTimeout = dwTimeout;

	return TRUE;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetHostAddress(
	IN HNDAS hNDASDevice,
	OUT PBYTE Buffer,
	IN OUT LPDWORD lpBufferLen)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	ASSERT_PARAM( lpBufferLen );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

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
	HNDAS hNDASDevice,
	PBYTE pDeviceId,
	LPDWORD pUnitNo)
{
	ASSERT_PARAM( ::pNdasCommIsHandleValid(hNDASDevice) );
	ASSERT_PARAM( !::IsBadWritePtr(pDeviceId, RTL_FIELD_SIZE(NDASCOMM_HANDLE_CONTEXT, device_node)) );
	ASSERT_PARAM( !::IsBadWritePtr(pUnitNo, sizeof(DWORD)) );

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	LOCK_CONTEXT(context);

	::CopyMemory(pDeviceId, context->device_node, sizeof(context->device_node));
	*pUnitNo = (DWORD) context->target_id;

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Functions, not for exporting
//
////////////////////////////////////////////////////////////////////////////////////////////////

UINT64 
pNdasCommGetPassword(CONST BYTE* pAddress)
{
	_ASSERTE(!::IsBadReadPtr(pAddress, 6));

	// password
	// if it's sample's address, use its password
	if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0xf0 &&
		pAddress[2] == 0x0f)
	{
		return  NDASCOMM_PW_SAMPLE;
	}
#ifdef OEM_RUTTER
	else if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0 &&
		pAddress[3] & 0xFE == 0x20
		)
	{
		return  NDASCOMM_PW_RUTTER;
	}
#endif // OEM_RUTTER
	else if(	pAddress[0] == 0x00 &&
		pAddress[1] == 0x0B &&
		pAddress[2] == 0xD0)
	{
		return NDASCOMM_PW_USER;
	}
	else
	{
		//	default to XIMETA
		return NDASCOMM_PW_USER;
	}
}

BOOL
pNdasCommIsHandleValid(HNDAS hNDASDevice)
{
	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;

	if (::IsBadWritePtr(context, sizeof(NDASCOMM_HANDLE_CONTEXT)))
		return FALSE;
	if (!context->hLSP)
		return FALSE;
	return TRUE;

//	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;
//
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
//		!::IsBadReadPtr(pPath, sizeof(LANSCSI_PATH)));
//
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->connsock);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, FLAG_FULL_FEATURE_PHASE == pPath->iSessionPhase);
//
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
//		0 == pPath->UnitDeviceID.UnitNo ||
//		1 == pPath->UnitDeviceID.UnitNo);
//
//	// HWVersion is detected after 1st step
//	/*
//	HWVersion			HWProtoVersion
//	V 1.0				0						0
//	V 1.1				1						1
//	V 2.0				2						1
//	*/
//
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, 
//		LANSCSIIDE_VERSION_1_0 == pPath->HWVersion || 
//		LANSCSIIDE_VERSION_1_1 == pPath->HWVersion ||
//		LANSCSIIDE_VERSION_2_0 == pPath->HWVersion);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER,
//		(LANSCSIIDE_VERSION_1_0 == pPath->HWVersion && LSIDEPROTO_VERSION_1_0 == pPath->HWProtoVersion) ||
//		((LANSCSIIDE_VERSION_1_1 == pPath->HWVersion || LANSCSIIDE_VERSION_2_0 == pPath->HWVersion )&& LSIDEPROTO_VERSION_1_1 == pPath->HWProtoVersion));
//
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->HWType == 0);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->HWProtoType == pPath->HWType);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iNumberofSlot > 0);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iMaxBlocks <= LANSCSI_MAX_REQUESTBLOCK);
//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->iMaxTargets >= pPath->iNumberofSlot);
//
//	return TRUE;
}

BOOL
pNdasCommIsHandleValidForRW(HNDAS hNDASDevice)
{
	if (!pNdasCommIsHandleValid(hNDASDevice)) return FALSE;

	PNDASCOMM_HANDLE_CONTEXT context = (PNDASCOMM_HANDLE_CONTEXT)hNDASDevice;
	if (context->packet_device) return FALSE;
	if (!context->write_access) return FALSE;
	return TRUE;

/*
	PLANSCSI_PATH pPath = (PLANSCSI_PATH)hNDASDevice;

	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA);
	//	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].bLBA48);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].SectorCount);
	TEST_AND_RETURN_WITH_ERROR_IF_FAIL(NDASCOMM_ERROR_INVALID_PARAMETER, pPath->PerTarget[pPath->UnitDeviceID.UnitNo].MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE);

	return TRUE;
*/
}

/*
#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL_DEBUG(error_code, condition) if(condition) {} else {_ASSERT(condition); ::SetLastError(error_code); return FALSE;}
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL_DEBUG(error_code, destination, condition) if(condition) {} else {_ASSERT(condition); ::SetLastError(error_code); goto destination;}

#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL_RELEASE(error_code, condition) if(condition) {} else {::SetLastError(error_code); return FALSE;}
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE(error_code, destination, condition) if(condition) {} else {::SetLastError(error_code); goto destination;}

#ifdef _DEBUG
#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL TEST_AND_RETURN_WITH_ERROR_IF_FAIL_DEBUG
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL TEST_AND_GOTO_WITH_ERROR_IF_FAIL_DEBUG
#else
#define TEST_AND_RETURN_WITH_ERROR_IF_FAIL TEST_AND_RETURN_WITH_ERROR_IF_FAIL_RELEASE
#define TEST_AND_GOTO_WITH_ERROR_IF_FAIL TEST_AND_GOTO_WITH_ERROR_IF_FAIL_RELEASE
#endif

#define TEST_LSP_RETURN(lsp_error_code) TEST_AND_RETURN_WITH_ERROR_IF_FAIL(0xECC00000 | (lsp_error_code), LSP_ERR_SUCCESS == (lsp_error_code))
#define TEST_LSP_GOTO(destination, lsp_error_code) TEST_AND_GOTO_WITH_ERROR_IF_FAIL(0xECC00000 | (lsp_error_code), destination, LSP_ERR_SUCCESS == (lsp_error_code))

#define TEST_AND_RETURN_IF_FAIL(condition) if(condition) {} else {return FALSE;}
#define TEST_AND_GOTO_IF_FAIL(destination, condition) if(condition) {} else {goto destination;}

*/