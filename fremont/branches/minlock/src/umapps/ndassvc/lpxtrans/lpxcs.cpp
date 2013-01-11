#include "lpxcs.h"
#include <winsock2.h>
#include <mswsock.h>
#include <socketlpx.h>
#include <windows.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "xtl/xtltrace.h"

//////////////////////////////////////////////////////////////////////////

CLpxDatagramMultiClient::CLpxDatagramMultiClient() :
	m_lpSocketAddressList(NULL),
	m_hTimer(NULL)
{
}

CLpxDatagramMultiClient::~CLpxDatagramMultiClient()
{
	DWORD err = ::GetLastError();
	if (NULL != m_lpSocketAddressList) {
		::LocalFree(m_lpSocketAddressList);
	}
	if (NULL != m_hTimer) {
		BOOL fSuccess = ::CloseHandle(m_hTimer);
		_ASSERTE(fSuccess);
	}
	::SetLastError(err);
}

BOOL
CLpxDatagramMultiClient::Initialize()
{
	BOOL fSuccess = FALSE;
	for (DWORD i = 0; i < m_nSenders; ++i) {
		fSuccess = m_senders[i].Initialize();
		if (!fSuccess) {
			return FALSE;
		}
	}

	if (NULL == m_hTimer) {
		m_hTimer = ::CreateWaitableTimer(NULL, TRUE, NULL);
		if (NULL == m_hTimer) {
			return FALSE;
		}
	}

	fSuccess = m_sockAddrChangeNotifier.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = m_sockAddrChangeNotifier.Reset();
	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL
CLpxDatagramMultiClient::Send(
	const SOCKADDR_LPX* pRemoteAddr, 
	DWORD cbData, 
	CONST BYTE* pbData)
{
	BOOL fSuccess = FALSE;

	// if there is a address list change
	// bind m_senders again

	DWORD dwWaitResult = ::WaitForSingleObject(
		m_sockAddrChangeNotifier.GetChangeEvent(), 
		0);
	if (WAIT_OBJECT_0 == dwWaitResult || NULL == m_lpSocketAddressList) {
		m_lpSocketAddressList = pCreateLocalLpxAddressList();
		if (NULL == m_lpSocketAddressList) {
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Getting Local Lpx Address list failed, error=0x%X\n",
				GetLastError());
			return FALSE;
		}
		if (WAIT_OBJECT_0 == dwWaitResult) {
			fSuccess = m_sockAddrChangeNotifier.Reset();
			if (!fSuccess) {
				return FALSE;
			}
		}
	}

	DWORD nLocalAddrs =
		min((DWORD)m_lpSocketAddressList->iAddressCount, m_nSenders);

	for (DWORD i = 0; i < m_nSenders; ++i) {
		if (INVALID_SOCKET != (SOCKET)m_senders[i]) {
			m_senders[i].Close();
		}
	}

	for (DWORD i = 0; i < nLocalAddrs && i < m_nSenders; ++i) {

		PSOCKADDR_LPX pSockAddr = (PSOCKADDR_LPX)
			m_lpSocketAddressList->Address[i].lpSockaddr;
		pSockAddr->LpxAddress.Port = 0;

		fSuccess = m_senders[i].Create();
		if (!fSuccess) {
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Creating a socket failed, error=0x%X\n", 
				GetLastError());
			continue;
		}

		static const BYTE BROADCAST_ADDR[] = {0xff,0xff,0xff,0xff,0xff,0xff};
		if (0 == ::memcmp(
			pRemoteAddr->LpxAddress.Node, 
			BROADCAST_ADDR,
			sizeof(BROADCAST_ADDR)))
		{
			const BOOL bBroadcast = TRUE;
			fSuccess = m_senders[i].SetSockOpt(
				SO_BROADCAST, 
				(CONST BYTE*)&bBroadcast, 
				sizeof(BOOL));

			if (!fSuccess) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Setting a sock option to broadcast failed, error=0x%X\n",
					GetLastError());
				(VOID) m_senders[i].Close();
				continue;
			}
		}

		fSuccess= m_senders[i].Bind(pSockAddr);
		if (!fSuccess) {
			DWORD error = GetLastError();
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Binding a sock %d to %s failed, error=0x%X\n",
				i, CSockLpxAddr(pSockAddr).ToStringA(), error);
			(VOID) m_senders[i].Close();
			SetLastError(error);
			continue;
		}

		fSuccess = m_senders[i].SendToSync(pRemoteAddr, cbData, pbData);
		if (!fSuccess) {
			DWORD error = GetLastError();
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Sending at %d failed, error=0x%X\n",
				i, GetLastError());
			(VOID) m_senders[i].Close();
			SetLastError(error);
			continue;
		}
	}

	return TRUE;
}

