/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "lpxcomm.h"
#include "autores.h"
#include "ndasdevhb.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASDEVHB
#include "xdebug.h"

CNdasDeviceHeartbeatListener::
CNdasDeviceHeartbeatListener(
	USHORT usListenPort, 
	DWORD dwWaitTimeout) :
	m_usListenPort(usListenPort),
	m_dwTimeout(dwWaitTimeout),
	m_hResetBindEvent(NULL)
{

	::InitializeCriticalSection(&m_crs);

	::ZeroMemory(
		&m_lastHeartbeatData,
		sizeof(NDAS_DEVICE_HEARTBEAT_DATA));

	::ZeroMemory(
		m_hDataEvents,
		sizeof(HANDLE) * MAX_SOCKETLPX_INTERFACE);
}

CNdasDeviceHeartbeatListener::
~CNdasDeviceHeartbeatListener()
{

	if (NULL != m_hResetBindEvent) {
		::CloseHandle(m_hResetBindEvent);
	}

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL != m_hDataEvents[i]) {
			::CloseHandle(m_hDataEvents[i]);
		}
	}

	::DeleteCriticalSection(&m_crs);
}

BOOL
CNdasDeviceHeartbeatListener::
GetHeartbeatData(PNDAS_DEVICE_HEARTBEAT_DATA pData)
{
	::EnterCriticalSection(&m_crs);
	::CopyMemory(pData, &m_lastHeartbeatData, sizeof(NDAS_DEVICE_HEARTBEAT_DATA));
	::LeaveCriticalSection(&m_crs);

	return TRUE;
}

BOOL 
CNdasDeviceHeartbeatListener::
Initialize()
{
	if (NULL == m_hResetBindEvent) {
		m_hResetBindEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	if (NULL == m_hResetBindEvent) {
		DPErrorEx(_FT("Creating an reset bind event failed.\n"));
		return FALSE;
	}

	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		if (NULL == m_hDataEvents[i]) {
			m_hDataEvents[i] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		}

		if (NULL == m_hDataEvents) {
			DPErrorEx(_FT("Creating data event[%d] failed: "), i);
			return FALSE;
		}
	}

	DWORD fSuccess = CTask::Initialize();

	return fSuccess;
}

BOOL 
CNdasDeviceHeartbeatListener::
ValidatePacketData(
	DWORD cbData,
	LPCVOID lpData)
{
	if (sizeof(PAYLOAD) != cbData) {
		DPWarning(
			_FT("Invalid packet data - size mismatch: ")
			_T("Size %d, should be %d.\n"), 
			cbData, sizeof(PAYLOAD));
		return FALSE;
	}

	PPAYLOAD lpmsg = (PPAYLOAD) lpData;

	//
	// version check: ucType == 0 and ucVersion = {0,1}
	//
	if (!(lpmsg->ucType == 0 && 
		(lpmsg->ucVersion == 0 || lpmsg->ucVersion == 1 || lpmsg->ucVersion == 2)))
	{
		DPWarning(
			_FT("Invalid packet data - version or type mismatch:")
			_T("Type %d, Version %d.\n"), 
			lpmsg->ucType, lpmsg->ucVersion);
		return FALSE;
	}

	DPNoise(_FT("Valid packet data received: ")
		_T("Type %d, Version %d\n"), 
		lpmsg->ucType, lpmsg->ucVersion);
	return TRUE;
}

