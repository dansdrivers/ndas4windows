#pragma once
#include "buffer.h"
// #include "packetppt.h"
// #include "ndascmddbg.h"

inline void PrintErrorMessage(DWORD dwError = ::GetLastError())
{
	if (dwError & APPLICATION_ERROR_MASK) {
		_tprintf(TEXT("Error from an application %d (0x%08X)\n"), dwError, dwError);
		return;
	} else {
		LPTSTR lpszErrorMessage;
		BOOL fSuccess = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, dwError, 0, (LPTSTR) &lpszErrorMessage, 0, NULL);
		if (!fSuccess) {
			_tprintf(TEXT("Error from system %d (0x%08X): (not available)"), dwError, dwError);
			return;
		}
		_tprintf(TEXT("Error from system %d (0x%08X): %s"), dwError, dwError, lpszErrorMessage);
		(VOID) ::LocalFree(lpszErrorMessage);
	}
}

#define XTRACE __noop

//typedef NDAS_CMD_HEADER THeader;
//typedef THeader* PTHeader;

static LPCTSTR SERVER_PIPE_NAME = TEXT("\\\\.\\pipe\\ndas\\svccmd");

template <typename T, typename ErrorT = NDAS_CMD_ERROR>
class CNdasServiceCommandClient
{
	CBuffer<CHeapBuffer> m_lpManagedBuffer;
	// CNamedPipeTransport m_transport;
	// AutoResourceT<HANDLE> m_hPipe;
	HANDLE m_hPipe;

public:

	CNdasServiceCommandClient()
	{
		m_hPipe = INVALID_HANDLE_VALUE; 
	}

	~CNdasServiceCommandClient()
	{
		XTRACE(TEXT("CNdasServiceCommandClient::~CNdasServiceCommandClient\n"));
		if (m_hPipe != INVALID_HANDLE_VALUE) {
			// ::DisconnectNamedPipe(m_hPipe);
			::CloseHandle(m_hPipe); 
		}
	}

	BOOL Connect()
	{
		HANDLE hPipe(INVALID_HANDLE_VALUE);
		LPCTSTR szPipeName(SERVER_PIPE_NAME);

		while (TRUE) {

			hPipe = ::CreateFile(
				szPipeName,
				GENERIC_WRITE | GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (INVALID_HANDLE_VALUE == hPipe && ERROR_PIPE_BUSY != GetLastError()) {
				XTRACE(TEXT("Unable to create a named pipe (%s) with error %d(0x%08X)\n"), szPipeName, ::GetLastError(), ::GetLastError());
				return FALSE;
			}

			if (INVALID_HANDLE_VALUE != hPipe) {
				break;
			}

			BOOL fSuccess = ::WaitNamedPipe(szPipeName, NMPWAIT_USE_DEFAULT_WAIT);
			if (!fSuccess) {
				XTRACE(TEXT("Unable to open a pipe (%s) with error %d(0x%08X)\n"), szPipeName, ::GetLastError(), ::GetLastError());
				return FALSE;
			}
		}

		DWORD dwMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
		BOOL fSuccess = ::SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);
		if (!fSuccess) {
			XTRACE(TEXT("Setting named pipe handle state failed with error %d(0x%08X)\n"), ::GetLastError(), ::GetLastError());
			::CloseHandle(hPipe);
			m_hPipe = INVALID_HANDLE_VALUE;
			return FALSE;
		}

		m_hPipe = hPipe;

		return TRUE;
	}

	BOOL WriteRequest(typename T::REQUEST* lpInData, DWORD cbExtraInData = 0)
	{
		_ASSERTE(m_hPipe != INVALID_HANDLE_VALUE);

		NDAS_CMD_TYPE cmd = static_cast<NDAS_CMD_TYPE>(T::CMD);
		DWORD cbHeader = sizeof(NDAS_CMD_HEADER);

		// we always assume that the payload (structure) size is
		// more than 1 not to send dummy structure
		// as the dummy structure is 1 byte in its size (sizeof)
		DWORD cbInData = (sizeof(T::REQUEST) > 1) ? sizeof(T::REQUEST) : 0;
		cbInData += cbExtraInData;

		NDAS_CMD_HEADER header;
		::ZeroMemory(&header, sizeof(NDAS_CMD_HEADER));

		_ASSERTE(sizeof(header.Protocol) == sizeof(NDAS_CMD_PROTOCOL));
		::CopyMemory(header.Protocol, NDAS_CMD_PROTOCOL, 
			sizeof(header.Protocol) / sizeof(header.Protocol[0]));

		header.VersionMajor = NDAS_CMD_PROTOCOL_VERSION_MAJOR;
		header.VersionMinor = NDAS_CMD_PROTOCOL_VERSION_MINOR;
		header.Status = NDAS_CMD_STATUS_REQUEST;
		header.Command = cmd;
		header.MessageSize = cbHeader + cbInData;

		DWORD cbWritten(0);
		BOOL fSuccess(FALSE);

		fSuccess = ::WriteFile(m_hPipe, &header, cbHeader, &cbWritten, NULL);
		if (!fSuccess || cbWritten != cbHeader) {
			return FALSE;
		}

		if (cbInData > 0) {
			fSuccess = ::WriteFile(m_hPipe, lpInData, cbInData, &cbWritten, NULL);
			if (!fSuccess || cbWritten != cbInData) {
				return FALSE;
			}
		}
		return TRUE;
	}