BOOL
CLpxDatagramMultiClient::SendReceive(
	IReceiveProcessor* pProcessor, 
	const SOCKADDR_LPX* pRemoteAddr,
	DWORD cbData, 
	CONST BYTE* pbData,
	DWORD cbMaxRecvData,
	DWORD dwTimeout,
	DWORD nMaxRecvHint)
{
	BOOL fSuccess = Send(pRemoteAddr, cbData, pbData);
	if (!fSuccess) {
		return FALSE;
	}

	DWORD nLocalAddrs =
		min((DWORD)m_lpSocketAddressList->iAddressCount, m_nSenders);

	LARGE_INTEGER liDueTime;
	// relative time and nanosec scale
	liDueTime.QuadPart = dwTimeout * 10000;
	liDueTime.QuadPart = -liDueTime.QuadPart;
	// liDueTime.HighPart |= 0x80000000;
	
	fSuccess = ::SetWaitableTimer(m_hTimer, &liDueTime, 0, NULL, NULL, FALSE);
	_ASSERTE(fSuccess);
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetWaitableTimer failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	DWORD nWaitingEvents = nLocalAddrs + 1;
	HANDLE *hWaitingEvents = new HANDLE[nWaitingEvents];
	
	if (NULL == hWaitingEvents) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"Allocating hWaitingEvents %d failed, error=0x%X\n",
			nWaitingEvents, GetLastError());
		return FALSE;
	}

	hWaitingEvents[0] = m_hTimer;
	DWORD i = 0;
	for (; i < nLocalAddrs && i < m_nSenders; ++i) {
		fSuccess = m_senders[i].RecvFrom(cbMaxRecvData);
		if (!fSuccess) {
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Initiating receive failed, error=0x%X\n",
				GetLastError());
		}
		hWaitingEvents[1 + i] = m_senders[i].GetReceivedEvent();
	}

	do {

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			nWaitingEvents, hWaitingEvents,
			FALSE, INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			return TRUE;
		} else if (WAIT_OBJECT_0 + 1 <= dwWaitResult &&
			dwWaitResult <= WAIT_OBJECT_0 + 1 + m_nSenders)
		{
			DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 1);
			BOOL fCont = pProcessor->OnReceive(m_senders[n]);
			if (!fCont) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Gathering stopped on Processor Request\n");
				return TRUE;
			}
			fSuccess = m_senders[n].RecvFrom(cbMaxRecvData);
			if (!fSuccess) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Initiating receive failed, error=0x%X\n",
					GetLastError());
			}
		} else {
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Waiting failed, error=0x%X\n",
				GetLastError());
			_ASSERTE(FALSE);
			return FALSE;
		}

	} while (TRUE);
}

BOOL 
CLpxDatagramMultiClient::Broadcast(
	USHORT usRemotePort,
	DWORD cbData,
	CONST BYTE* pbData)
{
	SOCKADDR_LPX bcastAddr = pCreateLpxBroadcastAddress(usRemotePort);
	return Send(
		&bcastAddr,
		cbData,
		pbData);
}

BOOL 
CLpxDatagramMultiClient::BroadcastReceive(
	IReceiveProcessor* pProcessor, 
	USHORT usRemotePort,
	DWORD cbData,
	CONST BYTE* pbData,
	DWORD cbMaxRecvData,
	DWORD dwTimeout,
	DWORD nMaxRecvHint /* = 0 */)
{
	SOCKADDR_LPX bcastAddr = pCreateLpxBroadcastAddress(usRemotePort);
	return SendReceive(
		pProcessor,
		&bcastAddr,
		cbData,
		pbData,
		cbMaxRecvData,
		dwTimeout,
		nMaxRecvHint);
}

CLpxDatagramServer::CLpxDatagramServer() :
	m_lpSocketAddressList(NULL)
{
}

CLpxDatagramServer::~CLpxDatagramServer()
{
	DWORD err = ::GetLastError();
	if (NULL != m_lpSocketAddressList) {
		::LocalFree(m_lpSocketAddressList);
	}
	::SetLastError(err);
}

