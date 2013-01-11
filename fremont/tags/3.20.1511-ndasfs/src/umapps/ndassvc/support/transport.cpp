#include "stdafx.h"
#include "transport.h"

#include "trace.h"
#ifdef RUN_WPP
#include "transport.tmh"
#endif

CNamedPipeTransport::
CNamedPipeTransport(
	HANDLE hExistingPipe) :
	m_hPipe(hExistingPipe)
{ 
	_ASSERTE(m_hPipe != NULL && m_hPipe != INVALID_HANDLE_VALUE);
}

BOOL 
CNamedPipeTransport::
Accept(
	LPOVERLAPPED lpOverlapped)
{
	_ASSERTE(
		(NULL == lpOverlapped) || 
		(!IsBadReadPtr(lpOverlapped, sizeof(OVERLAPPED)) &&
        NULL != lpOverlapped->hEvent &&
		INVALID_HANDLE_VALUE != lpOverlapped->hEvent));

	BOOL fConnected = ::ConnectNamedPipe(m_hPipe, lpOverlapped);

	if (fConnected) {
		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			"Accepting a new connection.\n");
		return TRUE;
	}

	if (lpOverlapped) {
		// if overlapped operation ConnectNamedPipe should return FALSE;
		DWORD dwError = ::GetLastError();
		switch (dwError) {
		case ERROR_PIPE_CONNECTED:	
			::SetEvent(lpOverlapped->hEvent);
			// omitting break is intentional
		case ERROR_IO_PENDING:
			XTLTRACE1(TRACE_LEVEL_INFORMATION, 
				"Accepting a new connection.\n");
			return TRUE;
		default: // An error occurs during the connect operation
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"ConnectNamedPipe failed, error=0x%X\n", GetLastError());
			return FALSE;
		}
	}

	return FALSE;
}

BOOL CNamedPipeTransport::Send(
	LPCVOID lpBuffer, DWORD cbToSend, LPDWORD lpcbSent, 
	LPOVERLAPPED lpOverlapped, DWORD dwFlags)
{
//	AUTOFUNCTRACE();

	_ASSERTE(!IsBadReadPtr(lpBuffer, cbToSend));
	_ASSERTE(!IsBadWritePtr(lpcbSent, sizeof(DWORD)));
	_ASSERTE(
		(NULL == lpOverlapped) || 
		(!IsBadReadPtr(lpOverlapped, sizeof(OVERLAPPED)) &&
		NULL != lpOverlapped->hEvent &&
		INVALID_HANDLE_VALUE != lpOverlapped->hEvent));

	UNREFERENCED_PARAMETER(dwFlags);

	// handling sending zero-byte packet
	if (cbToSend == 0) {
		*lpcbSent = 0;
		if (lpOverlapped) {
			BOOL fSuccess = ::SetEvent(lpOverlapped->hEvent);
			_ASSERT(fSuccess);
		}
		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			"Sending 0 byte ignored\n");
		return TRUE;
	}

	XTLTRACE1(TRACE_LEVEL_VERBOSE,
		"Sending %d bytes\n", cbToSend);

	BOOL fSuccess(FALSE);
	DWORD cbCurSent(0), cbCurToSend(cbToSend);
	const BYTE* lpCurBuffer = reinterpret_cast<const BYTE*>(lpBuffer);
	*lpcbSent = 0;

	while (cbCurToSend > 0) {

		fSuccess = ::WriteFile(m_hPipe, lpBuffer, cbCurToSend, &cbCurSent, lpOverlapped);
		if (!fSuccess && NULL != lpOverlapped && ::GetLastError() != ERROR_IO_PENDING) {
			break;
		}

		if (lpOverlapped) {
			// wait until timeout (to prevent indefinite waiting...)
			DWORD dwWaitResult = ::WaitForSingleObject(lpOverlapped->hEvent, TRANSMIT_TIMEOUT);
			if (dwWaitResult != WAIT_OBJECT_0) {
				switch (dwWaitResult) {
				case WAIT_TIMEOUT:		::SetLastError(WAIT_TIMEOUT); break;
				case WAIT_ABANDONED:	::SetLastError(WAIT_ABANDONED); break;
				default:	break;
				}
				return FALSE;
			}
			fSuccess = ::GetOverlappedResult(m_hPipe, lpOverlapped, &cbCurSent, TRUE);
			if (!fSuccess && ::GetLastError() != ERROR_IO_PENDING) {
				break;
			}
		}

		cbCurToSend -= cbCurSent;
		*lpcbSent += cbCurSent;
		lpCurBuffer += cbCurSent;

	}

	XTLTRACE1(TRACE_LEVEL_VERBOSE,
		"Sent %d bytes\n", *lpcbSent);

	return fSuccess;
}

