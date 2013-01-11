#pragma once
#include "resource.h"
#include <atlsimpcoll.h>

class CLanguageSelectionDlg : 
	public CDialogImpl<CLanguageSelectionDlg>
{
public:
	enum { IDD = IDD_SETUP_LANGUAGE };

	BEGIN_MSG_MAP_EX(CLanguageSelectionDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnOK(UINT, int, HWND);
	void OnCancel(UINT, int, HWND);

	LANGID GetSelectedLangID();
	void SetSelectedLangID(LANGID wLangID);
	void SetLanguages(const CSimpleArray<LANGID>& langIdArray);

private:
	LANGID m_selLangID;
	CComboBox m_wndLangList;
	CSimpleArray<LANGID> m_langIdArray;
};

inline
void
CLanguageSelectionDlg::SetLanguages(
	const CSimpleArray<LANGID>& langIdArray)
{
	m_langIdArray = langIdArray;
}

inline 
LRESULT
CLanguageSelectionDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndLangList.Attach(GetDlgItem(IDC_LANGUAGE));
	CenterWindow();

	int selIndex = 0;
	int size = m_langIdArray.GetSize();
	for (int i = 0; i < size; ++i)
	{
		LANGID langId = m_langIdArray[i];
		LCID lcid = MAKELCID(langId, SORT_DEFAULT);
		CString langName;
		int used = ::GetLocaleInfo(lcid, LOCALE_SNATIVELANGNAME, langName.GetBuffer(128), 128);
		langName.ReleaseBuffer();
		if (used > 0)
		{
			//CString enName;
			//used = ::GetLocaleInfo(lcid, LOCALE_SLANGUAGE/*LOCALE_SENGLANGUAGE*/, enName.GetBuffer(128), 128);
			//enName.ReleaseBuffer();
			//if (used > 0)
			//{
			//	langName += " - " + enName;
			//}
			int index = m_wndLangList.AddString(langName);
			m_wndLangList.SetItemData(index, langId);
			if (m_selLangID == langId) 
			{
				selIndex = index;
				m_wndLangList.SetCurSel(selIndex);
			}
			ATLTRACE("%d: %ls\n", index, langName);
		}
		// ATLVERIFY(str.LoadString(GetResourceID(langId)));
	}

	return TRUE;
}

inline void 
CLanguageSelectionDlg::OnOK(UINT wNotifyCode, int nID, HWND hWndCtl)
{
	int index = m_wndLangList.GetCurSel();
	DWORD_PTR itemData = m_wndLangList.GetItemData(index);
	m_selLangID = static_cast<LANGID>(itemData);

	EndDialog(nID);
}

inline void 
CLanguageSelectionDlg::OnCancel(UINT wNotifyCode, int nID, HWND hWndCtl)
{
	EndDialog(nID);
}

inline LANGID 
CLanguageSelectionDlg::GetSelectedLangID()
{
	return m_selLangID;
}

inline void 
CLanguageSelectionDlg::SetSelectedLangID(LANGID wLangID)
{
	m_selLangID = wLangID;
}

