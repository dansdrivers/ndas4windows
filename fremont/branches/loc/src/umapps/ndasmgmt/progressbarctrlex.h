#pragma once
#include <atlcrack.h>
#include "runtimeinfo.h"

#ifndef PBS_MARQUEE
#define PBS_MARQUEE             0x08
#endif

#ifndef PBM_SETMARQUEE
#define PBM_SETMARQUEE          (WM_USER+10)
#endif

template <typename TBase>
class CMarqueeProgressBarCtrlT : 
	public CWindowImpl<CMarqueeProgressBarCtrlT< TBase >, CProgressBarCtrlT<TBase> >
{
protected:
	typedef CWindowImpl<CMarqueeProgressBarCtrlT< TBase >, CProgressBarCtrlT<TBase> >
		BaseClass;
	
	bool m_useNativeMarquee;

public:

	BEGIN_MSG_MAP_EX(CMarqueeProgressBarT<TBase>)
		MSG_WM_TIMER(OnTimer)
	END_MSG_MAP()

	void OnTimer(UINT_PTR id)
	{
		StepIt();
	}

	void OnTimer(UINT id, TIMERPROC TimerProc)
	{
		OnTimer(id);
	}

	BOOL SubclassWindow(HWND hWnd)
	{
		BOOL success = BaseClass::SubclassWindow(hWnd);
		if (!success) return success;

		m_useNativeMarquee = RunTimeHelper::IsCommCtrl6();

		if (m_useNativeMarquee)
		{
			ModifyStyle(0, PBS_MARQUEE);
			return TRUE;
		}

		SetRange(0, 1000);
		SetStep(10);
		return TRUE;
	}

	BOOL SetMarquee(BOOL bMarquee, UINT uUpdateTime /* = 0U */)
	{
		if (m_useNativeMarquee)
		{
			return 0 != SendMessage(PBM_SETMARQUEE, 
				static_cast<WPARAM>(bMarquee),
				static_cast<LPARAM>(uUpdateTime));
		}

		if (bMarquee)
		{
			return 0 != SetTimer(9, uUpdateTime * 10, NULL);
		}
		else
		{
			return 0 != KillTimer(9);
		}
	}
};

typedef CMarqueeProgressBarCtrlT<ATL::CWindow> CMarqueeProgressBarCtrl;

