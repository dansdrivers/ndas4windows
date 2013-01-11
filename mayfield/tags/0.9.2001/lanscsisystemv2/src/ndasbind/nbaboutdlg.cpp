// nbaboutdlg.cpp : implementation of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbaboutdlg.h"
#include "prodver.h"
#include "nbdefine.h"

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	WTL::CString strProductVer, 
				 strFileVer,
				 strCopyWrite;

	strProductVer	= _T(PV_PRODUCTNAME);
	strFileVer.Format(_T("%s Version %d.%d.%d.%d"), 
		_T(PV_PRODUCTNAME), PV_VER_MAJOR, PV_VER_MINOR, PV_VER_BUILD, PV_VER_PRIVATE);
	strFileVer		+= PV_PRODUCTVERSION ;
	strCopyWrite	= _T(PV_LEGALCOPYRIGHT);
	SetDlgItemText( IDC_TEXT_PRODUCTVER, strProductVer );
	SetDlgItemText( IDC_TEXT_FILEVER, strFileVer );
	SetDlgItemText( IDC_TEXT_COPYWRITE, strCopyWrite );

	m_wndLink.SubclassWindow( GetDlgItem(IDC_ABOUTBOX_HYPERLINK) );

	return TRUE;
}

LRESULT CAboutDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
