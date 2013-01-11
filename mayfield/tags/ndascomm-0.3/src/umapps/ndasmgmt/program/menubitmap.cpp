#include "stdafx.h"
#include "ndasmgmt.h"
#include "menubitmap.h"

static
VOID
pDrawSquare(CDCHandle& dc, CONST CRect& rect, COLORREF color)
{
	CPen pen;
	pen.CreatePen(PS_SOLID | PS_COSMETIC, 1, RGB(192,192,192)); 
	HPEN hOldPen = dc.SelectPen(pen);

	CBrush brush;
	brush.CreateSolidBrush(color);
	HBRUSH hOldBrush = dc.SelectBrush(brush);
	// dc.Ellipse(rect); 
	dc.Rectangle(rect); 
	dc.SelectBrush(hOldBrush);

	dc.SelectPen(hOldPen);
}

static
VOID
pDrawError(CDCHandle& dc, CONST CRect& rect)
{
	pDrawSquare(dc, rect, RGB(200,30,30));
}

static
VOID 
pDrawDisconnected(CDCHandle& dc, CONST CRect& rect)
{
	pDrawSquare(dc, rect, RGB(10,10,10));
}

static
VOID 
pDrawDisabled(CDCHandle& dc, CONST CRect& rect)
{
	CBrush brush;
	brush.CreateSolidBrush(RGB(255,255,255)); // ::GetSysColor(COLOR_MENU));
	HBRUSH hOldBrush = dc.SelectBrush(brush);

	CPen pen;
	pen.CreatePen(PS_SOLID | PS_COSMETIC, 2, RGB(254,0,0)); 
	HPEN hOldPen = dc.SelectPen(pen);

	dc.Pie(rect, 
		CPoint(rect.right, rect.top), 
		CPoint(rect.left, rect.bottom));

	dc.Pie(rect, 
		CPoint(rect.left, rect.bottom),
		CPoint(rect.right, rect.top)); 

	dc.SelectBrush(hOldBrush);
	dc.SelectPen(hOldPen);
}

static
VOID
pDrawUnknown(CDCHandle& dc, CONST CRect& rect)
{
	pDrawSquare(dc, rect, RGB(128,128,0));
	dc.TextOut(4,4,_T("?"));
}

static
VOID
pDrawConnected(CDCHandle& dc, CONST CRect& rect, BYTE siPart)
{
	switch (siPart) {
	case NDSI_PART_MOUNTED_RW:
		pDrawSquare(dc, rect, RGB(53,53,251));
		break;
	case NDSI_PART_MOUNTED_RO:
		pDrawSquare(dc, rect, RGB(53,251,53));
		break;
	case NDSI_PART_UNMOUNTED:
	default:
		pDrawSquare(dc, rect, RGB(244,244,244));
		break;
		// ATLASSERT(FALSE);
	}
	return;
}

VOID
CNdasMenuBitmapHandler::OnDrawStatusText(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	CDCHandle dc = lpDrawItemStruct->hDC;
	CRect rect(&lpDrawItemStruct->rcItem);
	rect.bottom -= 2;

	COLORREF oldBkColor = dc.SetBkColor(RGB(255,255,225));

	CBrush brush;
	brush.CreateSolidBrush(RGB(255,255,225));
	
	CPen pen;
	pen.CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DDKSHADOW)); 

	HBRUSH hOldBrush = dc.SelectBrush(brush);
	HPEN hOldPen = dc.SelectPen(pen);

	dc.Rectangle(rect);

//	dc.RoundRect(rect,CPoint(rect.Height() /4, rect.Height() / 4));

	(VOID) dc.SelectPen(hOldPen);
	(VOID) dc.SelectBrush(hOldBrush);

	CRect rectText(rect);
	rectText.left += GetSystemMetrics(SM_CXMENUCHECK) + 2;

	CString strStatus, strStatusOut;
	BOOL fSuccess = strStatus.LoadString(lpDrawItemStruct->itemData);
	ATLASSERT(fSuccess);
//	fSuccess = strStatusOut.Format(_T("Status\r\n%s"), strStatus);
//	ATLASSERT(fSuccess);

	CFont hFont;
	hFont.Attach(dc.GetCurrentFont());
	LOGFONT logFont;
	hFont.GetLogFont(&logFont);
	logFont.lfWeight = FW_BOLD;
	hFont = CreateFontIndirect(&logFont);
	HFONT hOldFont = dc.SelectFont(hFont);

//	COLORREF oldTextColor = dc.SetTextColor(RGB(40,40,233));
	dc.DrawText(
		strStatus,
		strStatus.GetLength(),
		&rectText,
		DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_NOPREFIX | 
		DT_VCENTER);

	dc.SelectFont(hOldFont);
//	dc.SetTextColor(oldTextColor);
	dc.SetBkColor(oldBkColor);
}

VOID 
CNdasMenuBitmapHandler::OnDrawItem(
	UINT nIDCtl, 
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{

	if (100 == lpDrawItemStruct->itemID) {
		OnDrawStatusText(lpDrawItemStruct);
		return;
	}

	NDSI_DATA si;
	si.ulongCaster = lpDrawItemStruct->itemData;

	//
	// DrawData is interpreted as SI_DATA
	//

	CDCHandle dc = lpDrawItemStruct->hDC;
	CRect rect(&lpDrawItemStruct->rcItem);
	rect.DeflateRect(3,3);
	
	switch (si.Status) {
	case NDSI_UNKNOWN:
		pDrawUnknown(dc, rect);
		break;
	case NDSI_ERROR:
		pDrawError(dc, rect);
		break;
	case NDSI_DISABLED:
		pDrawDisabled(dc, rect);
		break;
	case NDSI_DISCONNECTED:
		pDrawDisconnected(dc, rect);
		break;
	case NDSI_CONNECTED:
		ATLASSERT(si.nParts <= 2);
		if (0 == si.nParts) {
			pDrawConnected(dc, rect, NDSI_PART_UNMOUNTED);
		} else if (1 == si.nParts) {
			pDrawConnected(dc, rect, si.StatusPart[0]);
		} else if (2 == si.nParts) {

			CRect rect1(
				rect.left, 
				rect.top, 
				rect.right - rect.Width() / 2,
				rect.bottom);

			CRect rect2(
				rect.left +  rect.Width() / 2, 
				rect.top, 
				rect.right, 
				rect.bottom);

			pDrawConnected(dc, rect1, si.StatusPart[0]);
			pDrawConnected(dc, rect2, si.StatusPart[1]);
		}
		break;
	default:
		ATLASSERT(FALSE);
	}
}

VOID
CNdasMenuBitmapHandler::OnMeasureItem(
	UINT nIDCtl, 
	LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	if (100 == lpMeasureItemStruct->itemID) {
		//
		// Calculate width for each item
		//
		CDC dc = GetDC(::GetDesktopWindow());
		CString strStatus;
		BOOL fSuccess = strStatus.LoadString(lpMeasureItemStruct->itemData);
		ATLASSERT(fSuccess);
		SIZE textSize;
		dc.GetTextExtent(strStatus,strStatus.GetLength(),&textSize);
		lpMeasureItemStruct->itemWidth =  textSize.cx;
		lpMeasureItemStruct->itemHeight = textSize.cy + 6;
	} else {
		lpMeasureItemStruct->itemWidth = lpMeasureItemStruct->itemHeight;
	}
}
