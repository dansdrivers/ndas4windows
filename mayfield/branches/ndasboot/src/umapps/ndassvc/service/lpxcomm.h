/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <winsock2.h>
#include <socketlpx.h>

const USHORT NDAS_DEVICE_LPX_PORT = 10000;
/*
class CLpxUdpListener;
typedef CLpxUdpListener *PCLpxUdpListener;

class CLpxUdpListener {
protected:
	const USHORT m_usListenPort;
	SOCKADDR_LPX m_localSockAddress;
	SOCKADDR_LPX m_remoteSockAddress;
	SOCKET m_sock;

	CLpxUdpListener(LPX_ADDRESS& localLpxAddress, USHORT usListenPort);

public:

	virtual ~CLpxUdpListener();
};

class CLpxUdpAsyncClient
{
protected:

	WSABUF m_wsaSendBuf;
	const USHORT m_usPort;
	SOCKADDR_LPX m_localSockAddress;
	SOCKADDR_LPX m_remoteSockAddress;
	INT m_iRemoteSockAddrLen;
	SOCKET m_sock;
	WSAOVERLAPPED m_wsaOverlapped;
	WSAEVENT m_hDataEvent;

public:
	CLpxUdpAsyncClient(
		LPX_ADDRESS& localLpxAddress, 
		LPX_ADDRESS& remoteLpxAddress, 
		USHORT usRemotePort);

	virtual LPX_ADDRESS GetLocalAddress();
	virtual LPX_ADDRESS GetRemoteAddress();

	virtual ~CLpxUdpAsyncClient();

	virtual BOOL Initialize();

	virtual BOOL SetSockOpt(
		INT level, 
		INT optname, 
		const CHAR* optval, 
		INT optlen);

	virtual BOOL Send(
		DWORD cbData, 
		const VOID* lpbData,
		LPDWORD lpcbSent);

	virtual BOOL GetResult(
		LPDWORD lpcbTransfer, 
		BOOL fWait, 
		LPDWORD lpdwFlags);
};

class CLpxDatagramSender :
	public CLpxUdpAsyncClient
{
public:
	CLpxDatagramSender(
		LPX_ADDRESS& localLpxAddress,
		LPX_ADDRESS& remoteLpxAddress,
		USHORT usRemotePort);

	virtual ~CLpxDatagramSender();

	virtual BOOL Send(
		DWORD cbData,
		const VOID* lpbData,
		LPDWORD lpcbSent);

private:
	virtual BOOL GetResult(
		LPDWORD lpcbTransfer,
		BOOL fWait,
		LPDWORD lpdwFlags);
};

class CLpxDatagramBroadcaster :
	public CLpxDatagramSender
{
public:
	CLpxDatagramBroadcaster(
		LPX_ADDRESS& localLpxAddress,
		USHORT usRemotePort);
	virtual ~CLpxDatagramBroadcaster();

	virtual BOOL Initialize();

	static LPX_ADDRESS LpxBroadcastAddress();
};

class CLpxUdpAsyncListener;
typedef CLpxUdpAsyncListener *PCLpxUdpAsyncListener;

class CLpxUdpAsyncListener : 
	public CLpxUdpListener 
{
protected:

	WSABUF m_wsaRecvBuf;
	WSABUF m_wsaSendBuf;
	WSAOVERLAPPED m_overlapped;
	DWORD m_dwReceiveFlags;
	INT m_iRemoteSockAddrLen;

public:
	
	CLpxUdpAsyncListener(LPX_ADDRESS& localLpxAddress, USHORT usListenPort);
	~CLpxUdpAsyncListener();

	BOOL Initialize();
	BOOL Bind();

	//
	// Initiate reception of the data.
	// This function returns immediately.
	// When the data is available hReceiveEvent is set.
	// Use GetReceivedData to get the result of the data.
	//
	BOOL StartReceive(
		IN HANDLE hReceiveEvent,
		IN DWORD cbReceiveBuffer, 
		OUT LPVOID lpReceiveBuffer,
		OUT LPDWORD lpcbReceived);

	//
	// Get the received data
	//
	BOOL GetReceivedData(LPDWORD lpcbReceived);

	//
	// Get the binded local LPX address
	//
	LPX_ADDRESS GetLocalAddress();

	//
	// Get the remote address where the data is received.
	// Only valid after GetReceivedData
	//
	LPX_ADDRESS GetRemoteAddress();

	//
	// Send a reply packet
	//
	BOOL SendReply(
		IN DWORD cbData,
		IN const BYTE* pData,
		OUT LPDWORD lpcbSent);

	//
	// Cleanup
	//
	VOID Cleanup();
};
*/

