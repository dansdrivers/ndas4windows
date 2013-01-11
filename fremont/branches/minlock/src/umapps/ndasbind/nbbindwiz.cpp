#include "stdafx.h"
#include "resource.h"
#include "nbbindwiz.h"
#include "ndas/ndasop.h"

#include "autocursor.h"

#include "apperrdlg.h"
#include "appconf.h"

class CDeviceIdEdit :
	public CWindowImpl<CDeviceIdEdit >
{
public:
	DECLARE_WND_SUPERCLASS(0, _T("EDIT"))

	BEGIN_MSG_MAP_EX(CDeviceIdEdit)
		MSG_WM_CHAR(OnChar)
	END_MSG_MAP()

	VOID OnChar(TCHAR nChar, UINT nRepCnt, UINT nFlags)
	{
//		SetMsgHandled(TRUE);
	}
};

UINT64 
pGetBoundDiskSize(int nBindType, int nDiskCount, NBUnitDevicePtrList &listUnitDevices)
{
	UINT64 nSize, nSizeMin = 0xFFFFFFFFFFFFFFFF, nSizeSum = 0;

	for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
		itUnitDevice != listUnitDevices.end(); itUnitDevice++)
	{
		nSize = (*itUnitDevice)->GetLogicalCapacityInByte();
		nSizeMin = (nSizeMin > nSize) ? nSize : nSizeMin;
		nSizeSum += nSize;
	}

	return 
		(NMT_AGGREGATE == nBindType) ? nSizeSum :
		(NMT_RAID0 == nBindType) ? nSizeMin * nDiskCount :
		(NMT_RAID1 == nBindType) ? nSizeMin * nDiskCount /2 :
		(NMT_RAID4 == nBindType) ? nSizeMin * (nDiskCount -1) :
		(NMT_RAID1R2 == nBindType) ? nSizeMin * nDiskCount /2 :
		(NMT_RAID4R2 == nBindType) ? nSizeMin * (nDiskCount -1) : 
		(NMT_RAID1R3 == nBindType) ? nSizeMin * nDiskCount /2 *199/200: // Reserve 0.5%. See NdasOpBind func.
		(NMT_RAID4R3 == nBindType) ? nSizeMin * (nDiskCount -1) :
		(NMT_RAID5 == nBindType) ? nSizeMin * (nDiskCount -1) : 0;
}

//
// return strSize with size text in GB
//
VOID
pGetDiskSizeString(CString &strSize, int nBindType, int nDiskCount, NBUnitDevicePtrList &listUnitDevices)
{
	UINT64 nSize = pGetBoundDiskSize(nBindType, nDiskCount, listUnitDevices);
	strSize = CNBDevice::GetCapacityString(nSize);
}

namespace nbbwiz {

static HFONT pGetTitleFont();

//////////////////////////////////////////////////////////////////////////
//
// Wizard Property Sheet
//
//////////////////////////////////////////////////////////////////////////

static const INT WIZARD_COMPLETION_INDEX = 4; 

CWizard::CWizard(HWND hWndParent) :
	m_fCentered(FALSE),
	m_pgIntro(hWndParent, &m_wizData),
	m_pgBindType(hWndParent, &m_wizData),
	m_pgSelectDisk(hWndParent, &m_wizData),
	m_pgComplete(hWndParent, &m_wizData),
	CPropertySheetImpl<CWizard>(IDS_BNZ_TITLE, 0, hWndParent)
{
	SetWizardMode();
	m_psh.dwFlags |= PSH_WIZARD97 | PSH_USEPAGELANG;
	SetWatermark(MAKEINTRESOURCE(IDB_WATERMARK256));
	SetHeader(MAKEINTRESOURCE(IDB_BANNER256));

	// StretchWatermark(true);
	AddPage(m_pgIntro);
	AddPage(m_pgBindType);
	AddPage(m_pgSelectDisk);
	AddPage(m_pgComplete);

//	::ZeroMemory(
//		&m_wizData, 
//		sizeof(WIZARD_DATA));
	m_wizData.ppsh = this;
	m_wizData.ppspComplete = &m_pgComplete;

	m_wizData.m_nBindType = NMT_AGGREGATE;
	m_wizData.m_nDiskCount = 2;
}

VOID 
CWizard::OnShowWindow(BOOL fShow, UINT nStatus)
{
	if (fShow && !m_fCentered) {
		// Center Windows only once!
		m_fCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

void CWizard::SetSingleDisks(NBUnitDevicePtrList &listUnitDevicesSingle)
{
	m_wizData.listUnitDevicesSingle = listUnitDevicesSingle;
}


//////////////////////////////////////////////////////////////////////////
//
// Intro Page
//
//////////////////////////////////////////////////////////////////////////

CIntroPage::CIntroPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CIntroPage>(IDS_BNZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
}


LRESULT 
CIntroPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());

	return 0;
}


//
// Non-zero if the page was successfully set active;
// otherwise 0
//
BOOL 
CIntroPage::OnSetActive()
{
/*
	CStatic stIntroCtl;
	stIntroCtl.Attach(GetDlgItem(IDC_INTRO_1));
	CString strIntro1;
	strIntro1.LoadString(IDS_DRZ_INTRO_1);
	stIntroCtl.SetWindowText(strIntro1);
*/

	SetWizardButtons(PSWIZB_NEXT);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Bind Type / Disk Number Page
//
//////////////////////////////////////////////////////////////////////////

CBindTypePage::CBindTypePage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	CPropertyPageImpl<CBindTypePage>(IDS_BNZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_BNZ_BIND_TYPE_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_BNZ_BIND_TYPE_HEADER_SUBTITLE));
}

