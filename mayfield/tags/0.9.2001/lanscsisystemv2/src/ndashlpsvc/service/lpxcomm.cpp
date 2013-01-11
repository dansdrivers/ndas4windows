/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "lpxcomm.h"
#include <socketlpx.h>
#include <binparam.h> // FIRST_TARGET_XXX
#include <stdio.h>
#include "autores.h"

namespace lscmd {
#include <lanscsiop.h>
}

#include "ndasdib.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_LPXCOMM
#include "xdebug.h"

#if 0
// TEST

void DumpCharArray(LPCTSTR fmt, PCHAR pch, DWORD n)
{
	for (DWORD i = 0; i < n; i++) {
		_tprintf(fmt, pch[i]);
	}
}

void DumpTargetInformation(lscmd::PLANSCSI_PATH pPath, DWORD dwTarget)
{
	_tprintf(
		_T("----- Target %d -----\n")
		_T("Present: %d\n")
		_T("RO Users: %d\n")
		_T("RW Users: %d\n")
		_T("Sector Count: %I64d\n")
		_T("LBA         : %d\n")
		_T("LBA46       : %d\n")
		,
		dwTarget,
		pPath->PerTarget[dwTarget].bPresent,
		pPath->PerTarget[dwTarget].NRROHost,
		pPath->PerTarget[dwTarget].NRRWHost,
		pPath->PerTarget[dwTarget].SectorCount,
		pPath->PerTarget[dwTarget].bLBA,
		pPath->PerTarget[dwTarget].bLBA48
		);

	char model[255] = {0};
	char fwrev[255] = {0};
	char serialno[255] = {0};

	CopyMemory(model, pPath->PerTarget[dwTarget].Model, 40);
	CopyMemory(fwrev, pPath->PerTarget[dwTarget].FwRev, 8);
	CopyMemory(serialno, pPath->PerTarget[dwTarget].SerialNo, 20);

	_tprintf(_T("Model:"));
	DumpCharArray(_T("%c"), pPath->PerTarget[dwTarget].Model, 40);
	_tprintf(_T("\n"));
	
	_tprintf(_T("FW Rev:"));
	DumpCharArray(_T("%c"), pPath->PerTarget[dwTarget].FwRev, 8);
	_tprintf(_T("\n"));
	
	_tprintf(_T("Serial No:"));
	DumpCharArray(_T("%c"), pPath->PerTarget[dwTarget].SerialNo, 40);
	_tprintf(_T("\n"));


}


void DumpControllerInformation(lscmd::PLANSCSI_PATH pPath)
{
	_tprintf(
		_T("=== CONTROLLER ====\n")
		_T("HWType      : %d\n")
		_T("HWVersion   : %d\n")
		_T("Max Blocks  : %d\n")
		_T("Max LUs     : %d\n")
		_T("Max Targets : %d\n")
		_T("# of Targets: %d\n")
		_T("# of Slots  : %d\n")
		,
		pPath->HWType,
		pPath->HWVersion,
		pPath->iMaxBlocks,
		pPath->iMaxLUs,
		pPath->iMaxTargets,
		pPath->iNRTargets,
		pPath->iNumberofSlot
		);

	for (DWORD i = 0; i < pPath->iMaxTargets; i++) {
		DumpTargetInformation(pPath, i);
	}
}

#define MAC_PARAM(x) x[0],x[1],x[2],x[3],x[4],x[5]
void DumpExtendedDiskInfo(NDAS_UNITDISK_INFORMATION_BLOCK *pdi)
{
	_tprintf(
		_T("=== DISK INFORMATION BLOCK ====\n")
		_T("Signature: %I64X\n")
		_T("Ethernet Address: %02X:%02X:%02X:%02X:%02X:%02X\n")
		_T("UnitNumber: %d\n")
		_T("DiskType: %d\n")
		_T("Peer Address: %02X:%02X:%02X:%02X:%02X:%02X\n")
		_T("Peer Unit Number: %d\n")
		_T("")
		_T("")
		_T("")
		_T("")
		_T("")
		_T("")
		,
		pdi->Signature,
		MAC_PARAM(pdi->EtherAddress),
		pdi->UnitNumber,
		pdi->DiskType,
		MAC_PARAM(pdi->PeerAddress),
		pdi->PeerUnitNumber
		);
}

