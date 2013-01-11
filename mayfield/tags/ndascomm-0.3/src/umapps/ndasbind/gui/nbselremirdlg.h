////////////////////////////////////////////////////////////////////////////
//
// Interface of CSelectMirDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBSELREMIRDLG_H_
#define _NBSELREMIRDLG_H_

#include "resource.h"
#include "ndasobject.h"
#include "nblistviewctrl.h"

class CSelectMirDlg :
	public CDialogImpl<CSelectMirDlg>,
	public CWinDataExchange<CSelectMirDlg>
{
protected:
	CDiskObjectList m_singleDisks;
	CUnitDiskObjectPtr m_pSelectedDisk;
	CUnitDiskObjectPtr m_pSourceDisk;

	CNBListViewCtrl m_wndListSingle;
public:
	int IDD;
	CSelectMirDlg(int nDialogID)
	{
		IDD = nDialogID;
	}


	BEGIN_DDX_MAP(CSelectMirDlg)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CSelectMirDlg)
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
	void SetSourceDisk(CUnitDiskObjectPtr source) { m_pSourceDisk = source; }

	CUnitDiskObjectPtr GetSelectedDisk() { return m_pSelectedDisk; }
};


#endif // _NBSELREMIRDLG_H_
