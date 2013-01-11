#pragma once

class CMsiProperty
{
	LPWSTR m_buffer;
	DWORD m_bufferLength;
	WCHAR Empty[1];
public:

	CMsiProperty() : m_buffer(Empty), m_bufferLength(1)
	{
		Empty[0] = 0;
	}

	~CMsiProperty()
	{
		if (Empty != m_buffer) free(m_buffer);
	}

	operator LPCWSTR()
	{
		return m_buffer;
	}

	UINT GetPropertyW(
		__in MSIHANDLE hInstall,
		__in LPCWSTR PropName,
		__out_opt LPDWORD ValueLength)
	{
		if (ValueLength) *ValueLength = 0;
		DWORD length = m_bufferLength;
		UINT ret = MsiGetPropertyW(
			hInstall, 
			PropName, 
			m_buffer, 
			&length);
		if (ERROR_MORE_DATA == ret)
		{
			++length; // returned length excludes the terminating null
			PVOID p = (Empty == m_buffer) ? 
				malloc(length * sizeof(WCHAR)) :
				realloc(m_buffer, length * sizeof(WCHAR));
			if (NULL == p)
			{
				return ERROR_OUTOFMEMORY;
			}
			m_buffer = (LPWSTR) p;
			m_bufferLength = length;
			ret = MsiGetPropertyW(hInstall, PropName, m_buffer, &length);
		}
		if (ValueLength) *ValueLength = length;
		return ret;
	}
};

UINT
pxMsiFormatRecord(
	__in MSIHANDLE hInstall,
	__in LPCWSTR Format,
	__deref_out LPWSTR* Result,
	__out_opt LPDWORD ResultLength);

UINT
pxMsiRecordGetString(
	__in MSIHANDLE hRecord,
	__in UINT Field,
	__deref_out LPWSTR* ValueBuffer,
	__out_opt LPDWORD ValueBufferLength);

UINT 
pxMsiErrorMessageBox(
	__in MSIHANDLE hInstall, 
	__in INT ErrorDialog, 
	__in DWORD ErrorCode);

UINT 
pxMsiErrorMessageBox(
	__in MSIHANDLE hInstall, 
	__in INT ErrorDialog, 
	__in DWORD ErrorCode,
	__in LPCWSTR ErrorText);

UINT
pxMsiQueueScheduleReboot(
	__in MSIHANDLE hInstall);

UINT
pxMsiQueueForceReboot(
	__in MSIHANDLE hInstall);

BOOL
pxMsiIsScheduleRebootQueued(
	__in MSIHANDLE hInstall);

BOOL
pxMsiIsForceRebootQueued(
	__in MSIHANDLE hInstall);

HRESULT
pxMsiClearScheduleReboot();

HRESULT
pxMsiClearForceReboot();