void InitializeLANSCSIPATH(lscmd::PLANSCSI_PATH pLSPath, DWORD dwTarget, BOOL bWritable)
{
	ZeroMemory(pLSPath, sizeof(lscmd::LANSCSI_PATH));
}

#endif



/*++

Description:

	GetLpxInterfaceList returns the list of the addresses of interfaces 
	that are bound to the LPX protocol.

	To get the required size of the list, set lpSocketAddress and 
	cbSocketAddress to NULL and 0.

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call WSAGetLastError.

--*/

BOOL GetLocalLpxAddressList(
	IN SOCKET s,
	IN DWORD cbBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpBuffer,
	OUT LPDWORD pcbBytesReturned)
{
	_ASSERTE(INVALID_SOCKET != s);
	_ASSERTE(!IsBadWritePtr(lpBuffer, cbBuffer));

	INT iError = WSAIoctl(
		s,
		SIO_ADDRESS_LIST_QUERY,
		NULL, 
		0,
		lpBuffer,
		cbBuffer,
		pcbBytesReturned,
		NULL,
		NULL);

	if (iError != 0) {
		DPErrorExWsa(_FT("SIO_ADDRESS_LIST_QUERY failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL GetLocalLpxAddressList(
	IN DWORD cbBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpBuffer,
	OUT LPDWORD pcbBytesReturned)
{
	SOCKET sock = ::WSASocket(
		AF_LPX, 
		SOCK_STREAM, 
		IPPROTO_LPXTCP,
		NULL,
		0,
		0);

	if (INVALID_SOCKET == sock) {
		DPErrorExWsa(_FT("Socket creation failed: "));
		return FALSE;
	}

	BOOL fSuccess = GetLocalLpxAddressList(
		sock, 
		cbBuffer,
		lpBuffer, 
		pcbBytesReturned);

	//
	// Close socket may shadow last error
	//
	int iWSALastError = ::WSAGetLastError();

	INT iResult = ::closesocket(sock);
	if (0 != iResult) {
		DPWarningExWsa(_FT("Closing a socket failed: "));
	}

	::WSASetLastError(iWSALastError);

	return fSuccess;
}

/*++

Description:

	make a LPX stream connection to the remote host
	as LPX is not routable, you have to explicitly specify
	the local address (interface) to use

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call WSAGetLastError.
	In this case, the value of lpSocketConnected is unspecified.
	
--*/

SOCKET CreateLpxConnection(
	IN const LPX_ADDRESS* pRemoteAddress, 
	IN const LPX_ADDRESS* pLocalAddress,
	IN USHORT usRemotePort)
{
	SOCKADDR_LPX saLocalSockAddr;
	SOCKADDR_LPX saRemoteSockAddr;

	//
	// create a socket
	// 
	SOCKET sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
	if (INVALID_SOCKET == sock) {
		// Cannot create socket
		DPErrorExWsa(_FT("Socket creation failed: "));
		return INVALID_SOCKET;
	}

	//
	// initialize local address struct
	//
	ZeroMemory(&saLocalSockAddr, sizeof(SOCKADDR_LPX));
	saLocalSockAddr.sin_family = AF_LPX;
	CopyMemory(saLocalSockAddr.LpxAddress.Node, pLocalAddress->Node, 6);

	//
	// bind a socket to the specified local address
	//
	int iResult = ::bind(
		sock,
		(struct sockaddr *) &saLocalSockAddr,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) {
		DPErrorExWsa(_FT("bind failed: "));
		closesocket(sock);
		return INVALID_SOCKET;
	}

	//
	// initialize remote address struct
	//
	ZeroMemory(&saRemoteSockAddr, sizeof(SOCKADDR_LPX));
	saRemoteSockAddr.sin_family = AF_LPX;
	CopyMemory(saRemoteSockAddr.LpxAddress.Node, pRemoteAddress->Node, 6);
	saRemoteSockAddr.LpxAddress.Port = htons(usRemotePort);

	//
	// connect to the remote host
	//
	iResult = ::connect(
		sock,
		(struct sockaddr *) &saRemoteSockAddr,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) {
		::closesocket(sock);
		return INVALID_SOCKET;
	}

	return sock;
}

/*++

Description:

	A utility function to create 

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call GetLastError.
	
--*/
BOOL CreateLpxAddressString(
	IN const PLPX_ADDRESS address,
	IN OUT LPTSTR lpAddress, 
	IN SIZE_T cchAddress)
{
	HRESULT hr = StringCchPrintf(lpAddress, cchAddress, 
		_T("%02X:%02X:%02X:%02X:%02X:%02X"),
		address->Node[0],
		address->Node[1],
		address->Node[2],
		address->Node[3],
		address->Node[4],
		address->Node[5]);
	return SUCCEEDED(hr);
}

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs)
{
	return
		lhs.Node[0] == rhs.Node[0] &&
		lhs.Node[1] == rhs.Node[1] &&
		lhs.Node[2] == rhs.Node[2] &&
		lhs.Node[3] == rhs.Node[3] &&
		lhs.Node[4] == rhs.Node[4] &&
		lhs.Node[5] == rhs.Node[5];		
}

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs)
{
	return
		lhs->Node[0] == rhs->Node[0] &&
		lhs->Node[1] == rhs->Node[1] &&
		lhs->Node[2] == rhs->Node[2] &&
		lhs->Node[3] == rhs->Node[3] &&
		lhs->Node[4] == rhs->Node[4] &&
		lhs->Node[5] == rhs->Node[5];		
}

BOOL GetAddressListChangeNotification(
	IN SOCKET sock,
	IN LPWSAOVERLAPPED lpOverlapped)
{
	int iError;
	DWORD cbBytesReturned;

	// query interfaces where LPX protocol is binded
	iError = ::WSAIoctl(
		sock,
		SIO_ADDRESS_LIST_CHANGE,
		NULL, 0,
		NULL, 0,
		&cbBytesReturned,
		lpOverlapped,
		NULL);

	if (iError != 0 && ::WSAGetLastError() != WSA_IO_PENDING) { // SOCKET_ERROR
		// TODO: Error Event Log from WSAGetLastError
		DPErrorEx2( ::WSAGetLastError(), _T("WSAIoctl Error on SIO_ADDRESS_LIST_CHANGE!\n"));
		return FALSE;
	}

	return TRUE;
}

/*++

Implementation of CLpxUdpListener

--*/

CLpxUdpListener::CLpxUdpListener(
	LPX_ADDRESS& localLpxAddress,
	USHORT usListenPort) :
	m_usListenPort(usListenPort),
	m_sock(INVALID_SOCKET)
{
	::ZeroMemory(&m_localSockAddress, sizeof(SOCKADDR_LPX));
	m_localSockAddress.LpxAddress = localLpxAddress;
	m_localSockAddress.LpxAddress.Port = HTONS(m_usListenPort);

	::ZeroMemory(&m_remoteSockAddress, sizeof(SOCKADDR_LPX));
}

CLpxUdpListener::~CLpxUdpListener()
{}

/*++

Implementation of CLpxUdpAsyncListener

--*/

CLpxUdpAsyncListener::CLpxUdpAsyncListener(
	LPX_ADDRESS& localLpxAddress, USHORT usListenPort) :
	m_dwReceiveFlags(0),
	CLpxUdpListener(localLpxAddress, usListenPort)
{
}

CLpxUdpAsyncListener::~CLpxUdpAsyncListener()
{
	Cleanup();
}

VOID CLpxUdpAsyncListener::Cleanup()
{
	if (INVALID_SOCKET != m_sock) {
		
		INT iResult = ::shutdown(m_sock, SD_BOTH);
		if (0 != iResult) {
			DPErrorExWsa(_FT("Shutdown a socket failed: "));
		}

		iResult = ::closesocket(m_sock);
		if (0 != iResult) {
			DPErrorExWsa(_FT("Close a socket failed: "));
		}

		m_sock = INVALID_SOCKET;
	}
}

BOOL CLpxUdpAsyncListener::StartReceive(
	IN HANDLE hReceiveEvent,
	IN DWORD cbReceiverBuffer, 
	OUT LPVOID lpReceiveBuffer, 
	OUT LPDWORD lpcbReceived)
{
	::ZeroMemory(&m_overlapped, sizeof(WSAOVERLAPPED));
	m_overlapped.hEvent = hReceiveEvent;

	INT iFromLen = sizeof(SOCKADDR_LPX);

	WSABUF buf;
	buf.buf = (char*) lpReceiveBuffer;
	buf.len = cbReceiverBuffer;

	INT iResult = ::WSARecvFrom(
		m_sock,
		&buf,
		1,
		lpcbReceived,
		&m_dwReceiveFlags,
		(sockaddr *) &m_remoteSockAddress,
		&iFromLen,
		&m_overlapped,
		NULL);

	if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Receive From %d failed: "), 
			CLpxAddress(m_localSockAddress.LpxAddress).ToString());
		return FALSE;
	}

	return TRUE;
}

