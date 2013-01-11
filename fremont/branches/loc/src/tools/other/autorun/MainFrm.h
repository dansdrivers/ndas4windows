// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <atlcrack.h>

typedef CWinTraits<
	WS_POPUPWINDOW | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	WS_EX_APPWINDOW> CMainFrameTraits;

class CMainFrame :
	public CFrameWindowImpl<CMainFrame, CWindow, CMainFrameTraits>, 
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, 
	public CIdleHandler
{
	typedef CFrameWindowImpl<CMainFrame, CWindow, CMainFrameTraits> WindowImplBase;
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CAutoRunView m_view;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMainFrame)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrame)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_CLOSE(OnClose)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(WindowImplBase)
	END_MSG_MAP()

	LRESULT OnCreate(LPCREATESTRUCT lpcs);
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnClose();
	void OnDestroy();
};
