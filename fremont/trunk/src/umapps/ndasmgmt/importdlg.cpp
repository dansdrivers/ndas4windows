#include "stdafx.h"
#include "importdlg.h"
#include "ndascls.h"
#include "ndasdevicerenamedlg.h"
#include "apperrdlg.h"

LRESULT 
CImportDlg::OnInitDialog(HWND hWndCtl, LPARAM lParam)
{
	m_wndListView.Attach(GetDlgItem(IDC_IMPORT_LIST));
	m_wndRegister.Attach(GetDlgItem(IDOK));
	m_wndDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	m_wndClose.Attach(GetDlgItem(IDCLOSE));

	m_wndListView.SetExtendedListViewStyle(
		LVS_EX_CHECKBOXES | 
		LVS_EX_FULLROWSELECT);

	m_wndListView.ModifyStyle(0, LVS_SHOWSELALWAYS | LVS_SINGLESEL);

	m_entries.RemoveAll();

	m_wndListView.AddColumn(CString(MAKEINTRESOURCE(IDS_IMPORT_COL_NAME)), 0);
	m_wndListView.SetColumnWidth(0, 230);
	m_wndListView.AddColumn(CString(MAKEINTRESOURCE(IDS_IMPORT_COL_STATUS)), 1);
	m_wndListView.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);

	const NifEntryArray* pInitEntries = 
		reinterpret_cast<const NifEntryArray*>(lParam);
	if (NULL != pInitEntries)
	{
		AddNifEntry(*pInitEntries);
	}
	if (m_wndListView.GetItemCount() > 0)
	{
		m_wndListView.SelectItem(0);
	}

    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != NULL);
    pLoop->AddMessageFilter(this);

	return TRUE;
}

void 
CImportDlg::OnCmdRegister(UINT, int, HWND)
{
	int count = m_wndListView.GetItemCount();
	bool success = true;
	for (int i = 0; i < count && success; ++i)
	{
#ifdef _DEBUG
		CString str;
		m_wndListView.GetItemText(i, 0, str.GetBuffer(50), 50);
		ATLTRACE("%d:LV=%ws, ENTRY=%ws\n", i, str, static_cast<LPCTSTR>(m_entries[i].Name));
#endif
		if (m_wndListView.GetCheckState(i))
		{
			ATLASSERT(i < m_entries.GetSize());
			success = _ProcessRegistration(m_entries[i]);
		}
	}
	_UpdateView();
}

void 
CImportDlg::OnCmdClose(UINT /*nNotifyCode*/, int nID, HWND /*hWndCtl*/)
{
	m_entries.RemoveAll();
	DestroyWindow();
}

void 
CImportDlg::OnSelChange(UINT, int, HWND)
{

}

void 
CImportDlg::_UpdateView()
{
	ndas::UpdateDeviceList();
	int size = m_entries.GetSize();
	for (int i = 0; i < size; ++i)
	{
		_UpdateView(i, m_entries[i]);
	}
}

void 
CImportDlg::_UpdateView(int nItem, const NifEntry& entry, bool modifyCheck)
{
	bool fRegister = false;
	m_wndListView.SetItemText(nItem, 0, entry.Name);
	ndas::DevicePtr p;
	if (ndas::FindDeviceByNdasId(p, entry.DeviceId))
	{
		m_wndListView.SetCheckState(nItem, FALSE);
		m_wndListView.SetItemText(nItem, 1, CString(MAKEINTRESOURCE(IDS_NIF_REGISTERED)));
		m_wndListView.SetItemData(nItem, ISRegistered);
	}
	else
	{
		LPCTSTR lpWriteKey = 
			entry.WriteKey.GetLength() > 0 ? 
			static_cast<LPCTSTR>(entry.WriteKey) :
			static_cast<LPCTSTR>(NULL);
		if (::NdasValidateStringIdKey(entry.DeviceId, lpWriteKey))
		{
			m_wndListView.SetItemText(nItem, 1, CString(MAKEINTRESOURCE(IDS_NIF_NEW)));
			m_wndListView.SetItemData(nItem, ISNew);
			fRegister = true;
		}
		else
		{
			m_wndListView.SetCheckState(nItem, FALSE);
			m_wndListView.SetItemText(nItem, 1, CString(MAKEINTRESOURCE(IDS_NIF_INVALID)));
			m_wndListView.SetItemData(nItem, ISInvalid);
		}
	}
	if (fRegister)
	{
		m_wndRegister.SetButtonStyle(BS_DEFPUSHBUTTON);
		m_wndClose.SetButtonStyle(BS_PUSHBUTTON);
	}
	else
	{
		m_wndRegister.SetButtonStyle(BS_PUSHBUTTON);
		m_wndClose.SetButtonStyle(BS_DEFPUSHBUTTON);
	}
}

