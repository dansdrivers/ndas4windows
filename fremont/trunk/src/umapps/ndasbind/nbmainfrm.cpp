// nbmainfrm.cpp : implementation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include <ndas/appconf.h>

#include "nbmainfrm.h"

#include "nbbindwiz.h"
#include "nbdevicelistdlg.h"
#include "nbaboutdlg.h"
#include "autocursor.h"

#include "apperrdlg.h"

LONG DbgLevelNbMain = DBG_LEVEL_NB_MAIN;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelNbMain) {								\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

namespace
{
BOOL CALLBACK CheckCapacityForMirror( CNBLogicalDev *pUnitDevice, HWND hWnd, LPVOID lpContext )
{
	if (!lpContext) {

		ATLASSERT(FALSE);
		return FALSE;
	}

	if (!pUnitDevice) {

		return FALSE;
	}

	CNBLogicalDev *logicalDev = (CNBLogicalDev *)lpContext;

	if (logicalDev->PhysicalCapacityInByte() > pUnitDevice->PhysicalCapacityInByte()) {

		CString strMsg;
		strMsg.LoadString( IDS_SELECTMIRDLG_SMALLER_DISK );

		CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);

		MessageBox( hWnd, strMsg, strTitle, MB_OK | MB_ICONWARNING );

		return FALSE;
	}

	return TRUE;
}

BOOL CALLBACK CheckCapacityForSpare(CNBLogicalDev *pUnitDevice, HWND hWnd, LPVOID lpContext)
{
	if(!lpContext)
		return TRUE;

	if(!pUnitDevice)
		return FALSE;

	CNBLogicalDev *logicalDev = (CNBLogicalDev *)lpContext;

	for(UINT32 i = 0; i < logicalDev->NumberOfChild(); i++)
	{
		if (logicalDev->Child(i) && 
			logicalDev->Child(i)->LogicalCapacityInByte() > pUnitDevice->PhysicalCapacityInByte())
		{
			CString strMsg;
			strMsg.LoadString( IDS_SPARE_ADD_DLG_SIZE_FAIL );
			CString strTitle;
			strTitle.LoadString(IDS_APPLICATION);

			MessageBox(
				hWnd,
				strMsg,
				strTitle, 
				MB_OK | MB_ICONWARNING
				);

			return FALSE;
		}
	}

	return TRUE;
}


// Return TRUE if selected device can be replaced.
BOOL CALLBACK CheckReplaceDevice(CNBLogicalDev *pUnitDevice, HWND hWnd, LPVOID lpContext)
{
	if(!lpContext)
		return TRUE;

	if(!pUnitDevice)
		return FALSE;

	CNBLogicalDev *pSrcUnitDevice = (CNBLogicalDev *)lpContext;
	CNBLogicalDev *logicalDev = (CNBLogicalDev*) pSrcUnitDevice->RootLogicalDev();

	UINT64 ui64CapacityRequired = logicalDev->UptodateChild()->LogicalCapacityInByte();
	UINT64 ui64CapacityOfNewMember = pUnitDevice->PhysicalCapacityInByte();

	if (ui64CapacityRequired > ui64CapacityOfNewMember) {
		CString strMsg;
		strMsg.LoadString( IDS_RAID_MEMBER_UNREPLACEABLE_SMALL_SIZE );
		CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			hWnd,
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return FALSE;
	}
	return TRUE;
}

VOID CALLBACK 
NdasEventCallback(DWORD dwError, PNDAS_EVENT_INFO pEventInfo, LPVOID lpContext)
{
	if (NULL == pEventInfo) {

		NdasUiDbgCall( 4, _T("Event Error %d (0x%08X)\n"), dwError, dwError );
		return;
	}

	HWND hWnd = reinterpret_cast<HWND>(lpContext);

	WPARAM wParam(0);
	LPARAM lParam(0);

	DWORD dwSlotNo = static_cast<DWORD>(lParam);
	DWORD dwUnitNo = static_cast<DWORD>(wParam);
	NDAS_LOGICALDEVICE_ID logDevId = static_cast<DWORD>(lParam);

	switch (pEventInfo->EventType) 
	{
/*
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_DEVICE_STATUS_CHANGED, wParam, dwSlotNo);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED, wParam, lParam);
		break;
*/	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_DEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	}
}

} // local namespace

CMainFrame::CMainFrame() :
	CInterAppMsgImpl<CMainFrame>(APP_INST_UID)/*,
	m_nAssigned(0)*/
{
	NdasUiDbgCall( 4, _T("create CmainFrame\n") );

	//m_hEventThread = CreateEvent(NULL, FALSE, FALSE, NULL);
	//ATLASSERT(NULL != m_hEventThread);

	return;
}

CMainFrame::~CMainFrame()
{
	//ClearDevices();
	//CloseHandle(m_hEventThread);

	return;
}

CString CMainFrame::GetName(CNBLogicalDev *LogicalDev)
{
	CString strText;

	if (LogicalDev->IsLeaf()) {

		UINT unitNo;

		strText = LogicalDev->GetName(&unitNo);

		if (unitNo == 0) {

			return strText;

		}

		CString strText2;

		strText2.Format( _T("%s:%d"), strText, unitNo );

		NdasUiDbgCall( 4, _T("CMainFrame::GetName %s\n"), strText2 );

		return strText2;
	}

	switch (LogicalDev->GetType()) {

	case NMT_AGGREGATE: strText.LoadString(IDS_LOGDEV_TYPE_AGGREGATED_DISK); break;
	case NMT_MIRROR:	strText.LoadString(IDS_LOGDEV_TYPE_MIRRORED_DISK); break;
	case NMT_RAID0:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID0); break;
	case NMT_RAID1:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1); break;
	case NMT_RAID1R2:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R2); break;
	case NMT_RAID1R3:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R3); break;	
	case NMT_RAID4:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4); break;
	case NMT_RAID4R2:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R2); break;
	case NMT_RAID4R3:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R3); break;	
	case NMT_RAID5:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID5); break;
	default:			strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, LogicalDev->GetType());

	}

	return strText;
}

