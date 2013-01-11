#pragma once
#include "autorunhelper.h"
#include <mshtmhst.h>

extern HWND hWndMainFrame;

class ATL_NO_VTABLE CCoAutoRun :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CCoAutoRun, &CLSID_CCoAutoRun>,
	public IDispatchImpl<IAutoRun, &IID_IAutoRun, &LIBID_AutoRunLibrary, 0xFFFF, 0xFFFF>
{
	BEGIN_COM_MAP(CCoAutoRun)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IAutoRun)
	END_COM_MAP()

public:

	STDMETHOD(Close)();
	STDMETHOD(MessageBox)(BSTR Message, BSTR Caption, int type, int* result);
	STDMETHOD(Run)(BSTR verb, BSTR program, VARIANT param, int* ret);
	STDMETHOD(ShellExec)(BSTR verb, BSTR program, VARIANT param, int* ret);
	STDMETHOD(ReadINF)(BSTR section, BSTR key, VARIANT defaultValue, BSTR* value);
	
	STDMETHOD(get_AutoRunPath)(BSTR* path);
	STDMETHOD(get_DefaultLangID)(int* plcid);
	STDMETHOD(get_OSVersion)(int* version);
	STDMETHOD(get_OSPlatform)(int* platform);
	STDMETHOD(get_ProcessorArchitecture)(int* arch);
	
	STDMETHOD(get_Title)(BSTR* title);
	STDMETHOD(put_Title)(BSTR title);

	STDMETHOD(GetLanguageName)(__in int LocaleId, __deref_out BSTR* Name);
	STDMETHOD(GetEnglishLanguageName)(__in int LocaleId, __deref_out BSTR* Name);
	STDMETHOD(GetNativeLanguageName)(__in int LocaleId, __deref_out BSTR* Name);
};

class ATL_NO_VTABLE CCoAutoRunUIHandler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CCoAutoRunUIHandler, &CLSID_CCoAutoRun>,
	public IDispatchImpl<IDocHostUIHandlerDispatch, &IID_IDocHostUIHandlerDispatch, &LIBID_ATLLib>
{
	BEGIN_COM_MAP(CCoAutoRunUIHandler)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IDocHostUIHandlerDispatch)
	END_COM_MAP()

	// IDocHostUIHandler Methods
protected:
	STDMETHOD(ShowContextMenu)(
		/* [in] */ DWORD dwID,
		/* [in] */ DWORD x,
		/* [in] */ DWORD y,
		/* [in] */ IUnknown *pcmdtReserved,
		/* [in] */ IDispatch *pdispReserved,
		/* [retval][out] */ HRESULT *dwRetVal)
	{
		ATLTRACE(__FUNCTION__"\n");
		// S_FALSE: Show context menus
		// S_OK   : Do not show context menus
		*dwRetVal = 0;
		return S_OK;
	}
	STDMETHOD(GetHostInfo)( 
		/* [out][in] */ DWORD *pdwFlags,
		/* [out][in] */ DWORD *pdwDoubleClick) 
	{
		ATLTRACE(__FUNCTION__ "\n");
		*pdwFlags = DOCHOSTUIFLAG_DIALOG | DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_SCROLL_NO;
		*pdwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
		return S_OK;
	}
	STDMETHOD(ShowUI)( 
		/* [in] */ DWORD dwID,
		/* [in] */ IUnknown *pActiveObject,
		/* [in] */ IUnknown *pCommandTarget,
		/* [in] */ IUnknown *pFrame,
		/* [in] */ IUnknown *pDoc,
		/* [retval][out] */ HRESULT *dwRetVal) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(HideUI)( void) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(UpdateUI)( void) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(EnableModeless)( 
		/* [in] */ VARIANT_BOOL fEnable) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(OnDocWindowActivate)( 
		/* [in] */ VARIANT_BOOL fActivate) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(OnFrameWindowActivate)( 
		/* [in] */ VARIANT_BOOL fActivate) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(ResizeBorder)( 
		/* [in] */ long left,
		/* [in] */ long top,
		/* [in] */ long right,
		/* [in] */ long bottom,
		/* [in] */ IUnknown *pUIWindow,
		/* [in] */ VARIANT_BOOL fFrameWindow) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(TranslateAccelerator)( 
		/* [in] */ DWORD_PTR hWnd,
		/* [in] */ DWORD nMessage,
		/* [in] */ DWORD_PTR wParam,
		/* [in] */ DWORD_PTR lParam,
		/* [in] */ BSTR bstrGuidCmdGroup,
		/* [in] */ DWORD nCmdID,
		/* [retval][out] */ HRESULT *dwRetVal) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(GetOptionKeyPath)( 
		/* [out] */ BSTR *pbstrKey,
		/* [in] */ DWORD dw) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(GetDropTarget)( 
		/* [in] */ IUnknown *pDropTarget,
		/* [out] */ IUnknown **ppDropTarget) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	CComPtr<IDispatch> m_pExternal;

	STDMETHOD(GetExternal)( 
		/* [out] */ IDispatch **ppDispatch) 
	{
		// ATLTRACENOTIMPL(__FUNCTION__);
		//	ATLTRACENOTIMPL(__FUNCTION__);
		ATLTRACE(__FUNCTION__ "\n");
		if (m_pExternal.p == NULL)
		{
			CComPtr<IAutoRun> pAutoRun;
			CCoAutoRun::CreateInstance(&pAutoRun);
			CComQIPtr<IDispatch> pDisp(pAutoRun);
			m_pExternal = pDisp;
		}
		m_pExternal.CopyTo(ppDispatch);
		return S_OK;
	}

	STDMETHOD(TranslateUrl)( 
		/* [in] */ DWORD dwTranslate,
		/* [in] */ BSTR bstrURLIn,
		/* [out] */ BSTR *pbstrURLOut) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

	STDMETHOD(FilterDataObject)( 
		/* [in] */ IUnknown *pDO,
		/* [out] */ IUnknown **ppDORet) 
	{
		ATLTRACENOTIMPL(__FUNCTION__);
	}

};

