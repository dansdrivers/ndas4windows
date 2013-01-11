// nbmainfrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"
#include "ndasdevice.h"
#include "nbuihandler.h"

#include "nbaboutdlg.h"
#include "nbunbinddlg.h"
#include "nbmirrordlgs.h"
#include "nbselremirdlg.h"
#include "ndasexception.h"
#include "nbbindsheet.h"
#include "ndashelper.h"
#include "ndasobjectbuilder.h"
#include "ndasop.h"
#include "apperrdlg.h"
#include "appconf.h"

#define ENTER_CRITICAL_SECTION(cs) {ATLTRACE("[ndasbind(%04d)]%s : CS IN  %08X\n", __LINE__, __FUNCTION__, cs); ::EnterCriticalSection(cs);}
#define LEAVE_CRITICAL_SECTION(cs) {ATLTRACE("[ndasbind(%04d)]%s : CS OUT %08X\n", __LINE__, __FUNCTION__, cs); ::LeaveCriticalSection(cs);}

VOID CALLBACK 
pNdasEventProc(
			   DWORD dwError, 
			   PNDAS_EVENT_INFO pEventInfo, 
			   LPVOID lpContext);

DWORD WINAPI 
pThreadRefreshStatus(
					 LPVOID lpParameter   // thread data
					 )
{
	CMainFrame *pMainFrame = (CMainFrame *)lpParameter;

	pMainFrame->ThreadRefreshStatus();

	ExitThread(0);
	return 0;
}

void CMainFrame::StartRefreshStatus()
{
#ifdef REFRESH_WITHOUT_THREAD
	ThreadRefreshStatus();
#else
	HANDLE hThread;
	hThread = CreateThread(
		NULL,
		0,
		pThreadRefreshStatus,
		this,
		NULL,
		NULL);

	if(!hThread) // direct call
		ThreadRefreshStatus();
#endif
}