BOOL 
CNdasDeviceHeartbeatListener::
ResetLocalAddressList()
{
	DWORD cbBuffer(0);

	//
	// Get the buffer size
	//

	BOOL fSuccess = GetLocalLpxAddressList(
		0,
		NULL,
		&cbBuffer);

	if (!fSuccess && WSAEFAULT != ::WSAGetLastError()) {
		DPErrorExWsa(_FT("Getting local LPX address list size failed: "));
		return FALSE;
	}

	LPSOCKET_ADDRESS_LIST lpSockAddrList = (LPSOCKET_ADDRESS_LIST)
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbBuffer);

	if (NULL == lpSockAddrList) {
		DPErrorEx(_FT("Memory allocation failed: "));
		return FALSE;
	}

	//
	// Get the actual list
	//

	DWORD cbReturned;
	fSuccess = GetLocalLpxAddressList(
		cbBuffer, 
		lpSockAddrList, 
		&cbReturned);

	if (!fSuccess) {
		// TODO: Event Log Error Here
		DPError(_FT("GetLocalLpxAddressList failed!\n"));
		::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
		return FALSE;
	}

	//
	// Issue a warning if no interfaces are bound to LPX
	//

	if (0 == lpSockAddrList->iAddressCount) {
		// TODO: Event Log for WARNING for "No interfaces are bound to LPX"
		DPWarning(_FT("No interfaces are bound to LPX!\n"));
	}

	//
	// Now reset m_vLocalLpxAddress
	//

	m_vLocalLpxAddress.clear();
	for (DWORD i = 0; i < lpSockAddrList->iAddressCount; i++) {
		m_vLocalLpxAddress.push_back(
			((PSOCKADDR_LPX)lpSockAddrList->Address[i].lpSockaddr)->LpxAddress);
	}

	::HeapFree(::GetProcessHeap(), 0, lpSockAddrList);
	return TRUE;
}

BOOL 
CNdasDeviceHeartbeatListener::
ProcessPacket(
	PCLpxUdpAsyncListener pListener, 
	PPAYLOAD pPayload)
{
	//
	// Call GetReceivedData to get the result from a buffer
	// pConnectionData is a buffer
	//

	_ASSERTE(!IsBadReadPtr(pListener, sizeof(CLpxUdpAsyncListener)));
	_ASSERTE(!IsBadReadPtr(pPayload, sizeof(PAYLOAD)));

	DWORD cbReceived(0);
	DWORD fSuccess = pListener->GetReceivedData(&cbReceived);
	if (!fSuccess) {
		DPErrorExWsa(_FT("GetReceivedData failed: "));
		return FALSE;
	}

	//
	// Now pPayload has the data
	//


	//
	// DEBUGGING ONLY
	//
#if 0
	{
		static BOOL bShowMessage = FALSE;
		static DWORD dwIntervalStartTick = 0;
		// Show the statistics only for 10 second in a minutes

		// we can just safely ignore wrap around time - 49.7 days
		// this is not a high-resolution work
		DWORD dwCurrentTick = GetTickCount();

		if (bShowMessage) {
			if ((dwCurrentTick - dwIntervalStartTick) < 5 * MilliSecondScale::SECOND) {
				bShowMessage = TRUE;
			} else {
				bShowMessage = FALSE;
				DPInfo(_FT("Stop updating...\n"));
			}
		} else {
			if (dwCurrentTick - dwIntervalStartTick > 1 * MilliSecondScale::MIN) {
				dwIntervalStartTick  = dwCurrentTick;
				bShowMessage = TRUE;
				DPInfo(_FT("Start updating...\n"));
			}
		}

		if (bShowMessage) {
			CLpxAddress localLpxAddress(&pConnectionData->localSockAddr.LpxAddress);
			CLpxAddress remoteLpxAddress(&pConnectionData->remoteSockAddr.LpxAddress);

			DPInfo(_FT("Received a packet at %s (local) from %s (remote)\n"),
				(LPCTSTR)localLpxAddress, (LPCTSTR)remoteLpxAddress);
		}
	}
#endif
	//
	// DEBUGGING ONLY
	//

	fSuccess = ValidatePacketData(cbReceived, (LPCVOID) pPayload);

	if (!fSuccess) {
		DPWarning(_FT("Invalid packet received!\n"));
		return FALSE;
	}

	//
	// Notify to observers
	//

	// subject data update
	// TODO: Synchronization?
	m_lastHeartbeatData.localAddr = pListener->GetLocalAddress();
	m_lastHeartbeatData.remoteAddr = pListener->GetRemoteAddress();
	m_lastHeartbeatData.ucType = pPayload->ucType;
	m_lastHeartbeatData.ucVersion = pPayload->ucVersion;

	Notify();

	return TRUE;
}