/*
* Shows error message if the command is not available
*/
BOOL CMainFrame::CheckCommandAvailable(CNBLogicalDev *LogicalDev, int wID)
{
	if (LogicalDev->IsCommandAvailable(wID)) {

		return TRUE;
	}

	ATLASSERT(FALSE);

	// Show error message

	CString strMsg;
	
	strMsg.FormatMessage(IDS_ERROR_NOT_IN_OPERATABLE_STATE);
	
	CString strTitle;
	
	strTitle.LoadString(IDS_APPLICATION);
	
	MessageBox( strMsg, strTitle, MB_OK|MB_ICONERROR );

	// device Status is not up to date. Refresh whole devices.
	
	RefreshStatus();

	return FALSE;	
}

/*
* Update m_listDevices, m_listLogicalDevices and m_listMissingDevices
* Refresh m_viewTreeList with new data
*/
BOOL CMainFrame::RefreshStatus()
{
	BOOL bResult;
	
	AutoCursor l_auto_cursor(IDC_WAIT);

	/*
	* UI Refreshing mode
	*/
	m_viewTreeList.GetTreeControl().DeleteAllItems();

	// Disable all commands

	UIEnableForDevice( NULL, IDM_TOOL_BIND );
	UIEnableForDevice( NULL, IDM_TOOL_UNBIND );
	
	UIEnableForDevice( NULL, IDM_TOOL_ADDMIRROR );
	UIEnableForDevice( NULL, IDM_TOOL_APPEND );
	UIEnableForDevice( NULL, IDM_TOOL_SPAREADD );

	UIEnableForDevice( NULL, IDM_TOOL_REPLACE_DEVICE );
	UIEnableForDevice( NULL, IDM_TOOL_REMOVE_FROM_RAID );
	UIEnableForDevice( NULL, IDM_TOOL_CLEAR_DEFECTIVE );

	UIEnableForDevice( NULL, IDM_TOOL_MIGRATE);
	UIEnableForDevice( NULL, IDM_TOOL_RESET_BIND_INFO );

//	UIEnableForDevice(NULL, IDM_TOOL_USE_AS_BASIC);
//	UIEnableForDevice(NULL, IDM_TOOL_FIX_RAID_STATE);
//	UIEnableForDevice(NULL, IDM_TOOL_USE_AS_MEMBER);
//	UIEnableForDevice(NULL, IDM_TOOL_SPAREREMOVE);

	m_wndRefreshProgress.ShowWindow(SW_SHOW);
	m_wndRefreshProgress.SetPos(0);

	UpdateWindow();

	bResult = m_ndasStatus.RefreshStatus(&m_wndRefreshProgress);

	if (bResult == FALSE) {

		ATLASSERT(FALSE);

		l_auto_cursor.Release();
		return FALSE;
	}

	// Complete

	m_viewTreeList.SetDevices(&m_ndasStatus.m_LogicalDevList);
	m_wndRefreshProgress.ShowWindow(SW_HIDE);

	l_auto_cursor.Release();
	return TRUE;
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
	{
		return TRUE;
	}

	return m_viewTreeList.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	UIUpdateToolBar();
	return FALSE;
}

VOID
CMainFrame::OnDestroy()
{
	::NdasUnregisterEventCallback(m_hEventCallback);
	PostQuitMessage(0);
}

