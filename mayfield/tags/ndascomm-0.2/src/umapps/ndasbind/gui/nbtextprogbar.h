////////////////////////////////////////////////////////////////////////////
//
// Interface of CTextProgressBarCtrl class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _TEXTPROGRESSBAR_H_
#define _TEXTPROGRESSBAR_H_

class CTextProgressBarCtrl :
	public CWindowImpl<CTextProgressBarCtrl, CProgressBarCtrl>
{
protected:
	COLORREF	m_crTextClr;
	int			m_nPos, 
				m_nStepSize, 
				m_nMax, 
				m_nMin;
	BOOL		m_bShowText;
	int			m_nBarWidth;
	COLORREF	m_crBarClr,
				m_crBgClr;
	WTL::CString m_strText;

public:

	BEGIN_MSG_MAP_EX(CNBListViewCtrl)
		MSG_WM_SETTEXT(OnSetText)
		MSG_WM_GETTEXT(OnGetText)
		MSG_WM_PAINT(OnPaint)
		MSG_WM_SIZE(OnSize)
		MSG_WM_ERASEBKGND(OnEraseBkgnd)
		MESSAGE_HANDLER_EX(PBM_GETPOS, OnGetPos)
		MESSAGE_HANDLER_EX(PBM_GETRANGE, OnGetRange)
		MESSAGE_HANDLER_EX(PBM_SETBARCOLOR, OnSetBarColor)
		MESSAGE_HANDLER_EX(PBM_SETBKCOLOR, OnSetBkColor)
		MESSAGE_HANDLER_EX(PBM_SETPOS, OnSetPos)
		MESSAGE_HANDLER_EX(PBM_SETRANGE, OnSetRange)
		MESSAGE_HANDLER_EX(PBM_SETRANGE32, OnSetRange32)
		MESSAGE_HANDLER_EX(PBM_SETSTEP, OnSetStep)
		MESSAGE_HANDLER_EX(PBM_STEPIT, OnStepIt)
		MESSAGE_HANDLER_EX(PBM_DELTAPOS, OnOffsetPos)
		//MESSAGE_HANDLER(PBM_SETTEXTCOLOR, OnSetTextColor)
		//MESSAGE_HANDLER(PBM_SETSHOWTEXT, OnSetShowText)
	END_MSG_MAP();
	CTextProgressBarCtrl();

	void SetShowText(BOOL bShow);
    COLORREF SetBarColour(COLORREF crBarClr = CLR_DEFAULT);
    COLORREF GetBarColour() const;
    COLORREF SetBgColour(COLORREF crBgClr = CLR_DEFAULT);
    COLORREF GetBgColour() const;
	COLORREF SetTextColour(COLORREF crTextClr = CLR_DEFAULT);
	COLORREF GetTextColour();

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	BOOL OnEraseBkgnd(HDC hDC);
	void OnPaint(HDC );
	void OnSize(UINT nType, CSize size);
	LRESULT OnSetText(LPCTSTR szText);
	LRESULT OnGetText(int cchTextMax, LPTSTR szText);
	// @return a UINT value that represents 
	//		the current position of the progress bar
	LRESULT OnGetPos(UINT, WPARAM, LPARAM);
	LRESULT OnGetRange(UINT, WPARAM bRetLowLimit, LPARAM pPBRRange);
	// @return previous color
	LRESULT OnSetBarColor(UINT, WPARAM, LPARAM clrBar);
	// @return previous color
	LRESULT OnSetBkColor(UINT, WPARAM, LPARAM clrBk);
	LRESULT OnSetPos(UINT, WPARAM nNewPos, LPARAM);
	LRESULT OnSetRange(UINT, WPARAM, LPARAM lParam);
	LRESULT OnSetRange32(UINT, WPARAM iLowLimit, LPARAM iHighLimit);
	LRESULT OnSetStep(UINT, WPARAM nStepInc, LPARAM);
	LRESULT OnStepIt(UINT, WPARAM, LPARAM);
	LRESULT OnOffsetPos(UINT, WPARAM Incr, LPARAM);
};


#endif // _TEXTPROGRESSBAR_H_