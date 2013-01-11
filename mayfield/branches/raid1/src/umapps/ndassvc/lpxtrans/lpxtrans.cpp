#include "lpxtrans.h"
#include <windows.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <mswsock.h>
#include "autores.h"
#include "xdebug.h"

SOCKADDR_LPX 
pCreateLpxBroadcastAddress(USHORT usPort)
{
	SOCKADDR_LPX lpxBroadcastAddr = {
		AF_LPX, { HTONS(usPort), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}};
	return lpxBroadcastAddr;
}

SOCKADDR_LPX
pCreateNullLpxAddress()
{
	SOCKADDR_LPX lpxAddr = {
		AF_LPX, { HTONS(0), {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};
	return lpxAddr;
}

LPSOCKET_ADDRESS_LIST 
pCreateSocketAddressList(SOCKET sock)
{
	DWORD cbList = sizeof(SOCKET_ADDRESS_LIST);
	LPSOCKET_ADDRESS_LIST lpList = NULL;

	INT iError = 0;

	do {

		lpList = (LPSOCKET_ADDRESS_LIST) ::LocalAlloc(LPTR, cbList);

		if (NULL == lpList) {
			return NULL;
		}

		// Detach this only if return success
		AutoHLocal hLocal = lpList;

		iError = ::WSAIoctl(
			sock,
			SIO_ADDRESS_LIST_QUERY,
			NULL,
			0,
			lpList,
			cbList,
			&cbList,
			NULL,
			NULL);

		if (0 != iError && WSAEFAULT != ::WSAGetLastError()) {
			return NULL;
		}

		if (0 == iError) {
			// equivalent to lpList = hLocal.Detach();
			(VOID) hLocal.Detach();
		}

	} while (0 != iError);

	return lpList;
}

LPSOCKET_ADDRESS_LIST
pCreateLocalLpxAddressList()
{
	AutoSocket sock = ::WSASocket(
		AF_LPX,
		SOCK_DGRAM,
		LPXPROTO_DGRAM,
		NULL,
		0,
		0);
	
	if (INVALID_SOCKET == sock) {
		return NULL;
	}

	return pCreateSocketAddressList(sock);
}

LPCTSTR
CSockLpxAddr::NodeString()
{
	HRESULT hr = ::StringCchPrintf(m_buffer, 60, 
		_T("%02X:%02X:%02X:%02X:%02X:%02X"),
		m_sockLpxAddr.LpxAddress.Node[0],
		m_sockLpxAddr.LpxAddress.Node[1],
		m_sockLpxAddr.LpxAddress.Node[2],
		m_sockLpxAddr.LpxAddress.Node[3],
		m_sockLpxAddr.LpxAddress.Node[4],
		m_sockLpxAddr.LpxAddress.Node[5]);
	_ASSERTE(SUCCEEDED(hr));
	return m_buffer;
}

LPCTSTR
CSockLpxAddr::ToString(DWORD radix)
{
	HRESULT hr = ::StringCchPrintf(m_buffer, 60, 
		(radix == 10) ? _T("%02X:%02X:%02X:%02X:%02X:%02X(%05d)") :
		_T("%02X:%02X:%02X:%02X:%02X:%02X(%04X)"),
		m_sockLpxAddr.LpxAddress.Node[0],
		m_sockLpxAddr.LpxAddress.Node[1],
		m_sockLpxAddr.LpxAddress.Node[2],
		m_sockLpxAddr.LpxAddress.Node[3],
		m_sockLpxAddr.LpxAddress.Node[4],
		m_sockLpxAddr.LpxAddress.Node[5],
		NTOHS(m_sockLpxAddr.LpxAddress.Port));
	_ASSERTE(SUCCEEDED(hr));
	return m_buffer;
}

//////////////////////////////////////////////////////////////////////////

CLpxSockAddrListChangeNotifier::CLpxSockAddrListChangeNotifier() :
	m_sock(INVALID_SOCKET),
	m_hEvent(NULL)
{
}

CLpxSockAddrListChangeNotifier::~CLpxSockAddrListChangeNotifier()
{
	DWORD err = ::GetLastError();
	if (INVALID_SOCKET != m_sock) {
		INT iResult = ::closesocket(m_sock);
		_ASSERTE(0 == iResult);
	}
	::SetLastError(err);
}

BOOL
CLpxSockAddrListChangeNotifier::Initialize()
{
	if (NULL == m_hEvent) {
		m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hEvent) {
			DBGPRT_ERR_EX(_FT("Creating a event failed: "));
			return FALSE;
		}
	}
	return TRUE;

}

BOOL
CLpxSockAddrListChangeNotifier::Reset()
{
	BOOL fSuccess = ::ResetEvent(m_hEvent);
	_ASSERT(fSuccess);

	if (INVALID_SOCKET != m_sock) {
		::closesocket(m_sock);
	}

	// ::ZeroMemory(&m_overlapped, sizeof(WSAOVERLAPPED));
	m_overlapped.Internal =
	m_overlapped.InternalHigh =
	m_overlapped.Offset =
	m_overlapped.OffsetHigh = 0;
	m_overlapped.hEvent = m_hEvent;

	AutoSocket sock = ::WSASocket(
		AF_LPX, 
		SOCK_DGRAM, 
		LPXPROTO_DGRAM, 
		NULL, 
		0, 
		WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == (SOCKET) sock) {
		return FALSE;
	}

	int iError;
	DWORD cbBytesReturned;

	iError = ::WSAIoctl(
		sock,
		SIO_ADDRESS_LIST_CHANGE,
		NULL, 0,
		NULL, 0,
		&cbBytesReturned,
		&m_overlapped,
		NULL);

	if (0 != iError && WSA_IO_PENDING != ::WSAGetLastError()) { 
		// SOCKET_ERROR
		// TODO: Error Event Log from WSAGetLastError
		DBGPRT_ERR_EX(_FT("WSAIoctl SIO_ADDRESS_LIST_CHANGE failed: "));
		return FALSE;
	}

	m_sock = sock.Detach();

	return TRUE;
}

HANDLE 
CLpxSockAddrListChangeNotifier::GetChangeEvent()
{
	return m_hEvent;
}

//////////////////////////////////////////////////////////////////////////

CLpxAsyncSocket::CLpxAsyncSocket() :
	m_sock(INVALID_SOCKET),
	m_hReceivedEvent(NULL),
	m_hSentEvent(NULL)
{
	m_wsaReceiveBuffer.len = 0;
	m_wsaReceiveBuffer.buf = NULL;

	m_wsaSendBuffer.len = 0;
	m_wsaSendBuffer.buf = NULL;

	m_iLocalAddrLen = sizeof(m_localAddr);
	m_iRemoteAddrLen = sizeof(m_remoteAddr);

	::ZeroMemory(&m_localAddr, (SIZE_T)m_iLocalAddrLen);
	::ZeroMemory(&m_remoteAddr, (SIZE_T)m_iRemoteAddrLen);
}

CLpxAsyncSocket::~CLpxAsyncSocket()
{
	DWORD err = ::GetLastError();
	BOOL fSuccess = FALSE;

	if (NULL != m_wsaReceiveBuffer.buf) {
		fSuccess = ::HeapFree(
			::GetProcessHeap(), 0, m_wsaReceiveBuffer.buf);
		_ASSERTE(fSuccess);
	}

	if (NULL != m_hReceivedEvent) {
		fSuccess = ::CloseHandle(m_hReceivedEvent);
		_ASSERTE(fSuccess);
	}

	if (NULL != m_hSentEvent) {
		fSuccess = ::CloseHandle(m_hSentEvent);
		_ASSERTE(fSuccess);
	}

	if (INVALID_SOCKET != m_sock) {
		INT iResult = ::closesocket(m_sock);
		_ASSERTE(0 == iResult);
	}

	::SetLastError(err);
}

BOOL
CLpxAsyncSocket::Initialize()
{
	::InitializeCriticalSection(&m_csSendQueue);
	::InitializeCriticalSection(&m_csRecvQueue);

	if (NULL == m_hReceivedEvent) {
		m_hReceivedEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hReceivedEvent) {
			return FALSE;
		}
	}

	if (NULL == m_hSentEvent) {
		m_hSentEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hSentEvent) {
			return FALSE;
		}
	}

	return TRUE;
}


