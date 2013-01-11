// AutoRunView.h : interface of the CAutoRunView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <atlcrack.h>
#include "autorunhelper.h"

class CAutoRunView : public CWindowImpl<CAutoRunView, CAxWindow>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, CAxWindow::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CAutoRunView)
		MSG_WM_CREATE(OnCreate)
	END_MSG_MAP()

	CComPtr<IDispatch> m_pAutoRun;
	CComPtr<IWebBrowser2> m_pBrowser;
	CComPtr<IDocHostUIHandlerDispatch> m_pUIHandler;

	LRESULT OnCreate(LPCREATESTRUCT lpcs);
	void OnDestroy();

	void SetUrl(LPCTSTR url);
};