BOOL CMainFrame::ThreadRefreshStatus()
{
	if(m_bRefreshing)
	{
		return FALSE;
	}
	// lock
	ActivateUI(FALSE);
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	m_bRefreshing = TRUE;

	// Get registered device list
	CDeviceInfoFactory *pFactory = CDeviceInfoFactory::GetInstance();
	CDeviceInfoList listDevice;
	try {
		listDevice = pFactory->Create();
	} catch ( CNDASException &e )
	{
		e.PrintStackTrace();
		// TODO : String resource
		WTL::CString strMsg;
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_MAINFRAME_SERVICE_FAIL_AT_START);
		MessageBox( 
			strMsg,
			strTitle, 
			MB_OK | MB_ICONERROR
			);

		m_bRefreshing = FALSE;
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		ActivateUI(TRUE);
		return FALSE;
	}

	if(m_pRoot)
	{
		CDiskObjectList::iterator itr;
		for ( itr = m_pRoot->begin(); itr != m_pRoot->end(); ++itr )
		{
			m_view.DeleteDiskObject( *itr );
		}
		m_mapObject.clear();
	}

	m_pRoot = 
		boost::dynamic_pointer_cast<CRootDiskObject>(
		CDiskObjectBuilder::Build(listDevice)
		);

	// Initialize treeview
	m_view.SetImageList( CObjectUIHandler::GetImageList(), TVSIL_NORMAL );
	m_view.InsertDiskObject( m_pRoot );
	BuildObjectMap(m_pRoot);

	m_bRefreshing = FALSE;
	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
	ActivateUI(TRUE);
	return TRUE;
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
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
	::DeleteCriticalSection(&m_csThreadRefreshStatus);
	PostQuitMessage(0);
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifndef BTNS_SHOWTEXT
#define BTNS_SHOWTEXT 0x0040
#endif

	// set title
	WTL::CString strTitle;
	strTitle.LoadString( IDS_APPLICATION );
	SetWindowText(strTitle);

	CreateSimpleStatusBar();

	HWND hWndToolBar = 
		CreateSimpleToolBarCtrl(
			m_hWnd, IDR_MAINFRAME, FALSE, 
			ATL_SIMPLE_TOOLBAR_STYLE | BTNS_SHOWTEXT | TBSTYLE_LIST | TBSTYLE_FLAT | TBSTYLE_FLAT);
	TBBUTTON tbButton = { 0 };
	TBBUTTONINFO tbButtonInfo = { 0 };
	TBREPLACEBITMAP replaceBitmap = { 0 };
	m_wndToolBar.Attach( hWndToolBar );
	m_wndToolBar.SetExtendedStyle( TBSTYLE_EX_DRAWDDARROWS );
	// Replace toolbar bitmap to display true color image
	replaceBitmap.hInstNew = _Module.GetResourceInstance();
	replaceBitmap.hInstOld = _Module.GetResourceInstance();
	replaceBitmap.nIDOld = IDR_MAINFRAME;
	replaceBitmap.nIDNew = IDB_MAINFRAME;
	replaceBitmap.nButtons = 4;
	m_wndToolBar.ReplaceBitmap( &replaceBitmap );
	// Add strings to the toolbar
	m_wndToolBar.SetButtonStructSize(sizeof(TBBUTTON));
	for ( int i=0; i < m_wndToolBar.GetButtonCount(); i++ )
	{
		WTL::CString strCommand;

		m_wndToolBar.GetButton( i, &tbButton );
		tbButtonInfo.cbSize	= sizeof(TBBUTTONINFO);
		tbButtonInfo.dwMask = TBIF_STYLE;
		m_wndToolBar.GetButtonInfo( tbButton.idCommand, &tbButtonInfo );
		tbButtonInfo.dwMask = TBIF_TEXT | TBIF_STYLE;
		strCommand.LoadString( tbButton.idCommand );
		strCommand = strCommand.Right(
						strCommand.GetLength() - strCommand.Find('\n') - 1
						);
		tbButtonInfo.pszText = 
			const_cast<LPTSTR>(static_cast<LPCTSTR>(strCommand));
		tbButtonInfo.cchText = strCommand.GetLength();
		tbButtonInfo.fsState |= BTNS_SHOWTEXT;
		m_wndToolBar.AddString( tbButton.idCommand );
		m_wndToolBar.SetButtonInfo( tbButton.idCommand, &tbButtonInfo );
	}
	// Modify mirror button as dropdown button
	m_wndToolBar.GetButton( 
		m_wndToolBar.CommandToIndex(IDM_AGGR_MIRROR), 
		&tbButton 
		);
	tbButtonInfo.cbSize = sizeof(TBBUTTONINFO);
	tbButtonInfo.dwMask = TBIF_STYLE;
	tbButtonInfo.fsStyle = tbButton.fsStyle | TBSTYLE_DROPDOWN;
	m_wndToolBar.SetButtonInfo( IDM_AGGR_MIRROR, &tbButtonInfo );
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);

	AddSimpleReBarBand(m_wndToolBar);
	UIAddToolBar(m_wndToolBar);

	m_hWndClient = m_view.Create(
					m_hWnd, 
					rcDefault, 
					NULL, 
					WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN 
					| TVS_HASLINES | /* TVS_LINESATROOT | */ TVS_SHOWSELALWAYS, 
					WS_EX_CLIENTEDGE
					);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// TODO : It will be better if we display splash window while
	//		the treeview is initialized

	m_bRefreshing = FALSE;
	::InitializeCriticalSection(&m_csThreadRefreshStatus);
	StartRefreshStatus();

	m_hEventCallback = 
		::NdasRegisterEventCallback(pNdasEventProc,m_hWnd);


	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	// FIXME : We need to remember the window size
	CRect rectResize;
	GetClientRect( rectResize );
	rectResize = CRect( rectResize.TopLeft(), CSize(500, 500) );
	ClientToScreen( rectResize );
	MoveWindow( rectResize );
	CenterWindow();
	return 0;
}