BOOL CLpxUdpAsyncListener::Initialize()
{
	m_sock = ::WSASocket(
		AF_LPX,
		SOCK_DGRAM,
		IPPROTO_LPXUDP,
		NULL,
		NULL,
		WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == m_sock) {
		DPErrorExWsa(_FT("Socket creation failed on %s: "),
			CLpxAddress(m_localSockAddress.LpxAddress).ToString());
		return FALSE;
	}
	return TRUE;
}

BOOL 
CLpxUdpAsyncListener::
Bind()
{	
	INT iResult = ::bind(
		m_sock,
		(struct sockaddr*) &m_localSockAddress,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) {
		DPErrorExWsa(_FT("Socket binding failed on %s: "),
			CLpxAddress(m_localSockAddress.LpxAddress).ToString());
		return FALSE;
	}

	return TRUE;
}

BOOL 
CLpxUdpAsyncListener::
GetReceivedData(LPDWORD lpcbRead)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	BOOL fSuccess = ::WSAGetOverlappedResult(
		m_sock,
		&m_overlapped,
		lpcbRead,
		TRUE,
		&m_dwReceiveFlags);

	return fSuccess;
}

LPX_ADDRESS
CLpxUdpAsyncListener::
GetLocalAddress()
{
	return m_localSockAddress.LpxAddress;
}