LRESULT 
CBindTypePage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	UpdateControls(NMT_AGGREGATE);

	CButton btnDefault;

	//
	//	Check the default button
	//

	btnDefault.Attach(GetDlgItem(IDC_BIND_TYPE_AGGR));

	btnDefault.SetCheck(BST_CHECKED);

#ifdef _DISABLE_RAID4_RAID5_BTN_ON_BINDDLG_

	//
	//	Hide RAID4 button
	//

	CButton btnRaid4R;

	btnRaid4R.Attach(GetDlgItem(IDC_BIND_TYPE_RAID4));
	btnRaid4R.EnableWindow(FALSE);
	btnRaid4R.ModifyStyle(WS_VISIBLE, 0, 0);

	CButton btnRaid5R;

	btnRaid5R.Attach(GetDlgItem(IDC_BIND_TYPE_RAID5));
	btnRaid5R.EnableWindow(FALSE);
	btnRaid5R.ModifyStyle(WS_VISIBLE, 0, 0);

#endif

	// Let the dialog manager set the initial focus
	return 1;
}

LRESULT CBindTypePage::OnCommand(UINT msg, int nID, HWND hWnd)
{	
	if(BN_CLICKED == msg)
	{
		switch(nID)
		{
		case IDC_BIND_TYPE_AGGR:
			UpdateControls(NMT_AGGREGATE);
			break;
		case IDC_BIND_TYPE_RAID0:
			UpdateControls(NMT_RAID0);
			break;
		case IDC_BIND_TYPE_RAID1:
			UpdateControls(NMT_RAID1R3);
			break;
		case IDC_BIND_TYPE_RAID4:
			UpdateControls(NMT_RAID4R3);
			break;
		case IDC_BIND_TYPE_RAID5:
			UpdateControls(NMT_RAID5);
			break;
		default:
			break;
		}
	}
	return TRUE;
}

VOID CBindTypePage::UpdateControls (UINT32 nType)
{
	CComboBox ctlComboBox;
	ctlComboBox.Attach(GetDlgItem(IDC_COMBO_DISKCOUNT));
	ctlComboBox.ResetContent();

	CString strDesc;

	switch(nType)
	{
	case NMT_AGGREGATE :
		ctlComboBox.InsertString(-1, _T("2"));
		ctlComboBox.InsertString(-1, _T("3"));
		ctlComboBox.InsertString(-1, _T("4"));
		ctlComboBox.InsertString(-1, _T("5"));
		ctlComboBox.InsertString(-1, _T("6"));
		ctlComboBox.InsertString(-1, _T("7"));
		ctlComboBox.InsertString(-1, _T("8"));
		strDesc.LoadString(IDS_BNZ_DESC_AGGREGATION);
		break;
	case NMT_RAID0 :
		ctlComboBox.InsertString(-1, _T("2"));
		ctlComboBox.InsertString(-1, _T("4"));
		ctlComboBox.InsertString(-1, _T("8"));
		strDesc.LoadString(IDS_BNZ_DESC_RAID0);
		break;
	case NMT_RAID1R3 :
		ctlComboBox.InsertString(-1, _T("2"));
		strDesc.LoadString(IDS_BNZ_DESC_RAID1);
		break;
	case NMT_RAID4R3 :
		ctlComboBox.InsertString(-1, _T("3"));
		ctlComboBox.InsertString(-1, _T("5"));
		ctlComboBox.InsertString(-1, _T("9"));
		strDesc.LoadString(IDS_BNZ_DESC_RAID4);
		break;
	case NMT_RAID5 :
		ctlComboBox.InsertString(-1, _T("3"));
		ctlComboBox.InsertString(-1, _T("5"));
		ctlComboBox.InsertString(-1, _T("9"));
		strDesc.LoadString(IDS_BNZ_DESC_RAID5);
		break;
	default:
		break;
	}

	ctlComboBox.SetCurSel(0);

	CStatic ctlStatic;
	ctlStatic.Attach(GetDlgItem(IDC_BIND_TYPE_DESCRIPTION));
	ctlStatic.SetWindowText(strDesc);

}


