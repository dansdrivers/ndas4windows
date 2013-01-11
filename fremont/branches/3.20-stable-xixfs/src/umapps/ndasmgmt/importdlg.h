#pragma once
#include <ndas/ndasnif.h>
#include <atlsimpcoll.h>
#include "resource.h"

struct NifEntry
{
	DWORD_PTR Flags;
	CString Name;
	CString DeviceId;
	CString WriteKey;
	CString Description;
	
	NifEntry(const NDAS_NIF_V1_ENTRY& e) : 
		Flags(e.Flags), 
		Name(e.Name),
		DeviceId(e.DeviceId),
		WriteKey(e.WriteKey),
		Description(e.Description) {}

};

inline bool operator==(const NifEntry& left, const NifEntry& right)
{
	return 0 == ::lstrcmpi(left.DeviceId, right.DeviceId);
}

typedef CSimpleArray<NifEntry> NifEntryArray;

class CImportDlg : 
	public CDialogImpl<CImportDlg>,
	public CMessageFilter
{
public:
	enum { IDD = IDD_IMPORT };
	
	BEGIN_MSG_MAP_EX(CImportDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnCmdRegister)
		COMMAND_ID_HANDLER_EX(IDCLOSE, OnCmdClose)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCmdClose)
		NOTIFY_HANDLER_EX(IDC_IMPORT_LIST,LVN_ITEMCHANGING,OnListViewItemChanging)
		NOTIFY_HANDLER_EX(IDC_IMPORT_LIST,LVN_ITEMCHANGED,OnListViewItemChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnCmdRegister(UINT nNotifyCode, int nID, HWND hWndCtl);
	void OnCmdClose(UINT nNotifyCode, int nID, HWND hWndCtl);
	void OnSelChange(UINT nNotifyCode, int nID, HWND hWndCtl);

	LRESULT OnListViewItemChanging(LPNMHDR lpnmhdr);
	LRESULT OnListViewItemChanged(LPNMHDR lpnmhdr);
	//
	// Modeless Dialog Support
	//
	virtual BOOL PreTranslateMessage(MSG* pMsg) 
	{
        return IsDialogMessage(pMsg);
    }

    virtual void OnFinalMessage(HWND /*hWnd*/)
    {
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        pLoop->RemoveMessageFilter(this);
		ATLTRACE("CImportDlg FinalMessage.\n");
		m_wndListView.Detach();
		m_wndRegister.Detach();
		m_wndDescription.Detach();
		m_wndClose.Detach();
		m_hWnd = NULL;
    }


	void AddNifEntry(const NifEntryArray& array);

private:

	CListViewCtrl m_wndListView;
	CEdit m_wndDescription;
	CButton m_wndRegister;
	CButton m_wndClose;

	NifEntryArray m_entries;

	CRect m_rcItem;

	enum ItemState { ISNew, ISRegistered, ISInvalid };
	void _SetDescription(const NifEntry& entry);
	bool _ProcessRegistration(NifEntry& entry);
	void _UpdateView();
	void _UpdateView(int nItem, const NifEntry& entry, bool modifyCheck = false);
};

