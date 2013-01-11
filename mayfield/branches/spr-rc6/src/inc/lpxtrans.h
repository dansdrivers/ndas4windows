#pragma once
#include <winsock2.h>
#include <socketlpx.h>
#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>

class CLpxSockAddrListChangeNotifier
{
protected:

	SOCKET m_sock;
	WSAOVERLAPPED m_overlapped;
	HANDLE m_hEvent;

public:

	CLpxSockAddrListChangeNotifier();
	virtual ~CLpxSockAddrListChangeNotifier();

	BOOL Initialize();
	BOOL Reset();
	HANDLE GetChangeEvent();
};

class CLpxAsyncSocket
{
protected:

	SOCKET m_sock;

	SOCKADDR_LPX m_localAddr;
	INT m_iLocalAddrLen;

	SOCKADDR_LPX m_remoteAddr;
	INT m_iRemoteAddrLen;

	HANDLE m_hReceivedEvent;
	WSABUF m_wsaReceiveBuffer;
	WSAOVERLAPPED m_ovReceive;

	HANDLE m_hSentEvent;
	WSABUF m_wsaSendBuffer;
	WSAOVERLAPPED m_ovSend;

	DWORD m_receiveFlags;

	BOOL AllocRecvBuf(DWORD cbSize);

	VOID ResetRecvOverlapped();
	VOID ResetSendOverlapped();

//	CRITICAL_SECTION m_csSendQueue; 
	LONG m_lSendQueueLocks;
//	CRITICAL_SECTION m_csRecvQueue; 
	LONG m_lRecvQueueLocks;

	VOID LockSendQueue() 
	{ 
//		::EnterCriticalSection(&m_csSendQueue);
		LONG result = ::InterlockedIncrement(&m_lSendQueueLocks);
		_ASSERTE(1 == result);
	}
	VOID UnlockSendQueue() 
	{ 
		LONG result = ::InterlockedDecrement(&m_lSendQueueLocks);
		_ASSERTE(0 == result);
//		::LeaveCriticalSection(&m_csSendQueue); 
	}

	VOID LockRecvQueue() 
	{ 
//		::EnterCriticalSection(&m_csRecvQueue); 
		LONG result = ::InterlockedIncrement(&m_lRecvQueueLocks);
		_ASSERTE(1 == result);
	}

	VOID UnlockRecvQueue() 
	{ 
		LONG result = ::InterlockedDecrement(&m_lRecvQueueLocks);
		_ASSERTE(0 == result);
//		::LeaveCriticalSection(&m_csRecvQueue); 
	}

	//
	// Returns the internal buffer pointer
	// Subsequent receive will invalidate this buffer.
	// If you need this buffer, copy before doing another Receive
	//
	virtual BOOL _GetRecvResult(
		OUT LPDWORD lpcbReceived,
		OUT BYTE** ppbData,
		OUT LPDWORD lpdwFlags = NULL);

	virtual BOOL _GetSendResult(
		OUT LPDWORD lpcbSent = NULL);

public:

	CLpxAsyncSocket();
	virtual ~CLpxAsyncSocket();

	virtual BOOL Initialize();
	virtual BOOL ShutDown(INT nHow);
	virtual BOOL Close();

	virtual BOOL Bind(CONST SOCKADDR_LPX* pBindAddr);

	BOOL SetSockOpt(
		IN INT nOptName, 
		IN CONST BYTE* lpOptVal, 
		IN INT nOptLen, 
		IN INT nLevel = SOL_SOCKET);

	BOOL CreateEx(INT nSocketType = SOCK_STREAM);

	VOID Attach(SOCKET hSocket);
	SOCKET Detach();

	virtual HANDLE GetReceivedEvent();
	virtual HANDLE GetSendEvent();

	operator SOCKET() { return m_sock; }
	virtual const SOCKADDR_LPX* GetBoundAddr() { return &m_localAddr; }
};

class CLpxStreamSocket :
	public CLpxAsyncSocket
{
public:
	CLpxStreamSocket() {}
	virtual ~CLpxStreamSocket() {}

	virtual BOOL Create();
};