LRESULT CMainFrame::OnCreate(LPCREATESTRUCT /*lParam*/)
{
	NdasUiDbgCall( 2, _T("m_hWnd = %p\n"), m_hWnd );

	// create command bar window

	HWND hWndCmdBar = m_CmdBar.Create( m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE );

	// attach menu

	m_CmdBar.AttachMenu(GetMenu());
	
	// load command bar images
	
	m_CmdBar.SetImageSize(CSize(9,9));
	
	// m_CmdBar.LoadImages(IDR_MAINFRAME);
	// remove old menu

	SetMenu(NULL);

	// set title

	CString strTitle;

	strTitle.LoadString( IDS_APPLICATION );

	SetWindowText(strTitle);

	// setting up a tool bar
	// patria:
	// We are loading an empty tool bar.
	// If we directly load a tool bar using a bitmap which does not
	// match with windows system palette, the application may crash
	// in Windows 2000.
	// As an workaround, we just create a simple tool bar with
	// an empty bitmap and replace them later.

	HWND hWndToolBar = CreateSimpleToolBarCtrl( m_hWnd, 
												IDR_EMPTY_TOOLBAR, 
												FALSE, 
												ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST );

	m_wndToolBar.Attach(hWndToolBar);

	m_wndToolBar.SetExtendedStyle( TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS );

	// patria:
	//
	// Some bitmaps are distorted when used with TB_ADDBITMAP
	// which is sent from CreateSimpleToolBarCtrl when the bitmap is not true color.
	// This is the case with IO-DATA's tool bar image.
	// As an workaround, we can directly create a image list directly
	// and replace the image list of the tool bar, which corrects such misbehaviors.

	CImageList imageList;
	WORD wWidth = 32; // we are using 32 x 32 buttons

	imageList.CreateFromImage( IDR_MAINFRAME, wWidth, 1, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION | LR_DEFAULTSIZE );
	m_wndToolBar.SetImageList(imageList);

	TBBUTTON tbButton = { 0 };
	TBBUTTONINFO tbButtonInfo = { 0 };

	// Add strings to the tool bar

	m_wndToolBar.SetButtonStructSize(sizeof(TBBUTTON));

	for (int i=0; i < m_wndToolBar.GetButtonCount(); i++) {

		CString strCommand;

		m_wndToolBar.GetButton( i, &tbButton );
		tbButtonInfo.cbSize	= sizeof(TBBUTTONINFO);
		tbButtonInfo.dwMask = TBIF_STYLE;
		m_wndToolBar.GetButtonInfo( tbButton.idCommand, &tbButtonInfo );
		tbButtonInfo.dwMask = TBIF_TEXT | TBIF_STYLE;
		strCommand.LoadString( tbButton.idCommand );

		strCommand = strCommand.Right(strCommand.GetLength() - strCommand.Find('\n') - 1);
		
		tbButtonInfo.pszText = const_cast<LPTSTR>(static_cast<LPCTSTR>(strCommand));
		tbButtonInfo.cchText = strCommand.GetLength();
		tbButtonInfo.fsStyle |= BTNS_SHOWTEXT | BTNS_AUTOSIZE;

		m_wndToolBar.AddString( tbButton.idCommand );
		m_wndToolBar.SetButtonInfo( tbButton.idCommand, &tbButtonInfo );
	}

#define ATL_CUSTOM_REBAR_STYLE ((ATL_SIMPLE_REBAR_STYLE & ~RBS_AUTOSIZE) | CCS_NODIVIDER)

	// patria: reason to use ATL_CUSTOM_REBAR_STYLE
	//
	// ATL_SIMPLE_REBAR_STYLE (not a NO_BRODER style) has a problem
	// with theme-enabled Windows XP, 
	// rendering some transparent lines above the rebar.

	CreateSimpleReBar(ATL_CUSTOM_REBAR_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(m_wndToolBar.m_hWnd, NULL, TRUE);

	CReBarCtrl reBar = m_hWndToolBar;
	DWORD cBands = reBar.GetBandCount();

	for (DWORD i = 0; i < cBands; ++i) {

		REBARBANDINFO rbi = {0};
		
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_STYLE;
		reBar.GetBandInfo(i, &rbi);
		rbi.fStyle |= RBBS_NOGRIPPER;
		reBar.SetBandInfo(i, &rbi);
	} 

	// work on status bar, progress bar

	CreateSimpleStatusBar();

	CRect rectPgs;
	
	::GetClientRect(m_hWndStatusBar, &rectPgs);
	
	rectPgs.DeflateRect(1,2,1,2);
	rectPgs.right = 300;
	m_wndRefreshProgress.Create(m_hWndStatusBar, &rectPgs, NULL, WS_CHILD | WS_VISIBLE);

	m_wndRefreshProgress.SetRange32(0, 100);
	m_wndRefreshProgress.SetPos(50);

	m_wndRefreshProgress.ShowWindow(SW_HIDE);

	// Comments from the author
	// (http://www.viksoe.dk/code/treelistview.htm)
	//
	// It is wise to add the TVS_DISABLEDRAGDROP and TVS_SHOWSELALWAYS 
	// styles to the tree control for best result.

	m_viewTreeList.Create( *this, 
						   rcDefault, 
						   NULL,
						   WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
						   TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP /* | TVS_HASLINES */ );


	m_viewTreeList.Initialize();
	m_viewTreeList.GetTreeControl().SetIndent(24);

	m_hWndClient = m_viewTreeList;

	UIAddToolBar(m_wndToolBar);

	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// TODO : It will be better if we display splash window while
	// the treeview is initialized

	PostMessage( WM_COMMAND, IDM_TOOL_REFRESH, 0 );

	m_hEventCallback = ::NdasRegisterEventCallback(NdasEventCallback, m_hWnd);

	// register object for message filtering and idle updates

	CMessageLoop* pLoop = _Module.GetMessageLoop();

	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnExit(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);

	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();

	return 0;
}

VOID CMainFrame::OnContextMenu(HWND /*hWnd*/, CPoint pos)
{
	// get selectedItemData

	CRect rect;
	CPoint posInView;
	HTREEITEM hItemSelected;

	// if clicked on tree, we need to change selection

	if (m_viewTreeList.GetWindowRect( rect ) && rect.PtInRect(pos)) {

		CTreeViewCtrlEx ctrlTree = m_viewTreeList.GetTreeControl();
		CHeaderCtrl ctrlHeader = m_viewTreeList.GetHeaderControl();

		CRect rectHeader;
		ctrlHeader.GetClientRect(rectHeader);

		// clicked point is inside the tree control
		// Change screen coordinates to client coordinates

		posInView = pos - rect.TopLeft();
		posInView.y -= rectHeader.Height();

		if (hItemSelected = ctrlTree.HitTest(posInView, NULL)) {

			ctrlTree.SelectItem(hItemSelected);
		}
	}

	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	if (!logicalDev) {

		return;
	}

	CMenu menu;
	CMenuHandle subMenu;

	menu.LoadMenu(IDR_MAINPOPUP);

	if (logicalDev->IsRoot() && logicalDev->IsLeaf()) {	// single

		subMenu = menu.GetSubMenu(0);

	} else if (logicalDev->IsRoot()) {

		subMenu = menu.GetSubMenu(1);

	} else {

		subMenu = menu.GetSubMenu(2);
	}
	
	subMenu.TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pos.x, pos.y, m_hWnd );
	
	return;
}

VOID CMainFrame::UIEnableForDevice(CNBLogicalDev *LogicalDev, UINT nMenuID)
{
	//NdasUiDbgCall( 4, _T("nMemuID = %d\n"), nMenuID );

	switch (nMenuID) {
	
	case IDM_TOOL_BIND: {

		if (LogicalDev) {

			UIEnable( nMenuID, LogicalDev->IsCommandAvailable(nMenuID) );
 			break;
		}

		NdasUiDbgCall( 4, _T("GetOperatableSingleDevices().size() = %d\n"), GetOperatableSingleDevices().size() );

		UIEnable( nMenuID, GetOperatableSingleDevices().size() >= 2 ? TRUE : TRUE );
		
		break;
	}

	default:

		UIEnable( nMenuID, LogicalDev ? LogicalDev->IsCommandAvailable(nMenuID) : FALSE );
		break;
	}
}

