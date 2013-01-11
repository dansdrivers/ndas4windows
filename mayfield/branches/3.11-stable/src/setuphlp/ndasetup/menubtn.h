//	Copyright (C) 2000, Lionhearth Technologies, Inc.
//
//	Project: BtnTest
//	File:    MenuBtn.h
//	Author:  Paul Bludov
//	Date:    09/04/2000
//
//	Description:
//		NONE
//
//	Update History:
//		NONE
//
//@//////////////////////////////////////////////////////////////////////////

#ifndef __MENUBTN_H__
#define __MENUBTN_H__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//@//////////////////////////////////////////////////////////////////////////
// CMenuButton - menu button implementation

namespace WTLEX
{

template <bool bAutoAdd, class T, class TBase = CButton, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CMenuButtonImpl : public CWindowImpl< T, TBase, TWinTraits>
{
	bool	m_bShowMenu;
	UINT	m_nMenuID;
	bool	m_bDefault;
	
	BOOL	ShowMenu(CPoint& pt)
	{
		ATLASSERT(m_nMenuID);
		
		m_bShowMenu = true;
		RedrawWindow();

		CMenu menu;
		menu.LoadMenu(m_nMenuID);

		CMenuHandle menuPopup = menu.GetSubMenu(0);

		ClientToScreen(&pt);

		if (bAutoAdd)
		{
			TCHAR szCaption[MAX_PATH];
			if (GetWindowText(szCaption, MAX_PATH) > 0)
			{
				menuPopup.InsertMenu(0, MF_BYPOSITION | MF_STRING, GetDlgCtrlID(), szCaption);
				::SetMenuDefaultItem(menuPopup, 0, true);
			}		
		}

		BOOL bRet = menuPopup.TrackPopupMenu(0, pt.x, pt.y, GetParent());

		MSG msg;
		while(PeekMessage(&msg, m_hWnd, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_REMOVE))
			;

		m_bShowMenu = false;
		RedrawWindow();

		return bRet;
	}

public:
	DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

// Implementation
	CMenuButtonImpl(UINT nMenuId = 0) :
		m_bShowMenu(false),
		m_nMenuID(nMenuId)
	{
	}

	void Init()
	{
		m_bDefault = GetButtonStyle() == BS_DEFPUSHBUTTON;
		// We need this style to prevent Windows from painting the button
		SetButtonStyle(BS_OWNERDRAW);
	}

	// overridden to provide proper initialization
	BOOL SubclassWindow(HWND hWnd)
	{
		BOOL bRet = baseClass::SubclassWindow(hWnd);
		if (bRet)
			Init();
		return bRet;
	}

// Operations
	void SetMenuID(UINT nMenuId)
	{
		m_nMenuID = nMenuId;
	}

// Message map and handlers
	typedef CMenuButtonImpl< bAutoAdd, T, TBase, TWinTraits >	thisClass;
	typedef CWindowImpl<T, TBase, TWinTraits> baseClass;

	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(OCM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDown)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
		MESSAGE_HANDLER(BM_SETSTYLE, OnSetStyle)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		Init();
		bHandled = false;
		return 0;
	}

	LRESULT OnSetStyle(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (BS_DEFPUSHBUTTON == wParam || BS_PUSHBUTTON == wParam)
		{
			m_bDefault = BS_DEFPUSHBUTTON == wParam;
			if (lParam)
				RedrawWindow();
		}
		else
			bHandled = false;

		return 0;
	}

	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
	{
		//	Dialog query
		if (0 == lParam)
			return DLGC_WANTARROWS | (m_bDefault ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);

		MSG *pMsg = (MSG*)lParam;

		if (WM_KEYDOWN == pMsg->message &&
			(VK_DOWN == pMsg->wParam || VK_UP == pMsg->wParam) &&
			(BST_FOCUS & GetState()))
		{
			CRect rect;
			GetClientRect(rect);
			ShowMenu(CPoint(rect.left, rect.bottom));
		}
		else
			bHandled = false;

		return DLGC_WANTARROWS;
	}

	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		int xPos = GET_X_LPARAM(lParam); 