	BOOL ReadReply(
		typename T::REPLY** lplpOutData, LPDWORD lpcbExtraOutData,
		typename ErrorT::REPLY** lplpErrorData, LPDWORD lpcbExtraErrorData,
		LPDWORD lpdwOperationStatus)
	{
		_ASSERTE(m_hPipe != INVALID_HANDLE_VALUE);

		_ASSERTE(NULL != lpdwOperationStatus);
		_ASSERTE(NULL != lplpOutData);
		_ASSERTE(NULL != lpcbExtraOutData);
		_ASSERTE(NULL != lplpErrorData);
		_ASSERTE(NULL != lpcbExtraErrorData);

		NDAS_CMD_HEADER header;
		DWORD cbHeader = sizeof(NDAS_CMD_HEADER);
		DWORD cbRead(0);

		DWORD cbReply = (sizeof(T::REPLY) > 1) ? sizeof(T::REPLY) : 0;
		DWORD cbErrorReply = (sizeof(ErrorT::REPLY) > 1) ? sizeof(ErrorT::REPLY) : 0;

		XTRACE(TEXT("ReadReply: To Read %d bytes\n"), cbHeader);
		BOOL fSuccess = ::ReadFile(m_hPipe, &header, cbHeader, &cbRead, NULL);
		if (!fSuccess || cbRead != cbHeader) {
			XTRACE(TEXT("ReadFile failed with error %d (0x%08X).\n"), ::GetLastError(), ::GetLastError());
			return FALSE;
		}
		XTRACE(TEXT("ReadReply: Read %d bytes\n"), cbRead);

		DWORD cbOutData = header.MessageSize - cbHeader;
		LPVOID lpBuffer = m_lpManagedBuffer.Alloc(cbOutData);
		
		if (cbOutData > 0) {
			LPBYTE lpBufferPos = reinterpret_cast<LPBYTE>(lpBuffer);
			DWORD cbToRead = cbOutData;
			do {
				XTRACE(TEXT("ReadReply: To Read %d bytes\n"), cbToRead);
				// read until we receive the enough data
				fSuccess = ::ReadFile(m_hPipe, lpBufferPos, cbToRead, &cbRead, NULL);
				if (!fSuccess) {
					DWORD dwError = ::GetLastError();
					return FALSE;
				}
				cbToRead -= cbRead;
				lpBufferPos += cbRead;
				XTRACE(TEXT("ReadReply: Read %d bytes\n"), cbRead);
			} while (cbToRead > 0);

			// DumpPacket(&header);
		}

		if (header.Status == NDAS_CMD_STATUS_SUCCESS) {
			*lpdwOperationStatus = header.Status;
			*lplpOutData = reinterpret_cast<T::REPLY*>(lpBuffer);
			*lpcbExtraOutData = cbOutData - cbReply;
			*lplpErrorData = NULL;
			*lpcbExtraErrorData = 0;
		} else {
			*lpdwOperationStatus = header.Status;
			*lplpOutData = NULL;
			*lpcbExtraOutData = 0;
			*lplpErrorData = reinterpret_cast<ErrorT::REPLY*>(lpBuffer);
			*lpcbExtraErrorData = cbOutData - cbErrorReply;
		}
		return TRUE;
	}

	BOOL Transact(
		typename T::REQUEST* lpInData, DWORD cbExtraInData,
		typename T::REPLY** lplpOutData, LPDWORD lpcbExtraOutData,
		typename ErrorT::REPLY** lplpErrorData, LPDWORD lpcbExtraErrorData,
		LPDWORD lpdwOperationStatus)
	{
		_ASSERTE(m_hPipe != INVALID_HANDLE_VALUE);

		BOOL fSuccess = WriteRequest(
			lpInData, 
			cbExtraInData);

		if (!fSuccess) {
			PrintErrorMessage();
			return FALSE;
		}

		fSuccess = ReadReply(
			lplpOutData, lpcbExtraOutData,
			lplpErrorData, lpcbExtraErrorData,
			lpdwOperationStatus);

		if (!fSuccess) {
			PrintErrorMessage();
			return FALSE;
		}

		switch (*lpdwOperationStatus) {
		case NDAS_CMD_STATUS_SUCCESS:	/* DumpPacket(*lplpOutData); */ break;
		case NDAS_CMD_STATUS_FAILED:	/* DumpPacket(*lplpErrorData); */ break;
		case NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED:
		case NDAS_CMD_STATUS_INVALID_REQUEST:
		case NDAS_CMD_STATUS_TERMINATION:
		case NDAS_CMD_STATUS_UNSUPPORTED_VERSION:
/*		default:
			_tprintf(TEXT("NDAS Command Failed with status %s (%d)\n"), 
				NdasCmdStatusString((NDAS_CMD_STATUS)*lpdwOperationStatus), 
				*lpdwOperationStatus);
*/		
			;
		}

		/* _tprintf(TEXT("Transact returns TRUE!\n")); */
		return TRUE;
	}
};