BOOL GetLocalLpxAddressList(
	IN DWORD cbOutBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpOutBuffer,
	OUT LPDWORD pcbBytesReturned);

BOOL GetLocalLpxAddressList(
	IN SOCKET s,
	IN DWORD cbOutBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpOutBuffer,
	OUT LPDWORD pcbBytesReturned);

SOCKET
CreateLpxConnection(
	IN const LPX_ADDRESS* pRemoteAddress, 
	IN const LPX_ADDRESS* pLocalAddress,
	IN USHORT usRemotePort = NDAS_DEVICE_LPX_PORT);

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs);

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs);


static const SIZE_T LPXADDRESS_STRING_LENGTH = 18;

BOOL 
CreateLpxAddressString(
	IN const PLPX_ADDRESS pLpxAddress,
	IN OUT LPTSTR lpszAddress, 
	IN SIZE_T cchAddress);

//BOOL GetAddressListChangeNotification(
//	IN SOCKET sock,
//	IN LPWSAOVERLAPPED lpOverlapped);

// wrapper for LPX Address
class CLpxAddress
{
	TCHAR m_szBuffer[18];
	LPX_ADDRESS m_lpxAddress;

public:
	CLpxAddress(const LPX_ADDRESS& lpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, &lpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = TEXT('\0');
	}

	CLpxAddress(const LPX_ADDRESS* pLpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, pLpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = TEXT('\0');
	}

	operator const LPX_ADDRESS&()
	{
		return m_lpxAddress;
	}

	LPCTSTR ToString()
	{
		if (m_szBuffer[0] == TEXT('\0')) {
			// 00:00:00:00:00:00
			HRESULT hr = StringCchPrintf(m_szBuffer, 18, 
				_T("%02X:%02X:%02X:%02X:%02X:%02X"),
				m_lpxAddress.Node[0], m_lpxAddress.Node[1],
				m_lpxAddress.Node[2], m_lpxAddress.Node[3],
				m_lpxAddress.Node[4], m_lpxAddress.Node[5]);
			_ASSERTE(SUCCEEDED(hr));
		}
		return m_szBuffer;
	}

	operator LPCTSTR()
	{
		return ToString();
	}

	//
	// only compares the Node
	//
	BOOL IsEqualAddress(const LPX_ADDRESS& rhs)
	{
		return (m_lpxAddress.Node[0] == rhs.Node[0]) &&
			(m_lpxAddress.Node[1] == rhs.Node[1]) &&
			(m_lpxAddress.Node[2] == rhs.Node[2]) &&
			(m_lpxAddress.Node[3] == rhs.Node[3]) &&
			(m_lpxAddress.Node[4] == rhs.Node[4]) &&
			(m_lpxAddress.Node[5] == rhs.Node[5]);
	}

	BOOL IsEqualAddress(const CLpxAddress& rhs)
	{
		return IsEqualAddress(rhs.m_lpxAddress);
	}

	BOOL IsEqualAddress(const LPX_ADDRESS* rhs)
	{
		return IsEqualAddress(*rhs);
	}

};


/*++

LPX UDP Packet Broadcaster

Broadcast a packet to all local LPX addresses

--*/
/*
class CLpxUdpBroadcaster
{
public:
	CLpxUdpBroadcaster();
	virtual ~CLpxUdpBroadcaster();

	BOOL Send(DWORD cbData, const BYTE* lpbData);
};
*/
/*++

LPX Address List Change Notification Class

Example:

CLpxAddressListChangeNotifier alc(hResetBindEvent);

while (TRUE) {

	BOOL fSuccess = alc.Reset();
	... handle error ...

	HANDLE hEvents[2];
	hEvents[0] = hResetBindEvent;
	...

	BOOL alReset = FALSE;
	while (!alReset) {
		DWORD dwWaitResult = WaitForMultipleObjects(hEvents, 2, ...);
		if (WAIT_OBJECT_0 == dwWaitResult) {
			alReset = TRUE;
		}
		...
	}
}

--*/
/*
class CLpxAddressListChangeNotifier
{
	SOCKET m_sock;
	WSAOVERLAPPED m_overlapped;
	HANDLE m_hEvent;

public:
	CLpxAddressListChangeNotifier(HANDLE hEvent);
	~CLpxAddressListChangeNotifier();

	BOOL Reset();
	HANDLE GetEventHandle();

};
*/