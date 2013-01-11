#include "stdafx.h"
#include "resource.h"

#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
#include "atlctrlxp.h"
#pragma warning(default: 4312)
#pragma warning(default: 4244)
#include "atlctrlxp2.h"
#include "appconf.h"
#include "optionpsh.h"
#include "muisel.h"
#include "autores.h"
#include <ndas/ndasuser.h>
#include <ndas/ndassvcparam.h>

static APP_OPT_VALUE_DEF _appConfirmOptValues[] = {

	{ _T("DontConfirmExit"), IDS_OG_DONT_CONFIRM_EXIT, 
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE },

	{ _T("DontConfirmRemoveWriteKey"), IDS_OG_DONT_CONFIRM_REMOVE_WRITE_KEY, 
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE },

	{ _T("DontConfirmUnregister"), IDS_OG_DONT_CONFIRM_UNREGISTER, 
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE },

	{ _T("DontConfirmUnmount"), IDS_OG_DONT_CONFIRM_UNMOUNT, 
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE },

	{ _T("DontConfirmUnmountAll"), IDS_OG_DONT_CONFIRM_UNMOUNT_ALL, 
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE }
};

static APP_OPT_VALUE_DEF _appMenuOptValues[] = {

	{ _T("UseRegWizard"), IDS_OG_MENU_USE_REG_WIZARD, 
		APP_OPT_VALUE_DEF::AOV_BOOL, TRUE, TRUE, TRUE },

	{ _T("ShowUnmountAll"), IDS_OG_MENU_DISPLAY_UNMOUNT_ALL,
		APP_OPT_VALUE_DEF::AOV_BOOL, FALSE, FALSE, FALSE },

	{ _T("ShowDeviceStatusText"), IDS_OG_MENU_DISPLAY_STATUS_TEXT, 
		APP_OPT_VALUE_DEF::AOV_BOOL, TRUE, TRUE, TRUE }
};

static VOID 
pLoadAppOptValues(DWORD nDefs, PAPP_OPT_VALUE_DEF pOptValueDef)
{
	for (DWORD i = 0; i < nDefs; ++i) {

		PAPP_OPT_VALUE_DEF p = &pOptValueDef[i];

		if (APP_OPT_VALUE_DEF::AOV_BOOL == p->ovType) {

			BOOL fSuccess = pGetAppConfigValue(
				p->szValueName, 
				&p->CurrentValue.fValue);

			if (!fSuccess) {
				p->CurrentValue.fValue = p->DefaultValue.fValue;
			}

			p->NewValue.fValue = p->CurrentValue.fValue;

		} else if (APP_OPT_VALUE_DEF::AOV_DWORD == p->ovType) {

			BOOL fSuccess = pGetAppConfigValue(
				p->szValueName, 
				&p->CurrentValue.dwValue);

			if (!fSuccess) {
				p->CurrentValue.dwValue = p->DefaultValue.dwValue;
			}

			p->NewValue.dwValue = p->CurrentValue.dwValue;

		} else {
			// ignore invalid ones
			continue;
		}
	}
}

static VOID 
pSaveAppOptValues(DWORD nDefs, PAPP_OPT_VALUE_DEF pOptValueDef)
{
	//
	// save changes if the new value and the current values is different
	//
	for (DWORD i = 0; i < nDefs; ++i) {

		PAPP_OPT_VALUE_DEF p = &pOptValueDef[i];

		if (APP_OPT_VALUE_DEF::AOV_BOOL == p->ovType) {

			if (p->CurrentValue.fValue == p->NewValue.fValue) {
				continue;
			}

			(VOID) pSetAppConfigValueBOOL(
				p->szValueName, 
				p->NewValue.fValue);

		} else if (APP_OPT_VALUE_DEF::AOV_DWORD == p->ovType) {

			if (p->CurrentValue.dwValue == p->NewValue.dwValue) {
				continue;
			}

			(VOID) pSetAppConfigValue(
				p->szValueName, 
				p->NewValue.dwValue);

		} else {
			// ignore invalid ones
			continue;
		}
	}
}

static BOOL
pLoadAvailableLanguageList(CComboBox& wndList, LANGID curLangID);

CAppOptPropSheet::CAppOptPropSheet(
	_U_STRINGorID title, 
	UINT uStartPage, 
	HWND hWndParent) :
	m_bCentered(FALSE)
{
	CPropertySheetImpl<CAppOptPropSheet>(
		MAKEINTRESOURCE(IDS_OPTIONDLG_TITLE),
		uStartPage,
		hWndParent);

	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_USEPAGELANG;

	AddPage(m_pgGeneral);
	AddPage(m_pgAdvanced);
}

LRESULT 
CAppOptPropSheet::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	return TRUE;
}

