#pragma once
#include "lpxtrans.h"

class CLpxDatagramMultiClient
{
protected:

	const static DWORD m_nSenders = MAX_SOCKETLPX_INTERFACE ;
	LPSOCKET_ADDRESS_LIST m_lpSocketAddressList;
	CLpxDatagramSocket m_senders[m_nSenders];

	HANDLE m_hTimer;
	CLpxSockAddrListChangeNotifier m_sockAddrChangeNotifier;

public:

	struct IReceiveProcessor {
		virtual BOOL OnReceive(CLpxDatagramSocket& bcaster) = 0;
	};

	CLpxDatagramMultiClient();
	virtual ~CLpxDatagramMultiClient();

	BOOL Initialize();

	BOOL Send(
		const SOCKADDR_LPX *pRemoteAddr,
		DWORD cbData,
		CONST BYTE* pbData);

	BOOL SendReceive(
		IReceiveProcessor* pProcessor, 
		const SOCKADDR_LPX *pRemoteAddr,
		DWORD cbData,
		CONST BYTE* pbData,
		DWORD cbMaxRecvData,
		DWORD dwTimeout,
		DWORD nMaxRecvHint = 0);

	BOOL Broadcast(
		USHORT usRemotePort,
		DWORD cbData,
		CONST BYTE* pbData);

	BOOL BroadcastReceive(
		IReceiveProcessor* pProcessor, 
		USHORT usRemotePort,
		DWORD cbData,
		CONST BYTE* pbData,
		DWORD cbMaxRecvData,
		DWORD dwTimeout,
		DWORD nMaxRecvHint = 0);

};

class CLpxDatagramServer
{
protected:

	const static DWORD m_nListeners = MAX_SOCKETLPX_INTERFACE ;
	LPSOCKET_ADDRESS_LIST m_lpSocketAddressList;
	CLpxDatagramSocket m_listeners[m_nListeners];
	CLpxSockAddrListChangeNotifier m_SockAddrChangeNotifier;

public:

	struct IReceiveProcessor {
		virtual VOID OnReceive(CLpxDatagramSocket& cListener) = 0; 
	};

	CLpxDatagramServer();
	virtual ~CLpxDatagramServer();

	BOOL Initialize();
	BOOL Receive(
		IReceiveProcessor* pProcessor, 
		USHORT usListenPort, 
		DWORD cbMaxBuffer, 
		HANDLE hStopEvent);
};

class CLpxStreamServer
{
protected:

	const static DWORD m_nListeners = MAX_SOCKETLPX_INTERFACE;
	LPSOCKET_ADDRESS_LIST m_lpSocketAddressList;
	CLpxStreamListener m_listeners[m_nListeners];
	CLpxSockAddrListChangeNotifier m_SockAddrChangeNotifier;

public:

	CLpxStreamServer();
	virtual ~CLpxStreamServer();

	struct IConnectProcessor {
		virtual VOID OnConnect(CLpxStreamConnection& sockConn) = 0; 
	};

	BOOL Initialize();
	BOOL Listen(
		IConnectProcessor* pProcessor,
		USHORT usListenPort,
		DWORD cbInitialBuffer,
		HANDLE hStopEvent);
};

