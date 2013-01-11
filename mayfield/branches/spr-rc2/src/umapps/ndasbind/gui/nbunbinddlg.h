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
#include "nblistviewctrl.h"

class CUnBindDlg : 
	public CDialogImpl<CUnBindDlg>,
	public CWinDataExchange<CUnBindDlg>
{
protected:
	CNBListViewCtrl m_wndListUnbind;
	CNBLogicalDevice *m_pLogicalDeviceUnbind;
public:
	int IDD;
	CUnBindDlg();

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

	void SetDiskToUnbind(CNBLogicalDevice *obj);
};

#endif // _NBUNBINDDLG_H_