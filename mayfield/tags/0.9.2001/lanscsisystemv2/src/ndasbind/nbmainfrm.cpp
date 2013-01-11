// nbmainfrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"
#include "ndasdevice.h"
#include "nbuihandler.h"

#include "nbaboutdlg.h"
#include "nbdefine.h"
#include "nbbinddlg.h"
#include "nbunbinddlg.h"
#include "nbmirrordlgs.h"
#include "nbselremirdlg.h"
#include "ndasexception.h"
#include "nbbindsheet.h"
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

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifndef BTNS_SHOWTEXT
#define BTNS_SHOWTEXT 0x0040
#endif
	CreateSimpleStatusBar();
	HWND hWndToolBar = 
		CreateSimpleToolBarCtrl(
			m_hWnd, IDR_MAINFRAME, FALSE, 
			ATL_SIMPLE_TOOLBAR_PANE_STYLE | BTNS_SHOWTEXT | TBSTYLE_LIST );
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
	replaceBitmap.nButtons = 3;
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

	// Get registered device list
	CDeviceInfoFactory *pFactory = CLocalDeviceInfoFactory::GetInstance();
	CDeviceInfoList listDevice;
	try {
		listDevice = pFactory->Create();
	} catch ( CNDASException &e )
	{
		e.PrintStackTrace();
		//SendMessage(WM_CLOSE);
		// TODO : String resource
		MessageBox( 
			_T("Fail to retrieve list of devices from service.\nCannot start the application."), 
			_T(PROGRAM_TITLE), 
			MB_OK | MB_ICONERROR
			);
		return -1;
	}
	
	// Build object tree from the device list
	m_pRoot = CDiskObjectBuilder::GetInstance()->BuildFromDeviceInfo(listDevice);
			
	// Initialize treeview
	m_view.SetImageList( CObjectUIHandler::GetImageList(), TVSIL_NORMAL );
	m_view.InsertDiskObject( m_pRoot );
	BuildObjectMap(m_pRoot);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	// FIXME : We need to remember the window size
	CRect rectResize;
	GetClientRect( rectResize );
	rectResize = CRect( rectResize.TopLeft(), CSize(400, 500) );
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
	CRect rect;
	CPoint posInView;
	CTreeItem itemSelected;
	m_view.GetWindowRect( rect );

	if ( !rect.PtInRect(pos) )
	{
		// If clicked point is outside the tree control, do nothing.
		return;
	}
	// Change screen coordinates to client coordinates
	posInView = pos - rect.TopLeft();

	itemSelected = m_view.HitTest( posInView, NULL );

	CMenu menu;
	CMenuHandle subMenu;

	menu.LoadMenu( MAKEINTRESOURCE(IDR_CONTEXT_MENU) );
	subMenu = menu.GetSubMenu(0);
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
		CObjectUIHandler::GetUIHandler(obj.get())->InsertMenu(obj.get(), subMenu);
		subMenu.TrackPopupMenu(
			TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
			pos.x, 
			pos.y, 
			m_hWnd
			);
	}
}