void CMainFrame::BuildObjectMap(CDiskObjectPtr o)
{
	o->Accept( o, this );	// To build object map
}

void CMainFrame::AddObjectToMap(CDiskObjectPtr o)
{
	o->Accept( o, this );	// To add object map
}

void CMainFrame::Visit(CDiskObjectPtr o)
{
	m_mapObject.insert( std::make_pair(o->GetUniqueID(), o) );
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

void CMainFrame::OnContextMenu(HWND /*hWnd*/, CPoint pos)
{
	if(m_bRefreshing)
		return;

	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	CRect rect;
	CPoint posInView;
	CTreeItem itemSelected;
	m_view.GetWindowRect( rect );

	if ( !rect.PtInRect(pos) )
	{
		// If clicked point is outside the tree control, do nothing.
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}
	// Change screen coordinates to client coordinates
	posInView = pos - rect.TopLeft();

	itemSelected = m_view.HitTest( posInView, NULL );

	CMenu menu;
	CMenu newSubMenu;
	CMenuHandle subMenu;

	menu.LoadMenu( MAKEINTRESOURCE(IDR_CONTEXT_MENU) );
	subMenu = menu.GetSubMenu(0);
	if ( subMenu.IsNull() )
	{
		newSubMenu.CreatePopupMenu();
		subMenu = newSubMenu;
	}
	if ( itemSelected.IsNull() )
	{
		// Display default menu
		subMenu.TrackPopupMenu(
			TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
			pos.x, 
			pos.y, 
			m_hWnd
			);
	}
	else
	{
		// Change select
		m_view.SelectItem(itemSelected);

		// Display context menu
		CDiskObjectPtr obj = m_mapObject[itemSelected.GetData()];
		ATLASSERT( obj.get() != NULL );
		CObjectUIHandler::GetUIHandler(obj)->InsertMenu(obj, subMenu);
		subMenu.TrackPopupMenu(
			TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
			pos.x, 
			pos.y, 
			m_hWnd
			);
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}

LRESULT CMainFrame::OnSelChanged(LPNMHDR /*lpNLHDR*/)
{
	CTreeItem itemSelected;
	itemSelected = m_view.GetSelectedItem();

	UIEnable(IDM_AGGR_UNBIND, FALSE);
//	UIEnable(IDM_AGGR_REMIRROR, FALSE);
	UIEnable(IDM_AGGR_SYNCHRONIZE, FALSE);
	UIEnable(IDM_AGGR_ADDMIRROR, FALSE);
	if ( !itemSelected.IsNull() )
	{
		CDiskObjectPtr obj = m_mapObject[itemSelected.GetData()];
		ATLASSERT( obj.get() != NULL );
		CCommandSet cmdSet = 
			CObjectUIHandler::GetUIHandler(obj)->GetCommandSet(obj);
		CCommandSet::iterator itr;
		for ( itr = cmdSet.begin(); itr != cmdSet.end(); itr++ )
		{
			UIEnable( itr->GetID(), !itr->IsDisabled() );
		}
	}
	return 0;
}

LRESULT CMainFrame::OnToolBarDropDown(LPNMHDR lpNMHDR)
{
	NMTOOLBAR* pnmtb = reinterpret_cast<NMTOOLBAR*>(lpNMHDR);
	switch(pnmtb->iItem)
	{
	case IDM_AGGR_MIRROR:
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
	nmtb.iItem =  IDM_AGGR_MIRROR;
	OnToolBarDropDown( reinterpret_cast<LPNMHDR>(&nmtb) );
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Implementation of command handling methods
//
///////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBind(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	CDiskObjectList singleDisks;
	CFindIfVisitor<FALSE> singleDiskFinder;
	WTL::CString strMsg;

	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsWritableUnitDisk);
	if ( singleDisks.size() == 0 )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		WTL::CString strMsg;
		strMsg.LoadString(IDS_MAINFRAME_NO_DISK_TO_BIND);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}
	else if ( singleDisks.size() == 1 )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		WTL::CString strMsg;
		strMsg.LoadString(IDS_MAINFRAME_NOT_ENOUGH_DISK_TO_BIND);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}
	CBindSheet dlgBind;
	dlgBind.SetSingleDisks(singleDisks);

	if ( dlgBind.DoModal() == IDOK )
	{
		// AING : Cause dlgBind use ndasop.lib to bind disks, 
		// you can't ensure each disk information is stable after bind process.
		OnRefreshStatus(NULL, NULL, NULL);
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}

void CMainFrame::OnSingle(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	WTL::CString strMsg;

	CTreeItem itemSelected = m_view.GetSelectedItem();
	if ( itemSelected.IsNull() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj, parent;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];

	if(!obj->IsUnitDisk())
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	// AING_TO_DO
	{
		strMsg.LoadString(IDS_WARNING_SINGLE);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);

		int ret = MessageBox( 
			strMsg,
			strTitle,
			MB_YESNO | MB_ICONWARNING
			);

		if(IDYES != ret)
		{
			LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
			return;
		}
	}

	//
	// Check whether any disk is being accessed by other program/computer
	//
	if ( !obj->CanAccessExclusive() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		strMsg.LoadString(IDS_MAINFRAME_UNBIND_ACCESS_FAIL);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	if(!(obj->GetAccessMask() & GENERIC_WRITE))
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
		strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
			obj->GetTitle()
			);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONERROR
			);

		return;
	}

	NDAS_CONNECTION_INFO ConnectionInfo;

	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	
	ZeroMemory(&ConnectionInfo, sizeof(NDAS_CONNECTION_INFO));

	ConnectionInfo.type = NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS;
	ConnectionInfo.UnitNo = unitDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ConnectionInfo.bWriteAccess = TRUE;
	ConnectionInfo.ui64OEMCode = NULL;
	ConnectionInfo.protocol = IPPROTO_LPXTCP;
	CopyMemory(ConnectionInfo.MacAddress, 
		unitDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		LPXADDR_NODE_LENGTH);

	BOOL bResults = NdasOpBind(
		1,
		&ConnectionInfo,
		NMT_SINGLE);

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	if(1 != bResults)
	{
		DWORD dwLastError = ::GetLastError();

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error
			strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, unitDisk->GetTitle());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);
	}

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		NDAS_UNITDEVICE_ID unitDeviceId;
		CopyMemory(unitDeviceId.DeviceId.Node, ConnectionInfo.MacAddress, 
			sizeof(unitDeviceId.DeviceId.Node));
		unitDeviceId.UnitNo = ConnectionInfo.UnitNo;
		HixChangeNotify.Notify(unitDeviceId);
	}

	OnRefreshStatus(NULL, NULL, NULL);


}

