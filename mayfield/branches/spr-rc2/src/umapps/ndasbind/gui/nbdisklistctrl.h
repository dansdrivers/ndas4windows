#pragma once

#include <list> 

#define DISK_WIDTH		35
#define DISK_HEIGHT		90
#define SPACE_WIDTH		10
#define DISK_SPAN		(DISK_WIDTH+SPACE_WIDTH)
#define LINE_HEIGHT		10
#define LABEL_WIDTH		(DISK_WIDTH)
#define LABEL_HEIGHT	20
#define HEIGHT_MARGIN	5

template<class T, class TBase = CStatic, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CDiskListCtrlImpl :
	public CWindowImpl<T, TBase, TWinTraits>
{
protected:
	int m_nItemCount;
	int m_nItemDepth;
	int m_nMaxItemCount;
public:
	typedef CDiskListCtrlImpl<T, TBase, TWinTraits> thisClass;
	typedef CWindowImpl<T, TBase, TWinTraits> superClass;

	CDiskListCtrlImpl()
		: m_nItemCount(0), m_nItemDepth(1), m_nMaxItemCount(0)
	{
	}

	BEGIN_MSG_MAP_EX( thisClass )
		MSG_WM_PAINT(OnPaint)
	END_MSG_MAP()

	void SetItemDepth(int nDepth)
	{
		ATLASSERT( (nDepth == 1) || (nDepth == 2) );
		m_nItemDepth = nDepth;
	}
	void SetMaxItemCount(int nCount)
	{
		m_nMaxItemCount = nCount;
	}
	int InsertItem()
	{
		m_nItemCount++;
		InvalidateRect( NULL );
		return m_nItemCount;
	}

	void DeleteItem()
	{
		m_nItemCount--;
		InvalidateRect( NULL );
	}
	void DeleteAllItems()
	{
		m_nItemCount = 0;
		InvalidateRect( NULL );
	}

	void DrawLabel(const HDC hDC, LPCRECT lpRect, WTL::CString strLabel)
	{
		CDCHandle dc(hDC);
		COLORREF clrBg, clrTitle;
		COLORREF clrShadow, clrHighlight;
		clrBg		 = ::GetSysColor(COLOR_3DFACE);//RGB(192, 192, 192);
		clrHighlight = ::GetSysColor(COLOR_3DLIGHT);
		clrShadow	 = ::GetSysColor(COLOR_3DSHADOW);
		clrTitle = RGB(0, 0, 0);
		CRect rtClient(lpRect);
		CPen penHighlight, penShadow;
		CPenHandle penOld;
		DWORD dwTextStyle = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
		dc.SetBkColor( clrBg );
		dc.SetTextColor( clrTitle );
		dc.SelectFont( GetParent().GetFont() ); // pja July 1, 2001

		dc.FillSolidRect( rtClient, clrBg );

		penHighlight.CreatePen( PS_SOLID, 1, clrHighlight );
		penShadow.CreatePen( PS_SOLID, 1, clrShadow );
		penOld = dc.SelectPen( penHighlight );
		dc.MoveTo( rtClient.TopLeft() );
		dc.LineTo( rtClient.right, rtClient.top );
		dc.MoveTo( rtClient.TopLeft() );
		dc.LineTo( rtClient.left, rtClient.bottom );
		dc.SelectPen( penShadow );
		dc.MoveTo( rtClient.BottomRight() );
		dc.LineTo( rtClient.left, rtClient.bottom );
		dc.MoveTo( rtClient.BottomRight() );
		dc.LineTo( rtClient.right, rtClient.top );
		dc.SelectPen( penOld );
		dc.DrawText( strLabel, strLabel.GetLength(), rtClient, dwTextStyle );

	}
	void OnPaint(HDC /*hDC*/)
	{
		int i, j;
		CSize sizeDisk(DISK_WIDTH, DISK_HEIGHT);
		CSize sizeSpace(SPACE_WIDTH, 0);
		CSize sizeDiskSpan(DISK_SPAN, 0);
		CSize sizeLabel(LABEL_WIDTH, LABEL_HEIGHT);
		CSize sizeMargin;
		CRect rtClient;
		int nDiskVisible;					// Number of disks can fit on the window

		CPaintDC dc(m_hWnd);
		CPen penOutline, penOutlineBlur;
		CPenHandle penOld;
		CBrushHandle brushOld;

		GetClientRect( rtClient );
		nDiskVisible = rtClient.Width() / sizeDiskSpan.cx;
		sizeMargin = CSize(rtClient.Width() - sizeDiskSpan.cx*nDiskVisible, 0);
		//
		// Draw Disks
		//
		CPoint ptDisk;
		CRect rtDisk;

		ptDisk = CPoint( rtClient.left, rtClient.bottom );
		ptDisk.x += (sizeMargin.cx + sizeSpace.cx) / 2;
		ptDisk.y -= sizeDisk.cy;
		ptDisk.y -= sizeLabel.cy;
		ptDisk.y -= HEIGHT_MARGIN;
		penOutline.CreatePen( PS_SOLID, 3, RGB(0, 0, 0) );
		penOutlineBlur.CreatePen( PS_DASH, 3, RGB(0x80, 0x80, 0x80) );
		penOld = dc.SelectPen( penOutline );
		brushOld = dc.SelectBrush( ::GetSysColorBrush(COLOR_3DHILIGHT) );
		for ( i=0; i < nDiskVisible; i++ )
		{
			if ( i == m_nMaxItemCount )
			{
				dc.SelectPen( penOutlineBlur );
			}
			if ( i == m_nItemCount )
			{
				dc.SelectBrush( :: GetSysColorBrush(COLOR_3DFACE) );
			}
			rtDisk = CRect( ptDisk, sizeDisk );
			dc.Rectangle( rtDisk );
			ptDisk += sizeDiskSpan;
		}
		//
		// Draw Lines
		//
		CPoint ptLeftMostLine;
		CPoint ptLineNext;
		CSize  sizeLineSpan;
		CSize  sizeLineHeight(0, LINE_HEIGHT);

		ptLeftMostLine = CPoint( rtClient.left, rtClient.bottom );
		ptLeftMostLine.x += (sizeMargin.cx + sizeSpace.cx + sizeDisk.cx) / 2;
		ptLeftMostLine.y -= sizeDisk.cy;
		ptLeftMostLine.y -= sizeLabel.cy;
		ptLeftMostLine.y -= HEIGHT_MARGIN;
		sizeLineSpan = sizeDiskSpan;
		dc.SelectPen( penOutline );
		for ( i=0; i < m_nItemDepth; i++ )
		{
			ptLineNext = ptLeftMostLine;
			for ( j=0; j < nDiskVisible/(i+1) -1; j++ )
			{
				dc.MoveTo( ptLineNext );
				ptLineNext -= sizeLineHeight;
				dc.LineTo( ptLineNext );
				ptLineNext.x += sizeDiskSpan.cx * (i+1);
				if ( j%2 == 0 )
					dc.LineTo( ptLineNext );
				else
					dc.MoveTo( ptLineNext );
				ptLineNext += sizeLineHeight;
				dc.LineTo( ptLineNext );
			}
			if ( i== m_nItemDepth-1 )
			{
				ptLineNext -= sizeLineHeight;
				dc.MoveTo( ptLineNext );
				dc.LineTo( ptLeftMostLine - sizeLineHeight );
			}
			ptLeftMostLine.x += sizeDiskSpan.cx / 2;
			ptLeftMostLine.y -= sizeLineHeight.cy;
		}

		// 
		// Create label
		CPoint ptLabel;
		CRect rtLabel;
		int nLabelCount;
		WTL::CString strLabel;

		ptLabel = CPoint( rtClient.left, rtClient.bottom );
		ptLabel.x += (sizeMargin.cx + sizeSpace.cx) / 2;
		ptLabel.y -= sizeLabel.cy;
		rtLabel = CRect( ptLabel, sizeLabel );
		rtLabel.DeflateRect(1, 1, 1, 1);
		nLabelCount = (nDiskVisible > m_nMaxItemCount)? m_nMaxItemCount: nDiskVisible;
		for ( i=0; i < nLabelCount; i++ )
		{
			strLabel.Format( _T("%d"), i );
			DrawLabel( dc, rtLabel, strLabel);
			rtLabel += sizeDiskSpan;
		}
		dc.SelectBrush( brushOld );
		dc.SelectPen( penOld );
	}

};

class CDiskListCtrl : 
	public CDiskListCtrlImpl<CDiskListCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, GetWndClassName());
};