LRESULT CMainFrame::OnTreeSelChanged(LPNMHDR lpNLHDR)
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	// If the selection is empty, all commands should be disabled
	// And it is okay to pass null as logicalDev calling UIEnableForDevice

	UIEnableForDevice( logicalDev, IDM_TOOL_BIND );
	UIEnableForDevice( logicalDev, IDM_TOOL_UNBIND );

	UIEnableForDevice( logicalDev, IDM_TOOL_ADDMIRROR );
	UIEnableForDevice( logicalDev, IDM_TOOL_APPEND );
	UIEnableForDevice( logicalDev, IDM_TOOL_SPAREADD );
	
	UIEnableForDevice( logicalDev, IDM_TOOL_REPLACE_DEVICE );
	UIEnableForDevice( logicalDev, IDM_TOOL_REMOVE_FROM_RAID );
	UIEnableForDevice( logicalDev, IDM_TOOL_CLEAR_DEFECTIVE );

	UIEnableForDevice( logicalDev, IDM_TOOL_MIGRATE );
	UIEnableForDevice( logicalDev, IDM_TOOL_RESET_BIND_INFO );

//	UIEnableForDevice(logicalDev, IDM_TOOL_USE_AS_BASIC);
//	UIEnableForDevice(logicalDev, IDM_TOOL_FIX_RAID_STATE);
//	UIEnableForDevice(logicalDev, IDM_TOOL_USE_AS_MEMBER);
//	UIEnableForDevice(logicalDev, IDM_TOOL_SINGLE);
//	UIEnableForDevice(logicalDev, IDM_TOOL_SPAREREMOVE);

	return 0;
}

LRESULT CMainFrame::OnToolBarDropDown(LPNMHDR lpNMHDR)
{
	NMTOOLBAR* pnmtb = reinterpret_cast<NMTOOLBAR*>(lpNMHDR);
	switch(pnmtb->iItem)
	{
	case IDM_TOOL_MIRROR:
		{
			// Display dropdown menu
			CMenu menu;
			CMenuHandle subMenu;
			CRect rect;
			m_wndToolBar.GetRect( pnmtb->iItem, rect );
			m_wndToolBar.ClientToScreen( rect );
			menu.LoadMenu( MAKEINTRESOURCE(IDR_MIRROR_MENU) );
			subMenu = menu.GetSubMenu(0);
			subMenu.TrackPopupMenu(
				TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_VERTICAL,
				rect.left,
				rect.bottom,
				m_hWnd
				);
		}
	default:
		break;
	}
	
	return 0;
}

LRESULT CMainFrame::OnToolBarClickMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	NMTOOLBAR nmtb = { 0 };
	nmtb.iItem =  IDM_TOOL_MIRROR;
	OnToolBarDropDown( reinterpret_cast<LPNMHDR>(&nmtb) );
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Implementation of command handling methods
//
///////////////////////////////////////////////////////////////////////////////

void CMainFrame::OnRefreshStatus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	RefreshStatus();
}

VOID CMainFrame::OnBind( UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/ )
{
	nbbwiz::CWizard dlgBindWizard;

	dlgBindWizard.SetSingleDisks( this, GetOperatableSingleDevices() );

	if (dlgBindWizard.DoModal() == IDOK) {

		RefreshStatus();
	}
}

VOID CMainFrame::OnUnBind( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	if (!CheckCommandAvailable(logicalDev, wID)) {

		return;
	}

	BOOL bUnbindMirror = (NMT_RAID1 == logicalDev->GetType()	||
						  NMT_RAID1R2 == logicalDev->GetType()	||
						  NMT_RAID1R3 == logicalDev->GetType()	||
						  NMT_MIRROR == logicalDev->GetType() );

	// warning message

	CString strTitle;
	CString strMsg;

	NBLogicalDevPtrList  targetlogicalDevs = GetBindOperatableDevices(logicalDev);
	
	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_UNBIND_DLG_CAPTION, 
										(bUnbindMirror) ? IDS_WARNING_UNBIND_MIR : IDS_WARNING_UNBIND, 
										targetlogicalDevs, 
										0, 
										NULL, 
										NULL );

	if (dlgSelectDevice.DoModal() != IDOK) {

		return;
	}

	if (logicalDev->NumberOfChild() != targetlogicalDevs.size()) {
		
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_MISSING_MEMBER);
		
		int id = MessageBox( strMsg, strTitle, MB_YESNO|MB_ICONEXCLAMATION );

		if (IDYES != id) {

			return;
		}
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	UINT32 failChildIdx = 0xFFFF;

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnUnBind( logicalDev, 
														 logicalDev->NumberOfChild() != targetlogicalDevs.size(), 
														 &failChildIdx );
	
	l_auto_cursor.Release();

	DWORD dwLastError = ::GetLastError();

	if (bindStatus == NDAS_BIND_STATUS_OK) {

		CString strTitle;
		
		strTitle.LoadString(IDS_APPLICATION);

		strMsg.LoadString( (bUnbindMirror) ? IDS_WARNING_UNBIND_AFTER_MIR : IDS_WARNING_UNBIND_AFTER );

		MessageBox( strMsg, strTitle, MB_OK|MB_ICONINFORMATION );

	} else {

		::SetLastError(dwLastError);

		switch (dwLastError) {

		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error

			if (failChildIdx < logicalDev->NumberOfChild()) {

				strMsg.FormatMessage( IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, GetName(logicalDev->Child(failChildIdx)) );
			
			} else {

				strMsg.LoadString(IDS_BIND_FAIL);
			}

			break;

		default:

			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(IDS_MAINFRAME_SINGLE_ACCESS_FAIL);
	}

	RefreshStatus();

	return;
}

VOID CMainFrame::OnAddMirror( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	if (!logicalDev) {

		ATLASSERT(FALSE);
		return;
	}

	NBLogicalDevPtrList targetlogicalDevs = GetOperatableSingleDevices();

	// Remove self from device list to show.
	
	targetlogicalDevs.remove(logicalDev);

	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_MIRROR_ADD_DLG_CAPTION, 
										IDS_MIRROR_ADD_DLG_MESSAGE, 
										targetlogicalDevs, 
										1, 
										CheckCapacityForMirror, 
										logicalDev );

	if (dlgSelectDevice.DoModal() != IDOK) {

		return;
	}

	CNBLogicalDev *devToAdd = dlgSelectDevice.GetSelectedDevice();

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnAddMirror( logicalDev, devToAdd );

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();

	return;
}

