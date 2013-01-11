#ifndef _XGUID_H_
#define _XGUID_H_
#pragma once

namespace ximeta {

	// Guid Wrapper
	class CGuid
	{
		GUID m_guid;
		TCHAR m_szBuffer[39];

	public:
		CGuid(const GUID& guid)
		{
			m_guid = guid;
			m_szBuffer[0] = NULL;
		}
		CGuid(LPCGUID lpGuid)
		{
			::CopyMemory(&m_guid, lpGuid, sizeof(GUID));
			m_szBuffer[0] = NULL;
		}

		operator LPCTSTR()
		{
			return ToString();
		}

		LPCTSTR ToString()
		{
			if (NULL == m_szBuffer[0]) {
				// {a4f3cd16-e134-4d13-bafe-3e22abd4fcd8} : 38 chars + null
				HRESULT hr = ::StringCchPrintf(m_szBuffer, 39,
					TEXT("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"),
					m_guid.Data1, m_guid.Data2, m_guid.Data3,
					m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
					m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7]);
				_ASSERT(SUCCEEDED(hr));
			}
			return m_szBuffer;
		}
	};

}

#endif