BOOL 
CLpxAsyncSocket::AllocRecvBuf(DWORD cbSize)
{
	if (cbSize > m_wsaReceiveBuffer.len) {

		if (NULL != m_wsaReceiveBuffer.buf) {
			m_wsaReceiveBuffer.buf = (char*) ::HeapReAlloc(
				::GetProcessHeap(),
				HEAP_ZERO_MEMORY,
				m_wsaReceiveBuffer.buf,
				cbSize);
		} else {
			m_wsaReceiveBuffer.buf = (char*) ::HeapAlloc(
				::GetProcessHeap(),
				HEAP_ZERO_MEMORY,
				cbSize);
		}

		if (NULL == m_wsaReceiveBuffer.buf) {
			DBGPRT_ERR_EX(_FT("Allocating RecvBuf %d bytes failed: "), cbSize);
			m_wsaReceiveBuffer.len = 0;
			return FALSE;
		}

		m_wsaReceiveBuffer.len = cbSize;
	}

	return TRUE;
}

VOID
CLpxAsyncSocket::ResetRecvOverlapped()
{
	_ASSERTE(NULL != m_hReceivedEvent);

	BOOL fSuccess = ::ResetEvent(m_hReceivedEvent);
	_ASSERTE(fSuccess);

	m_ovReceive.Internal =
	m_ovReceive.InternalHigh =
	m_ovReceive.Offset =
	m_ovReceive.OffsetHigh = 0;
	m_ovReceive.hEvent = m_hReceivedEvent;
}

