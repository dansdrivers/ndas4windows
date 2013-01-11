#pragma once

class ATL_NO_VTABLE CAutoPlayConfig : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CAutoPlayConfig, &CLSID_CAutoPlayConfig>,
	public IAutoPlayConfig
{
public:

	DECLARE_REGISTRY_RESOURCEID(IDR_AUTOPLAYCONTROL)

	BEGIN_COM_MAP(CAutoPlayConfig)
		COM_INTERFACE_ENTRY(IAutoPlayConfig)
	END_COM_MAP()

	STDMETHOD(SetNoDriveTypeAutoRun)(
		__in ULONG_PTR RootKey, 
		__in DWORD Mask, 
		__in DWORD Value);

	STDMETHOD(GetNoDriveTypeAutoRun)(
		__in ULONG_PTR RootKey, 
		__out DWORD* Value);

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}
};

OBJECT_ENTRY_AUTO(CLSID_CAutoPlayConfig, CAutoPlayConfig)

