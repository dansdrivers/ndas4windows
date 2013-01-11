/*
  Module : XTaskDialogStatic.H
  Purpose: Defines the CXTaskDialogStatic class as used by CXTaskDialog.
  Created: PJN / 14-03-2007

  Copyright (c) 2007 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

  All rights reserved.

  Copyright / Usage Details:

  You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
  when your product is released in binary form. You are allowed to modify the source code in any way you want 
  except you cannot modify the copyright details at the top of each module. If you want to distribute source 
  code with your application, then you are only allowed to distribute versions released by the author. This is 
  to maintain a single distribution point for the source code. 

*/

/////////////////////////////// Macros / Defines //////////////////////////////

#pragma once

#ifndef __XTASKDIALOGSTATIC_H__
#define __XTASKDIALOGSTATIC_H__

#ifndef CXTASKDIALOG_EXT_CLASS
#define CXTASKDIALOG_EXT_CLASS
#endif


////////////////////////////// Classes ////////////////////////////////////////

//Class which draws the static controls in a specified background color and specified text color
class CXTASKDIALOG_EXT_CLASS CXTaskDialogStatic : public CWindowImpl<CXTaskDialogStatic>
{
public:
//Constructors / Destructors
	CXTaskDialogStatic() : 
		m_textColor(RGB(0, 0, 0)), 
		m_backgroundColor(RGB(255, 255, 255)),
		m_pBrushBackground(NULL)
	{
	}
  
//Methods  
	void Init(COLORREF colorText, COLORREF colorBackground, HBRUSH* pBrushBackground)
	{
		m_textColor = colorText;
		m_backgroundColor = colorBackground;
		m_pBrushBackground = pBrushBackground;
	}
  
protected:
	BEGIN_MSG_MAP(CTaskDialogStatic)
		MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnControlColorStatic)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
	END_MSG_MAP()
  
	LRESULT OnControlColorStatic(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		HDC hDC = reinterpret_cast<HDC>(wParam);
		::SetBkColor(hDC, m_backgroundColor);
		::SetTextColor(hDC, m_textColor);
		return reinterpret_cast<LRESULT>(*m_pBrushBackground);
	}
  
	LRESULT OnEraseBackground(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		//Fill the entire client area with the background color
		HDC hDC = reinterpret_cast<HDC>(wParam);
		::SetBkColor(hDC, m_backgroundColor);
		CRect rClient;
		GetClientRect(&rClient);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rClient, NULL, 0, NULL);

		return 1;
	}

//Member variables	
	COLORREF m_textColor;		 //The color to draw the text in  
	COLORREF m_backgroundColor;	 //The color of our background
	HBRUSH*	 m_pBrushBackground; //The background brush to use
};

#include <atlapp.h>
#include <atlctrls.h>
#include <atlctrlx.h>