VOID 
CAppOptPropSheet::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow && !m_bCentered) {
		// Center Windows only once!
		m_bCentered = TRUE;
		CenterWindow();
	}
	SetMsgHandled(FALSE);
}


LRESULT 
CAppOptGeneralPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	BOOL fSuccess = FALSE;

	m_wndUILangList.Attach(GetDlgItem(IDC_UILANG));

	CString str;
	INT nIndex = 0;

	str.LoadString(IDS_LANG_AUTO);
	nIndex = m_wndUILangList.AddString(str);
	m_wndUILangList.SetItemData(nIndex, 0);

	DWORD dwConfigLangID;
	m_wConfigLangID = 0;
	fSuccess = pGetAppConfigValue(_T("Language"),&dwConfigLangID);
	if (fSuccess) {
		m_wConfigLangID = (LANGID) dwConfigLangID;
	}

	m_wndUILangList.SetCurSel(0);
	pLoadAvailableLanguageList(m_wndUILangList, m_wConfigLangID);

	m_wndAlertDisconnect.Attach(GetDlgItem(IDC_ALERT_DISCONNECT));
	m_wndAlertReconnect.Attach(GetDlgItem(IDC_ALERT_RECONNECT));
//	m_wndRemountOnBoot.Attach(GetDlgItem(IDC_REMOUNT_ON_BOOT));

	BOOL fUseDisconnectAlert = TRUE;
	BOOL fUseReconnectAlert = TRUE;

	fSuccess = pGetAppConfigValue(_T("UseDisconnectAlert"), &fUseDisconnectAlert);
	if (fSuccess && !fUseDisconnectAlert) {
		m_wndAlertDisconnect.SetCheck(BST_UNCHECKED);
	} else {
		m_wndAlertDisconnect.SetCheck(BST_CHECKED);
	}

	fSuccess = pGetAppConfigValue(_T("UseReconnectAlert"), &fUseReconnectAlert);
	if (fSuccess && !fUseReconnectAlert) {
		m_wndAlertReconnect.SetCheck(BST_UNCHECKED);
	} else {
		m_wndAlertReconnect.SetCheck(BST_CHECKED);
	}

	return TRUE;
}

INT
CAppOptGeneralPage::OnApply()
{
	BOOL fSuccess = FALSE;

	ATLTRACE(_T("CAppOptGeneral::OnApply\n"));

	BOOL fUseDisconnectAlert = (BST_CHECKED == m_wndAlertDisconnect.GetCheck());
	BOOL fUseReconnectAlert = (BST_CHECKED == m_wndAlertReconnect.GetCheck());

	pSetAppConfigValue(_T("UseDisconnectAlert"), fUseDisconnectAlert);
	pSetAppConfigValue(_T("UseReconnectAlert"), fUseReconnectAlert);

	INT nSelItem = m_wndUILangList.GetCurSel();
	DWORD_PTR dwSelLangID = m_wndUILangList.GetItemData(nSelItem);
	if (m_wConfigLangID != (LANGID) dwSelLangID) {

		pSetAppConfigValue(_T("Language"), (DWORD)dwSelLangID);

		CString strTitle, strMessage;
		fSuccess = strTitle.LoadString(IDS_MAIN_TITLE);
		ATLASSERT(fSuccess);
		fSuccess = strMessage.LoadString(IDS_LANGUAGE_CHANGE);
		ATLASSERT(fSuccess);

		(VOID) MessageBox(
			strMessage,
			strTitle,
			MB_OK | MB_ICONINFORMATION);

	}


	return PSNRET_NOERROR;
}