BOOL
CBindTypePage::OnSetActive()
{
	SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);

	return TRUE;
}

// 0 to automatically advance to the next page
// -1 to prevent the page from changing
INT 
CBindTypePage::OnWizardNext()
{
/*
	BOOL fSuccess = m_wndDevName.GetWindowText(
		m_pWizData->szDeviceName,
		MAX_NDAS_DEVICE_NAME_LEN + 1);

	ATLASSERT(fSuccess);
*/
	DoDataExchange(TRUE);

	/* m_nBindType is radio button number. Keep these number is matched to dialog resource */
	m_pWizData->m_nBindType = 
		(m_nBindType == 0) ? NMT_AGGREGATE :
		(m_nBindType == 1) ? NMT_RAID0 :
		(m_nBindType == 2) ? NMT_RAID1R3 : 
		(m_nBindType == 3) ? NMT_RAID4R3 :
		(m_nBindType == 4) ? NMT_RAID5 : 
			NMT_AGGREGATE; /* Default to aggregate */
	m_pWizData->m_nDiskCount = m_nDiskCount;

	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Select disk Page
//
//////////////////////////////////////////////////////////////////////////

CSelectDiskPage::CSelectDiskPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData), m_wndListSingle(2, TRUE), m_wndListBind(2, FALSE),
	CPropertyPageImpl<CSelectDiskPage>(IDS_BNZ_TITLE)
{
	SetHeaderTitle(MAKEINTRESOURCE(
		IDS_BNZ_SELECT_DISK_HEADER_TITLE));
	SetHeaderSubTitle(MAKEINTRESOURCE(
		IDS_BNZ_SELECT_DISK_HEADER_SUBTITLE));
}

LRESULT 
CSelectDiskPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_wndListSingle.SubclassWindow( GetDlgItem(IDC_LIST_SINGLE) );
	m_wndListSingle.SetExtendedListViewStyle( LVS_EX_FULLROWSELECT );
	m_wndListSingle.InitColumn();
	m_wndListSingle.AddDiskObjectList(m_pWizData->listUnitDevicesSingle);

	m_wndListBind.SubclassWindow( GetDlgItem(IDC_LIST_BIND) );
	m_wndListBind.SetExtendedListViewStyle( LVS_EX_FULLROWSELECT );
	m_wndListBind.InitColumn();

	UpdateControls();
	// Let the dialog manager set the initial focus
	return 1;
}

void 
CSelectDiskPage::OnClickAddRemove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	CNBListViewCtrl *pWndFrom, *pWndTo;

	switch(wID)
	{
	case IDC_BTN_ADD:
		pWndFrom = &m_wndListSingle;
		pWndTo = &m_wndListBind;
		break;
	case IDC_BTN_REMOVE_ALL:
		m_wndListBind.SelectAllDiskObject();
	case IDC_BTN_REMOVE:
		pWndFrom = &m_wndListBind;
		pWndTo = &m_wndListSingle;
		break;
	default:
		return;
	}

	NBUnitDevicePtrList selectedDisks = pWndFrom->GetSelectedDiskObjectList();
	if ( selectedDisks.size() == 0 )
		return;
	pWndFrom->DeleteDiskObjectList( selectedDisks );
	pWndTo->AddDiskObjectList( selectedDisks );
	pWndTo->SelectDiskObjectList( selectedDisks );