VOID
CLpxAsyncSocket::ResetSendOverlapped()
{
	_ASSERTE(NULL != m_hSentEvent);

	BOOL fSuccess = ::ResetEvent(m_hSentEvent);
	_ASSERTE(fSuccess);

	m_ovSend.Internal =
	m_ovSend.InternalHigh =
	m_ovSend.Offset =
	m_ovSend.OffsetHigh = 0;
	m_ovSend.hEvent = m_hSentEvent;
}

BOOL
CLpxAsyncSocket::Close()
{
	_ASSERTE(INVALID_SOCKET != m_sock);
	
	INT iResult = ::closesocket(m_sock);
	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("Closing a sock %p failed: "), m_sock);
		return FALSE;
	}

	m_sock = INVALID_SOCKET;
	return TRUE;
}

BOOL
CLpxAsyncSocket::ShutDown(INT nHow)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	INT iResult = ::shutdown(m_sock, nHow);
	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("Shutting down a sock %p failed: "), m_sock);
		return FALSE;
	}
	return TRUE;
}

BOOL
CLpxAsyncSocket::_GetRecvResult( 
	OUT LPDWORD lpcbReceived, 
	OUT BYTE** ppbData, 
	OUT LPDWORD lpdwFlags /* = NULL */)
{
	DWORD cbReceived = 0;
	DWORD dwFlags = 0;

	BOOL fSuccess = ::WSAGetOverlappedResult(
		m_sock,
		&m_ovReceive,
		&cbReceived,
		TRUE,
		&dwFlags);

	*ppbData = (BYTE*)m_wsaReceiveBuffer.buf;
	if (lpcbReceived) *lpcbReceived = cbReceived;
	if (lpdwFlags) *lpdwFlags = dwFlags;

	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL 
CLpxAsyncSocket::_GetSendResult(LPDWORD lpcbSent)
{
	DWORD dwFlags = 0;
	DWORD cbSent = 0;

	BOOL fSuccess = ::WSAGetOverlappedResult(
		m_sock,
		&m_ovSend,
		&cbSent,
		TRUE,
		&dwFlags);

	if (lpcbSent) *lpcbSent = cbSent;

	return fSuccess;
}

BOOL
CLpxDatagramSocket::GetRecvFromResult(
	OUT SOCKADDR_LPX* pRemoteAddr, 
	OUT LPDWORD lpcbReceived, 
	OUT BYTE** ppbData, 
	OUT LPDWORD lpdwFlags)
{
	BOOL fSuccess = CLpxAsyncSocket::_GetRecvResult(
		lpcbReceived, 
		ppbData, 
		lpdwFlags);

	*pRemoteAddr = m_remoteAddr;

	UnlockRecvQueue();
	return fSuccess;
}


HANDLE 
CLpxAsyncSocket::GetReceivedEvent()
{
	return m_hReceivedEvent;
}


HANDLE 
CLpxAsyncSocket::GetSendEvent()
{
	return m_hSentEvent;
}

//const SOCKADDR_LPX*
//CLpxAsyncSocket::GetBoundAddr()
//{
//	return &m_localAddr;
//}

//const SOCKADDR_LPX*
//CLpxAsyncSocket::GetRemoteLpxAddr()
//{
//	return &m_remoteAddr;
//}

VOID
CLpxAsyncSocket::Attach(SOCKET hSocket)
{
	m_sock = hSocket;
}

SOCKET 
CLpxAsyncSocket::Detach()
{
	SOCKET s = m_sock;
	m_sock = INVALID_SOCKET;
	return s;
}

BOOL
CLpxAsyncSocket::CreateEx(INT nSocketType /* = SOCK_STREAM */)
{
	_ASSERTE(INVALID_SOCKET == m_sock);
	_ASSERTE(SOCK_STREAM == nSocketType || SOCK_DGRAM == nSocketType);

	m_sock = ::WSASocket(
		AF_LPX,
		nSocketType,
		(nSocketType == SOCK_STREAM) ? LPXPROTO_STREAM : LPXPROTO_DGRAM,
		NULL,
		0,
		WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == m_sock) {
		DBGPRT_ERR_EX(_FT("Creating a sock failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL
CLpxAsyncSocket::Bind(CONST SOCKADDR_LPX* pBindAddr)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	INT iResult = ::bind(
		m_sock,
		(const sockaddr*) pBindAddr,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("Binding a socket %p to %s failed: "),
			m_sock, CSockLpxAddr(pBindAddr).ToString());
		return FALSE;
	}

	m_localAddr = *pBindAddr;

	return TRUE;
}

BOOL
CLpxAsyncSocket::SetSockOpt(
	INT nOptName, 
	CONST BYTE* lpOptVal, 
	INT nOptLen, 
	INT nLevel /* = SOL_SOCKET */)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	INT iResult = ::setsockopt(
		m_sock,
		nLevel,
		nOptName,
		(const char*) lpOptVal,
		nOptLen);

	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("Setting a sock opt to a sock %p failed: "),
			m_sock);
		return FALSE;
	}
	return TRUE;
}