LRESULT CMainFrame::OnSelChanged(LPNMHDR /*lpNLHDR*/)
{
	CTreeItem itemSelected;
	itemSelected = m_view.GetSelectedItem();

	UIEnable(IDM_AGGR_UNBIND, FALSE);
	UIEnable(IDM_AGGR_REMIRROR, FALSE);
	UIEnable(IDM_AGGR_SYNCHRONIZE, FALSE);
	UIEnable(IDM_AGGR_ADDMIRROR, FALSE);
	if ( !itemSelected.IsNull() )
	{
		CDiskObjectPtr obj = m_mapObject[itemSelected.GetData()];
		ATLASSERT( obj.get() != NULL );
		CCommandSet cmdSet = 
			CObjectUIHandler::GetUIHandler(obj.get())->GetCommandSet(obj.get());
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
	CDiskObjectList singleDisks;
	CFindIfVisitor<FALSE> singleDiskFinder;

	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsUnitDisk);
	if ( singleDisks.size() == 0 )
	{
		// TODO : String resource
		MessageBox(
			_T("There is no disk available to bind."), 
			_T(PROGRAM_TITLE), 
			MB_OK
			);
		return;
	}
	else if ( singleDisks.size() == 1 )
	{
		// TODO : String resource
		MessageBox(
			_T("There is only one disk available.\nAt least 2 disks are required to bind."), 
			_T(PROGRAM_TITLE), 
			MB_OK
			);
		return;
	}
	/*
	CBindDlg dlgBind;
	dlgBind.SetSingleDisks(singleDisks);
	*/
	CBindSheet dlgBind;
	dlgBind.SetSingleDisks(singleDisks);
	if ( dlgBind.DoModal() == IDOK )
	{
		unsigned int i;
		CDiskObjectVector boundDisks = dlgBind.GetBoundDisks();
		CDiskObjectCompositePtr root = 
			boost::dynamic_pointer_cast<CDiskObjectComposite>(m_pRoot);
		// Rebuild tree
		// Delete objects from tree
		for ( i=0; i < boundDisks.size(); i++ )
		{
			m_view.DeleteDiskObject( boundDisks[i] );
			root->DeleteChild( boundDisks[i] );
		}
		// Build tree of objects
		CDiskObjectCompositePtr aggrDisk = 
						CDiskObjectCompositePtr( new CAggrDiskObject() );
		
		BOOL bUseMirror = (dlgBind.GetBindType() != BIND_TYPE_AGGR_ONLY);
		for ( i=0; i < boundDisks.size(); i+= (bUseMirror?2:1) )
		{
			if ( bUseMirror )
			{
				CDiskObjectCompositePtr mirDisk = 
					CDiskObjectCompositePtr( new CMirDiskObject() );
				mirDisk->AddChild( mirDisk, boundDisks[i] );
				mirDisk->AddChild( mirDisk, boundDisks[i+1] );
				aggrDisk->AddChild( aggrDisk, mirDisk );
			}
			else
			{
				aggrDisk->AddChild( aggrDisk, boundDisks[i] );
			}
		}
		// Add new objects to tree
		if ( aggrDisk->size() == 1 )
		{
			// Use only one mirroring
			root->AddChild( root, aggrDisk->front() );
			m_view.InsertDiskObject( aggrDisk->front(), m_pRoot );
			aggrDisk->front()->Accept( aggrDisk->front(), this );
		}
		else
		{
			root->AddChild( root, aggrDisk );
			m_view.InsertDiskObject( aggrDisk, root );
			aggrDisk->Accept( aggrDisk, this );
		}
	}
}

void CMainFrame::OnUnBind(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CTreeItem itemSelected = m_view.GetSelectedItem();
	if ( itemSelected.IsNull() )
		return;

	CDiskObjectPtr obj, parent;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];
	
	// Find topmost group composite of bind
	parent = obj->GetParent();
	while ( !parent->IsRoot() )
	{
		obj = parent;
		parent = obj->GetParent();
	}

	// Unbind disks
	CUnBindDlg dlgUnbind;

	dlgUnbind.SetDiskToUnbind(obj);
	if ( dlgUnbind.DoModal() == IDOK )
	{
		CDiskObjectList unboundDisks;
		CDiskObjectList::iterator itr;
		CDiskObjectCompositePtr root 
			= boost::dynamic_pointer_cast<CDiskObjectComposite>(m_pRoot);

		unboundDisks = dlgUnbind.GetUnboundDisks();
		m_view.DeleteDiskObject(obj);
		for ( itr=unboundDisks.begin(); itr!=unboundDisks.end(); ++itr )
		{
			root->AddChild( root, *itr );
			m_view.InsertDiskObject( *itr );
		}
	}
}