/*
	for ( UINT i=0; i < selectedDisks.size(); i++ )
	{
		if ( bFromSingle )
			m_wndDiskList.InsertItem();
		else
			m_wndDiskList.DeleteItem();
	}
*/
	UpdateControls();	
}

LRESULT CSelectDiskPage::OnListSelChanged(LPNMHDR /*lpNMHDR*/)
{
	UpdateControls();
	return 0;
}

LRESULT CSelectDiskPage::OnDblClkList(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case IDC_LIST_SINGLE:
		if (m_wndListBind.GetDiskObjectList().size() < m_pWizData->m_nDiskCount) {
			OnClickAddRemove(BN_CLICKED, IDC_BTN_ADD, NULL);
		}
		break;
	case IDC_LIST_BIND:
		OnClickAddRemove(BN_CLICKED, IDC_BTN_REMOVE, NULL);
		break;
	}
	UpdateControls();
	return 0;
}

void CSelectDiskPage::UpdateControls()
{
	GetDlgItem(IDC_BTN_ADD).EnableWindow(
		m_wndListSingle.GetSelectedDiskObjectList().size() > 0 &&
		(
		m_wndListSingle.GetSelectedDiskObjectList().size() +
		m_wndListBind.GetDiskObjectList().size() <= m_pWizData->m_nDiskCount
		)
		);
	GetDlgItem(IDC_BTN_REMOVE).EnableWindow(m_wndListBind.GetSelectedDiskObjectList().size() > 0);
	GetDlgItem(IDC_BTN_REMOVE_ALL).EnableWindow( m_wndListBind.GetItemCount() > 0 );

	CEdit ctlEditCount;
	ctlEditCount.Attach(GetDlgItem(IDC_BIND_WIZ_COUNT));
	CString strDiskCount;
	strDiskCount.Format(_T("%d"), m_pWizData->m_nDiskCount);
	ctlEditCount.SetWindowText(strDiskCount);

	CEdit ctlEditSize;
	ctlEditSize.Attach(GetDlgItem(IDC_BIND_WIZ_SIZE));

	if ( m_wndListBind.GetDiskObjectList().size() == m_pWizData->m_nDiskCount )
	{
		CString strSize;
		pGetDiskSizeString(
			strSize, 
			m_pWizData->m_nBindType, 
			m_pWizData->m_nDiskCount, 
			m_wndListBind.GetDiskObjectList());

		ctlEditSize.SetWindowText(strSize);


		SetWizardButtons( PSWIZB_BACK | PSWIZB_NEXT );
	}
	else
	{
		ctlEditSize.SetWindowText(_T(""));
		SetWizardButtons( PSWIZB_BACK);
	}
}


BOOL
CSelectDiskPage::OnSetActive()
{
	SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	UpdateControls();

	return TRUE;
}