LPX_ADDRESS
CLpxUdpAsyncListener::
GetRemoteAddress()
{
	return m_remoteSockAddress.LpxAddress;
}

BOOL
CLpxUdpAsyncListener::
SendReply(
	IN DWORD cbData, 
	IN const BYTE* pData, 
	OUT LPDWORD lpcbSent)
{
	_ASSERTE(INVALID_SOCKET != m_sock);
	_ASSERTE(!IsBadReadPtr(pData, cbData));
	_ASSERTE(!IsBadWritePtr(lpcbSent, sizeof(DWORD)));

	WSABUF wsaBuffer = {0};
	wsaBuffer.buf = (char*)pData;
	wsaBuffer.len = cbData;

	INT iResult = ::WSASendTo(
		m_sock,
		&wsaBuffer,
		1,
		lpcbSent,
		0,
		(struct sockaddr*)&m_remoteSockAddress,
		sizeof(SOCKADDR_LPX),
		NULL,
		NULL);

	if (0 != iResult) {
		DPErrorExWsa(_FT("Failed to send a reply: "));
		return FALSE;
	}

	return TRUE;
}

#if 0

CLpxUdpAsyncMultiListener::
CLpxUdpAsyncMultiListener(USHORT usListenPort) :
	m_usListenPort(usListenPort)
{
}