void CMainFrame::OnUnBind(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	CTreeItem itemSelected = m_view.GetSelectedItem();
	if ( itemSelected.IsNull() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj, parent;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];
	
	// Find topmost group composite of bind
	parent = obj->GetParent();
	while ( !parent->IsRoot() )
	{
		obj = parent;
		parent = obj->GetParent();
	}

	//
	// Check whether any disk is being accessed by other program/computer
	//
	if ( !obj->CanAccessExclusive() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		WTL::CString strMsg;
		strMsg.LoadString(IDS_MAINFRAME_UNBIND_ACCESS_FAIL);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	// Unbind disks
	CUnBindDlg dlgUnbind;

	dlgUnbind.SetDiskToUnbind(obj);
	if ( dlgUnbind.DoModal() == IDOK )
	{
		// AING : Cause dlgBind use ndasop.lib to bind disks, 
		// you can't ensure each disk information is stable after bind process.
		OnRefreshStatus(NULL, NULL, NULL);
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}

void CMainFrame::OnReestablishMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}
	CDiskObjectList singleDisks;
	CFindIfVisitor<FALSE> singleDiskFinder;
	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsWritableUnitDisk);
	if ( singleDisks.size() == 0 )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// TODO : No disk is available
		WTL::CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NO_DISK_TO_MIRROR );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}
	CDiskObjectPtr obj;
	CMirDiskObjectPtr mirDisks;
	CUnitDiskObjectPtr sourceDisk, replacedDisk;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];

	// Get reference to the mirror group if selected object is a unit disk
	if ( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL )
		obj = obj->GetParent();
	mirDisks = boost::dynamic_pointer_cast<CMirDiskObject>(obj);
	ATLASSERT( mirDisks != NULL );
	if ( mirDisks->front()->IsUsable() )
	{
		sourceDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->front());
		replacedDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->back());
	}
	else
	{
		sourceDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->back());
		replacedDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->front());
	}

	//
	// Reestablishing mirror supports disks bound by DIBv2 only.
	//
	CUnitDiskInfoHandlerPtr handler = sourceDisk->GetInfoHandler();
	if ( !handler->HasBlockV2() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// TODO : String resource
		WTL::CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_OLD_VERSION );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			strMsg,
			strTitle, 
			MB_OK | MB_ICONERROR
			);
		return;
	}

	DWORD UnitNo = 0;
	CSelectMirDlg dlgSelDisk( IDD_REMIR );
	dlgSelDisk.SetSingleDisks( singleDisks );
	dlgSelDisk.SetSourceDisk( sourceDisk );
	if ( dlgSelDisk.DoModal() == IDOK )
	{
		CMirrorDlg dlgReMir(NBSYNC_TYPE_REMIRROR);
		CUnitDiskObjectPtr newDisk = dlgSelDisk.GetSelectedDisk();
		dlgReMir.SetReMirDisks(sourceDisk, newDisk);
		dlgReMir.DoModal();

		CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
		BOOL bResults = HixChangeNotify.Initialize();
		if(bResults)
		{
			NDAS_UNITDEVICE_ID unitDeviceId;

			CopyMemory(unitDeviceId.DeviceId.Node,
				sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);

			CopyMemory(unitDeviceId.DeviceId.Node,
				newDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				newDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);
		}

		// Update tree
		//	- Update tree even when the user canceled.
		//    Since the status of tree can be changed.
		CUnitDiskInfoHandlerPtr handler = newDisk->GetInfoHandler();
		if ( handler->IsBound() )
		{
			m_pRoot->DeleteChild( newDisk );
			m_view.DeleteDiskObject( newDisk );
			mirDisks->AddChild( mirDisks, newDisk );
			m_view.InsertDiskObject( newDisk, mirDisks );
			m_view.UpdateDiskObject(mirDisks);
		}
	}
	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}