template <class T, class TBase = ATL::CWindow, class TWinTraits = ATL::CControlWinTraits>
class ATL_NO_VTABLE CHyperLink2Impl : public WTL::CHyperLinkImpl< T, TBase, TWinTraits >
{
public:
	CHyperLink2Impl(DWORD dwExtendedStyle = HLINK_UNDERLINED) :
		CHyperLinkImpl(dwExtendedStyle)
	{
	}

// Implementation (Override)
	void Init()
	{
		ATLASSERT(::IsWindow(m_hWnd));

		// Check if we should paint a label
		const int cchBuff = 8;
		TCHAR szBuffer[cchBuff] = { 0 };
		if(::GetClassName(m_hWnd, szBuffer, cchBuff))
		{
			if(lstrcmpi(szBuffer, _T("static")) == 0)
			{
				ModifyStyle(0, SS_NOTIFY);	 // we need this
				DWORD dwStyle = GetStyle() & 0x000000FF;
#ifndef _WIN32_WCE
				if(dwStyle == SS_ICON || dwStyle == SS_BLACKRECT || dwStyle == SS_GRAYRECT || 
				   dwStyle == SS_WHITERECT || dwStyle == SS_BLACKFRAME || dwStyle == SS_GRAYFRAME || 
				   dwStyle == SS_WHITEFRAME || dwStyle == SS_OWNERDRAW || 
				   dwStyle == SS_BITMAP || dwStyle == SS_ENHMETAFILE)
#else // CE specific
					if(dwStyle == SS_ICON || dwStyle == SS_BITMAP)
#endif //_WIN32_WCE
						m_bPaintLabel = false;
			}
		}

		// create or load a cursor
#if (WINVER >= 0x0500) || defined(_WIN32_WCE)
		m_hCursor = ::LoadCursor(NULL, IDC_HAND);
#else
#if (_ATL_VER >= 0x0700)
		m_hCursor = ::CreateCursor(ATL::_AtlBaseModule.GetModuleInstance(), _AtlHyperLink_CursorData.xHotSpot, _AtlHyperLink_CursorData.yHotSpot, _AtlHyperLink_CursorData.cxWidth, _AtlHyperLink_CursorData.cyHeight, _AtlHyperLink_CursorData.arrANDPlane, _AtlHyperLink_CursorData.arrXORPlane);
#else //!(_ATL_VER >= 0x0700)
		m_hCursor = ::CreateCursor(_Module.GetModuleInstance(), _AtlHyperLink_CursorData.xHotSpot, _AtlHyperLink_CursorData.yHotSpot, _AtlHyperLink_CursorData.cxWidth, _AtlHyperLink_CursorData.cyHeight, _AtlHyperLink_CursorData.arrANDPlane, _AtlHyperLink_CursorData.arrXORPlane);
#endif //!(_ATL_VER >= 0x0700)
#endif
		ATLASSERT(m_hCursor != NULL);

		// set font
		if(m_bPaintLabel)
		{
			if(m_hFontNormal == NULL)
				m_hFontNormal = (HFONT)::GetStockObject(SYSTEM_FONT);
			if(m_hFontNormal != NULL && m_hFont == NULL)
			{
				LOGFONT lf = { 0 };
				CFontHandle font = m_hFontNormal;
				font.GetLogFont(&lf);
				if(IsUsingTagsBold())
					lf.lfWeight = FW_BOLD;
				else if(!IsNotUnderlined())
					lf.lfUnderline = TRUE;
				m_hFont = ::CreateFontIndirect(&lf);
				m_bInternalLinkFont = true;
				ATLASSERT(m_hFont != NULL);
			}
		}

#ifndef _WIN32_WCE
		// create a tool tip
		m_tip.Create(m_hWnd);
		ATLASSERT(m_tip.IsWindow());
#endif //!_WIN32_WCE

		// set label (defaults to window text)
		if(m_lpstrLabel == NULL)
		{
			int nLen = GetWindowTextLength();
			if(nLen > 0)
			{
				LPTSTR lpszText = (LPTSTR)_alloca((nLen + 1) * sizeof(TCHAR));
				if(GetWindowText(lpszText, nLen + 1))
					SetLabel(lpszText);
			}
		}

		T* pT = static_cast<T*>(this);
		pT->CalcLabelRect();

		// set hyperlink (defaults to label), or just activate tool tip if already set
		if(m_lpstrHyperLink == NULL && !IsCommandButton())
		{
			if(m_lpstrLabel != NULL)
				SetHyperLink(m_lpstrLabel);
		}
#ifndef _WIN32_WCE
		else
		{
			m_tip.Activate(TRUE);
			m_tip.AddTool(m_hWnd, m_lpstrHyperLink, &m_rcLink, 1);
		}
#endif //!_WIN32_WCE

		// set link colors
		if(m_bPaintLabel)
		{
			ATL::CRegKey rk;
			LONG lRet = rk.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Internet Explorer\\Settings"));
			if(lRet == 0)
			{
				const int cchBuff = 12;
				TCHAR szBuff[cchBuff] = { 0 };
#if (_ATL_VER >= 0x0700)
				ULONG ulCount = cchBuff;
				lRet = rk.QueryStringValue(_T("Anchor Color"), szBuff, &ulCount);
#else
				DWORD dwCount = cchBuff * sizeof(TCHAR);
				lRet = rk.QueryValue(szBuff, _T("Anchor Color"), &dwCount);
#endif
				if(lRet == 0)
				{
					COLORREF clr = pT->_ParseColorString(szBuff);
					ATLASSERT(clr != CLR_INVALID);
					if(clr != CLR_INVALID)
						m_clrLink = clr;
				}

#if (_ATL_VER >= 0x0700)
				ulCount = cchBuff;
				lRet = rk.QueryStringValue(_T("Anchor Color Visited"), szBuff, &ulCount);
#else
				dwCount = cchBuff * sizeof(TCHAR);
				lRet = rk.QueryValue(szBuff, _T("Anchor Color Visited"), &dwCount);
#endif
				if(lRet == 0)
				{
					COLORREF clr = pT->_ParseColorString(szBuff);
					ATLASSERT(clr != CLR_INVALID);
					if(clr != CLR_INVALID)
						m_clrVisited = clr;
				}
			}
		}
	}

};
//Class which draws the static controls in a specified background color and specified text color
class CXTASKDIALOG_EXT_CLASS CXTaskDialogHyperLink : public CHyperLink2Impl<CXTaskDialogHyperLink>
{
public:
//Constructors / Destructors
	CXTaskDialogHyperLink() : m_textColor(RGB(0, 0, 0)), 
		m_backgroundColor(RGB(255, 255, 255)),
		m_pBrushBackground(NULL)
	{
	}

	HFONT m_hPreInitFont;
	void PreInit(HFONT hFont)
	{
		m_hPreInitFont = hFont;
		DWORD type = GetObjectType(hFont);
		ATLASSERT(OBJ_FONT == type);
	}
	void Init()
	{
		m_hFontNormal = m_hPreInitFont;
		// SetFont(m_hPreInitFont);
		CHyperLink2Impl<CXTaskDialogHyperLink>::Init();
	}

//Methods  
	void Init(COLORREF colorText, COLORREF colorBackground, HBRUSH* pBrushBackground)
	{
		m_textColor = colorText;
		m_backgroundColor = colorBackground;
		m_pBrushBackground = pBrushBackground;
	}
  
protected:
	typedef CHyperLinkImpl<CXTaskDialogHyperLink> BaseClass;
	BEGIN_MSG_MAP(CXTaskDialogHyperLink);
	MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnControlColorStatic)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		//REFLECT_NOTIFICATIONS()
		CHAIN_MSG_MAP(BaseClass)
		END_MSG_MAP()
  
		LRESULT OnControlColorStatic(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		HDC hDC = reinterpret_cast<HDC>(wParam);
		::SetBkColor(hDC, m_backgroundColor);
		::SetTextColor(hDC, m_textColor);
		return reinterpret_cast<LRESULT>(*m_pBrushBackground);
	}
  
	LRESULT OnEraseBackground(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		//Fill the entire client area with the background color
		HDC hDC = reinterpret_cast<HDC>(wParam);
		::SetBkColor(hDC, m_backgroundColor);
		CRect rClient;
		GetClientRect(&rClient);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rClient, NULL, 0, NULL);

		return 1;
	}

//Member variables	
	COLORREF m_textColor;		 //The color to draw the text in  
	COLORREF m_backgroundColor;	 //The color of our background
	HBRUSH*	 m_pBrushBackground; //The background brush to use
};

#endif