CLpxUdpAsyncMultiListener::
~CLpxUdpAsyncMultiListener()
{
}

//
// Initialize listeners.
// Returns number of listeners successfully initialized.
//
DWORD 
CLpxUdpAsyncMultiListener::
Initialize()
{
}

//
// Returns number of listeners which are started successfully.
//
DWORD 
CLpxUdpAsyncMultiListener::
StartReceive(
	IN HANDLE hDataEvent,
	IN DWORD cbReceiveBuffer,
	OUT LPVOID lpReceiveBuffer,
	OUT LPDWORD lpcbReceived)
{
	LPSOCKET_ADDRESS_LIST pLocalAddrList =
		new BYTE[sizeof(SOCKET_ADDRESS_LIST) + 
			sizeof(SOCKET_ADDRESS) * (MAX_SOCKETLPX_INTERFACE - 1)];

	DWORD cbReturned;
	BOOL fSuccess = GetLocalLpxAddressList(
		MAX_SOCKETLPX_INTERFACE, 
		pLocalAddrList, 
		&cbReturned);

	pLocalAddrList->iAddressCount;

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		m_pListeners[i] = new CLpxUdpAsyncListener(
	}
}

//
// Fetch the data in an event of data.
// Only the most recent one is available.
//
BOOL 
CLpxUdpAsyncMultiListener::
GetReceivedData(
	OUT LPDWORD lpcbReceived,
	OUT PLPX_ADDRESS localLpxAddress,
	OUT PLPX_ADDRESS remoteLpxAddress)
{
}

#endif


CLpxAddressListChangeNotifier::
CLpxAddressListChangeNotifier(HANDLE hEvent) :
	m_sock(INVALID_SOCKET),
	m_hEvent(hEvent)
{
	_ASSERTE(NULL != m_hEvent);
}

CLpxAddressListChangeNotifier::
~CLpxAddressListChangeNotifier()
{
	if (INVALID_SOCKET != m_sock) {
		::closesocket(m_sock);
	}
}

BOOL
CLpxAddressListChangeNotifier::
Reset()
{
	BOOL fSuccess = ::ResetEvent(m_hEvent);
	_ASSERT(fSuccess);

	::ZeroMemory(&m_overlapped, sizeof(WSAOVERLAPPED));
	m_overlapped.hEvent = m_hEvent;

	AutoSocket autosock = ::WSASocket(
		AF_LPX, 
		SOCK_DGRAM, 
		IPPROTO_LPXUDP, 
		NULL, 
		0, 
		WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == (SOCKET) autosock) {
		return FALSE;
	}

	int iError;
	DWORD cbBytesReturned;

	iError = ::WSAIoctl(
		autosock,
		SIO_ADDRESS_LIST_CHANGE,
		NULL, 0,
		NULL, 0,
		&cbBytesReturned,
		&m_overlapped,
		NULL);

	if (0 != iError && WSA_IO_PENDING != ::WSAGetLastError()) { 
		// SOCKET_ERROR
		// TODO: Error Event Log from WSAGetLastError
		DPErrorExWsa(_FT("WSAIoctl SIO_ADDRESS_LIST_CHANGE failed: "));
		return FALSE;
	}

	autosock.Detach();
	if (INVALID_SOCKET != m_sock) {
		::closesocket(m_sock);
	}
	m_sock = autosock;

	return TRUE;
}

HANDLE 
CLpxAddressListChangeNotifier::
GetEventHandle()
{
	return m_hEvent;
}