LRESULT
CAppOptAdvancedPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	BOOL fSuccess;

	m_images.Create(IDB_PROPTREE, 16, 1, RGB(255,0,255));

	m_tree.SubclassWindow( GetDlgItem(IDC_OPTION_TREE) );
	m_tree.SetImageList(m_images, TVSIL_NORMAL);
	m_tree.SetExtendedTreeStyle(PTS_EX_NOCOLLAPSE);

	CString str;
	fSuccess = str.LoadString(IDS_OPTION_GROUP_CONFIRM);
	ATLASSERT(fSuccess);
	
	HTREEITEM hItem;
	hItem = m_tree.InsertItem(PropCreateReadOnlyItem(str),13,13,TVI_ROOT);

	m_tiGroupConfirm = hItem;

	DWORD nConfirm = RTL_NUMBER_OF(_appConfirmOptValues);
	pLoadAppOptValues(nConfirm, _appConfirmOptValues);

	for (DWORD i = 0; i < nConfirm; ++i) {
		PAPP_OPT_VALUE_DEF p = &_appConfirmOptValues[i];
		fSuccess = str.LoadString(p->nResID);
		ATLASSERT(fSuccess);
		HTREEITEM hNewItem = m_tree.InsertItem(PropCreateCheckmark(str),0, 0, hItem);
		m_tree.SetItemData(hNewItem, (DWORD_PTR)p);
		m_tree.SetCheckState(hNewItem, p->CurrentValue.fValue);
	}

	fSuccess = str.LoadString(IDS_OPTION_GROUP_MENU);
	ATLASSERT(fSuccess);
	hItem = m_tree.InsertItem(PropCreateReadOnlyItem(str),13,13,TVI_ROOT);

	m_tiGroupMisc = hItem;

	DWORD nMiscOpts = RTL_NUMBER_OF(_appMenuOptValues);
	pLoadAppOptValues(nMiscOpts, _appMenuOptValues);

	for (DWORD i = 0; i < nMiscOpts; ++i) {
		PAPP_OPT_VALUE_DEF p = &_appMenuOptValues[i];
		fSuccess = str.LoadString(p->nResID);
		ATLASSERT(fSuccess);
		HTREEITEM hNewItem = m_tree.InsertItem(PropCreateCheckmark(str),0, 0, hItem);
		m_tree.SetItemData(hNewItem, (DWORD_PTR)p);
		m_tree.SetCheckState(hNewItem, p->CurrentValue.fValue);
	}

	//
	// Hibernation Support options
	//
	NDAS_SERVICE_PARAM serviceParam = {0};
	fSuccess = ::NdasGetServiceParam(
		NDASSVC_PARAM_SUSPEND_PROCESS, 
		&serviceParam);

	m_tiGroupSuspend = NULL;

	if (!fSuccess) {
		//
		// if cannot retrieve the service param
		// hide the entire group
		//
		return 1;
	}

	m_dwSuspend = serviceParam.Value.DwordValue;
	NDASSVC_SUSPEND_DENY;
	NDASSVC_SUSPEND_ALLOW; //  == serviceParam.DwordValue;

	fSuccess = str.LoadString(IDS_OPTION_GROUP_SUSPEND);
	ATLASSERT(fSuccess);
	hItem = m_tree.InsertItem(PropCreateReadOnlyItem(str),13,13,TVI_ROOT);

	m_tiGroupSuspend = hItem;

	fSuccess = str.LoadString(IDS_OG_SUSPEND_DENY);
	ATLASSERT(fSuccess);
	HTREEITEM hItemSub = m_tree.InsertItem(PropCreateOptionCheck(str),2,2,hItem);
	m_tree.SetItemData(hItemSub, NDASSVC_SUSPEND_DENY);
	if (NDASSVC_SUSPEND_DENY == m_dwSuspend) {
		m_tree.SetCheckState(hItemSub, TRUE);
	} else {
		m_tree.SetCheckState(hItemSub, FALSE);
	}
	
	fSuccess = str.LoadString(IDS_OG_SUSPEND_ALLOW);
	ATLASSERT(fSuccess);
	hItemSub = m_tree.InsertItem(PropCreateOptionCheck(str),2,2,hItem);
	m_tree.SetItemData(hItemSub, NDASSVC_SUSPEND_ALLOW);
	if (NDASSVC_SUSPEND_ALLOW == m_dwSuspend) {
		m_tree.SetCheckState(hItemSub, TRUE);
	} else {
		m_tree.SetCheckState(hItemSub, FALSE);
	}

	// set initial focus
	return 1;
}

