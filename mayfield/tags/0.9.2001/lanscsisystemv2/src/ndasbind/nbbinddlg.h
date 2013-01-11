////////////////////////////////////////////////////////////////////////////
//
// Interface of CBindDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _NBBINDDLG_H_
#define _NBBINDDLG_H_

#include "ndasobject.h"
#include "resource.h"
#include "nblistview.h"
#include "nbarrowbutton.h"
class CBindDlg : 
	public CDialogImpl<CBindDlg>,
	public CWinDataExchange<CBindDlg>
{
protected:
	BOOL				m_bUseMirror;
	CNBListViewCtrl	m_wndListSingle;
	CNBListViewCtrl	m_wndListPrimary;
	CNBListViewCtrl	m_wndListMirror;

	CArrowButton<ARROW_BOTTOM>	m_btnToPrimary;
	CArrowButton<ARROW_TOP>	m_btnFromPrimary;
	CArrowButton<ARROW_BOTTOM>  m_btnToMirror;
	CArrowButton<ARROW_TOP> m_btnFromMirror;

	CDiskObjectList		m_singleDisks;
	CDiskObjectVector	m_boundDisks;
	//
	// Change size of controls 
	// when the user selects mirroring, list of mirroring disk will be displayed
	// when the user deselects mirroring, list of mirroring disk will be hidden
	//
	void AdjustControls(BOOL bMirror);
	//
	// Enable/Disable controls
	//
	void UpdateControls();
public:
	CBindDlg();
	enum { IDD = IDD_BIND };

	BEGIN_DDX_MAP(CBindDlg)
		DDX_CHECK(IDC_CHK_MIRROR, m_bUseMirror)
	END_DDX_MAP()

	BEGIN_MSG_MAP(CBindDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_CHK_MIRROR, OnClickUseMirror)
		COMMAND_ID_HANDLER_EX(IDC_BTN_TO_PRIMARY, OnClickMove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_FROM_PRIMARY, OnClickMove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_TO_MIRROR, OnClickMove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_FROM_MIRROR, OnClickMove)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/);
	void OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/);
	void OnClickUseMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/);
	void OnClickMove(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/);

	// Methods
	// Sets the list of single disks used for binding
	void SetSingleDisks(CDiskObjectList singleDisks) { m_singleDisks = singleDisks; }

	// Methods that return results
	BOOL GetUseMirror() { return m_bUseMirror; }
	CDiskObjectVector GetBoundDisks(){ return m_boundDisks; }
};

#endif // _NBBINDDLG_H_
