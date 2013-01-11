////////////////////////////////////////////////////////////////////////////
//
// Interface of CArrowButton class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBARROWBUTTON_H_
#define _NBARROWBUTTON_H_

#define ARROW_BOTTOM	1
#define ARROW_TOP		2 
#define ARROW_RIGHT		3
#define ARROW_LEFT		4

template<class T, int nDirection=ARROW_BOTTOM, class TBase = CButton, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CArrowButtonImpl :
	public CWindowImpl<T, TBase, TWinTraits>
{
public:
#define WM_UPDATEUISTATE                0x0128

	typedef CArrowButtonImpl<T, nDirection, TBase, TWinTraits> thisClass;
	typedef CWindowImpl<T, TBase, TWinTraits> superClass;
	BEGIN_MSG_MAP_EX(thisClass)
		MSG_WM_PAINT(OnPaint)
		MSG_WM_ENABLE(OnEnable)
		MESSAGE_HANDLER_EX( WM_UPDATEUISTATE, OnUpdateUIState )
	END_MSG_MAP();

	LRESULT OnUpdateUIState(UINT, WPARAM, LPARAM)
	{
		InvalidateRect( NULL );
		return 0;
	}

	void OnEnable(BOOL /*bEnable*/)
	{
		// When the button is enabled. 
		// The window is repainted without sending WM_PAINT message.
		// Therefore, here manually call it.
		InvalidateRect( NULL );
	}
	void OnPaint(HDC wParam)
	{
		superClass::DefWindowProc( WM_PAINT, (WPARAM)wParam, 0 );
		CClientDC dc(this->m_hWnd);
		CRect rcButton;
		CPoint ptArrowCenter;

		GetClientRect( rcButton );
		ptArrowCenter = rcButton.CenterPoint();
		DrawArrow( dc, ptArrowCenter, !IsWindowEnabled());
	}

	CPoint RotatePoint(CPoint pt)
	{
		CPoint ptRotated;
		switch( nDirection )
		{
		case ARROW_TOP:
			ptRotated = CPoint( -1*pt.x, -1*pt.y );
			break;
		case ARROW_RIGHT:
			ptRotated = CPoint( pt.y, -1*pt.x);
			break;
		case ARROW_LEFT:
			ptRotated = CPoint( -1*pt.y, pt.x );
			break;
		case ARROW_BOTTOM:
		default:
			ptRotated = pt;
			break;
		}
		return ptRotated;
	}
	void DrawArrow(CDC DC,CPoint ArrowCenter, BOOL bDisabled = FALSE)
	{
		CPoint ArrowTip = ArrowCenter+ RotatePoint(CPoint(0,2));
		CPoint ptDest;
		CPoint ptOrig = ArrowTip;
		COLORREF cr = bDisabled ? GetSysColor(COLOR_GRAYTEXT) : RGB(0,0,0);
		CPen penArrow, penOld;

		penArrow.CreatePen(PS_SOLID,1,cr);
		penOld = DC.SelectPen(penArrow);

		DC.SetPixel(ArrowTip,cr);

		ArrowTip -= RotatePoint( CPoint(1,1) );
		DC.MoveTo(ArrowTip);

		ptDest = ArrowTip;
		ptDest += RotatePoint( CPoint(3,0) );
		DC.LineTo(ptDest);

		ArrowTip -= RotatePoint( CPoint(1,1) );
		DC.MoveTo(ArrowTip);

		ptDest = ArrowTip;
		ptDest += RotatePoint( CPoint(5,0) );
		DC.LineTo(ptDest);

		ArrowTip -= RotatePoint( CPoint(1,1) );
		DC.MoveTo(ArrowTip);

		ptDest = ArrowTip;
		ptDest += RotatePoint( CPoint(7,0) );
		DC.LineTo(ptDest);

		if ( bDisabled )
		{
			ptOrig += RotatePoint( CPoint(0,1) );
			penArrow.DeleteObject();
			penArrow.CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DHILIGHT));	
			DC.SelectPen(penArrow);
			DC.LineTo(ptOrig);
		}
		DC.SelectPen(penOld);
	}
};

template<int nDirection>
class CArrowButton : 
	public CArrowButtonImpl<CArrowButton, nDirection>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, GetWndClassName());
};

#endif // _NBARROWBUTTON_H_