void
CImportDlg::AddNifEntry(const NifEntryArray& array)
{
	int entrySize = m_entries.GetSize();
	int size = array.GetSize();
	for (int i = 0; i < size; ++i)
	{
		NifEntry entry = array[i];
		
		entry.Name.TrimLeft();
		entry.Name.TrimRight();
		entry.DeviceId.TrimLeft();
		entry.DeviceId.TrimRight();
		entry.DeviceId.Remove(_T('-'));
		entry.DeviceId.Remove(_T(' '));
		entry.DeviceId.Remove(_T(':'));
		entry.WriteKey.TrimLeft();
		entry.WriteKey.TrimRight();

		if (-1 == m_entries.Find(entry))
		{
			if (!m_entries.Add(entry))
			{
				ATLTRACE("m_entries.Add failed\n");
				continue;
			}
			int nItem = m_wndListView.AddItem(entrySize, 0, entry.Name);
			ATLASSERT(nItem == entrySize);
			m_wndListView.AddItem(entrySize, 1, _T(""));
			m_wndListView.SetCheckState(entrySize, TRUE);
			_UpdateView(nItem, entry);
			++entrySize;
		}
	}
}

LRESULT 
CImportDlg::OnListViewItemChanging(LPNMHDR lpnmhdr)
{
	LPNMLISTVIEW lpnmlv = reinterpret_cast<LPNMLISTVIEW>(lpnmhdr);
	ATLTRACE("Item=%d,SubItem=%d,New=%08X,Old=%08X,Changed=%08X,Data=%p\n",
		lpnmlv->iItem, lpnmlv->iSubItem, lpnmlv->uNewState, lpnmlv->uOldState,
		lpnmlv->uChanged, lpnmlv->lParam);
	if (lpnmlv->uChanged == 0x8 && lpnmlv->uNewState == 0x2000)
	{
		if (lpnmlv->lParam != ISNew)
		{
			return TRUE;
		}
	}
	else if (lpnmlv->uChanged == 0x8 && (lpnmlv->uNewState & LVIS_FOCUSED))
	{
		int n = lpnmlv->iItem;
		if (n < m_entries.GetSize())
		{
			_SetDescription(m_entries[n]);
		}
	}
	return FALSE;
}

LRESULT 
CImportDlg::OnListViewItemChanged(LPNMHDR lpnmhdr)
{
	int count = m_wndListView.GetItemCount();
	for (int i = 0; i < count; ++i)
	{
		if (m_wndListView.GetCheckState(i))
		{
			m_wndRegister.EnableWindow(TRUE);
			return 0;
		}
	}
	m_wndRegister.EnableWindow(FALSE);
	return 0;
}

void
CImportDlg::_SetDescription(const NifEntry& entry)
{
	CString strDescription;
	strDescription.FormatMessage(
		IDS_NIF_DESCRIPTION_FMT,
		entry.DeviceId.Mid(0,5),
		entry.DeviceId.Mid(5,5),
		entry.DeviceId.Mid(10,5),
		entry.DeviceId.Mid(15,5),
		entry.WriteKey.IsEmpty() ? 
		static_cast<LPCTSTR>(CString(MAKEINTRESOURCE(IDS_WRITE_KEY_NONE))) :
		static_cast<LPCTSTR>(CString(MAKEINTRESOURCE(IDS_WRITE_KEY_PRESENT))),
		entry.Description);
	m_wndDescription.SetWindowText(strDescription);
}

bool
CImportDlg::_ProcessRegistration(NifEntry& entry)
{
	ndas::DevicePtr pExistingDevice;

	if (ndas::FindDeviceByNdasId(pExistingDevice, entry.DeviceId))
	{
		AtlTaskDialogEx(
			m_hWnd,
			IDS_MAIN_TITLE,
			IDS_ERROR_DUPLICATE_ENTRY,
			0U,
			TDCBF_OK_BUTTON,
			TD_WARNING_ICON);

		return false;
	}

	CNdasDeviceRenameDlg renameDlg;
	while (ndas::FindDeviceByName(pExistingDevice, entry.Name))
	{
		int response = AtlTaskDialogEx(
			m_hWnd,
			IDS_MAIN_TITLE,
			0U,
			IDS_ERROR_DUPLICATE_NAME,
			TDCBF_OK_BUTTON| TDCBF_CANCEL_BUTTON,
			TD_WARNING_ICON);

		if (IDOK == response)
		{
			renameDlg.SetName(entry.Name);
			renameDlg.DoModal(m_hWnd);
			entry.Name = renameDlg.GetName();
		}
		else
		{
			return false;
		}
	}

	LPCWSTR lpWriteKey = entry.WriteKey.GetLength() > 0 ? 
		static_cast<LPCTSTR>(entry.WriteKey) : NULL;

	DWORD dwSlotNo = ::NdasRegisterDevice(
		entry.DeviceId, lpWriteKey, 
		entry.Name, NDAS_DEVICE_REG_FLAG_NONE);

	// Registration failure will not close dialog
	if (0 == dwSlotNo) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_REGISTER_DEVICE_FAILURE);
		return false;
	}

	//
	// Enable on register is an optional feature
	// Even if it's failed, still go on to close.
	//
	if (!ndas::UpdateDeviceList()) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo)) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	if (!pDevice->Enable()) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	return true;
}