VOID CMainFrame::OnAppend( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	NBLogicalDevPtrList targetlogicalDevs = GetOperatableSingleDevices();

	if (logicalDev->IsRoot() && logicalDev->IsLeaf()) {

		// remove self
		targetlogicalDevs.remove(logicalDev);
	}

	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_SPARE_APPEND_DLG_CAPTION, 
										IDS_SPARE_APPEND_DLG_MESSAGE, 
										targetlogicalDevs, 
										1, 
										NULL, 
										logicalDev );

	if (dlgSelectDevice.DoModal() != IDOK) {

		return;
	}

	CNBLogicalDev *devToAppend = dlgSelectDevice.GetSelectedDevice();

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnAppend( logicalDev, devToAppend );

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();
}


VOID CMainFrame::OnSpareAdd( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_SPARE_ADD_DLG_CAPTION, 
										IDS_SPARE_ADD_DLG_MESSAGE, 
										GetOperatableSingleDevices(), 
										1, 
										CheckCapacityForSpare, 
										logicalDev );

	if (dlgSelectDevice.DoModal() != IDOK) {

		return;
	}

	CNBLogicalDev *devToAdd = dlgSelectDevice.GetSelectedDevice();

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bResults = m_ndasStatus.OnSpareAdd( logicalDev, devToAdd );

	l_auto_cursor.Release();

	if (bResults != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();
}

VOID CMainFrame::OnReplaceDevice( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	DWORD i;

	CNBLogicalDev *pOldUnitDevice = m_viewTreeList.GetSelectedDevice();
	BOOLEAN ConvertToBasic;

	if (!pOldUnitDevice) {

		ATLASSERT(FALSE);
		return;
	}

	CNBLogicalDev *logicalDev = pOldUnitDevice->RootLogicalDev();

	if (!logicalDev) {

		ATLASSERT(FALSE);
		return;
	}
	
	// Choose the replacement disk.

	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_REPLACE_DEVICE_DLG_CAPTION, 
										IDS_REPLACE_DEVICE_DLG_MESSAGE, 
										GetOperatableSingleDevices(), 
										1, 
										CheckReplaceDevice, 
										pOldUnitDevice );

	if (dlgSelectDevice.DoModal() != IDOK) {
		
		return;
	}

	CNBLogicalDev *pNewUnitDevice = dlgSelectDevice.GetSelectedDevice();

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnReplaceDevice( pOldUnitDevice, pNewUnitDevice );

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();
	return;
}

// Reconfigure RAID without this disk. If spare disk does not exist, we cannot reconfigure.

VOID CMainFrame::OnRemoveFromRaid( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	DWORD i;
	CNBLogicalDev *DevToRemove = m_viewTreeList.GetSelectedDevice();
	BOOLEAN ConvertToBasic;

	if (DevToRemove == NULL) {

		ATLASSERT(FALSE);	
		return;
	}

	CNBLogicalDev *logicalDev = DevToRemove->RootLogicalDev();

	if (!logicalDev) {

		ATLASSERT( FALSE );	
		return;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnRemoveFromRaid(DevToRemove);

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();
	return;	
}

VOID CMainFrame::OnClearDefect( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *selectLogicalDev = m_viewTreeList.GetSelectedDevice();
	DWORD i;
	
	if (selectLogicalDev == NULL) {

		ATLASSERT(FALSE);
		return;
	}

	CNBLogicalDev *logicalDev = selectLogicalDev->RootLogicalDev();
	
	if (logicalDev == NULL) {

		ATLASSERT(FALSE);
		return;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnClearDefect( selectLogicalDev );

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();

	return;
}

VOID CMainFrame::OnMigrate( UINT wNotifyCode, int wID, HWND hwndCtl )
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	// warning message

	CString strMsg;

	NBLogicalDevPtrList  targetlogicalDevs = GetBindOperatableDevices(logicalDev);

	CNBSelectDeviceDlg dlgSelectDevice( IDD_DEVICE_LIST, 
										IDS_MIGRATE_DLG_CAPTION, 
										IDS_MIGRATE_DLG_MESSAGE, 
										targetlogicalDevs, 
										0, 
										NULL, 
										NULL );

	if (dlgSelectDevice.DoModal() != IDOK) {

		return;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnMigrate( logicalDev );
	
	l_auto_cursor.Release();

	DWORD dwLastError = ::GetLastError();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		ShowErrorMessageBox(IDS_MAINFRAME_MIGRATE_FAIL);
	}

	RefreshStatus();

	return;
}

// Convert disk type to default even if it is unknown type.

void CMainFrame::OnResetBindInfo(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBLogicalDev *logicalDev = m_viewTreeList.GetSelectedDevice();

	if (logicalDev == NULL) {

		return;
	}

	// Show warning that RAID may be broken.

	CString strTitle;
	CString strMsg;

	strTitle.LoadString(IDS_APPLICATION);
	strMsg.LoadString(IDS_WARNING_RESET_BIND_INFO);

	int id = MessageBox( strMsg, strTitle, MB_YESNO|MB_ICONEXCLAMATION );

	if (IDYES != id) {
	
		return;
	}

	NDASCOMM_CONNECTION_INFO ci;
	AutoCursor l_auto_cursor(IDC_WAIT);

	NDAS_BIND_STATUS bindStatus = m_ndasStatus.OnResetBindInfo( logicalDev );

	l_auto_cursor.Release();

	if (bindStatus != NDAS_BIND_STATUS_OK) {

		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);
	}

	RefreshStatus();	
}

VOID CMainFrame::OnNdasDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH);
}

VOID CMainFrame::OnNdasLogDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH);
}