void CMainFrame::OnAddMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectList singleDisks;
	CUnitDiskObjectPtr  sourceDisk;
	sourceDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(
			m_mapObject[m_view.GetItemData(itemSelected)]
			);
	ATLASSERT( sourceDisk.get() != NULL );

	CFindIfVisitor<FALSE> singleDiskFinder;
	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsWritableUnitDisk);
	singleDisks.remove( sourceDisk );
	if ( singleDisks.size() == 0 )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// TODO : No disk is available
		WTL::CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NO_DISK_TO_MIRROR );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg, 
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	DWORD UnitNo = 0;
	CSelectMirDlg dlgSelDisk(IDD_ADDMIR);
	dlgSelDisk.SetSingleDisks( singleDisks );
	dlgSelDisk.SetSourceDisk( sourceDisk );
	if ( dlgSelDisk.DoModal() == IDOK )
	{
		// Bind & Synchronize 
		CUnitDiskObjectPtr mirDisk = dlgSelDisk.GetSelectedDisk();
		CMirrorDlg dlgAddMir(NBSYNC_TYPE_ADDMIRROR);
		dlgAddMir.SetAddMirDisks( sourceDisk, mirDisk );
		// Delete items from the tree 
		// If we do not delete items before we change the status of items,
		// error can be occure because the status that tree knows differs
		// from the items's real status
		m_view.DeleteDiskObject( sourceDisk );
		m_view.DeleteDiskObject( mirDisk );
		dlgAddMir.DoModal();

		CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
		BOOL bResults = HixChangeNotify.Initialize();
		if(bResults)
		{
			NDAS_UNITDEVICE_ID unitDeviceId;

			CopyMemory(unitDeviceId.DeviceId.Node, 
				sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);

			CopyMemory(unitDeviceId.DeviceId.Node, 
				mirDisk->GetLocation()->GetUnitDiskLocation()->MACAddr, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				mirDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);
		}

		// Update tree
		//	- Update tree even when the user canceled.
		//    Since the status of tree can be changed.
		CUnitDiskInfoHandlerPtr pHandler = sourceDisk->GetInfoHandler();
		// Check if the disks are updated
		if ( pHandler->IsBound() )
		{
			m_pRoot->DeleteChild( sourceDisk );
			m_pRoot->DeleteChild( mirDisk );
			CMirDiskObjectPtr mirDisks = CMirDiskObjectPtr( new CMirDiskObject() );
			mirDisks->AddChild( mirDisks, sourceDisk );
			mirDisks->AddChild( mirDisks, mirDisk );
			AddObjectToMap(mirDisks);
			m_pRoot->AddChild(m_pRoot, mirDisks);
			m_view.InsertDiskObject( mirDisks, m_pRoot );
		}
		else
		{
			m_view.InsertDiskObject( sourceDisk, m_pRoot );
			m_view.InsertDiskObject( mirDisk, m_pRoot );
		}
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}