DWORD
CNdasDeviceHeartbeatListener::
OnTaskStart()
{
	BOOL fSuccess;

	HANDLE hEvents[2 + MAX_SOCKETLPX_INTERFACE];
	PCLpxUdpAsyncListener ppListener[MAX_SOCKETLPX_INTERFACE];
	PAYLOAD pPayload[MAX_SOCKETLPX_INTERFACE];

	hEvents[0] = m_hTaskTerminateEvent;
	hEvents[1] = m_hResetBindEvent;
	for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE; ++i) {
		hEvents[i + 2] = m_hDataEvents[i];
	}

	BOOL bTerminate(FALSE);

	while (!bTerminate) {

		CLpxAddressListChangeNotifier alcn(m_hResetBindEvent);

		BOOL fSuccess = alcn.Reset();
		if (!fSuccess) {
			DPErrorEx(_FT("Reset LpxAddressListChangeNotifier Event failed: "));
		}

		//
		// Reset m_vLocalLpxAddressList
		//
		ResetLocalAddressList();

		//
		// Now we got new local LPX address lists
		//
		std::vector<LPX_ADDRESS>::const_iterator itr = 
			m_vLocalLpxAddress.begin();

		DWORD dwBinded(0);

		for (DWORD i = 0; i < MAX_SOCKETLPX_INTERFACE && 
			itr != m_vLocalLpxAddress.end(); 
			++itr, ++i)
		{
			PCLpxUdpAsyncListener pListener = 
				new CLpxUdpAsyncListener(static_cast<LPX_ADDRESS>(*itr), m_usListenPort);
			_ASSERTE(NULL != pListener);

			BOOL fSuccess = pListener->Initialize();
			if (!fSuccess) {
				DPWarningExWsa(_FT("Listener initialization failed at %s: "),
					CLpxAddress(*itr).ToString());
				continue;
			}
			fSuccess = pListener->Bind();
			if (!fSuccess) {
				DPWarningExWsa(_FT("Listener bind failed on %s: "), 
					CLpxAddress(*itr).ToString());
				continue;
			}
			ppListener[dwBinded] = pListener;
			++dwBinded;

			DPInfo(_FT("LPX bound to %s\n"), 
				CLpxAddress(*itr).ToString());
		}

		for (DWORD i = 0; i < dwBinded; ++i) {
			DWORD cbReceived(0);
			BOOL fSuccess = ppListener[i]->StartReceive(
				m_hDataEvents[i], 
				sizeof(PAYLOAD), 
				&pPayload[i], 
				&cbReceived);
		}

		BOOL bResetBind(FALSE);
		while (!bResetBind && !bTerminate) {

			DWORD dwWaitResult = ::WSAWaitForMultipleEvents(
				2 + MAX_SOCKETLPX_INTERFACE,
				hEvents,
				FALSE,
				m_dwTimeout,
				FALSE);

			if (WSA_WAIT_FAILED == dwWaitResult) {
			} else if (WSA_WAIT_TIMEOUT == dwWaitResult) {
			} else if (WAIT_OBJECT_0 == dwWaitResult) {
				// Terminate Event
				bTerminate = TRUE;
				continue;
			} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
				// Address List Change Event
				bResetBind = TRUE;
				DPInfo(_FT("LPX Address List Change Event Issued.\n"));
				continue;
			} else if (dwWaitResult >= WAIT_OBJECT_0 + 2 &&
				dwWaitResult < WAIT_OBJECT_0 + 2 + MAX_SOCKETLPX_INTERFACE)
			{
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 2);

				(VOID) ProcessPacket(ppListener[n], &pPayload[n]);

				DWORD cbReceived(0);
				BOOL fSuccess = ppListener[n]->StartReceive(
					m_hDataEvents[n], 
					sizeof(PAYLOAD), 
					&pPayload[n], 
					&cbReceived);

			} else {
				DPWarningEx(_FT("Wait failed: "));
			}
		} /* Reset Bind */

		for (DWORD i = 0; i < dwBinded; ++i) {
			_ASSERTE(!IsBadReadPtr(ppListener[i],sizeof(CLpxUdpAsyncListener)));
			ppListener[i]->Cleanup();
			delete ppListener[i];

			BOOL fSuccess = ::ResetEvent(m_hDataEvents[i]);
			_ASSERT(fSuccess);
		}
	}

	return 0;
}