LRESULT 
CAppOptAdvancedPage::OnTreeNotify(LPNMHDR pnmh)
{
	BOOL fSuccess = FALSE;
	// pnmh->hwndFrom;
	
	if (IDC_OPTION_TREE != pnmh->idFrom) {
		// ignore other notification
		SetMsgHandled(FALSE);
		return TRUE;
	}

	if (PIN_CLICK != pnmh->code) {
		// ignore notifications other than PIN_CLICK
		SetMsgHandled(FALSE);
		return TRUE;
	}

	LPNMPROPERTYITEM pnmProp = reinterpret_cast<LPNMPROPERTYITEM>(pnmh);
	if (NULL == pnmProp->prop) {
		SetMsgHandled(FALSE);
		return TRUE;
	}

	IProperty* prop = pnmProp->prop;

	if (NDASSVC_SUSPEND_ALLOW == prop->GetItemData())
	{
		VARIANT varValue;
		::VariantInit(&varValue);
		fSuccess = prop->GetValue(&varValue);
		ATLASSERT(fSuccess);
		ATLASSERT(varValue.vt = VT_BOOL);
		if (!fSuccess || VT_BOOL != varValue.vt) {
			::VariantClear(&varValue);
			SetMsgHandled(FALSE);
			return TRUE;
		}

		//
		// Ignore the status of already checked.
		//
		if (varValue.boolVal) {
			::VariantClear(&varValue);
			SetMsgHandled(FALSE);
			return TRUE;
		}
		::VariantClear(&varValue);

		CString strText, strTitle;
		fSuccess = strText.LoadString(IDS_SUSPEND_WARNING);
		ATLASSERT(fSuccess);
		fSuccess = strTitle.LoadString(IDS_MAIN_TITLE);
		ATLASSERT(fSuccess);
		INT_PTR iResponse = ::MessageBox(
			::GetActiveWindow(),
			strText, 
			strTitle, 
			MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
		if (IDYES != iResponse) {
			SetMsgHandled(TRUE);
			return TRUE;
		}
	}

	SetMsgHandled(FALSE);
	return TRUE;
}

INT
CAppOptAdvancedPage::OnApply()
{
	//PSNRET_NOERROR; 
	//PSNRET_INVALID;
	//PSNRET_INVALID_NOCHANGEPAGE;

	ATLTRACE(_T("CAppOptAdvancedPage::OnApply\n"));

	HTREEITEM hItem = NULL;

	ATLASSERT(NULL != m_tiGroupConfirm);
	hItem = m_tree.GetChildItem(m_tiGroupConfirm);
	for (;NULL != hItem; hItem = m_tree.GetNextSiblingItem(hItem)) {
		DWORD_PTR dwPtr = m_tree.GetItemData(hItem);
		if (0 == dwPtr) {
			continue;
		}
		PAPP_OPT_VALUE_DEF pDef = reinterpret_cast<PAPP_OPT_VALUE_DEF>(dwPtr);
		pDef->NewValue.fValue = m_tree.GetCheckState(hItem);
	}

	ATLASSERT(NULL != m_tiGroupMisc);
	hItem = m_tree.GetChildItem(m_tiGroupMisc);
	for (;NULL != hItem; hItem = m_tree.GetNextSiblingItem(hItem)) {
		DWORD_PTR dwPtr = m_tree.GetItemData(hItem);
		if (0 == dwPtr) {
			continue;
		}
		PAPP_OPT_VALUE_DEF pDef = reinterpret_cast<PAPP_OPT_VALUE_DEF>(dwPtr);
		pDef->NewValue.fValue = m_tree.GetCheckState(hItem);
	}

	if (NULL != m_tiGroupSuspend) {
		hItem = m_tree.GetChildItem(m_tiGroupSuspend);
		for (;NULL != hItem; hItem = m_tree.GetNextSiblingItem(hItem)) {
			if (m_tree.GetCheckState(hItem)) {
				DWORD_PTR dwPtr = m_tree.GetItemData(hItem);
				NDAS_SERVICE_PARAM serviceParam = {0};
				serviceParam.ParamCode = NDASSVC_PARAM_SUSPEND_PROCESS;
				serviceParam.Value.DwordValue = (DWORD)(dwPtr);
				::NdasSetServiceParam(&serviceParam);
				break;
			}
		}
	}

	DWORD nConfirmOpts = RTL_NUMBER_OF(_appConfirmOptValues);
	DWORD nMiscOpts = RTL_NUMBER_OF(_appMenuOptValues);
	pSaveAppOptValues(nConfirmOpts, _appConfirmOptValues);
	pSaveAppOptValues(nMiscOpts, _appMenuOptValues);

	return PSNRET_NOERROR;
}


static BOOL
pLoadAvailableLanguageList(CComboBox& wndList, LANGID curLangID)
{
	DWORD_PTR cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, 
		NULL, 
		0, 
		NULL);

	if (cbResDlls == 0) {
		return FALSE;
	}

	LPVOID lpResDlls = ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbResDlls);

	if (NULL == lpResDlls) {
		return FALSE;
	}

	AutoProcessHeap autoResDlls = lpResDlls;
	PNUI_RESDLL_INFO pResDlls = static_cast<PNUI_RESDLL_INFO>(lpResDlls);

	cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, 
		NULL, 
		cbResDlls, 
		pResDlls);

	if (0 == cbResDlls) {
		return FALSE;
	}

	DWORD nAvailLangID = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++nAvailLangID)
	{
		HMODULE hModule = ::LoadLibrary(pCur->lpszFilePath);
		if (NULL == hModule) {
			continue;
		}
		TCHAR szLanguage[100];
		BOOL fSuccess = ::LoadString(
			hModule, 
			IDS_CURRENT_LANGUAGE,
			szLanguage,
			100);

		if (fSuccess) {
			INT nItem = wndList.AddString(szLanguage);
			wndList.SetItemData(nItem, pCur->wLangID);
			if (curLangID == pCur->wLangID) {
				wndList.SetCurSel(nItem);
			}
		}

		::FreeLibrary(hModule);

		ATLTRACE(_T("Available Resource: %d(%08X) %s\n"), 
			pCur->wLangID,
			pCur->wLangID,
			pCur->lpszFilePath);
	}

	return TRUE;
}