void 
CMainFrame::OnInterAppMsg(WPARAM wParam, LPARAM lParam)
{
	if (NDASBIND_INSTMSG_POPUP == wParam)
	{
		::SetForegroundWindow(m_hWnd);
	}
}

// ex: 120.74GB

CString CMainFrame::GetCapacityString(UINT64 ui64capacity)
{
	CString strText;
	UINT64 ui64capacityMB = ui64capacity / (1024 * 1024);

	if (ui64capacityMB == 0) {

		strText = _T("");
	
	} else {

		strText.FormatMessage( IDS_DISK_SIZE_IN_GB,
							   (UINT32)(ui64capacityMB / 1024),
							   (UINT32)(ui64capacityMB % 1024) * 100 / 1024 );
	}

	return strText;
}

NBLogicalDevPtrList CMainFrame::GetOperatableSingleDevices()
{
	NBLogicalDevPtrList listUnitDevicesSingle;
	CNBLogicalDev		*logicalDev;

	for (NBLogicalDevPtrList::iterator itLogicalDevice = m_ndasStatus.m_LogicalDevList.begin();
		itLogicalDevice != m_ndasStatus.m_LogicalDevList.end(); itLogicalDevice++) {

		logicalDev = *itLogicalDevice;

		if (logicalDev->IsLeaf() && logicalDev->IsRoot() && logicalDev->IsBindOperatable()) {

			listUnitDevicesSingle.push_back(logicalDev);
		}
	}

	return listUnitDevicesSingle;
}

NBLogicalDevPtrList CMainFrame::GetBindOperatableDevices(CNBLogicalDev *LogicalDevice)
{
	NBLogicalDevPtrList listUnitDevices;

	ATLASSERT( !LogicalDevice->IsLeaf() );

	for (UINT32 i = 0; i < LogicalDevice->NumberOfChild(); i++) {

		if (LogicalDevice->Child(i)->IsBindOperatable() == FALSE) {

			continue;
		}

		listUnitDevices.push_back(LogicalDevice->Child(i));
	}

	return listUnitDevices;
}

CString CMainFrame::GetIDString(CNBLogicalDev *LogicalDevice, TCHAR HiddenChar)
{
	CString strText;
	NDAS_ID ndasId;

	if (!LogicalDevice->IsLeaf()) {

		strText = _T("");

		return strText;
	}

	LogicalDevice->GetNdasID(&ndasId);

	CString strID = ndasId.Id;

	strID.Remove(_T('-'));

	strText += strID.Mid(0, 5) + _T("-") + strID.Mid(5, 5) + _T("-") + strID.Mid(10, 5) + _T("-");

	strText += HiddenChar;
	strText += HiddenChar;
	strText += HiddenChar;
	strText += HiddenChar;
	strText += HiddenChar;

	return strText;
}

CString CMainFrame::GetRaidStatusString(CNBLogicalDev *LogicalDev)
{
	CString strText;
	CString strTemp;

	if (LogicalDev->IsLeaf()) {

		switch (LogicalDev->GetType()) {

		case NMT_CONFLICT:	
			
			strText.LoadString(IDS_RAID_STATUS_DIB_CONFLICT);	

			break;	
	
		case NMT_AOD: 
		case NMT_VDVD: 
		case NMT_CDROM: 
		case NMT_OPMEM: 
		case NMT_FLASH: 
		case NMT_SINGLE: 		

			// Not a RAID.

			return _T("");

		case NMT_MIRROR: 
		case NMT_SAFE_RAID1: 	
		case NMT_RAID1: 
		case NMT_RAID1R2: 	
		case NMT_RAID4:
		case NMT_RAID4R2:

			// No status string. Logical dev will show Migrate status.
			
			return _T("");

		case NMT_AGGREGATE: 
		case NMT_RAID0:

			return _T("");

		case NMT_INVALID: {

			NdasUiDbgCall( 4, _T("GetRaidStatusString NMT_INVALID\n") );

			if (LogicalDev->IsRoot()) {
				
				return _T("");
			}

			if (LogicalDev->RootLogicalDev()->GetType() == NMT_AGGREGATE || LogicalDev->RootLogicalDev()->GetType() == NMT_RAID0) {

				return _T("");
			}

			ATLASSERT( LogicalDev->RootLogicalDev()->GetType() == NMT_RAID1R3 || 
					   LogicalDev->RootLogicalDev()->GetType() == NMT_RAID4R3 ||
					   LogicalDev->RootLogicalDev()->GetType() == NMT_RAID5 );
		}

		case NMT_RAID1R3: 
		case NMT_RAID4R3: 
		case NMT_RAID5: {

			if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

				if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_BAD_DISK)) {

					strText.LoadString(IDS_RAID_STATUS_BAD_DISK);
					strText += _T("\n");
				} 

				if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)) {

					strText.LoadString(IDS_RAID_STATUS_BAD_SECTOR);
					strText += _T("\n");
				} 

				if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)) {

					strTemp.LoadString(IDS_RAID_STATUS_REPLACED_BY_SPARE);
					strText += strTemp;
				}
		
			} else if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

				strText.LoadString(IDS_RAID_STATUS_OUT_OF_SYNC);

			} else if (FlagOn(LogicalDev->RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_SPARE)) {

				strText.LoadString(IDS_RAID_STATUS_SPARE);
		
			} else {
			
				strText = _T("");
			}
		
			break;
		}

		default:

			strText = _T(""); // anyway this does not happen.
			break;
		}
	
		return strText;
	}

	switch (LogicalDev->GetType()) {

	case NMT_INVALID:	strText = _T(""); break;
	case NMT_SINGLE:	strText = _T(""); break;
	case NMT_CDROM:		strText.LoadString(IDS_LOGDEV_TYPE_DVD_DRIVE);		break;
	case NMT_OPMEM:		strText.LoadString(IDS_LOGDEV_TYPE_MO_DRIVE);		break;
	case NMT_FLASH:		strText.LoadString(IDS_LOGDEV_TYPE_CF_DRIVE);		break;
	case NMT_CONFLICT:	strText.LoadString(IDS_RAID_STATUS_DIB_CONFLICT);	break;

		ATLASSERT(FALSE);

		break;

	case NMT_MIRROR: 
	case NMT_RAID1: 
	case NMT_RAID1R2: 
	case NMT_RAID4: 
	case NMT_RAID4R2: {

		strText.LoadString(IDS_RAID_STATUS_MIGRATION_REQUIRED);
		break;
	}

	case NMT_AGGREGATE: 
	case NMT_RAID0: 
	case NMT_RAID1R3: 
	case NMT_RAID4R3: 
	case NMT_RAID5: {

		if (LogicalDev->IsMountable()) {
			
			if (LogicalDev->IsFixRequired()) {

				if (LogicalDev->IsDefective()) {

					strText.LoadString(IDS_RAID_STATUS_DEFECTIVE_DISK);

				} else {

					strText.LoadString(IDS_RAID_STATUS_MISSING_MEMBER);				
				}
			
			} else {

				if (LogicalDev->NdasrStatus() == NRMX_RAID_STATE_DEGRADED) {

					strText.LoadString(IDS_RAID_STATUS_DEGRADED);				

				} else if (LogicalDev->NdasrStatus() == NRMX_RAID_STATE_OUT_OF_SYNC) {

					DWORD SynchingProgress;

					if (LogicalDev->NdasRinfo()->TotalBitCount) {

						SynchingProgress = 100 - (LogicalDev->NdasRinfo()->OosBitCount * 100/LogicalDev->NdasRinfo()->TotalBitCount);

						if (SynchingProgress == 100) {

							SynchingProgress =99;
						}

						strText.FormatMessage(IDS_RAID_STATUS_NEED_RESYNC_PROGRESS, SynchingProgress);

					} else {

						strText.LoadString(IDS_RAID_STATUS_NEED_RESYNC);
					}

				} else {

					strText.LoadString(IDS_RAID_STATUS_HEALTHY);
				}
			}
		
		} else {

			strText.LoadString(IDS_RAID_STATUS_MISSING_MEMBER);				
		}

		break;
	}

	default:

		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, LogicalDev->GetType());
		break;
	}

	return strText;
}