		CRect rect;
		GetClientRect(rect);
		DWORD dwState = GetState();
		if (rect.right - GetSystemMetrics(SM_CXHSCROLL) < xPos 
   			&& 0 == (BST_PUSHED & dwState))      
		{
			if (0 == (BST_FOCUS & dwState))
				SetFocus();
			ShowMenu(CPoint(rect.left, rect.bottom));
		}
		else
			bHandled = false;
		return 0;
	}

	LRESULT OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		DRAWITEMSTRUCT* lpDIS = (DRAWITEMSTRUCT*)lParam;
		ATLASSERT(lpDIS->CtlType == ODT_BUTTON);

		const int  nArrowWidth = GetSystemMetrics(SM_CXHSCROLL);

		const BOOL bDisabled = ODS_DISABLED & lpDIS->itemState;
		const BOOL bSelected = ODS_SELECTED & lpDIS->itemState;
		const BOOL bHasFocus = ODS_FOCUS & lpDIS->itemState;
		const BOOL bFlat = GetStyle() & BS_FLAT;

		CDC DC = lpDIS->hDC;
		CRect rect(lpDIS->rcItem);

		// Button Background
		if (bHasFocus || m_bDefault)
		{
			DC.FrameRect(rect, ::GetSysColorBrush(COLOR_BTNTEXT));
			rect.DeflateRect(1, 1);
		}

		DC.FillRect(rect, ::GetSysColorBrush(COLOR_BTNFACE));

		// Button Frame
		DC.DrawFrameControl(&rect, DFC_BUTTON, DFCS_BUTTONPUSH | 
			(bSelected ? DFCS_PUSHED : 0) |
			(bFlat ? DFCS_MONO | DFCS_FLAT : 0));

		// Arrow Frame
		CRect rectDropArrow(rect);
		rectDropArrow.left = rectDropArrow.right - nArrowWidth;
		DC.DrawFrameControl(&rectDropArrow, DFC_SCROLL, DFCS_SCROLLCOMBOBOX | 
			(bSelected || m_bShowMenu ? DFCS_PUSHED : 0) |
			(bFlat ? DFCS_MONO | DFCS_FLAT : 0) |
			(bDisabled ? DFCS_INACTIVE : 0));

		// Button Text
		TCHAR szCaption[MAX_PATH];
		if (GetWindowText(szCaption, MAX_PATH) > 0)
		{
			CSize sizeExtent;
			DC.GetTextExtent(szCaption, -1, &sizeExtent);
			CPoint ptCentre(rect.CenterPoint());
			CPoint pt = CPoint(ptCentre.x - sizeExtent.cx/2 - nArrowWidth / 2, ptCentre.y - sizeExtent.cy/2 );

			if (bSelected)
				pt.Offset(1,1);

			DC.SetBkMode(TRANSPARENT);
			DC.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
			DC.DrawState(pt, sizeExtent, szCaption, bDisabled ? DSS_DISABLED : DSS_NORMAL);
		}

		// Button Focus
		if (bHasFocus && !m_bShowMenu)
		{
			CRect rectFocus(rect);
			rectFocus.right = rectFocus.right - nArrowWidth;
			if (bFlat)
				rectFocus.DeflateRect(2, 2);
			else
				rectFocus.DeflateRect(4, 4);
			DC.DrawFocusRect(rectFocus);
		}
   		
		return 0;
	}
};

class CMenuButton : public CMenuButtonImpl<false, CMenuButton>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTLEX::MenuButton"), GetWndClassName())

	CMenuButton(UINT nMenuId = 0) :
		CMenuButtonImpl<false, CMenuButton>(nMenuId)
	{
	}

	CMenuButton& operator=(HWND hWnd)
	{
		if (m_hWnd)
			DestroyWindow();

		SubclassWindow(hWnd);
		return *this;
	}
};

class CAutoMenuButton : public CMenuButtonImpl<true, CAutoMenuButton>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTLEX::AutoMenuButton"), GetWndClassName())

	CAutoMenuButton(UINT nMenuId = 0) :
		CMenuButtonImpl<true, CAutoMenuButton>(nMenuId)
	{
	}

	CAutoMenuButton& operator=(HWND hWnd)
	{
		if (m_hWnd)
			DestroyWindow();

		SubclassWindow(hWnd);
		return *this;
	}
};
//@//////////////////////////////////////////////////////////////////////////

}; //namespace WTLEX

#ifndef _WTL_NO_AUTOMATIC_NAMESPACE
using namespace WTLEX;
#endif //!_WTL_NO_AUTOMATIC_NAMESPACE

#endif	__MENUBTN_H__