BOOL
CLpxDatagramSocket::Create()
{
	return CLpxAsyncSocket::CreateEx(SOCK_DGRAM);
}

BOOL 
CLpxDatagramSocket::RecvFrom(DWORD cbBufferMax, DWORD dwRecvFlags)
{
	LockRecvQueue();

	_ASSERTE(cbBufferMax > 0);

	BOOL fSuccess = FALSE;

	fSuccess = AllocRecvBuf(cbBufferMax);
	if (!fSuccess) {
		UnlockRecvQueue();
		return FALSE;
	}

	fSuccess = ::ResetEvent(m_hReceivedEvent);
	_ASSERTE(fSuccess);

	m_ovReceive.hEvent = m_hReceivedEvent;
	m_ovReceive.Internal = 
	m_ovReceive.InternalHigh = 
	m_ovReceive.Offset = 
	m_ovReceive.OffsetHigh = 0;

	DWORD cbReceived = 0;
	m_receiveFlags = dwRecvFlags;
	m_iRemoteAddrLen = sizeof(m_remoteAddr);

	INT iResult = ::WSARecvFrom(
		m_sock,
		&m_wsaReceiveBuffer, 1,
		&cbReceived,
		&m_receiveFlags,
		(struct sockaddr*) &m_remoteAddr,
		&m_iRemoteAddrLen,
		&m_ovReceive,
		NULL);

	if (0 == iResult) {
		fSuccess = ::SetEvent(m_hReceivedEvent);
		_ASSERTE(fSuccess);
		return TRUE;
	}

	if (WSA_IO_PENDING == ::WSAGetLastError()) {
		return TRUE;
	}

	UnlockRecvQueue();
	return FALSE;
}

