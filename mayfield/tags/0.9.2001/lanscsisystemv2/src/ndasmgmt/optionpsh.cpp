#include "stdafx.h"
#include "resource.h"

#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
#include "atlctrlxp.h"
#pragma warning(default: 4312)
#pragma warning(default: 4244)
#include "atlctrlxp2.h"

#include "optionpsh.h"

namespace opt {

COptionsPropSheet::
COptionsPropSheet(
	_U_STRINGorID title, 
	UINT uStartPage, 
	HWND hWndParent) :
	m_bCentered(FALSE)
{
	CPropertySheetImpl<COptionsPropSheet>(MAKEINTRESOURCE(IDS_OPTIONDLG_TITLE),uStartPage,hWndParent);
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPage(m_pgGeneral);
//	AddPage(m_pgAbout);
}

LRESULT 
COptionsPropSheet::
OnInitDialog(HWND hWnd, LPARAM lParam)
{
	return TRUE;
}


LRESULT 
CGeneralPage::
OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CComboBox cmbUILang;
	cmbUILang.Attach(GetDlgItem(IDC_UILANG));
	cmbUILang.AddString(TEXT("English"));
//	cmbUILang.AddString(TEXT("(Default User Interface Language - English)"));
//	cmbUILang.AddString(TEXT("Korean"));
//	cmbUILang.AddString(TEXT("German"));
//	cmbUILang.AddString(TEXT("French"));
//	cmbUILang.AddString(TEXT("Spanish"));
//	cmdUILang.AddString(_T("

	cmbUILang.SetCurSel(0);

	return TRUE;
}

}