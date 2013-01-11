#pragma once
#ifndef _XTLEVENTTRACE_H_
#define _XTLEVENTTRACE_H_

#include "xtldef.h"
#include "xtltrace.h"
#include <wmistr.h>
#include <evntrace.h>

namespace XTL
{

// {1106BB3E-BE2D-4c68-AFB2-2BD3D47149EC}
const GUID xtlEventTracer = { 0x1106bb3e, 0xbe2d, 0x4c68, { 0xaf, 0xb2, 0x2b, 0xd3, 0xd4, 0x71, 0x49, 0xec} };

// Control GUID for this provider
// {9219E43F-D999-4387-BD8E-BFF19E7D3FE8}
const GUID xtlEventTracerControlGuid = { 0x9219e43f, 0xd999, 0x4387, { 0xbd, 0x8e, 0xbf, 0xf1, 0x9e, 0x7d, 0x3f, 0xe8 } };

// Only one transaction GUID will be registered for this provider.
// {9FF69AC6-2562-4186-8DDE-E3B420FF4A3E}
const GUID xtlEventTracerTransactionGuid = { 0x9ff69ac6, 0x2562, 0x4186, { 0x8d, 0xde, 0xe3, 0xb4, 0x20, 0xff, 0x4a, 0x3e } };


#if 0
class CXtlEventTracer : public IXDbgOutput
{
	// User Event Layout
	typedef struct _XDEBUG_EVENT {
		EVENT_TRACE_HEADER Header;
		DWORD dwLevel;
		TCHAR szMessage[1];
	} XDEBUG_EVENT, *PXDEBUG_EVENT;

	TRACEHANDLE m_hLoggerHandle;
	TRACEHANDLE m_hRegistrationHandle;

	BOOL m_bTraceOnFlag;
	TRACE_GUID_REGISTRATION m_TraceGuidReg[1];

	GUID m_ControlGuid;
	GUID m_TransactionGuid;
	ULONG m_EnableLevel;
	ULONG m_EnableFlags;

public:

	CXtlEventTracer(LPCGUID lpControlGuid, LPCGUID lpTransactionGuid) :
		m_hLoggerHandle(NULL),
		m_hRegistrationHandle(NULL),
		m_bTraceOnFlag(FALSE)
	{
		m_ControlGuid = *lpControlGuid;
		m_TransactionGuid = *lpTransactionGuid;
		m_TraceGuidReg[0].Guid = &m_TransactionGuid;
		m_TraceGuidReg[0].RegHandle = NULL;
	}

	BOOL Register()
	{
		ULONG Status = RegisterTraceGuidsW(
			(WMIDPREQUEST)sControlCallback, // callback function
			this,
			&m_ControlGuid,
			1,
			m_TraceGuidReg,
			NULL,
			NULL,
			&m_hRegistrationHandle);

		if (Status != ERROR_SUCCESS) {
			::SetLastError(Status);
			return FALSE;
		}
		return TRUE;
	}

	BOOL Unregister()
	{
		ULONG Status = UnregisterTraceGuids(m_hRegistrationHandle);
		if (Status != ERROR_SUCCESS) {
			::SetLastError(Status);
			return FALSE;
		}
		return TRUE;
	}

	/*++

	Routine Description:

	Callback function when enabled.

	Arguments:

	RequestCode - Flag for either enable or disable.
	Context - User-defined context.
	InOutBufferSize - not used.
	Buffer - WNODE_HEADER for the logger session.

	Return Value:

	Error Status. ERROR_SUCCESS if successful.

	--*/
	ULONG ControlCallback(
		IN WMIDPREQUESTCODE RequestCode,
		IN OUT ULONG *InOutBufferSize,
		IN OUT PVOID Buffer)
	{
		ULONG Status;
		ULONG RetSize;

		Status = ERROR_SUCCESS;

		switch (RequestCode)
		{
		case WMI_ENABLE_EVENTS:
			{
				RetSize = 0;
				m_hLoggerHandle = GetTraceLoggerHandle( Buffer );
				m_EnableLevel = GetTraceEnableLevel(m_hLoggerHandle);
				m_EnableFlags = GetTraceEnableFlags(m_hLoggerHandle);
				_tprintf(_T("Logging enabled to 0x%016I64x(%d,%d,%d)\n"),
					m_hLoggerHandle, RequestCode, m_EnableLevel, m_EnableFlags);
				m_bTraceOnFlag = TRUE;
				break;
			}
		case WMI_DISABLE_EVENTS:
			{
				m_bTraceOnFlag = FALSE;
				RetSize = 0;
				m_hLoggerHandle = 0;
				_tprintf(_T("\nLogging Disabled\n"));
				break;
			}
		default:
			{
				RetSize = 0;
				Status = ERROR_INVALID_PARAMETER;
				break;
			}

		}

		*InOutBufferSize = RetSize;
		return(Status);
	}

	static ULONG sControlCallback(
		IN WMIDPREQUESTCODE RequestCode,
		IN PVOID Context,
		IN OUT ULONG *InOutBufferSize,
		IN OUT PVOID Buffer)
	{
		return reinterpret_cast<XDbgEventTrace*>(Context)->
			ControlCallback(RequestCode, InOutBufferSize, Buffer);
	}

	virtual VOID OutputDebugStringEx(
		LPCTSTR lpDebugInfo,
		LPCTSTR lpOutputString, 
		DWORD cchOutputString = 0, 
		DWORD dwLevel = 0)
	{
		if (!m_hLoggerHandle) {
			return;
		}

		BYTE buffer[sizeof(XDEBUG_EVENT) + 4096];

		PXDEBUG_EVENT pXDebugEvent = 
			reinterpret_cast<PXDEBUG_EVENT>(&buffer[0]);

		DWORD ccbOutputString = 
			(4096 > (cchOutputString + 1) * sizeof(TCHAR)) ?
			(cchOutputString + 1) * sizeof(TCHAR) : 4096;

		DWORD ccbEvent = sizeof(XDEBUG_EVENT) + ccbOutputString;

		::ZeroMemory(pXDebugEvent, ccbEvent);
		pXDebugEvent->Header.Size = (USHORT)ccbEvent;
		pXDebugEvent->Header.Flags = WNODE_FLAG_TRACED_GUID;
		pXDebugEvent->Header.Guid = m_TransactionGuid;
		pXDebugEvent->Header.Class.Type = EVENT_TRACE_TYPE_INFO;
		pXDebugEvent->dwLevel = dwLevel;

		HRESULT hr = ::StringCchPrintf(
			pXDebugEvent->szMessage, 
			4096, 
			_T("%s%s"),
			lpDebugInfo,
			lpOutputString);

		XTLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);

		ULONG status = ::TraceEvent(
			m_hLoggerHandle, 
			(PEVENT_TRACE_HEADER) pXDebugEvent);

		XTLASSERT(status == ERROR_SUCCESS);
	}

	virtual VOID OutputDebugString(
		LPCTSTR lpOutputString, 
		DWORD cchOutputString = 0, 
		DWORD dwLevel = 0)
	{
		OutputDebugStringEx(_T(""), lpOutputString, cchOutputString, dwLevel);
	}
};

#endif

} // namespace XTL

#endif /* _XTLEVENTTRACE_H_ */
