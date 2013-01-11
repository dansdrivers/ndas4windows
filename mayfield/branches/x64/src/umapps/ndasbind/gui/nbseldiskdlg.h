////////////////////////////////////////////////////////////////////////////
//
// Interface of CSelectDiskDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBSELDISKDLG_H_
#define _NBSELDISKDLG_H_

#include "resource.h"
#include "ndasobject.h"
#include "nblistviewctrl.h"

class CSelectDiskDlg :
	public CDialogImpl<CSelectDiskDlg>,
	public CWinDataExchange<CSelectDiskDlg>
{
protected:
	CDiskObjectList m_singleDisks;
	CUnitDiskObjectPtr m_pSelectedDisk;

	CNBListViewCtrl m_wndListSingle;
public:
	int IDD;
	CSelectDiskDlg(int nDialogID)
	{
		IDD = nDialogID;
	}

	BEGIN_DDX_MAP(CSelectDiskDlg)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CSelectDiskDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnOK(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnCancel(UINT wNotifyCode, int wID, HWND hwndCtl);

	// Sets the list of single disks used for reestablishing mirror
	void SetSingleDisks(CDiskObjectList singleDisks) { m_singleDisks = singleDisks; }
	
	CUnitDiskObjectPtr GetSelectedDisk() { return m_pSelectedDisk; }
};



#endif // _NBSELDISKDLG_H_