BOOL 
CLpxDatagramSocket::SendTo(
	CONST SOCKADDR_LPX* pRemoteAddr,
	DWORD cbData, 
	CONST BYTE* lpbData,
	DWORD dwSendFlags)
{
	LockSendQueue();

	m_wsaSendBuffer.len = (u_long) cbData;
	m_wsaSendBuffer.buf = (char*) lpbData;

	BOOL fSuccess = ::ResetEvent(m_hSentEvent);
	_ASSERTE(fSuccess);
	m_ovSend.hEvent = m_hSentEvent;
	m_ovSend.Internal = 
	m_ovSend.InternalHigh = 
	m_ovSend.Offset = 
	m_ovSend.OffsetHigh = 0;

	DWORD cbSent = 0;
	INT iResult = ::WSASendTo(
		m_sock,
		&m_wsaSendBuffer, 1,
		&cbSent,
		dwSendFlags,
		(const sockaddr*) pRemoteAddr, 
		sizeof(SOCKADDR_LPX),
		&m_ovSend, 
		NULL);

	if (0 == iResult) {
		fSuccess = ::SetEvent(m_hSentEvent);
		_ASSERTE(fSuccess);
		return TRUE;
	}

	if (WSA_IO_PENDING != ::WSAGetLastError()) {
		UnlockSendQueue();
		return FALSE;
	}

	return TRUE;
}

BOOL
CLpxDatagramSocket::GetSendToResult(LPDWORD lpcbSent)
{
	BOOL fSuccess = CLpxAsyncSocket::_GetSendResult(lpcbSent);
	if (!fSuccess) {
		return FALSE;
	}

	UnlockSendQueue();
	return TRUE;
}

BOOL
CLpxDatagramSocket::SendToSync(
	CONST SOCKADDR_LPX* pRemoteAddr,
	DWORD cbToSend, 
	CONST BYTE* lpbData, 
	DWORD dwSendFlags,
	LPDWORD lpcbSent)
{
	BOOL fSuccess = SendTo(pRemoteAddr, cbToSend, lpbData, dwSendFlags);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Send failed: "));
		return FALSE;
	}
	fSuccess = GetSendToResult(lpcbSent);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("GetSendToResult failed: "));
		return FALSE;
	}
	return TRUE;
}

BOOL
CLpxDatagramBroadcastSocket::Create()
{
	BOOL fSuccess = CLpxDatagramSocket::Create();
	if (!fSuccess) {
		return FALSE;
	}
	
	BOOL bBroadcast = TRUE;

	fSuccess = CLpxAsyncSocket::SetSockOpt(
		SO_BROADCAST, 
		(CONST BYTE*) &bBroadcast, 
		sizeof(bBroadcast));

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Setting sock opt to SO_BROADCAST failed: "));
		(VOID) CLpxAsyncSocket::Close();
		return FALSE;
	}

	return TRUE;
}

BOOL
CLpxDatagramBroadcastSocket::SendTo(
	USHORT usRemotePort, 
	DWORD cbToSend, 
	CONST BYTE* lpbData)
{
	SOCKADDR_LPX BroadcastAddr = pCreateLpxBroadcastAddress(usRemotePort);
	return CLpxDatagramSocket::SendTo(
		&BroadcastAddr, 
		cbToSend, 
		lpbData);
}

BOOL 
CLpxStreamSocket::Create()
{
	return CLpxAsyncSocket::CreateEx(SOCK_STREAM);
}

BOOL 
CLpxStreamListener::Accept(SOCKET sockAccept, DWORD cbDataBuffer)
{
	_ASSERTE(INVALID_SOCKET != m_sock);
	_ASSERTE(INVALID_SOCKET != sockAccept);

	BOOL fSuccess = FALSE;

	DWORD cbBufReq = (sizeof(SOCKADDR_LPX) + 16) * 2 + cbDataBuffer;
	fSuccess = AllocRecvBuf(cbBufReq);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("AllocRecvBuf failed: "));
		return FALSE;
	}

	DWORD cbAcceptBuffer = m_wsaReceiveBuffer.len;
	PVOID pbAcceptBuffer = m_wsaReceiveBuffer.buf;

	ResetRecvOverlapped();

	//----------------------------------------
	// Load the AcceptEx function into memory using WSAIoctl.
	// The WSAIoctl function is an extension of the ioctlsocket()
	// function that can use overlapped I/O. The function's 3rd
	// through 6th parameters are input and output buffers where
	// we pass the pointer to our AcceptEx function. This is used
	// so that we can call the AcceptEx function directly, rather
	// than refer to the Mswsock.lib library.
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD cbRead;
	INT iResult = ::WSAIoctl(m_sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidAcceptEx, 
		sizeof(GuidAcceptEx),
		&lpfnAcceptEx, 
		sizeof(lpfnAcceptEx), 
		&cbRead,
		NULL, 
		NULL);

	if (NULL == lpfnAcceptEx) {
		DBGPRT_ERR_EX(_FT("Cannot load AcceptEx function: "));
		return FALSE;
	}

	DWORD cbReceived = 0;
	fSuccess = lpfnAcceptEx(
		m_sock, 
		sockAccept, 
		pbAcceptBuffer, 
		cbDataBuffer,
		sizeof(SOCKADDR_LPX) + 16,
		sizeof(SOCKADDR_LPX) + 16,
		&cbReceived,
		&m_ovReceive);

	if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
		return FALSE;
	}

	iResult = ::setsockopt(
		sockAccept, 
		SOL_SOCKET, 
		SO_UPDATE_ACCEPT_CONTEXT, 
		(char *)&m_sock, 
		sizeof(m_sock));

	if (0 != iResult) {
		DBGPRT_WARN_EX(_FT("Setting SO_UPDATE_ACCEPT_CONTEXT failed: "));
	}

	if (fSuccess) {
		::SetEvent(m_hReceivedEvent);
		return TRUE;
	}

	return TRUE;
}