CString CMainFrame::pGetMenuString(UINT MenuId)
{
	CMenu menu;
	CString menuString;

	menu.LoadMenu(MAKEINTRESOURCE(IDR_MAINPOPUP));
	menu.GetMenuString(MenuId, menuString, MF_BYCOMMAND);
	menuString.Remove(_T('&'));
	menu.DestroyMenu();
	
	return menuString;
}

CString CMainFrame::GetCommentStringLeaf(CNBLogicalDev *LogicalDev)
{
	CString strText;
	CString strFmt;
	CString menuStr1;
	CString menuStr2;
	CString menuStr3;

	switch (LogicalDev->GetType()) {

	case NMT_CONFLICT: 

		strFmt.LoadString(IDS_RAID_COMMENT_DIB_CONFLICT_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_RESET_BIND_INFO);
		strText.FormatMessage(strFmt, menuStr1);

		break;

	case NMT_INVALID:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_SINGLE:
		
		// No comment.

		break;

	case NMT_MIRROR:
	case NMT_SAFE_RAID1:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID4:
	case NMT_RAID4R2:
		
		// Older version. No comment.

		break;
	
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_RAID5:		

		NdasUiDbgCall( 4, _T("LogicalDev->RaidMemberStatus() = %d\n"), LogicalDev->RaidMemberStatus() );

		if (LogicalDev->RaidMemberStatus() & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE) {

			strFmt.LoadString(IDS_RAID_COMMENT_REPLACED_BY_SPARE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_REPLACE_DEVICE);
			menuStr2 = pGetMenuString(IDM_TOOL_REMOVE_FROM_RAID);
			menuStr3 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);			
			strText.FormatMessage(strFmt, menuStr1, menuStr2, menuStr3);
	
		} else if (LogicalDev->RaidMemberStatus() & (NDAS_UNIT_META_BIND_STATUS_BAD_DISK | NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)) {
		
			strFmt.LoadString(IDS_RAID_COMMENT_DEFECTIVE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);
			menuStr2 = pGetMenuString(IDM_TOOL_CLEAR_DEFECTIVE);
			strText.FormatMessage(strFmt, menuStr1, menuStr2);
	
		} else if (LogicalDev->RaidMemberStatus() & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED) {

			strText.LoadString(IDS_RAID_COMMENT_OUT_OF_SYNC);
		
		} else if (LogicalDev->RaidMemberStatus() & NDAS_UNIT_META_BIND_STATUS_SPARE) {
		
			strText.LoadString(IDS_RAID_COMMENT_SPARE);
		}

		break;

#if 0
		// Check serious case first

		if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED) {
			strFmt.LoadString(IDS_RAID_COMMENT_RMD_CORRUPTED_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);
			strText.FormatMessage(strFmt, menuStr1);
		} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET) {
			// Hard to happen??
			strText = _T("");
		} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_IO_FAILURE) {
			strText.LoadString(IDS_RAID_COMMENT_IO_FAILURE);
		} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH) {
			strText.LoadString(IDS_RAID_COMMENT_DIB_MISMATCH);
		} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE) {
			strFmt.LoadString(IDS_RAID_COMMENT_IRRECONCILABLE_FROM_DEGRADED_USE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_BASIC);
			strText.FormatMessage(strFmt, menuStr1, menuStr2);
		} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_REPLACED_BY_SPARE) {
			strFmt.LoadString(IDS_RAID_COMMENT_REPLACED_BY_SPARE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_REPLACE_DEVICE);
			menuStr2 = pGetMenuString(IDM_TOOL_REMOVE_FROM_RAID);
			menuStr3 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);			
			strText.FormatMessage(strFmt, menuStr1, menuStr2, menuStr3);
		} else if (LogicalDev->RaidMemberFlag() & (NDAS_RAID_MEMBER_FLAG_BAD_SECTOR |NDAS_RAID_MEMBER_FLAG_BAD_DISK)) {
			strFmt.LoadString(IDS_RAID_COMMENT_DEFECTIVE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);
			menuStr2 = pGetMenuString(IDM_TOOL_CLEAR_DEFECTIVE);
			strText.FormatMessage(strFmt, menuStr1, menuStr2);
		} else {
			// Normal status.
			if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC) {
				strText.LoadString(IDS_RAID_COMMENT_OUT_OF_SYNC);
			} else if (LogicalDev->RaidMemberFlag() & NDAS_RAID_MEMBER_FLAG_SPARE) {
				strText.LoadString(IDS_RAID_COMMENT_SPARE);
			}
		}

		break;
