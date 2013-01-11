////////////////////////////////////////////////////////////////////////////
//
// Implementation of CTextProgressBarCtrl class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "nbtextprogbar.h"

CTextProgressBarCtrl::CTextProgressBarCtrl()
{
	m_nPos      = 0;
	m_nStepSize = 1;
	m_nMax      = 10000;
	m_nMin      = 0;
	m_bShowText = TRUE;
	m_strText.Empty();
	m_crBarClr  = CLR_DEFAULT;
	m_crBgClr   = CLR_DEFAULT;
	m_crTextClr = CLR_DEFAULT;
	m_nBarWidth = -1;
}

void CTextProgressBarCtrl::SetShowText(BOOL bShow)
{
	if (::IsWindow(m_hWnd) && m_bShowText != bShow)
		Invalidate();

	m_bShowText = bShow;
}

COLORREF CTextProgressBarCtrl::SetBarColour(COLORREF crBarClr)
{
	if (::IsWindow(m_hWnd))
		Invalidate();

	COLORREF crOldBarClr = m_crBarClr;
	m_crBarClr = crBarClr;
	return crOldBarClr;
}

COLORREF CTextProgressBarCtrl::GetBarColour() const
{
	return m_crBarClr;
}
COLORREF CTextProgressBarCtrl::SetBgColour(COLORREF crBgClr)
{
	if (::IsWindow(m_hWnd))
		Invalidate();

	COLORREF crOldBgClr = m_crBgClr;
	m_crBgClr = crBgClr;
	return crOldBgClr;
}
COLORREF CTextProgressBarCtrl::GetBgColour() const
{ 
	return m_crBgClr;
}
COLORREF CTextProgressBarCtrl::SetTextColour(COLORREF crTextClr)
{
	if (::IsWindow(m_hWnd))
		Invalidate();

	COLORREF crOldTextClr = m_crTextClr;
	m_crTextClr = crTextClr;
	return crOldTextClr;
}
COLORREF CTextProgressBarCtrl::GetTextColour()
{
	return m_crTextClr;
}

BOOL CTextProgressBarCtrl::OnEraseBkgnd(HDC)
{    
	return TRUE;
}
void CTextProgressBarCtrl::OnPaint(HDC )
{
	if (m_nMin >= m_nMax) 
		return;

	COLORREF crBarColour, crBgColour;

	crBarColour = (m_crBarClr == CLR_DEFAULT)? ::GetSysColor(COLOR_HIGHLIGHT) : m_crBarClr;
	crBgColour = (m_crBgClr == CLR_DEFAULT)? ::GetSysColor(COLOR_WINDOW) : m_crBgClr;

	CRect LeftRect, RightRect, ClientRect;
	GetClientRect(ClientRect);

	double Fraction = (double)(m_nPos - m_nMin) / ((double)(m_nMax - m_nMin));
	CPaintDC dc(this->m_hWnd);    // device context for painting (if not double buffering)
	dc.SelectFont(GetParent().GetFont()); // pja July 1, 2001

	// Draw Bar
	LeftRect = RightRect = ClientRect;
#ifdef PBS_VERTICAL
	DWORD dwStyle = GetStyle();
	if (dwStyle & PBS_VERTICAL)
	{
		LeftRect.top = LeftRect.bottom - (int)((LeftRect.bottom - LeftRect.top)*Fraction);
		RightRect.bottom = LeftRect.top;
	}
	else
#endif
	{
		LeftRect.right = LeftRect.left + (int)((LeftRect.right - LeftRect.left)*Fraction);
		RightRect.left = LeftRect.right;
	}
	dc.FillSolidRect(LeftRect, crBarColour);
	dc.FillSolidRect(RightRect, crBgColour);
	// Draw Text if not vertical
	if (m_bShowText
#ifdef PBS_VERTICAL
		&& (dwStyle & PBS_VERTICAL) == 0
#endif
		)
	{
		WTL::CString str;
		if (m_strText.GetLength())
			str = m_strText;
		else
			str.Format(_T("%3.2f%%"), Fraction*100.0);

		dc.SetBkMode(TRANSPARENT);

		DWORD dwTextStyle = DT_CENTER | DT_VCENTER | DT_SINGLELINE;

		// If we are drawing vertical, then create a new verticla font
		// based on the current font (only works with TrueType fonts)
		CFont font, oldFont;
#ifdef PBS_VERTICAL
		if (dwStyle & PBS_VERTICAL)
		{
			LOGFONT lf;
			CFont(GetFont()).GetLogFont(&lf);
			lf.lfEscapement = lf.lfOrientation = 900;
			font.CreateFontIndirect(&lf);
			oldFont = dc.SelectFont(font);
			dwTextStyle = DT_VCENTER|DT_CENTER|DT_SINGLELINE;
		}
#endif

		CRgn rgn;
		rgn.CreateRectRgn(LeftRect.left, LeftRect.top, LeftRect.right, LeftRect.bottom);
		dc.SelectClipRgn(rgn);
		dc.SetTextColor(m_crTextClr == CLR_DEFAULT ? crBgColour : m_crTextClr);
		dc.DrawText(str, str.GetLength(), ClientRect, dwTextStyle);

		rgn.DeleteObject();
		rgn.CreateRectRgn(RightRect.left, RightRect.top, RightRect.right, RightRect.bottom);
		dc.SelectClipRgn(rgn);
		dc.SetTextColor(m_crTextClr == CLR_DEFAULT ? crBarColour : m_crTextClr);
		dc.DrawText(str, str.GetLength(), ClientRect, dwTextStyle);

		if (!oldFont.IsNull())
		{
			dc.SelectFont(oldFont);
			font.DeleteObject();
		}
	}
}
void CTextProgressBarCtrl::OnSize(UINT , CSize )
{
	SetMsgHandled(FALSE);

	m_nBarWidth    = -1;   // Force update if SetPos called
}