void CMainFrame::OnSynchronize(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
		return;

	CDiskObjectPtr obj;
	CMirDiskObjectPtr mirDisks;
	obj = m_mapObject[m_view.GetItemData(itemSelected)];

	// Get reference to the mirror group if selected object is a unit disk
	if ( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL )
		obj = obj->GetParent();
	
	mirDisks = boost::dynamic_pointer_cast<CMirDiskObject>(obj);
	ATLASSERT( mirDisks != NULL );

	CMirrorDlg dlgSync;
	dlgSync.SetSyncDisks( mirDisks );
	if ( dlgSync.DoModal() == IDOK )
	{
		// Update tree items
		m_view.UpdateDiskObject(mirDisks);
	}
	return;
}

void CMainFrame::OnReestablishMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
		return;

	CDiskObjectList singleDisks;
	CFindIfVisitor<FALSE> singleDiskFinder;
	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsUnitDisk);
	if ( singleDisks.size() == 0 )
	{
		// TODO : No disk is available
		MessageBox(
			_T("There is no disk available to be used as a mirror disk."), 
			_T(PROGRAM_TITLE), 
			MB_OK
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

	CSelectMirDlg dlgSelDisk;
	dlgSelDisk.SetSingleDisks(singleDisks);
	dlgSelDisk.SetSourceDisk(sourceDisk);
	if ( dlgSelDisk.DoModal() == IDOK )
	{
		CMirrorDlg dlgReMir(NBSYNC_TYPE_REMIRROR);
		CUnitDiskObjectPtr newDisk = dlgSelDisk.GetSelectedDisk();
		dlgReMir.SetReMirDisks(mirDisks, newDisk);
		dlgReMir.DoModal();
		// Update tree
		//	- Update tree even when the user canceled.
		//    Since the status of tree can be changed.
		m_view.DeleteDiskObject(newDisk);
		m_view.InsertDiskObject(newDisk, newDisk->GetParent());
		m_view.UpdateDiskObject(mirDisks);
	}
}

void CMainFrame::OnAddMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CTreeItem itemSelected = m_view.GetSelectedItem();

	if ( itemSelected.IsNull() )
		return;

	CDiskObjectList singleDisks;
	CUnitDiskObjectPtr  sourceDisk;
	sourceDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(
			m_mapObject[m_view.GetItemData(itemSelected)]
			);
	ATLASSERT( sourceDisk.get() != NULL );

	CFindIfVisitor<FALSE> singleDiskFinder;
	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsUnitDisk);
	singleDisks.remove( sourceDisk );
	if ( singleDisks.size() == 0 )
	{
		// TODO : No disk is available
		MessageBox(
			_T("There is no disk available to be used as a mirror disk."), 
			_T(PROGRAM_TITLE), 
			MB_OK
			);
		return;
	}

	CSelectMirDlg dlgSelDisk;
	dlgSelDisk.SetSingleDisks( singleDisks );
	dlgSelDisk.SetSourceDisk( sourceDisk );
	if ( dlgSelDisk.DoModal() == IDOK )
	{
		// Bind & Synchronize 
		CUnitDiskObjectPtr mirDisk = dlgSelDisk.GetSelectedDisk();
		CMirrorDlg dlgAddMir(NBSYNC_TYPE_ADDMIRROR);
		dlgAddMir.SetAddMirDisks( sourceDisk, mirDisk );
		dlgAddMir.DoModal();

		// Update tree
		//	- Update tree even when the user canceled.
		//    Since the status of tree can be changed.
		CUnitDiskInfoHandlerPtr pHandler = sourceDisk->GetInfoHandler();
		// Check if the disks are updated
		if ( pHandler->IsBound() )
		{
			m_view.DeleteDiskObject( sourceDisk );
			m_view.DeleteDiskObject( mirDisk );
			CMirDiskObjectPtr mirDisks = CMirDiskObjectPtr( new CMirDiskObject() );
			mirDisks->AddChild( mirDisks, sourceDisk );
			mirDisks->AddChild( mirDisks, mirDisk );
			AddObjectToMap(mirDisks);
			boost::dynamic_pointer_cast<CRootDiskObject>(m_pRoot)->AddChild(m_pRoot, mirDisks);
			m_view.InsertDiskObject( mirDisks, m_pRoot );
		}
	}
}