BOOL
CLpxDatagramServer::Initialize()
{
	for (DWORD i = 0; i < m_nListeners; ++i) {
		BOOL fSuccess = m_listeners[i].Initialize();
		if (!fSuccess) {
			return FALSE;
		}
	}
	
	BOOL fSuccess = m_SockAddrChangeNotifier.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL
CLpxDatagramServer::Receive(
	IReceiveProcessor* pProcessor, 
	USHORT usListenPort, 
	DWORD cbMaxBuffer, 
	HANDLE hStopEvent)
{
	BOOL fSuccess = FALSE;

	do {

		fSuccess = m_SockAddrChangeNotifier.Reset();
		if (!fSuccess) {
			return FALSE;
		}

		m_lpSocketAddressList = pCreateLocalLpxAddressList();
		if (NULL == m_lpSocketAddressList) {
			return FALSE;
		}

		DWORD nLocalAddrs =
			min((DWORD)m_lpSocketAddressList->iAddressCount, m_nListeners);
		_ASSERTE(nLocalAddrs <= MAX_SOCKETLPX_INTERFACE);
		if (nLocalAddrs > MAX_SOCKETLPX_INTERFACE)
		{
			nLocalAddrs = MAX_SOCKETLPX_INTERFACE;
		}

		for (DWORD i = 0; i < m_nListeners; ++i) {
			if (INVALID_SOCKET != (SOCKET)m_listeners[i]) {
				XTLVERIFY( m_listeners[i].Close() );
			}
		}

		for (DWORD i = 0; i < nLocalAddrs; ++i) {

			PSOCKADDR_LPX pSockAddr = (PSOCKADDR_LPX) 
				m_lpSocketAddressList->Address[i].lpSockaddr;
			pSockAddr->LpxAddress.Port = htons(usListenPort);

			fSuccess = m_listeners[i].Create();
			if (!fSuccess) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Creating a sock, address=%s, error=0x%X\n",
					CSockLpxAddr(pSockAddr).ToStringA(), GetLastError());
				continue;
			}

			fSuccess = m_listeners[i].Bind(pSockAddr);
			if (!fSuccess) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"bind failed, address=%s, error=0x%X\n",
					CSockLpxAddr(pSockAddr).ToStringA(), GetLastError());
				XTLVERIFY( m_listeners[i].Close() );
				continue;
			}

			fSuccess = m_listeners[i].RecvFrom(cbMaxBuffer);
			if (!fSuccess) {
				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Initiating receive failed, error=0x%X\n",
					GetLastError());
				XTLVERIFY( m_listeners[i].Close() );
				continue;
			}
		}

		DWORD nWaitingEvents = 2 + nLocalAddrs;
		HANDLE hWaitingEvents[2 + MAX_SOCKETLPX_INTERFACE];

		hWaitingEvents[0] = hStopEvent;
		hWaitingEvents[1] = m_SockAddrChangeNotifier.GetChangeEvent();
		for (DWORD i = 0; i < nLocalAddrs; ++i) {
			hWaitingEvents[2 + i] = m_listeners[i].GetReceivedEvent();
		}

		do {

			DWORD dwWaitResult = ::WaitForMultipleObjects(
				nWaitingEvents, hWaitingEvents,
				FALSE, INFINITE);

			if (WAIT_OBJECT_0 == dwWaitResult) {

				return TRUE;

			} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {

				XTLTRACE1(TRACE_LEVEL_INFORMATION, 
					"SockAddrListChange event issued.\n");
				break;

			} else if (WAIT_OBJECT_0 + 2 <= dwWaitResult &&
				dwWaitResult < WAIT_OBJECT_0 + 2 + nLocalAddrs)
			{

				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 2);
				pProcessor->OnReceive(m_listeners[n]);
				fSuccess = m_listeners[n].RecvFrom(cbMaxBuffer);
				if (!fSuccess) {
					XTLTRACE1(TRACE_LEVEL_ERROR, 
						"Initiating receive failed, error=0x%X\n",
						GetLastError());
				}

			} else {

				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"Waiting failed, error=0x%X\n", GetLastError());
				_ASSERTE(FALSE);
				return FALSE;

			}

		} while (TRUE);

	} while (TRUE);
}

CLpxStreamServer::CLpxStreamServer()
{
}

CLpxStreamServer::~CLpxStreamServer()
{
}

BOOL
CLpxStreamServer::Initialize()
{
	return FALSE;
}

BOOL 
CLpxStreamServer::Listen(
	IConnectProcessor* pProcessor,
	USHORT usListenPort,
	DWORD cbInitialBuffer,
	HANDLE hStopEvent)
{
	return FALSE;
}


