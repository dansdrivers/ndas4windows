#include "stdafx.h"
#include "ndasmgmt.h"
#include "menubitmap.h"

//
// Unnamed namespace for local functions
//
namespace 
{
	HFONT
	pGetMenuFont(
		BOOL bBoldFont = FALSE)
	{
		//get system font for any owner drawing
		NONCLIENTMETRICS metrics = { sizeof(metrics) };
		::SystemParametersInfo(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);
		if (bBoldFont)
		{
			if (metrics.lfMenuFont.lfCharSet == ANSI_CHARSET)
			{
				metrics.lfMenuFont.lfWeight = FW_BOLD;
			}
		}
		return ::CreateFontIndirect(&metrics.lfMenuFont);
	}

	// Image List Indexes
	const int nDISCONNECTED = 0;
	const int nCONNECTING = 1;
	const int nCONNECTED = 2;
	const int nMOUNTED_RW = 3;
	const int nMOUNTED_RO = 4;
	const int nERROR = 5;
	const int nDEACTIVATED = 6;

	const int nENCRYPTED = 7;

	const int nOV_ENCRYPTED = 1;
	const int cxStatusBitmap = 14; // 14 by 14 images
}

BOOL
CNdasMenuBitmapHandler::
Initialize()
{
	BOOL fSuccess;
	
	fSuccess = m_imageList.Create(cxStatusBitmap, cxStatusBitmap, ILC_COLOR32 | ILC_MASK, 0, 8);
	ATLASSERT(fSuccess); if (!fSuccess) return fSuccess;

	HBITMAP hBitmap = AtlLoadBitmapImage(IDB_STATUS, LR_CREATEDIBSECTION);
	ATLASSERT(hBitmap); if (!hBitmap) return FALSE;

	ATLVERIFY(m_imageList.Add(hBitmap, (COLORREF)CLR_DEFAULT) != -1);
	fSuccess = m_imageList.SetOverlayImage(nENCRYPTED, nOV_ENCRYPTED);
	ATLASSERT(fSuccess); 

	return fSuccess;;
}

void
CNdasMenuBitmapHandler::
Cleanup()
{
	ATLASSERT(m_imageList.Destroy());
}

void
CNdasMenuBitmapHandler::
OnDrawStatusText(
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	CDCHandle dc = lpDrawItemStruct->hDC;
	CRect rect(&lpDrawItemStruct->rcItem);
	rect.bottom -= 2;

	COLORREF oldBkColor = dc.SetBkColor(RGB(255,255,225));

	CBrush brush = ::CreateSolidBrush(RGB(255,255,225));
	
	CPen pen = ::CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DDKSHADOW)); 

	HBRUSH hOldBrush = dc.SelectBrush(brush);
	HPEN hOldPen = dc.SelectPen(pen);

//	dc.Rectangle(rect);

	dc.RoundRect(rect,CPoint(rect.Height() /4, rect.Height() / 4));

	(void) dc.SelectPen(hOldPen);
	(void) dc.SelectBrush(hOldBrush);

	CRect rectText(rect);
	rectText.left += GetSystemMetrics(SM_CXMENUCHECK) + 2;

	CString strStatus;
	ATLVERIFY(strStatus.LoadString(lpDrawItemStruct->itemData));

	CFontHandle curFont = dc.GetCurrentFont();
	LOGFONT logFont;
	curFont.GetLogFont(&logFont);
	logFont.lfWeight = FW_BOLD;
	CFont newFont = ::CreateFontIndirect(&logFont);
	CFontHandle oldFont = dc.SelectFont(newFont);

//	COLORREF oldTextColor = dc.SetTextColor(RGB(40,40,233));

	dc.DrawText(
		strStatus,
		strStatus.GetLength(),
		&rectText,
		DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_NOPREFIX | 
		DT_VCENTER);

	dc.SelectFont(oldFont);
//	dc.SetTextColor(oldTextColor);
	dc.SetBkColor(oldBkColor);
}