void CMainFrame::OnRefreshStatus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	StartRefreshStatus();
}

void CMainFrame::OnCommand(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	// Commands which do not change the tree are sent to the object directly.
	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj;
	const CObjectUIHandler *phandler;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];
	ATLASSERT( obj.get() != NULL );
	phandler = CObjectUIHandler::GetUIHandler( obj );
	phandler->OnCommand( obj, wID );
	
	m_view.UpdateDiskObject( obj );

	OnSelChanged(NULL);

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
}


void CMainFrame::ActivateUI(BOOL bActivate)
{
	m_wndToolBar.EnableWindow(bActivate);	

	HMENU hMenu = ATL::CWindow::GetMenu();
	if(!hMenu)
		return;

	UINT uEnableMenuItem = (bActivate) ? MF_ENABLED : MF_GRAYED;
	EnableMenuItem(hMenu, IDM_AGGR_REFRESH, uEnableMenuItem);
	EnableMenuItem(hMenu, IDM_AGGR_BIND, uEnableMenuItem);
	EnableMenuItem(hMenu, IDM_AGGR_UNBIND, uEnableMenuItem);
	EnableMenuItem(hMenu, IDM_AGGR_ADDMIRROR, uEnableMenuItem);
	EnableMenuItem(hMenu, IDM_AGGR_SYNCHRONIZE, uEnableMenuItem);
	EnableMenuItem(hMenu, IDM_AGGR_SINGLE, uEnableMenuItem);

	if(bActivate)
	{
		::SetWindowText(m_hWndStatusBar, _T(""));
	}
	else
	{
		WTL::CString strStatusText;
		strStatusText.LoadString(IDS_STATUS_REFRESH);
		::SetWindowText(m_hWndStatusBar, strStatusText);
	}
}

VOID CMainFrame::OnNdasDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_AGGR_REFRESH);
}
VOID CMainFrame::OnNdasLogDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_AGGR_REFRESH);
}

VOID 
CALLBACK 
pNdasEventProc(
			   DWORD dwError, 
			   PNDAS_EVENT_INFO pEventInfo, 
			   LPVOID lpContext)
{
	if (NULL == pEventInfo) {
		ATLTRACE(_T("Event Error %d (0x%08X)\n"), dwError, dwError);
		return;
	}

	HWND hWnd = reinterpret_cast<HWND>(lpContext);

	WPARAM wParam(0);
	LPARAM lParam(0);

	switch (pEventInfo->EventType) 
	{
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_DEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	default:
		;
	}
}