class CLpxDatagramSocket :
	public CLpxAsyncSocket
{
protected:

public:

	CLpxDatagramSocket() {}
	virtual ~CLpxDatagramSocket() {}

	virtual BOOL Create();

	virtual BOOL RecvFrom(DWORD cbBufferMax, DWORD dwRecvFlags = 0);

	virtual BOOL GetRecvFromResult(
		OUT SOCKADDR_LPX* pRemoteAddr,
		OUT LPDWORD lpcbReceived,
		OUT BYTE** lpbData,
		OUT LPDWORD lpdwFlags);

	virtual BOOL SendTo(
		IN CONST SOCKADDR_LPX* pRemoteAddr,
		IN DWORD cbToSend, 
		IN CONST BYTE* lpbData,
		IN DWORD dwSendFlags = 0);

	virtual BOOL GetSendToResult(
		OUT LPDWORD lpcbSent = NULL);

	virtual BOOL SendToSync(
		IN CONST SOCKADDR_LPX* pRemoteAddr,
		IN DWORD cbToSend, 
		IN CONST BYTE* lpbData, 
		IN DWORD dwSendFlags = 0,
		OUT LPDWORD lpcbSent = NULL);
};


class CLpxStreamListener :
	public CLpxStreamSocket
{
public:
	CLpxStreamListener() {}
	virtual ~CLpxStreamListener() {}

	virtual BOOL Accept(
		SOCKET sockAccept, 
		DWORD cbDataBuffer);

	virtual BOOL GetAcceptResult(
		OUT SOCKADDR_LPX* lpLocalAddr,
		OUT SOCKADDR_LPX* lpRemoteAddr,
		OUT LPDWORD lpcbReceived,
		OUT CONST BYTE** ppbData,
		OUT LPDWORD lpdwFlags = NULL);

	virtual BOOL Listen(INT nBacklog = SOMAXCONN);
};

class CLpxStreamConnection :
	public CLpxStreamSocket
{
public:
	CLpxStreamConnection();

	virtual BOOL Connect(
		CONST SOCKADDR_LPX* pRemoteAddr, 
		CONST BYTE* lpSendBuffer, 
		DWORD dwSendDataLen, 
		LPDWORD lpcbSent);

	virtual BOOL GetConnectResult(
		OUT LPDWORD lpcbSent);

#if WINVER >= 0x0500
// #if WINVER >= 0x0501
	// optionally TF_REUSE_SOCKET
	virtual BOOL Disconnect(DWORD dwFlags);
#endif

	virtual BOOL Recv(DWORD cbBufferMax, LPDWORD lpFlags);
	virtual BOOL Send(DWORD cbToSend, CONST BYTE* ppbData, DWORD dwFlags = 0);
};

class CLpxDatagramBroadcastSocket :
	public CLpxDatagramSocket
{
public:
	virtual BOOL Create();
	virtual BOOL SendTo(
		IN USHORT usRemotePort, 
		IN DWORD cbToSend, 
		IN CONST BYTE* pbData);
};

SOCKADDR_LPX pCreateLpxBroadcastAddress(USHORT usPort);
LPSOCKET_ADDRESS_LIST pCreateSocketAddressList(SOCKET sock);
LPSOCKET_ADDRESS_LIST pCreateLocalLpxAddressList();

class CSockLpxAddr
{
	TCHAR m_buffer[50];
public:

	SOCKADDR_LPX m_sockLpxAddr;

	CSockLpxAddr()
	{
		::ZeroMemory(&m_sockLpxAddr, sizeof(m_sockLpxAddr));
	}

	CSockLpxAddr(BYTE* Node, USHORT usPort)
	{
		_ASSERTE(!IsBadReadPtr(Node, sizeof(m_sockLpxAddr.LpxAddress.Node)));
		m_sockLpxAddr.sin_family = AF_LPX;
		::CopyMemory(
			m_sockLpxAddr.LpxAddress.Node,
			Node,
			sizeof(m_sockLpxAddr.LpxAddress.Node));
		m_sockLpxAddr.LpxAddress.Port = HTONS(usPort);
	}

	CSockLpxAddr(const LPX_ADDRESS& lpxAddress)
	{
		m_sockLpxAddr.sin_family = AF_LPX;
		m_sockLpxAddr.LpxAddress = lpxAddress;
	}

	CSockLpxAddr(const SOCKADDR_LPX& SockLpxAddrRef)
	{
		_ASSERTE(SockLpxAddrRef.sin_family == AF_LPX);
		m_sockLpxAddr = SockLpxAddrRef;
	}

	CSockLpxAddr(const SOCKADDR_LPX* pSockLpxAddr)
	{
		_ASSERTE(pSockLpxAddr->sin_family == AF_LPX);
		m_sockLpxAddr = *pSockLpxAddr;
	}

	operator SOCKADDR_LPX& ()
	{
		return m_sockLpxAddr;
	}

	LPCTSTR NodeString();
	LPCTSTR ToString(DWORD radix = 16);
};
