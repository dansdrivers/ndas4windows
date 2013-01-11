////////////////////////////////////////////////////////////////////////////
//
// Interface of CUnBindDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _NBUNBINDDLG_H_
#define _NBUNBINDDLG_H_

#include <map>
#include "resource.h"
#include "ndasobject.h"
#include "nblistview.h"

class CUnBindDlg : 
	public CDialogImpl<CUnBindDlg>,
	public CWinDataExchange<CUnBindDlg>
{
protected:
	CNBListViewCtrl m_wndListUnbind;
	CDiskObjectPtr	m_pDiskUnbind;
	CDiskObjectList m_unboundDisks;
public:
	CUnBindDlg();
	enum { IDD = IDD_UNBIND };

	BEGIN_DDX_MAP(CUnBindDlg)
	END_DDX_MAP()

	BEGIN_MSG_MAP(CUnBindDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnOK(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnCancel(UINT wNotifyCode, int wID, HWND hwndCtl);

	void SetDiskToUnbind(CDiskObjectPtr obj) { m_pDiskUnbind = obj; }
	CDiskObjectList GetUnboundDisks() { return m_unboundDisks; }
};

#endif // _NBUNBINDDLG_H_