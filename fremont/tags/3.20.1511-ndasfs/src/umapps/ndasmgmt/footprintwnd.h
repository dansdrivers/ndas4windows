#pragma once

typedef CWinTraits<
	WS_CLIPCHILDREN | 
	WS_CLIPSIBLINGS, 
	WS_EX_TOOLWINDOW> CFootprintWindowTraits;

class CFootprintWindow :
	public CWindowImpl<CFootprintWindow, CWindow, CFootprintWindowTraits>
{
public:

	BEGIN_MSG_MAP_EX(CFootprintWindow)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_SETTINGCHANGE(OnSettingChange)
	END_MSG_MAP()

	LRESULT OnCreate(LPCREATESTRUCT lpcs)
	{
		// create window in an invisible area
		int cx = ::GetSystemMetrics(SM_CXVIRTUALSCREEN) + 1;
		int cy = ::GetSystemMetrics(SM_CYVIRTUALSCREEN) + 1;

		CRect rect(cx,cy,0,0);
		CString strTitle = (LPCTSTR) IDS_MAIN_TITLE;
		SetWindowText(strTitle);

		HICON hIcon = ::AtlLoadIcon(IDR_MAINFRAME);
		SetIcon(hIcon, FALSE);

		MoveWindow(rect);

		return TRUE;
	}

	void OnSettingChange(UINT nIndicator, LPCTSTR lpszArea)
	{
		// move this window to an invisible area
		int cx = ::GetSystemMetrics(SM_CXVIRTUALSCREEN) + 1;
		int	cy = ::GetSystemMetrics(SM_CYVIRTUALSCREEN) + 1;
		CRect rect(cx,cy,0,0);
		MoveWindow(rect);
	}

};

