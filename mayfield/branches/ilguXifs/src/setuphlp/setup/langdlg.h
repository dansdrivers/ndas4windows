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
UINT GetResourceID(LANGID langID)
{
	const struct {
		UINT ResID;
		LANGID LangID;
	} Languages[] = {
		IDS_ENU, 1033,
		IDS_DEU, 1031,
		IDS_FRA, 1036,
		IDS_ITA, 1040,
		IDS_ESN, 1034,
		IDS_PTG, 2070,
		IDS_JPN, 1041,
		IDS_KOR, 1042
	};

	for (int i = 0; i < RTL_NUMBER_OF(Languages); ++i)
	{
		if (Languages[i].LangID == langID)
		{
			return Languages[i].ResID;
		}
	}
	return 0;
}

inline 
LRESULT
CLanguageSelectionDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndLangList.Attach(GetDlgItem(IDC_LANGUAGE));
	CenterWindow();

	int selIndex = 0;
	int size = m_langIdArray.GetSize();
	CString str;
	for (int i = 0; i < size; ++i)
	{
		LANGID langId = m_langIdArray[i];
		ATLVERIFY(str.LoadString(GetResourceID(langId)));
		int index = m_wndLangList.AddString(str);
		m_wndLangList.SetItemData(index, langId);
		if (m_selLangID == langId) 
		{
			selIndex = index;
		}
	}

	m_wndLangList.SetCurSel(selIndex);
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

LANGID 
CLanguageSelectionDlg::GetSelectedLangID()
{
	return m_selLangID;
}

void 
CLanguageSelectionDlg::SetSelectedLangID(LANGID wLangID)
{
	m_selLangID = wLangID;
}