void 
CNdasMenuBitmapHandler::
OnDrawItem(
	UINT nIDCtl, 
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (100 == lpDrawItemStruct->itemID) 
	{
		OnDrawStatusText(lpDrawItemStruct);
		return;
	}

	CDCHandle dc = lpDrawItemStruct->hDC;

	NDSI_DATA si;
	si.ulongCaster = lpDrawItemStruct->itemData;

	//
	// DrawData is interpreted as SI_DATA
	//
	
	int x = lpDrawItemStruct->rcItem.left;
	int y = lpDrawItemStruct->rcItem.top;

	switch (si.Status) 
	{
	case NDSI_UNKNOWN:
		ATLVERIFY(m_imageList.Draw(dc, nERROR, x, y, ILD_NORMAL));
		break;
	case NDSI_ERROR:
		ATLVERIFY(m_imageList.Draw(dc, nERROR, x, y, ILD_NORMAL));
		break;
	case NDSI_DISABLED:
		ATLVERIFY(m_imageList.Draw(dc, nDEACTIVATED, x, y, ILD_NORMAL));
		break;
	case NDSI_DISCONNECTED:
		ATLVERIFY(m_imageList.Draw(dc, nDISCONNECTED, x, y, ILD_NORMAL));
		break;
	case NDSI_CONNECTING:
		ATLVERIFY(m_imageList.Draw(dc, nCONNECTING, x, y, ILD_NORMAL));
		break;
	case NDSI_CONNECTED:
		ATLASSERT(si.nParts <= 2);
		if (0 == si.nParts)
		{
			ATLVERIFY(m_imageList.Draw(dc, nCONNECTED, x, y, ILD_NORMAL));
		}
		else
		{
			for (int i = 0; i < si.nParts && i < 2; ++i)
			{
				int index = nCONNECTED;
				switch (si.StatusPart[i] & 0x0F) {
				case NDSI_PART_MOUNTED_RW: index = nMOUNTED_RW; break;
				case NDSI_PART_MOUNTED_RO: index = nMOUNTED_RO; break;
				case NDSI_PART_ERROR:      index = nERROR; break;
				case NDSI_PART_UNMOUNTED:
				default:                   break;
				}
				UINT style = ILD_NORMAL;
				if ((si.StatusPart[i] & NDSI_PART_CONTENT_IS_ENCRYPTED))
				{
					style |= INDEXTOOVERLAYMASK(nOV_ENCRYPTED);
				}
				ATLVERIFY(m_imageList.Draw(dc, index, x, y, style));
				x += cxStatusBitmap;
			}
		}
		break;
	default:
		ATLASSERT(FALSE);
	}
}

void
CNdasMenuBitmapHandler::
OnMeasureItem(
	UINT nIDCtl, 
	LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	if (100 == lpMeasureItemStruct->itemID) 
	{
		//
		// Calculate width for each item
		//
		CString strStatus;
		BOOL fSuccess = strStatus.LoadString(lpMeasureItemStruct->itemData);
		ATLASSERT(fSuccess);

		CDC dc = GetDC(::GetDesktopWindow());
		CFont newFont = pGetMenuFont(TRUE);
		CFontHandle oldFont = dc.SelectFont(newFont);

		CSize textSize;
		dc.GetTextExtent(strStatus,strStatus.GetLength(),&textSize);

		(void) dc.SelectFont(oldFont);

		int margin = ::GetSystemMetrics(SM_CXMENUCHECK);
		lpMeasureItemStruct->itemWidth =  textSize.cx + margin; // plus margin
		lpMeasureItemStruct->itemHeight = textSize.cy + 6;
	} 
	else 
	{
		NDSI_DATA si;
		si.ulongCaster = lpMeasureItemStruct->itemData;
		DWORD nParts = max(si.nParts, 1);
		lpMeasureItemStruct->itemWidth = cxStatusBitmap * nParts;
		lpMeasureItemStruct->itemHeight = cxStatusBitmap;
	}
}

