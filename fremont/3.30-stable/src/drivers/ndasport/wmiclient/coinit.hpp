#pragma once

class CCoInit
{
	BOOL m_init;
public:
	CCoInit() : m_init(FALSE) {}
	~CCoInit()
	{
		if (m_init) Uninitialize();
	}
	HRESULT Initialize(DWORD Flags)
	{
		ATLASSERT(!m_init);
		HRESULT hr = CoInitializeEx(NULL, Flags);
		if (SUCCEEDED(hr)) m_init = TRUE;
		return hr;
	}
	void Uninitialize()
	{
		CoUninitialize();
	}
};
