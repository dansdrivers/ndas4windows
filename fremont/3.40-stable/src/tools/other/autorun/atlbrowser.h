/*
Filename: AtlBrowser.h
Description: A class wrapper for IE.
Date: 08/05/2005

Copyright (c) 2005 by Gilad Novik.  

License Agreement (zlib license)
-------------------------
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

* If you use this code in a product, I would appreciate a small email from.

Gilad Novik (Web: http://gilad.gsetup.com, Email: gilad@gsetup.com)
*/

#pragma once
#include <comdef.h>
#include <exdisp.h>
#include <exdispid.h>
#include <atlsafe.h>

#include <initguid.h>
#include <exdisp.h>

class CAtlWebBrowser : public CComPtr<IWebBrowser2>
{
public:
	HRESULT PutAddressBar(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_AddressBar(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetAddressBar(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_AddressBar(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT GetApplication(IDispatch** ppDispatch)
	{
		ATLASSERT(p);
		return p->get_Application(ppDispatch);
	}
	HRESULT GetBusy(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_Busy(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT GetFullName(CComBSTR& szValue) const
	{
		ATLASSERT(p);
		return p->get_FullName(&szValue);
	}
#if defined(__ATLSTR_H__)
	HRESULT GetFullName(CString& szValue) const
	{
		ATLASSERT(p);
		CComBSTR szResult;
		HRESULT hResult=p->get_FullName(&szResult);
		szValue=szResult;
		return hResult;
	}
#endif
	HRESULT GetLocationName(CComBSTR& szValue) const
	{
		ATLASSERT(p);
		return p->get_LocationName(&szValue);
	}
#if defined(__ATLSTR_H__)
	HRESULT GetLocationName(CString& szValue) const
	{
		ATLASSERT(p);
		CComBSTR szResult;
		HRESULT hResult=p->get_LocationName(&szResult);
		szValue=szResult;
		return hResult;
	}
#endif
	HRESULT GetLocationURL(CComBSTR& szValue) const
	{
		ATLASSERT(p);
		return p->get_LocationURL(&szValue);
	}
#if defined(__ATLSTR_H__)
	HRESULT GetLocationURL(CString& szValue) const
	{
		ATLASSERT(p);
		CComBSTR szResult;
		HRESULT hResult=p->get_LocationURL(&szResult);
		szValue=szResult;
		return hResult;
	}
#endif
	HRESULT PutRegisterAsBrowser(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_RegisterAsBrowser(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetRegisterAsBrowser(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_RegisterAsBrowser(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutRegisterAsDropTarget(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_RegisterAsDropTarget(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetRegisterAsDropTarget(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_RegisterAsDropTarget(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutTheaterMode(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_TheaterMode(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetTheaterMode(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_TheaterMode(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutVisible(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_Visible(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetVisible(BOOL& bVisible)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_Visible(&bResult);
		bVisible=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutMenuBar(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_MenuBar(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetMenuBar(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_MenuBar(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutToolBar(int nNewValue)
	{
		ATLASSERT(p);
		return p->put_ToolBar(nNewValue);
	}
	HRESULT GetToolBar(BOOL& bValue)
	{
		ATLASSERT(p);
		int nResult;
		HRESULT hResult=p->get_ToolBar(&nResult);
		bValue=nResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutOffline(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_Offline(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetOffline(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_Offline(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutSilent(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_Silent(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetSilent(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_Silent(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutFullScreen(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_FullScreen(bNewValue ? VARIANT_TRUE : VARIANT_FALSE);
	}
	HRESULT GetFullScreen(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_FullScreen(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutStatusBar(BOOL bNewValue)
	{
		ATLASSERT(p);
		return p->put_StatusBar(bNewValue ? VARIANT_TRUE : VARIANT_FALSE); 
	}
	HRESULT GetStatusBar(BOOL& bValue)
	{
		ATLASSERT(p);
		VARIANT_BOOL bResult;
		HRESULT hResult=p->get_StatusBar(&bResult);
		bValue=bResult ? TRUE : FALSE;
		return hResult;
	}
	HRESULT PutLeft(LONG nNewValue)
	{
		ATLASSERT(p);
		return p->put_Left(nNewValue);
	}
	HRESULT GetLeft(LONG& nValue)
	{
		ATLASSERT(p);
		return p->get_Left(&nValue);
	}
	HRESULT PutTop(LONG nNewValue)
	{
		ATLASSERT(p);
		return p->put_Top(nNewValue);
	}
	HRESULT GetTop(LONG& nValue)
	{
		ATLASSERT(p);
		HRESULT hResult=p->get_Top(&nValue);
	}
	HRESULT PutWidth(LONG nNewValue)
	{
		ATLASSERT(p);
		return p->put_Width(nNewValue);
	}
	HRESULT GetWidth(LONG& nValue)
	{
		ATLASSERT(p);
		return p->get_Width(&nValue);
	}
	HRESULT PutHeight(LONG nNewValue)
	{
		ATLASSERT(p);
		return p->put_Height(nNewValue);
	}
	HRESULT GetHeight(LONG& nValue)
	{
		ATLASSERT(p);
		return p->get_Height(&nValue);
	}
	CComPtr<IDispatch> GetDocument()
	{
		ATLASSERT(p);
		CComPtr<IDispatch> pDispatch;
		p->get_Document(&pDispatch);
		return pDispatch;
	}
	HRESULT PutProperty(LPCTSTR szProperty,const VARIANT& vtValue)
	{
		ATLASSERT(p);
		return p->PutProperty(CComBSTR(szProperty),vtValue);
	}
	CComVariant GetProperty(LPCTSTR szProperty,CComVariant& vtValue)
	{
		ATLASSERT(p);
		CComDispatchDriver pDriver(p);
		ATLASSERT(pDriver);
		return pDriver.GetPropertyByName(CT2COLE(szProperty),&vtValue);
	}

	// Methods
	void Quit()
	{
		ATLASSERT(p);
		p->Quit();
	}
	HRESULT ClientToWindow(POINT& Point)
	{
		ATLASSERT(p);
		return p->ClientToWindow((int*)&Point.x,(int*)&Point.y);
	}
	HRESULT ExecWB(OLECMDID nCmd,OLECMDEXECOPT nCmdOptions,VARIANT* pvInput=NULL,VARIANT* pvOutput=NULL)
	{
		ATLASSERT(p);
		return p->ExecWB(nCmd,nCmdOptions,pvInput,pvOutput);
	}
	HRESULT GoBack()
	{
		ATLASSERT(p);
		return p->GoBack();
	}
	HRESULT GoForward()
	{
		ATLASSERT(p);
		return p->GoForward();
	}
	HRESULT GoHome()
	{
		ATLASSERT(p);
		return p->GoHome();
	}
	HRESULT GoSearch()
	{
		ATLASSERT(p);
		return p->GoSearch();
	}
	HRESULT Navigate(LPCTSTR szURL,DWORD dwFlags=0,LPCTSTR szTargetFrameName=NULL,LPCVOID pPostData=NULL,DWORD dwPostDataLength=0,LPCTSTR szHeaders=NULL)
	{
		ATLASSERT(p);
		ATLASSERT(szURL);
		CComVariant vtTargetFrameName,vtPostData,vtHeaders;
		if (szTargetFrameName)
			vtTargetFrameName=szTargetFrameName;
		if (pPostData && dwPostDataLength>0)
		{
			CComSafeArray<BYTE> Post(dwPostDataLength);
			CopyMemory(((LPSAFEARRAY)Post)->pvData,pPostData,dwPostDataLength);
			V_VT(&vtPostData)=VT_ARRAY|VT_UI1;
			vtPostData.parray=Post.Detach();
		}
		if (szHeaders)
			vtHeaders=szHeaders;
		return p->Navigate(CComBSTR(szURL),&CComVariant((LONG)dwFlags),&vtTargetFrameName,&vtPostData,&vtHeaders);
	}
	HRESULT Navigate2(LPCTSTR szURL,DWORD dwFlags=0,LPCTSTR szTargetFrameName=NULL,LPCVOID pPostData=NULL,DWORD dwPostDataLength=0,LPCTSTR szHeaders=NULL)
	{
		ATLASSERT(p);
		ATLASSERT(szURL);
		CComVariant vtTargetFrameName,vtPostData,vtHeaders;
		if (szTargetFrameName)
			vtTargetFrameName=szTargetFrameName;
		if (pPostData && dwPostDataLength>0)
		{
			CComSafeArray<BYTE> Post(dwPostDataLength);
			CopyMemory(((LPSAFEARRAY)Post)->pvData,pPostData,dwPostDataLength);
			V_VT(&vtPostData)=VT_ARRAY|VT_UI1;
			vtPostData.parray=Post.Detach();
		}
		if (szHeaders)
			vtHeaders=szHeaders;
		return p->Navigate2(&CComVariant(szURL),&CComVariant((LONG)dwFlags),&vtTargetFrameName,&vtPostData,&vtHeaders);
	}
	HRESULT Refresh()
	{
		ATLASSERT(p);
		return p->Refresh();
	}
	HRESULT Refresh2(LONG nLevel)
	{
		ATLASSERT(p);
		return p->Refresh2(&CComVariant(nLevel));
	}
	HRESULT Stop()
	{
		ATLASSERT(p);
		return p->Stop();
	}
	OLECMDF QueryStatusWB(OLECMDID nCmd)
	{
		ATLASSERT(p);
		OLECMDF nResult;
		p->QueryStatusWB(nCmd,&nResult);
		return nResult;
	}

	HRESULT LoadFromResource(UINT nID) 
	{
		TCHAR szFilename[MAX_PATH];
		GetModuleFileName(_Module.GetModuleInstance(),szFilename,sizeof(szFilename)/sizeof(TCHAR));
#if defined(__ATLSTR_H__)
		CString szURL;
		szURL.Format(_T("res://%s/%u"),szFilename,nID);
#else
		TCHAR szURL[4096];
		_stprintf(szURL,_T("res://%s/%u"),szFilename,nID);
#endif
		return Navigate(szURL);
	}
	HRESULT LoadFromResource(LPCTSTR szID) 
	{
		TCHAR szFilename[MAX_PATH];
		GetModuleFileName(_Module.GetModuleInstance(),szFilename,sizeof(szFilename)/sizeof(TCHAR));
#if defined(__ATLSTR_H__)
		CString szURL;
		szURL.Format(_T("res://%s/%s"),szFilename,szID);
#else
		TCHAR szURL[4096];
		_stprintf(szURL,_T("res://%s/%s"),szFilename,szID);
#endif
		return Navigate(szURL);
	}
};

struct CAtlWebBrowserEventsBase
{
	static _ATL_FUNC_INFO StatusTextChangeStruct;
	static _ATL_FUNC_INFO TitleChangeStruct;
	static _ATL_FUNC_INFO PropertyChangeStruct;
	static _ATL_FUNC_INFO OnQuitStruct;
	static _ATL_FUNC_INFO OnToolBarStruct;
	static _ATL_FUNC_INFO OnMenuBarStruct;
	static _ATL_FUNC_INFO OnStatusBarStruct;
	static _ATL_FUNC_INFO OnFullScreenStruct;
	static _ATL_FUNC_INFO OnTheaterModeStruct;
	static _ATL_FUNC_INFO DownloadBeginStruct;
	static _ATL_FUNC_INFO DownloadCompleteStruct;
	static _ATL_FUNC_INFO NewWindow2Struct; 
	static _ATL_FUNC_INFO CommandStateChangeStruct;
	static _ATL_FUNC_INFO BeforeNavigate2Struct;
	static _ATL_FUNC_INFO ProgressChangeStruct;
	static _ATL_FUNC_INFO NavigateComplete2Struct;
	static _ATL_FUNC_INFO DocumentComplete2Struct;
	static _ATL_FUNC_INFO OnVisibleStruct;
	static _ATL_FUNC_INFO SetSecureLockIconStruct;
	static _ATL_FUNC_INFO NavigateErrorStruct;
	static _ATL_FUNC_INFO PrivacyImpactedStateChangeStruct;
};

template<class T, UINT nID=0>
class CAtlWebBrowserEvents : public IDispEventSimpleImpl<nID, CAtlWebBrowserEvents<T,nID>, &DIID_DWebBrowserEvents2>, private CAtlWebBrowserEventsBase
{
public:
	BEGIN_SINK_MAP(CAtlWebBrowserEvents)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_STATUSTEXTCHANGE, __StatusTextChange, &StatusTextChangeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_PROGRESSCHANGE, __ProgressChange, &ProgressChangeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, __CommandStateChange, &CommandStateChangeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN, __DownloadBegin, &DownloadBeginStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_DOWNLOADCOMPLETE, __DownloadComplete, &DownloadCompleteStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_TITLECHANGE, __TitleChange, &TitleChangeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, __NavigateComplete2, &NavigateComplete2Struct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, __BeforeNavigate2, &BeforeNavigate2Struct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_PROPERTYCHANGE, __PropertyChange, &PropertyChangeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2, __NewWindow2, &NewWindow2Struct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, __DocumentComplete, &DocumentComplete2Struct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONQUIT, __OnQuit, &OnQuitStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONVISIBLE, __OnVisible, &OnVisibleStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONTOOLBAR, __OnToolBar, &OnToolBarStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONMENUBAR, __OnMenuBar, &OnMenuBarStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONSTATUSBAR, __OnStatusBar, &OnStatusBarStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONFULLSCREEN, __OnFullScreen, &OnFullScreenStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_ONTHEATERMODE, __OnTheaterMode, &OnTheaterModeStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_SETSECURELOCKICON, __SetSecureLockIcon, &SetSecureLockIconStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR, __NavigateError, &NavigateErrorStruct)
		SINK_ENTRY_INFO(nID, DIID_DWebBrowserEvents2, DISPID_PRIVACYIMPACTEDSTATECHANGE, __PrivacyImpactedStateChange, &PrivacyImpactedStateChangeStruct)
	END_SINK_MAP()

	void OnSetSecureLockIcon(long nSecureLockIcon) { }
	BOOL OnNavigateError(IDispatch* pDisp, LPCTSTR szURL, LPCTSTR szTargetFrameName, LONG nStatusCode)
	{   // Return TRUE to cancel
		return FALSE;
	}
	void OnPrivacyImpactedStateChange(BOOL bImpacted) { }
	void OnStatusTextChange(LPCTSTR szText) { }
	void OnProgressChange(long nProgress, long nProgressMax) { }
	void OnCommandStateChange(long nCommand, BOOL bEnable) { }
	void OnDownloadBegin() { }
	void OnDownloadComplete() { }
	void OnTitleChange(LPCTSTR szTitle) { }
	void OnNavigateComplete2(IDispatch* pDisp, LPCTSTR szURL) { }
	BOOL OnBeforeNavigate2(IDispatch* pDisp, LPCTSTR szURL, DWORD dwFlags, LPCTSTR szTargetFrameName, const CSimpleArray<BYTE>& pPostedData, LPCTSTR szHeaders)
	{   // Return TRUE to cancel
		return FALSE;
	}
	void OnPropertyChange(LPCTSTR szProperty) { }
	BOOL OnNewWindow2(IDispatch** ppDisp)
	{   // Return TRUE to cancel
		return FALSE;
	}
	void OnDocumentComplete(IDispatch* pDisp, LPCTSTR szURL) { }
	void OnQuit() { }
	void OnVisible(BOOL bVisible) { }
	void OnToolBar(BOOL bToolBar) { }
	void OnMenuBar(BOOL bMenuBar) { }
	void OnStatusBar(BOOL bStatusBar) { }
	void OnFullScreen(BOOL bFullScreen) { }
	void OnTheaterMode(BOOL bTheaterMode) { }

private:
	void __stdcall __SetSecureLockIcon(long nSecureLockIcon)
	{
		T* pT = static_cast<T*>(this);
		pT->OnSetSecureLockIcon(nSecureLockIcon);
	}

	void __stdcall __NavigateError(IDispatch* pDisp, VARIANT* pvURL, VARIANT* pvTargetFrameName, VARIANT* pvStatusCode, VARIANT_BOOL* pbCancel)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(V_VT(pvURL) == VT_BSTR);
		ATLASSERT(V_VT(pvTargetFrameName) == VT_BSTR);
		ATLASSERT(V_VT(pvStatusCode) == (VT_I4));
		ATLASSERT(pbCancel != NULL);
		*pbCancel=pT->OnNavigateError(pDisp,CW2CT(V_BSTR(pvURL)),CW2CT(V_BSTR(pvTargetFrameName)),V_I4(pvStatusCode));
	}

	void __stdcall __PrivacyImpactedStateChange(VARIANT_BOOL bImpacted)
	{
		T* pT = static_cast<T*>(this);
		pT->OnPrivacyImpactedStateChange(bImpacted);
	}

	void __stdcall __StatusTextChange(BSTR szText)
	{
		T* pT = static_cast<T*>(this);
		pT->OnStatusTextChange(CW2CT(szText));
	}

	void __stdcall __ProgressChange(long nProgress, long nProgressMax)
	{
		T* pT = static_cast<T*>(this);
		pT->OnProgressChange(nProgress, nProgressMax);
	}

	void __stdcall __CommandStateChange(long nCommand, VARIANT_BOOL bEnable)
	{
		T* pT = static_cast<T*>(this);
		pT->OnCommandStateChange(nCommand, (bEnable==VARIANT_TRUE) ? TRUE : FALSE);
	}

	void __stdcall __DownloadBegin()
	{
		T* pT = static_cast<T*>(this);
		pT->OnDownloadBegin();
	}

	void __stdcall __DownloadComplete()
	{
		T* pT = static_cast<T*>(this);
		pT->OnDownloadComplete();
	}

	void __stdcall __TitleChange(BSTR szText)
	{
		T* pT = static_cast<T*>(this);
		pT->OnTitleChange(CW2CT(szText));
	}

	void __stdcall __NavigateComplete2(IDispatch* pDisp, VARIANT* pvURL)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(V_VT(pvURL) == VT_BSTR);
		pT->OnNavigateComplete2(pDisp, CW2CT(V_BSTR(pvURL)));
	}

	void __stdcall __BeforeNavigate2(IDispatch* pDisp, VARIANT* pvURL, VARIANT* pvFlags, VARIANT* pvTargetFrameName, VARIANT* pvPostData, VARIANT* pvHeaders, VARIANT_BOOL* pbCancel)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(V_VT(pvURL) == VT_BSTR);
		ATLASSERT(V_VT(pvTargetFrameName) == VT_BSTR);
		ATLASSERT(V_VT(pvPostData) == (VT_VARIANT | VT_BYREF));
		ATLASSERT(V_VT(pvHeaders) == VT_BSTR);
		ATLASSERT(pbCancel != NULL);

		VARIANT* pvtPostedData = V_VARIANTREF(pvPostData);
		CSimpleArray<BYTE> pArray;
		if (V_VT(pvtPostedData) & VT_ARRAY)
		{
			ATLASSERT(V_ARRAY(pvtPostedData)->cbElements == 1);
			CComSafeArray<BYTE> Post;
			Post.Attach(V_ARRAY(pvtPostedData));
			pArray.m_nSize=pArray.m_nAllocSize=Post.GetCount();
			pArray.m_aT=(BYTE*)(((LPSAFEARRAY)Post)->pvData);
			Post.Detach();
		}
		*pbCancel=pT->OnBeforeNavigate2(pDisp, CW2CT(V_BSTR(pvURL)), V_I4(pvFlags), CW2CT(V_BSTR(pvTargetFrameName)), pArray, CW2CT(V_BSTR(pvHeaders))) ? VARIANT_TRUE : VARIANT_FALSE;
		pArray.m_aT=NULL;
	}

	void __stdcall __PropertyChange(BSTR szProperty)
	{
		T* pT = static_cast<T*>(this);
		pT->OnPropertyChange(CW2CT(szProperty));
	}

	void __stdcall __NewWindow2(IDispatch** ppDisp, VARIANT_BOOL* pbCancel)
	{
		T* pT = static_cast<T*>(this);
		*pbCancel = pT->OnNewWindow2(ppDisp) ? VARIANT_TRUE : VARIANT_FALSE;
	}

	void __stdcall __DocumentComplete(IDispatch* pDisp, VARIANT* pvURL)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(V_VT(pvURL) == VT_BSTR);
		pT->OnDocumentComplete(pDisp, CW2CT(V_BSTR(pvURL)));
	}

	void __stdcall __OnQuit()
	{
		T* pT = static_cast<T*>(this);
		pT->OnQuit();
	}

	void __stdcall __OnVisible(VARIANT_BOOL bVisible)
	{
		T* pT = static_cast<T*>(this);
		pT->OnVisible(bVisible == VARIANT_TRUE ? TRUE : FALSE);
	}

	void __stdcall __OnToolBar(VARIANT_BOOL bToolBar)
	{
		T* pT = static_cast<T*>(this);
		pT->OnToolBar(bToolBar == VARIANT_TRUE ? TRUE : FALSE);
	}

	void __stdcall __OnMenuBar(VARIANT_BOOL bMenuBar)
	{
		T* pT = static_cast<T*>(this);
		pT->OnMenuBar(bMenuBar == VARIANT_TRUE ? TRUE : FALSE);
	}

	void __stdcall __OnStatusBar(VARIANT_BOOL bStatusBar)
	{
		T* pT = static_cast<T*>(this);
		pT->OnStatusBar(bStatusBar == VARIANT_TRUE ? TRUE : FALSE);
	}

	void __stdcall __OnFullScreen(VARIANT_BOOL bFullScreen)
	{
		T* pT = static_cast<T*>(this);
		pT->OnFullScreen(bFullScreen == VARIANT_TRUE ? TRUE : FALSE);
	}

	void __stdcall __OnTheaterMode(VARIANT_BOOL bTheaterMode)
	{
		T* pT = static_cast<T*>(this);
		pT->OnTheaterMode(bTheaterMode == VARIANT_TRUE ? TRUE : FALSE);
	}
};

__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::StatusTextChangeStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BSTR}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::TitleChangeStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BSTR}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::PropertyChangeStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BSTR}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::DownloadBeginStruct = {CC_STDCALL, VT_EMPTY, 0, {NULL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::DownloadCompleteStruct = {CC_STDCALL, VT_EMPTY, 0, {NULL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnQuitStruct = {CC_STDCALL, VT_EMPTY, 0, {NULL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::NewWindow2Struct = {CC_STDCALL, VT_EMPTY, 2, {VT_BYREF|VT_BOOL,VT_BYREF|VT_DISPATCH}}; 
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::CommandStateChangeStruct = {CC_STDCALL, VT_EMPTY, 2, {VT_I4,VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::BeforeNavigate2Struct = {CC_STDCALL, VT_EMPTY, 7, {VT_DISPATCH,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::ProgressChangeStruct = {CC_STDCALL, VT_EMPTY, 2, {VT_I4,VT_I4}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::NavigateComplete2Struct = {CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF|VT_VARIANT}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::DocumentComplete2Struct = {CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_BYREF|VT_VARIANT}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnVisibleStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnToolBarStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnMenuBarStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnStatusBarStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnFullScreenStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::OnTheaterModeStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::SetSecureLockIconStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_I4}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::NavigateErrorStruct = {CC_STDCALL, VT_EMPTY, 5, {VT_BYREF|VT_BOOL,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_VARIANT,VT_BYREF|VT_DISPATCH}};
__declspec(selectany) _ATL_FUNC_INFO CAtlWebBrowserEventsBase::PrivacyImpactedStateChangeStruct = {CC_STDCALL, VT_EMPTY, 1, {VT_BOOL}};

