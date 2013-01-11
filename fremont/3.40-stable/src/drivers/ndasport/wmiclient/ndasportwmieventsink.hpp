#pragma once
#include <wbemidl.h>

class CCoNdasPortWmiEventSink :
	public CComObjectRootEx<CComMultiThreadModel>,
	public IWbemObjectSink
{
	bool bDone;

public:
	BEGIN_COM_MAP(CCoNdasPortWmiEventSink)
		COM_INTERFACE_ENTRY(IWbemObjectSink)
	END_COM_MAP()

	CCoNdasPortWmiEventSink() { }
	~CCoNdasPortWmiEventSink() { bDone = true; }

	
	STDMETHOD(Indicate)( 
		LONG lObjectCount,
		IWbemClassObject __RPC_FAR *__RPC_FAR *apObjArray
		);

	STDMETHOD(SetStatus)( 
		/* [in] */ LONG lFlags,
		/* [in] */ HRESULT hResult,
		/* [in] */ BSTR strParam,
		/* [in] */ IWbemClassObject __RPC_FAR *pObjParam
		);
};

