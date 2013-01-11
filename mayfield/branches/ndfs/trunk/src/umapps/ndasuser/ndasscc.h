#pragma once
#include "buffer.h"
#include <xtl/xtltrace.h>
#include <xtl/xtlautores.h>

namespace
{
	inline 
	void 
	TraceErrorMessageA(DWORD dwError = ::GetLastError())
	{
		if (dwError & APPLICATION_ERROR_MASK) 
		{
			XTLTRACE("Error from an application %d (0x%08X)\n", dwError, dwError);
			return;
		} 
		else 
		{
			LPSTR lpszErrorMessage = NULL;
			DWORD nChars = ::FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, dwError, 0, (LPSTR) &lpszErrorMessage, 0, NULL);
			if (nChars > 0)
			{
				XTLTRACE("Error from system %d (0x%08X): (not available)", dwError, dwError);
				return;
			}
			XTLTRACE("Error from system %d (0x%08X): %s", dwError, dwError, lpszErrorMessage);
			(void) ::LocalFree(lpszErrorMessage);
		}
	}

	LPCSTR NdasCmdStatusString(NDAS_CMD_STATUS status)
	{
		switch (status)
		{
		case NDAS_CMD_STATUS_REQUEST: return "REQUEST";
		case NDAS_CMD_STATUS_SUCCESS: return "SUCCESS";
		case NDAS_CMD_STATUS_FAILED: return "FAILED";
		case NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED: return "ERROR_NOT_IMPLEMENTED";
		case NDAS_CMD_STATUS_INVALID_REQUEST: return "INVALID_REQUEST";
		case NDAS_CMD_STATUS_TERMINATION: return "TERMINATION";
		case NDAS_CMD_STATUS_UNSUPPORTED_VERSION: return "UNSUPPORTED_VERSION";
		default: return "UNKNOWN";
		}
	}
}

static LPCTSTR SERVER_PIPE_NAME = TEXT("\\\\.\\pipe\\ndas\\svccmd");

template <typename T, typename ErrorT = NDAS_CMD_ERROR>
class CNdasServiceCommandClient
{
	CBuffer<CHeapBuffer> m_lpManagedBuffer;
	XTL::AutoFileHandle m_hPipe;

public:

	CNdasServiceCommandClient()
	{
	}

	~CNdasServiceCommandClient()
	{
	}

	BOOL Connect()
	{
		XTL::AutoFileHandle hPipe;
		const LPCTSTR szPipeName = SERVER_PIPE_NAME;

		while (TRUE) 
		{
			hPipe = ::CreateFile(
				szPipeName,
				GENERIC_WRITE | GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (hPipe.IsInvalid() && ERROR_PIPE_BUSY != GetLastError()) 
			{
				XTLTRACE("Unable to create a named pipe (%s).\n", szPipeName);
				return FALSE;
			}

			if (!hPipe.IsInvalid()) 
			{
				break;
			}

			BOOL fSuccess = ::WaitNamedPipe(szPipeName, NMPWAIT_USE_DEFAULT_WAIT);
			if (!fSuccess) 
			{
				XTLTRACE("Unable to open a pipe (%s)\n", szPipeName);
				return FALSE;
			}
		}

		DWORD dwMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
		BOOL fSuccess = ::SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);
		if (!fSuccess) 
		{
			XTLTRACE("Setting named pipe handle state failed.\n");
			return FALSE;
		}

		m_hPipe = hPipe.Detach();

		return TRUE;
	}