LRESULT CTextProgressBarCtrl::OnSetText(LPCTSTR szText)
{
	if ( (!szText && m_strText.GetLength()) ||
		(szText && (m_strText != szText))   )
	{
		m_strText = szText;
		Invalidate();
	}
	SetMsgHandled(FALSE);
	return TRUE;
}

LRESULT CTextProgressBarCtrl::OnGetText(int cchTextMax, LPTSTR szText)
{
	if (!_tcsncpy(szText, m_strText, cchTextMax))
		return 0;
	else 
		return min(cchTextMax, m_strText.GetLength());
}

// @return a UINT value that represents 
//		the current position of the progress bar
LRESULT CTextProgressBarCtrl::OnGetPos(UINT, WPARAM, LPARAM)
{
	return (LRESULT)m_nPos;
}

LRESULT CTextProgressBarCtrl::OnGetRange(UINT, WPARAM bRetLowLimit, LPARAM pPBRRange)
{
	BOOL bType = (BOOL)bRetLowLimit;
	PPBRANGE pRange = (PPBRANGE)pPBRRange;
	if (pRange)
	{
		pRange->iHigh = m_nMax;
		pRange->iLow = m_nMin;
	}
	return (LRESULT)(bType ? m_nMin : m_nMax);
}
// @return previous color
LRESULT CTextProgressBarCtrl::OnSetBarColor(UINT, WPARAM, LPARAM clrBar)
{
	return (LRESULT)SetBarColour((COLORREF)clrBar);
}
// @return previous color
LRESULT CTextProgressBarCtrl::OnSetBkColor(UINT, WPARAM, LPARAM clrBk)
{
	return (LRESULT)SetBgColour((COLORREF)clrBk);
}
LRESULT CTextProgressBarCtrl::OnSetPos(UINT, WPARAM newPos, LPARAM)
{    
	if (!::IsWindow(m_hWnd))
		return -1;

	int nPos = (int)newPos;

	int nOldPos = m_nPos;
	m_nPos = nPos;

	CRect rect;
	GetClientRect(rect);

	double Fraction = (double)(m_nPos - m_nMin) / ((double)(m_nMax - m_nMin));
	int nBarWidth = (int) (Fraction * rect.Width());

	if (nBarWidth != m_nBarWidth)
	{
		m_nBarWidth = nBarWidth;
		RedrawWindow();
	}

	return (LRESULT)nOldPos;
}

LRESULT CTextProgressBarCtrl::OnSetRange(UINT, WPARAM, LPARAM range)
{
	return OnSetRange32(0, LOWORD(range), HIWORD(range));
}
LRESULT CTextProgressBarCtrl::OnSetRange32(UINT, WPARAM iLowLimit, LPARAM iHighLimit)
{
	LRESULT ret = MAKELPARAM (m_nMin, m_nMax);
	m_nMax = (int)iHighLimit;
	m_nMin = (int)iLowLimit;
	return ret;
}
LRESULT CTextProgressBarCtrl::OnSetStep(UINT, WPARAM nStepInc, LPARAM)
{
	int nOldStep = m_nStepSize;
	m_nStepSize = (int)nStepInc;
	return (LRESULT)nOldStep;
}
LRESULT CTextProgressBarCtrl::OnStepIt(UINT, WPARAM, LPARAM)
{    
	return (LRESULT)SetPos(m_nPos + m_nStepSize);
}

LRESULT CTextProgressBarCtrl::OnOffsetPos(UINT, WPARAM Incr, LPARAM)
{
	return (LRESULT)SetPos(m_nPos + (int)Incr);
}