#endif

	default:

		strFmt.LoadString(IDS_RAID_COMMENT_UNKNOWN_TYPE_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_BASIC);
		strText.FormatMessage(strFmt, menuStr1);

		break;
	}

	return strText;
}

CString CMainFrame::GetCommentString(CNBLogicalDev *LogicalDev)
{
	if (LogicalDev->IsLeaf()) {

		return GetCommentStringLeaf(LogicalDev);
	}

	CString strText;
	CString strFmt;
	CString menuStr1;
	
	switch(LogicalDev->GetType()) {

	case NMT_INVALID:		
	case NMT_SINGLE: 
	case NMT_CDROM: 
	case NMT_OPMEM: 
	case NMT_FLASH: 

		strText = _T(""); 
		
		break;

	case NMT_CONFLICT: 

		strFmt.LoadString(IDS_RAID_COMMENT_DIB_CONFLICT_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_RESET_BIND_INFO);
		strText.FormatMessage(strFmt, menuStr1);

		break;

	case NMT_MIRROR: 
	case NMT_RAID1: 
	case NMT_RAID1R2: 

		strFmt.LoadString(IDS_RAID_COMMENT_MIGRATE_MIRROR_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_MIGRATE);
		strText.FormatMessage(strFmt, menuStr1);

		break;

	case NMT_RAID4: 
	case NMT_RAID4R2:

		strFmt.LoadString(IDS_RAID_COMMENT_MIGRATE_RAID4_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_MIGRATE);
		strText.FormatMessage(strFmt, menuStr1);

		break;

	case NMT_AGGREGATE: 
	case NMT_RAID0: 
	case NMT_RAID1R3: 
	case NMT_RAID4R3: 
	case NMT_RAID5: {	

		if (LogicalDev->IsMountable()) {
			
			if (LogicalDev->IsFixRequired()) {

				if (LogicalDev->IsDefective()) {

					strText.LoadString(IDS_RAID_COMMENT_DEFECTIVE_RAID);

				} else {

					strText.LoadString(IDS_RAID_COMMENT_NOT_ENOUGH_MEMBER);				
				}
			
			} else {

				if (LogicalDev->NdasrStatus() == NRMX_RAID_STATE_DEGRADED) {

					strText.LoadString(IDS_RAID_COMMENT_DEGRADED);				
			
				} else if (LogicalDev->NdasrStatus() == NRMX_RAID_STATE_OUT_OF_SYNC) {
			
					strText.LoadString(IDS_RAID_COMMENT_RESYNC_OUT_OF_SYNC_RAID);

				} else {
	
					strText = _T("");
				}
			}
		
		} else {

			strText.LoadString(IDS_RAID_COMMENT_NOT_ENOUGH_MEMBER);				
		}
			
#if 0

			if (logicalDevice->RAID_INFO()->FailReason & 
				(NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID |
				NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HIGHER_VER);	
			} 
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_IRRECONCILABLE) 
			{
				strText.LoadString(IDS_RAID_COMMENT_IRRECONCILABLE);
			}
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_RMD_CORRUPTED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_RECONF_RMD_CORRUPTED);
			}
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_SPARE_USED)
			{
				strText.LoadString(IDS_RAID_COMMENT_SPARE_USED);
			} else if (logicalDevice->RAID_INFO()->FailReason & 
				(NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE|
				NDAS_RAID_FAIL_REASON_DIB_MISMATCH|
				NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET|
				NDAS_RAID_FAIL_REASON_NOT_A_RAID|
				NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT |
				NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL))
			{
				if (logicalDevice->GetType() == NMT_AGGREGATE ||
					logicalDevice->GetType() == NMT_RAID0) {
					strText.LoadString(IDS_RAID_COMMENT_NO_FAULT_TOLERANT);
				} else {
					strText.LoadString(IDS_RAID_COMMENT_NOT_ENOUGH_MEMBER);				
				}
			}
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HAS_UNREGISTERED_MEMBER);
			}
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_MEMBER_DISABLED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HAS_DISABLED_MEMBER);
			}
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)
			{
				strText.LoadString(IDS_RAID_COMMENT_MIGRATION_REQ);
			} 
			else if (logicalDevice->RAID_INFO()->FailReason & NDAS_RAID_FAIL_REASON_DEFECTIVE)
			{
				strText.LoadString(IDS_RAID_COMMENT_DEFECTIVE_RAID);
			} else {
				strText = _T("");
			}

		} else {

			if (logicalDevice->NdasrStatus() == NRMX_RAID_STATE_DEGRADED) {

				strText.LoadString(IDS_RAID_COMMENT_DEGRADED);				
			
			} else if (logicalDevice->NdasrStatus() == NRMX_RAID_STATE_OUT_OF_SYNC) {
			
				strText.LoadString(IDS_RAID_COMMENT_RESYNC_OUT_OF_SYNC_RAID);

			} else {
				strText = _T("");
			}
		}
#endif

		break;
	}

	default:

		strFmt.LoadString(IDS_RAID_COMMENT_UNKNOWN_TYPE_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_RESET_BIND_INFO);
		strText.FormatMessage(strFmt, menuStr1);
		
		break;
	}

	return strText;
}