	BOOL WriteRequest(typename T::REQUEST* lpInData, DWORD cbExtraInData = 0)
	{
		XTLASSERT(!m_hPipe.IsInvalid());

		NDAS_CMD_TYPE cmd = static_cast<NDAS_CMD_TYPE>(T::CMD);
		DWORD cbHeader = sizeof(NDAS_CMD_HEADER);

		// we always assume that the payload (structure) size is
		// more than 1 not to send dummy structure
		// as the dummy structure is 1 byte in its size (sizeof)
		DWORD cbInData = (sizeof(T::REQUEST) > 1) ? sizeof(T::REQUEST) : 0;
		cbInData += cbExtraInData;

		NDAS_CMD_HEADER header;
		::ZeroMemory(&header, sizeof(NDAS_CMD_HEADER));

		XTLASSERT(sizeof(header.Protocol) == sizeof(NDAS_CMD_PROTOCOL));
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
		if (!fSuccess || cbWritten != cbHeader) 
		{
			return FALSE;
		}

		if (cbInData > 0) 
		{
			fSuccess = ::WriteFile(m_hPipe, lpInData, cbInData, &cbWritten, NULL);
			if (!fSuccess || cbWritten != cbInData) 
			{
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
		XTLASSERT(!m_hPipe.IsInvalid());

		XTLASSERT(NULL != lpdwOperationStatus);
		XTLASSERT(NULL != lplpOutData);
		XTLASSERT(NULL != lpcbExtraOutData);
		XTLASSERT(NULL != lplpErrorData);
		XTLASSERT(NULL != lpcbExtraErrorData);

		NDAS_CMD_HEADER header;
		DWORD cbHeader = sizeof(NDAS_CMD_HEADER);
		DWORD cbRead(0);

		DWORD cbReply = (sizeof(T::REPLY) > 1) ? sizeof(T::REPLY) : 0;
		DWORD cbErrorReply = (sizeof(ErrorT::REPLY) > 1) ? sizeof(ErrorT::REPLY) : 0;

		XTLTRACE("ReadReply: To Read %d bytes\n", cbHeader);
		BOOL fSuccess = ::ReadFile(m_hPipe, &header, cbHeader, &cbRead, NULL);
		if (!fSuccess || cbRead != cbHeader) 
		{
			XTLTRACE_ERR("ReadFile failed.\n");
			return FALSE;
		}
		XTLTRACE("ReadReply: Read %d bytes\n", cbRead);

		DWORD cbOutData = header.MessageSize - cbHeader;
		LPVOID lpBuffer = m_lpManagedBuffer.Alloc(cbOutData);
		
		if (cbOutData > 0) 
		{
			LPBYTE lpBufferPos = reinterpret_cast<LPBYTE>(lpBuffer);
			DWORD cbToRead = cbOutData;
			do
			{
				XTLTRACE("ReadReply: To Read %d bytes\n", cbToRead);
				// read until we receive the enough data
				fSuccess = ::ReadFile(m_hPipe, lpBufferPos, cbToRead, &cbRead, NULL);
				if (!fSuccess) 
				{
					XTLTRACE_ERR("ReadFile failed.\n");
					return FALSE;
				}
				cbToRead -= cbRead;
				lpBufferPos += cbRead;
				XTLTRACE("ReadReply: Read %d bytes\n", cbRead);
			} while (cbToRead > 0);

			// DumpPacket(&header);
		}

		if (header.Status == NDAS_CMD_STATUS_SUCCESS) 
		{
			*lpdwOperationStatus = header.Status;
			*lplpOutData = reinterpret_cast<T::REPLY*>(lpBuffer);
			*lpcbExtraOutData = cbOutData - cbReply;
			*lplpErrorData = NULL;
			*lpcbExtraErrorData = 0;
		}
		else 
		{
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
		XTLASSERT(!m_hPipe.IsInvalid());

		BOOL fSuccess = WriteRequest(
			lpInData, 
			cbExtraInData);

		if (!fSuccess) 
		{
			TraceErrorMessageA();
			return FALSE;
		}

		fSuccess = ReadReply(
			lplpOutData, lpcbExtraOutData,
			lplpErrorData, lpcbExtraErrorData,
			lpdwOperationStatus);

		if (!fSuccess) 
		{
			TraceErrorMessageA();
			return FALSE;
		}

		switch (*lpdwOperationStatus) 
		{
		case NDAS_CMD_STATUS_SUCCESS:
			break;
		case NDAS_CMD_STATUS_FAILED:
			break;
		case NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED:
		case NDAS_CMD_STATUS_INVALID_REQUEST:
		case NDAS_CMD_STATUS_TERMINATION:
		case NDAS_CMD_STATUS_UNSUPPORTED_VERSION:
		default:
			XTLTRACE("NDAS Command Failed with status %s (%d).\n", 
				NdasCmdStatusString((NDAS_CMD_STATUS)*lpdwOperationStatus), 
				*lpdwOperationStatus);
		}

		return TRUE;
	}
};