CLpxUdpAsyncClient::
CLpxUdpAsyncClient(
	LPX_ADDRESS& localLpxAddress, 
	LPX_ADDRESS& remoteLpxAddress, 
	USHORT usRemotePort) :
	m_usPort(usRemotePort),
	m_sock(INVALID_SOCKET),
	m_hDataEvent(NULL)
{
	::ZeroMemory(&m_localSockAddress, sizeof(SOCKADDR_LPX));
	::CopyMemory(
		m_localSockAddress.LpxAddress.Node,
		localLpxAddress.Node,
		sizeof(localLpxAddress.Node));
	m_localSockAddress.LpxAddress.Port = 0;
	m_localSockAddress.sin_family = AF_LPX;

	::ZeroMemory(&m_remoteSockAddress, sizeof(SOCKADDR_LPX));
	::CopyMemory(
		m_remoteSockAddress.LpxAddress.Node,
		remoteLpxAddress.Node,
		sizeof(remoteLpxAddress.Node));
	m_remoteSockAddress.LpxAddress.Port = htons(usRemotePort);
	m_remoteSockAddress.sin_family = AF_LPX;
}

CLpxUdpAsyncClient::
~CLpxUdpAsyncClient()
{
	if (INVALID_SOCKET != m_sock) {
		INT iResult = ::shutdown(m_sock, SD_BOTH);
		if (0 != iResult) {
			DPWarningExWsa(_FT("Shutting down a socket failed: "));
		}
		iResult = ::closesocket(m_sock);
		if (0 != iResult) {
			DPWarningExWsa(_FT("Closing a socket failed: "));
		}
	}
	if (NULL != m_hDataEvent) {
		BOOL fSuccess = ::WSACloseEvent(m_hDataEvent);
		if (!fSuccess) {
			DPWarningExWsa(_FT("Closing an event failed: "));
		}
	}
}

BOOL
CLpxUdpAsyncClient::
Initialize()
{
	_ASSERTE(NULL == m_hDataEvent);
	_ASSERTE(INVALID_SOCKET == m_sock);

	WSAEVENT hEvent = ::WSACreateEvent();
	if (NULL == hEvent) {
		DPErrorExWsa(_FT("Creating an event failed: "));
		return FALSE;
	}

	SOCKET sock = ::WSASocket(
		AF_LPX,
		SOCK_DGRAM,
		IPPROTO_LPXUDP,
		NULL,
		NULL,
		WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == sock) {
		DPErrorExWsa(_FT("Create a LPX DGRAM socket failed: "));
		return FALSE;
	}

	INT iResult = ::bind(
		sock, 
		(const struct sockaddr*) &m_localSockAddress, 
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) {
		DPErrorExWsa(_FT("Binding failed on local address %d: "),
			CLpxAddress(m_localSockAddress.LpxAddress).ToString());
		::closesocket(sock);
		return FALSE;
	}

	m_sock = sock;
	m_hDataEvent = hEvent;

	return TRUE;
}

BOOL
CLpxUdpAsyncClient::
SetSockOpt(
	INT level, 
	INT optname, 
	const CHAR* optval, 
	INT optlen)
{
	INT iResult = ::setsockopt(m_sock, level, optname, optval, optlen);
	if (0 != iResult) {
		DPErrorExWsa(_FT("Setting a socket options failed: "));
	}
	return (0 == iResult);
}

BOOL
CLpxUdpAsyncClient::
Send(
	DWORD cbData, 
	const VOID* lpbData,
	LPDWORD lpcbSent)
{
	_ASSERTE(!IsBadReadPtr(lpbData, cbData));
	_ASSERTE(INVALID_SOCKET != m_sock);

	WSABUF wsaBuffer;
	wsaBuffer.buf = (char*)(lpbData);
	wsaBuffer.len = cbData;

	::ZeroMemory(&m_wsaOverlapped, sizeof(WSAOVERLAPPED));
	m_wsaOverlapped.hEvent = m_hDataEvent;

	INT iResult = ::WSASendTo(
		m_sock, 
		&wsaBuffer, 
		1, 
		lpcbSent, 
		0, // no flags
		(const sockaddr*) &m_remoteSockAddress,
		sizeof(SOCKADDR_LPX),
		&m_wsaOverlapped,
		NULL);

	if (0 != iResult && WSA_IO_PENDING != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Sending a data (%d bytes) failed (%d): "), cbData, iResult);
	}

	return (0 == iResult);
}