BOOL
CLpxStreamListener::GetAcceptResult(
	OUT SOCKADDR_LPX* lpLocalAddr,
	OUT SOCKADDR_LPX* lpRemoteAddr, 
	OUT LPDWORD lpcbReceived, 
	OUT CONST BYTE** ppbData, 
	OUT LPDWORD lpdwFlags /* = NULL */)
{
	BOOL fSuccess = ::WSAGetOverlappedResult(
		m_sock,
		&m_ovReceive,
		lpcbReceived,
		TRUE,
		lpdwFlags);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("WSAGetOverlappedResult failed: "));
		return FALSE;
	}

	*ppbData = (CONST BYTE*) m_wsaReceiveBuffer.buf;

	SOCKADDR_LPX *pLocalAddr, *pRemoteAddr;
	INT iLocalAddrLen, iRemoteAddrLen;

	LPFN_GETACCEPTEXSOCKADDRS lpfnAcceptExSockaddrs = NULL;
	GUID GuidAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD cbRead;
	INT iResult = ::WSAIoctl(m_sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidAcceptExSockaddrs, 
		sizeof(GuidAcceptExSockaddrs),
		&lpfnAcceptExSockaddrs, 
		sizeof(lpfnAcceptExSockaddrs), 
		&cbRead,
		NULL, 
		NULL);

	if (NULL == lpfnAcceptExSockaddrs) {
		DBGPRT_ERR_EX(_FT("Cannot load AcceptEx function: "));
		return FALSE;
	}

	lpfnAcceptExSockaddrs(
		m_wsaReceiveBuffer.buf, 
		m_wsaReceiveBuffer.len,
		sizeof(SOCKADDR_LPX) + 16,
		sizeof(SOCKADDR_LPX) + 16,
		(sockaddr**) &pLocalAddr,
		&iLocalAddrLen,
		(sockaddr**) &pRemoteAddr,
		&iRemoteAddrLen);

	*lpLocalAddr = *pLocalAddr;
	*lpRemoteAddr = *pRemoteAddr;

	return TRUE;
}

BOOL 
CLpxStreamListener::Listen(INT nBacklog /* = SOMAXCONN */)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	INT iResult = ::listen(m_sock, nBacklog);
	if (0 != iResult) {
		DBGPRT_ERR_EX(_FT("listen on %p failed: "), m_sock);
		return FALSE;
	}
	return TRUE;
}

