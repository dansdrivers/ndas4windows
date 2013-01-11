// AutoRunView.cpp : implementation of the CAutoRunView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "AutoRunCom.h"
#include "AutoRunView.h"

BOOL CAutoRunView::PreTranslateMessage(MSG* pMsg)
{
	if((pMsg->message < WM_KEYFIRST || pMsg->message > WM_KEYLAST) &&
	   (pMsg->message < WM_MOUSEFIRST || pMsg->message > WM_MOUSELAST))
		return FALSE;

	// give HTML page a chance to translate this message
	return (BOOL)SendMessage(WM_FORWARDMSG, 0, (LPARAM)pMsg);
}
#include <strsafe.h>

LRESULT CAutoRunView::OnCreate(LPCREATESTRUCT lpcs)
{
	ATLTRACE(">>>>> " __FUNCTION__ "\n");

	HRESULT hr;
	
	ATLENSURE_SUCCEEDED( hr = CCoAutoRun::CreateInstance(&m_pAutoRun) );
	ATLENSURE_SUCCEEDED( hr = CreateControl(140) );

	CComPtr<IUnknown> spUnk;
	AtlAxGetHost(m_hWnd, &spUnk);
	CComQIPtr<IAxWinAmbientDispatch> spWinAmb(spUnk);
	spWinAmb->put_AllowContextMenu(VARIANT_FALSE);

	ATLENSURE_SUCCEEDED( hr = QueryControl(IID_IWebBrowser2, (void**)&m_pBrowser) );
	ATLENSURE_SUCCEEDED( hr = CCoAutoRunUIHandler::CreateInstance(&m_pUIHandler) );

	ATLENSURE_SUCCEEDED( hr = SetExternalUIHandler(m_pUIHandler) );

	LPCTSTR url = static_cast<LPCTSTR>(lpcs->lpCreateParams);
	ATLASSERT(::lstrlen(url) > 0);

	CComVariant vURL(url);
	CComVariant vEmpty;
	m_pBrowser->Navigate2(&vURL, &vEmpty, &vEmpty, &vEmpty, &vEmpty);

	return TRUE;
}