// 0 to automatically advance to the next page
// -1 to prevent the page from changing
INT 
CSelectDiskPage::OnWizardNext()
{
	ATLASSERT( m_wndListBind.GetItemCount() == (int)m_pWizData->m_nDiskCount);
	ATLASSERT( 
		NMT_AGGREGATE == m_pWizData->m_nBindType || 
		NMT_RAID0 == m_pWizData->m_nBindType || 
		NMT_RAID1R3 == m_pWizData->m_nBindType || 
		NMT_RAID4R3 == m_pWizData->m_nBindType ||
		NMT_RAID5 == m_pWizData->m_nBindType
		);

	NBUnitDevicePtrList listBind;
	unsigned int i;
	CString strMsg;

	// Warning message if disk is over 2T.
	{
		UINT64 nSize = pGetBoundDiskSize(
			m_pWizData->m_nBindType, 
			m_pWizData->m_nDiskCount, 
			m_wndListBind.GetDiskObjectList());
		if (nSize >= 2LL * 1024 * 1024 * 1024 * 1024) { // 2 Tera.
			CString strTitle;
			strTitle.LoadString(IDS_APPLICATION);
			strMsg.LoadString(IDS_WARNING_BIND_SIZE);
			int id = MessageBox(
				strMsg,
				strTitle,
				MB_YESNO|MB_ICONEXCLAMATION
				);
			if(IDYES != id)
				return -1;
		}
			
	}
	// warning message
	{
		CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_BIND);
		int id = MessageBox(
			strMsg,
			strTitle,
			MB_YESNO|MB_ICONEXCLAMATION
			);
		if(IDYES != id)
			return -1;
	}

	m_pWizData->listUnitDevicesBind = m_wndListBind.GetDiskObjectList();

	NDASCOMM_CONNECTION_INFO *pConnectionInfo;
	pConnectionInfo = new NDASCOMM_CONNECTION_INFO[m_pWizData->m_nDiskCount];
	ZeroMemory(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * m_pWizData->m_nDiskCount);

	listBind = m_wndListBind.GetDiskObjectList();
	CString strTitle;
	CNBUnitDevice *UnitDiskObject;
	NBUnitDevicePtrList::const_iterator itr;
	
	for (i = 0, itr = listBind.begin(); itr != listBind.end(); ++itr, i++ )
	{
		UnitDiskObject = *itr;

		if(!UnitDiskObject->InitConnectionInfo(&pConnectionInfo[i], TRUE))
//		if(!((*itr)->GetAccessMask() & GENERIC_WRITE))
		{
			// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
			strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
				(*itr)->GetName()
				);
			strTitle.LoadString(IDS_APPLICATION);
			MessageBox(
				strMsg,
				strTitle,
				MB_OK|MB_ICONERROR);

			delete [] pConnectionInfo;
			return -1;
		}
	}

	DWORD dwUserSpace = 0;
	if(!pGetAppConfigValue(_T("UserSpace"), &dwUserSpace))
	{
		dwUserSpace = 0;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);
	m_pWizData->m_BindResult = NdasOpBind(
		m_pWizData->m_nDiskCount, pConnectionInfo, m_pWizData->m_nBindType, dwUserSpace);
	l_auto_cursor.Release();

	if(m_pWizData->m_BindResult != m_pWizData->m_nDiskCount) // error
		m_pWizData->dwBindLastError = ::GetLastError();		

	LPCGUID hostGuid = pGetNdasHostGuid();

	for (i = 0; i < m_pWizData->m_nDiskCount; i++ )
	{
		BOOL success = NdasCommNotifyUnitDeviceChange(
			NDAS_DIC_NDAS_ID,
			pConnectionInfo[i].Address.NdasId.Id,
			pConnectionInfo[i].UnitNo,
			hostGuid);

		if (!success)
		{
			ATLTRACE("NdasCommNotifyUnitDeviceChange failed, error=0x%X\n",
				GetLastError());
		}
	}

	delete [] pConnectionInfo;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Completion Page
//
//////////////////////////////////////////////////////////////////////////

CCompletionPage::CCompletionPage(HWND hWndParent, PWIZARD_DATA pData) :
	m_pWizData(pData),
	m_hWndParent(hWndParent),
	CPropertyPageImpl<CCompletionPage>(IDS_BNZ_TITLE)
{
	m_psp.dwFlags |= PSP_HIDEHEADER;
//	m_uIDCompletionMsg = IDS_REGWIZ_COMPLETE_NORMAL;
}

LRESULT 
CCompletionPage::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CContainedWindow wndTitle;
	wndTitle.Attach(GetDlgItem(IDC_BIG_BOLD_TITLE));
	wndTitle.SetFont(pGetTitleFont());

	return 1;
}