BOOL 
CLpxStreamConnection::Connect(
	CONST SOCKADDR_LPX* pRemoteAddr, 
	CONST BYTE* lpSendBuffer, 
	DWORD dwSendDataLen, 
	LPDWORD lpcbSent)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	ResetSendOverlapped();

	LPFN_CONNECTEX lpfnConnectEx = NULL;
	GUID GuidConnectEx = WSAID_CONNECTEX;
	DWORD cbRead;
	INT iResult = ::WSAIoctl(m_sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidConnectEx, 
		sizeof(GuidConnectEx),
		&lpfnConnectEx, 
		sizeof(lpfnConnectEx), 
		&cbRead,
		NULL, 
		NULL);

	if (NULL == lpfnConnectEx) {
		DBGPRT_ERR_EX(_FT("Cannot load ConnectEx function: "));
		return FALSE;
	}

	BOOL fSuccess = lpfnConnectEx(
		m_sock,
		(const sockaddr*) pRemoteAddr,
		sizeof(SOCKADDR_LPX),
		(PVOID) lpSendBuffer,
		dwSendDataLen,
		lpcbSent,
		&m_ovSend);

	if (fSuccess) {
		fSuccess = ::SetEvent(m_hSentEvent);
		_ASSERTE(fSuccess);
		return TRUE;
	}

	if (ERROR_IO_PENDING != ::WSAGetLastError()) {
		DBGPRT_ERR_EX(_FT("ConnectEx at sock %p failed: "), m_sock);
		return FALSE;
	}

	return TRUE;
}

BOOL 
CLpxStreamConnection::GetConnectResult(
	OUT LPDWORD lpcbSent)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	BOOL fSuccess = _GetSendResult(lpcbSent);

	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = CLpxAsyncSocket::SetSockOpt(
		SO_UPDATE_CONNECT_CONTEXT,
		NULL, 
		0);

	if (!fSuccess) {
		DBGPRT_WARN_EX(_FT("Setsockopt SO_UPDATE_ACCEPT_CONTEXT failed: "));
	}

	return TRUE;
}

#if WINVER >= 0x0500
// #if WINVER >= 0x0501
BOOL
CLpxStreamConnection::Disconnect(DWORD dwFlags)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	LPFN_DISCONNECTEX lpfnDisconnectEx = NULL;
	GUID GuidDisconnectEx = WSAID_CONNECTEX;
	DWORD cbRead;
	INT iResult = ::WSAIoctl(m_sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidDisconnectEx, 
		sizeof(GuidDisconnectEx),
		&lpfnDisconnectEx, 
		sizeof(lpfnDisconnectEx), 
		&cbRead,
		NULL, 
		NULL);

	if (NULL == lpfnDisconnectEx) {
		DBGPRT_ERR_EX(_FT("Cannot load DisconnectEx function: "));
		return FALSE;
	}

	BOOL fSuccess = lpfnDisconnectEx(
		m_sock,
		NULL,
		dwFlags,
		0);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("DisconnectEx failed: "));
		return FALSE;
	}

	return TRUE;
}
#endif

BOOL 
CLpxStreamConnection::Recv(DWORD cbBufferMax, LPDWORD lpdwFlags)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	BOOL fSuccess = FALSE;
	DWORD cbReceived = 0;

	fSuccess = AllocRecvBuf(cbBufferMax);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("AllocRecvBuf failed: "));
		return FALSE;
	}

	ResetRecvOverlapped();

	INT iResult = ::WSARecv(
		m_sock,
		&m_wsaReceiveBuffer, 1,
		&cbReceived, lpdwFlags,
		&m_ovReceive, NULL);

	if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
		DBGPRT_ERR_EX(_FT("Recv at sock %p up to %d bytes failed: "),
			m_sock, cbBufferMax);
		return FALSE;
	}

	if (0 == iResult) {
		fSuccess = ::SetEvent(m_hReceivedEvent);
		_ASSERTE(fSuccess);
		return TRUE;
	}

	return TRUE;
}

BOOL
CLpxStreamConnection::Send(
	DWORD cbToSend, 
	CONST BYTE* lpbData,
	DWORD dwFlags /* = 0 */)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	BOOL fSuccess = FALSE;

	ResetSendOverlapped();

	m_wsaSendBuffer.len = cbToSend;
	m_wsaSendBuffer.buf = (char*) lpbData;

	DWORD cbSent = 0;
	INT iResult = ::WSASend(
		m_sock,
		&m_wsaSendBuffer, 1,
		&cbSent, dwFlags,
		&m_ovSend, NULL);

	if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
		DBGPRT_ERR_EX(_FT("Send at sock %p (%d bytes) failed: "),
			m_sock, cbToSend);
		return FALSE;
	}

	if (0 == iResult) {
		fSuccess = ::SetEvent(m_hSentEvent);
		_ASSERTE(fSuccess);
		return TRUE;
	}

	return TRUE;
}

