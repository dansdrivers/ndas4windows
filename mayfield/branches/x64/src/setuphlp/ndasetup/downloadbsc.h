#pragma once

struct ISetupUI;

class CDownloadBindStatusCallback : 
	public IBindStatusCallback
{
public: // IUnknown implemented virtual functions
	STDMETHODIMP QueryInterface(const IID& riid, void** ppvObj);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
public: // IBindStatusCallback implemented virtual functions
	CDownloadBindStatusCallback(ISetupUI* pSetupUI);
	~CDownloadBindStatusCallback();

	STDMETHODIMP OnStartBinding(DWORD, IBinding*) {return S_OK;}
	STDMETHODIMP GetPriority(LONG*) {return S_OK;}
	STDMETHODIMP OnLowResource(DWORD ) {return S_OK;}
	STDMETHODIMP OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText);
	STDMETHODIMP OnStopBinding(HRESULT, LPCWSTR ) {return S_OK;}
	STDMETHODIMP GetBindInfo(DWORD*, BINDINFO*) {return S_OK;}
	STDMETHODIMP OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) {return S_OK;}
	STDMETHODIMP OnObjectAvailable(REFIID, IUnknown*) {return S_OK;}
private:
	ISetupUI* m_pSetupUI; // pointer to actual UI
	int       m_iRefCnt;
	ULONG     m_ulProgressSoFar;
};