BOOL 
CCompletionPage::OnSetActive()
{
	CString strText, strText2;
	SetWizardButtons(PSWIZB_FINISH);

	CStatic ctlStaticResult;
	ctlStaticResult.Attach(GetDlgItem(IDC_BIND_RESULT));

	CEdit ctlEditResult;
	ctlEditResult.Attach(GetDlgItem(IDC_EDIT_BIND_SETTING));

	// add bind type to list
	strText2.LoadString(
		(NMT_AGGREGATE == m_pWizData->m_nBindType) ? IDS_LOGDEV_TYPE_AGGREGATED_DISK :
		(NMT_RAID0 == m_pWizData->m_nBindType) ? IDS_LOGDEV_TYPE_DISK_RAID0 :
		(NMT_RAID1R3 == m_pWizData->m_nBindType) ? IDS_LOGDEV_TYPE_DISK_RAID1R3 :
		(NMT_RAID4R3 == m_pWizData->m_nBindType) ? IDS_LOGDEV_TYPE_DISK_RAID4R3 :
		(NMT_RAID5 == m_pWizData->m_nBindType) ? IDS_LOGDEV_TYPE_DISK_RAID5 :
		IDS_LOGDEV_TYPE
		);
	strText.FormatMessage(IDS_BNZ_COMPLETE_RESULT_TYPE_FMT, strText2);
	ctlEditResult.AppendText(strText); 
	ctlEditResult.AppendText(_T("\r\n"));


	// add disk titles to list
	strText.LoadString(IDS_BNZ_COMPLETE_RESULT_DISKS);
	ctlEditResult.AppendText(strText); 
	ctlEditResult.AppendText(_T("\r\n"));

	int i;
	NBUnitDevicePtrList::iterator itr;
	CNBUnitDevice *UnitDiskObject, *UnitDiskObjectFailed;
	for(i = 0, itr = m_pWizData->listUnitDevicesBind.begin(); itr != m_pWizData->listUnitDevicesBind.end(); ++itr, i++)
	{
		UnitDiskObject = *itr;
		if(m_pWizData->m_BindResult == i)
		{
			UnitDiskObjectFailed = UnitDiskObject;
		}
		strText = UnitDiskObject->GetName();
		ctlEditResult.AppendText(strText); 
		ctlEditResult.AppendText(_T("\r\n"));
	}

	if(m_pWizData->m_BindResult == m_pWizData->m_nDiskCount)
	{
		// success
		strText.LoadString(IDS_BNZ_COMPLETE_SUCCESS);
		ctlStaticResult.SetWindowText(strText);

		// size
		pGetDiskSizeString(strText2, m_pWizData->m_nBindType, m_pWizData->m_nDiskCount, m_pWizData->listUnitDevicesBind);
		strText.FormatMessage(IDS_BNZ_COMPLETE_RESULT_SIZE_FMT, strText2);
		ctlEditResult.AppendText(strText); 
		ctlEditResult.AppendText(_T("\r\n"));
	}
	else
	{
		// fail
		strText.LoadString(IDS_BNZ_COMPLETE_FAIL);
		ctlStaticResult.SetWindowText(strText);

		if(m_pWizData->m_BindResult != 0xFFFFFFFF) // single disk error
		{
			strText.FormatMessage(IDS_BNZ_COMPLETE_RESULT_FAILED_ON_FMT,
				UnitDiskObjectFailed->GetName());
			ctlEditResult.AppendText(strText); 
			ctlEditResult.AppendText(_T("\r\n"));
		}

		GetDescription(strText, m_pWizData->dwBindLastError);
		ctlEditResult.AppendText(strText); 
		ctlEditResult.AppendText(_T("\r\n"));
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
//////////////////////////////////////////////////////////////////////////

static 
HFONT 
pGetTitleFont()
{
	BOOL fSuccess = FALSE;
	static HFONT hTitleFont = NULL;
	if (NULL != hTitleFont) {
		return hTitleFont;
	}

	CString strFontName;
	CString strFontSize;
	fSuccess = strFontName.LoadString(IDS_BIG_BOLD_FONT_NAME);
	ATLASSERT(fSuccess);
	fSuccess = strFontSize.LoadString(IDS_BIG_BOLD_FONT_SIZE);
	ATLASSERT(fSuccess);

	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	fSuccess = ::SystemParametersInfo(
		SPI_GETNONCLIENTMETRICS, 
		sizeof(NONCLIENTMETRICS), 
		&ncm, 
		0);
	ATLASSERT(fSuccess);

	LOGFONT TitleLogFont = ncm.lfMessageFont;
	TitleLogFont.lfWeight = FW_BOLD;

	HRESULT hr = ::StringCchCopy(TitleLogFont.lfFaceName,
		(sizeof(TitleLogFont.lfFaceName)/sizeof(TitleLogFont.lfFaceName[0])),
		strFontName);

	ATLASSERT(SUCCEEDED(hr));

	INT TitleFontSize = ::StrToInt(strFontSize);
	if (TitleFontSize == 0) {
		TitleFontSize = 12;
	}

	HDC hdc = ::GetDC(NULL);
	TitleLogFont.lfHeight = 0 - 
		::GetDeviceCaps(hdc,LOGPIXELSY) * TitleFontSize / 72;

	hTitleFont = ::CreateFontIndirect(&TitleLogFont);
	::ReleaseDC(NULL, hdc);

	return hTitleFont;
}

} // namespace nbbwiz