BOOL
CLpxUdpAsyncClient::
GetResult(
	LPDWORD lpcbTransfer,
	BOOL fWait,
	LPDWORD lpdwFlags)
{
	_ASSERTE(INVALID_SOCKET != m_sock);

	BOOL fSuccess = ::WSAGetOverlappedResult(
		m_sock,
		&m_wsaOverlapped,
		lpcbTransfer,
		fWait,
		lpdwFlags);

	if (!fSuccess) {
		DPErrorExWsa(_FT("Get Overlapped Result failed: "));
		return FALSE;
	}

	return TRUE;
}

LPX_ADDRESS
CLpxUdpAsyncClient::
GetLocalAddress()
{
	return m_localSockAddress.LpxAddress;
}

LPX_ADDRESS
CLpxUdpAsyncClient::
GetRemoteAddress()
{
	return m_remoteSockAddress.LpxAddress;
}

CLpxDatagramSender::
CLpxDatagramSender(
	LPX_ADDRESS& localLpxAddress,
	LPX_ADDRESS& remoteLpxAddress,
	USHORT usRemotePort) :
	CLpxUdpAsyncClient(localLpxAddress, remoteLpxAddress, usRemotePort)
{
}

CLpxDatagramSender::
~CLpxDatagramSender()
{
}

BOOL
CLpxDatagramSender::
Send(
	DWORD cbData,
	const VOID* lpbData,
	LPDWORD lpcbSent)
{
	BOOL fSuccess = CLpxUdpAsyncClient::Send(cbData, lpbData, lpcbSent);
	if (!fSuccess) {
		return FALSE;
	}

	DWORD dwFlags;
	fSuccess = CLpxUdpAsyncClient::GetResult(lpcbSent, TRUE, &dwFlags);
	
	return fSuccess;
}

BOOL
CLpxDatagramSender::
GetResult(
	LPDWORD lpcbTransfer,
	BOOL fWait,
	LPDWORD lpdwFlags)
{
	_ASSERTE(FALSE && "Don't call this for sync client");
	return FALSE;
}

LPX_ADDRESS
CLpxDatagramBroadcaster::
LpxBroadcastAddress()
{
	LPX_ADDRESS lpxBcastAddr = {0};
	lpxBcastAddr.Node[0] = 0xFF;
	lpxBcastAddr.Node[1] = 0xFF;
	lpxBcastAddr.Node[2] = 0xFF;
	lpxBcastAddr.Node[3] = 0xFF;
	lpxBcastAddr.Node[4] = 0xFF;
	lpxBcastAddr.Node[5] = 0xFF;
	return lpxBcastAddr;
}

CLpxDatagramBroadcaster::
CLpxDatagramBroadcaster(
	LPX_ADDRESS& localLpxAddress,
	USHORT usRemotePort) :
	CLpxDatagramSender(
		localLpxAddress, 
		LpxBroadcastAddress(), 
		usRemotePort)
{
}

CLpxDatagramBroadcaster::
~CLpxDatagramBroadcaster()
{
}

BOOL
CLpxDatagramBroadcaster::
Initialize()
{
	BOOL fSuccess = CLpxDatagramSender::Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	BOOL bOptionValue = TRUE;
	int iOptionLength = sizeof(BOOL);

	INT iResult = ::setsockopt(
		m_sock, 
		SOL_SOCKET, 
		SO_BROADCAST, 
		(char*)&bOptionValue, 
		iOptionLength);

	if (0 != iResult) {
		DPErrorEx(_FT("Enabling socket broadcast failed: "));
	}

	return (0 == iResult);
}