BOOL CNamedPipeTransport::Receive(
	LPVOID lpBuffer, DWORD cbToReceive, LPDWORD lpcbReceived,
	LPOVERLAPPED lpOverlapped, LPDWORD lpdwFlags)
{
	// AUTOFUNCTRACE();

	_ASSERTE(!IsBadWritePtr(lpBuffer, cbToReceive));
	_ASSERTE(!IsBadWritePtr(lpcbReceived, sizeof(DWORD)));
	_ASSERTE(NULL == lpOverlapped ||
		!IsBadWritePtr(lpOverlapped, sizeof(OVERLAPPED)) &&
		NULL != lpOverlapped->hEvent &&
		INVALID_HANDLE_VALUE != lpOverlapped->hEvent);
		
	UNREFERENCED_PARAMETER(lpdwFlags);

	// handling sending zero-byte packet
	if (cbToReceive == 0) {
		*lpcbReceived = 0;
		if (lpOverlapped) {
			BOOL fSuccess = ::SetEvent(lpOverlapped->hEvent);
			_ASSERT(fSuccess);
		}
		return TRUE;
	}

	// we have to receive by the request size

	XTLTRACE1(TRACE_LEVEL_VERBOSE,
		"Reading %d bytes\n", cbToReceive);

	BOOL fSuccess(FALSE);
	DWORD cbCurReceived(0), cbCurToReceive(cbToReceive);
	LPBYTE lpCurBuffer = reinterpret_cast<LPBYTE>(lpBuffer);
	*lpcbReceived = 0;

	while (cbCurToReceive > 0) {

		fSuccess = ::ReadFile(m_hPipe, lpBuffer, cbCurToReceive, &cbCurReceived, lpOverlapped);
		if (!fSuccess && NULL != lpOverlapped && ::GetLastError() != ERROR_IO_PENDING) {
			break;
		}

		if (lpOverlapped) {
			// wait until timeout (to prevent indefinite waiting...)
			DWORD dwWaitResult = ::WaitForSingleObject(lpOverlapped->hEvent, TRANSMIT_TIMEOUT);
			if (dwWaitResult != WAIT_OBJECT_0) {
				switch (dwWaitResult) {
				case WAIT_TIMEOUT:		::SetLastError(WAIT_TIMEOUT); break;
				case WAIT_ABANDONED:	::SetLastError(WAIT_ABANDONED); break;
				default:	break;
				}
				return FALSE;
			}
			fSuccess = ::GetOverlappedResult(m_hPipe, lpOverlapped, &cbCurReceived, TRUE);
			if (!fSuccess && ::GetLastError() != ERROR_IO_PENDING) {
				break;
			}
		}

		cbCurToReceive -= cbCurReceived;
		*lpcbReceived += cbCurReceived;
		lpCurBuffer += cbCurReceived;

	}

	XTLTRACE1(TRACE_LEVEL_VERBOSE,
		"Read %d bytes\n", *lpcbReceived);

	return fSuccess;
}

CStreamSocketTransport::CStreamSocketTransport(SOCKET sock) :
	m_sock(sock)
{ }

BOOL CStreamSocketTransport::Send(
	LPCVOID lpBuffer, DWORD cbToSend, LPDWORD lpcbSent, 
	LPOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	if (lpOverlapped) {
		BOOL fSuccess = ::ResetEvent(lpOverlapped->hEvent);
		_ASSERTE(fSuccess);
	}

	WSABUF wsaBuffer;
	wsaBuffer.buf = const_cast<char*>(reinterpret_cast<const char*>(lpBuffer));
	wsaBuffer.len = cbToSend;
	int iResult = ::WSASend(m_sock, &wsaBuffer, 1, lpcbSent, dwFlags, lpOverlapped, NULL);
	if (0 == iResult) {
		return TRUE;
	}
	if (lpOverlapped && iResult == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
		_ASSERTE(lpOverlapped->hEvent != INVALID_HANDLE_VALUE);
		return ::WSAGetOverlappedResult(m_sock, lpOverlapped, lpcbSent, TRUE, &dwFlags);
	}
	return FALSE;
}

BOOL CStreamSocketTransport::Receive(
	LPVOID lpBuffer, DWORD cbToReceive, LPDWORD lpcbReceived, 
	LPOVERLAPPED lpOverlapped, LPDWORD lpdwFlags)
{
	if (lpOverlapped) {
		BOOL fSuccess = ::ResetEvent(lpOverlapped->hEvent);
		_ASSERTE(fSuccess);
	}

	WSABUF wsaBuffer;
	wsaBuffer.buf = reinterpret_cast<char*>(lpBuffer);
	wsaBuffer.len = cbToReceive;
	int iResult = ::WSARecv(m_sock, &wsaBuffer, 1, lpcbReceived, lpdwFlags, lpOverlapped, NULL);
	if (0 == iResult) {
		return TRUE;
	}
	if (lpOverlapped && iResult == SOCKET_ERROR && ::WSAGetLastError() == WSA_IO_PENDING) {
		_ASSERTE(lpOverlapped->hEvent != INVALID_HANDLE_VALUE);
		return ::WSAGetOverlappedResult(m_sock, lpOverlapped, lpcbReceived, TRUE, lpdwFlags);
	}
	return FALSE;
}

