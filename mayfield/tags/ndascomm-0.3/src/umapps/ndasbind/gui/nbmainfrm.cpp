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
#include "nbselremirdlg.h"
#include "ndasexception.h"
#include "nbbindwiz.h"
#include "ndashelper.h"
#include "ndasobjectbuilder.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"
#include "appconf.h"
#include "nbrecoverdlgs.h"
#include "nbseldiskdlg.h"

#define REFRESH_WITHOUT_THREAD
#ifdef REFRESH_WITHOUT_THREAD
#define ENTER_CRITICAL_SECTION(cs)
#define LEAVE_CRITICAL_SECTION(cs)
#else
#define ENTER_CRITICAL_SECTION(cs) {ATLTRACE("[ndasbind(%04d)]%s : CS IN  %08X\n", __LINE__, __FUNCTION__, cs); ::EnterCriticalSection(cs);}
#define LEAVE_CRITICAL_SECTION(cs) {ATLTRACE("[ndasbind(%04d)]%s : CS OUT %08X\n", __LINE__, __FUNCTION__, cs); ::LeaveCriticalSection(cs);}
#endif

static
VOID CALLBACK 
pNdasEventProc(
			   DWORD dwError, 
			   PNDAS_EVENT_INFO pEventInfo, 
			   LPVOID lpContext);

static
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

BOOL WINAPI CallBackRefreshStatus(UINT number, void *context)
{
	CMainFrame *pMainFrame = (CMainFrame *)context;
	if(!pMainFrame)
		return FALSE;

	pMainFrame->m_wndRefreshProgress.StepIt();

	return TRUE;
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
			m_viewTreeList.DeleteDiskObject( *itr );
		}
		m_mapObject.clear();
	}

	m_wndRefreshProgress.ShowWindow(SW_SHOW);
	m_wndRefreshProgress.SetRange32(0, listDevice.size());
	m_wndRefreshProgress.SetStep(1);
	m_wndRefreshProgress.SetPos(0);

	m_pRoot = 
		boost::dynamic_pointer_cast<CRootDiskObject>(
			CDiskObjectBuilder::Build(listDevice, CallBackRefreshStatus, this)
		);

	m_wndRefreshProgress.ShowWindow(SW_HIDE);


	// Initialize treeview
	m_viewTreeList.GetTreeControl().SetImageList( CObjectUIHandler::GetImageList(), LVSIL_NORMAL );
	m_viewTreeList.InsertDiskObject( m_pRoot );
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
	::DeleteCriticalSection(&m_csThreadRefreshStatus);
	PostQuitMessage(0);
}

/*
void SetPaneWidths(int* arrWidths, int nPanes)
{ 
    // find the size of the borders
    int arrBorders[3];
    m_status.GetBorders(arrBorders);

    // calculate right edge of default pane (0)
    arrWidths[0] += arrBorders[2];
    for (int i = 1; i < nPanes; i++)
        arrWidths[0] += arrWidths[i];

    // calculate right edge of remaining panes (1 thru nPanes-1)
    for (int j = 1; j < nPanes; j++)
        arrWidths[j] += arrBorders[2] + arrWidths[j - 1];

    // set the pane widths
    m_status.SetParts(m_status.m_nPanes, arrWidths); 
}

*/
LRESULT CMainFrame::OnCreate(LPCREATESTRUCT /*lParam*/)
{
	//
	// create command bar window
	//
	HWND hWndCmdBar = m_CmdBar.Create(
		m_hWnd,
		rcDefault,
		NULL,
		ATL_SIMPLE_CMDBAR_PANE_STYLE);

	// attach menu
	m_CmdBar.AttachMenu(GetMenu());
	// load command bar images
	m_CmdBar.SetImageSize(CSize(9,9));
	//	m_CmdBar.LoadImages(IDR_MAINFRAME);
	// remove old menu
	SetMenu(NULL);

	// set title
	WTL::CString strTitle;
	strTitle.LoadString( IDS_APPLICATION );
	SetWindowText(strTitle);

	//
	// setting up a tool bar
	//
	HWND hWndToolBar = CreateSimpleToolBarCtrl(
		m_hWnd, 
		IDR_MAINFRAME, 
		FALSE, 
		ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST);

	m_wndToolBar.Attach( hWndToolBar );
	m_wndToolBar.SetExtendedStyle( TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS );

	//
	// patria:
	//
	// Some bitmaps are distorted when used with TB_ADDBITMAP
	// which is sent from CreateSimpleToolBarCtrl when the bitmap is not true color.
	// This is the case with IO-DATA's tool bar image.
	// As an workaround, we can directly create a image list directly
	// and replace the image list of the tool bar, which corrects such misbehaviors.
	//
	{
		CImageList imageList;
		WORD wWidth = 32; // we are using 32 x 32 buttons
		imageList.CreateFromImage(
			IDR_MAINFRAME, 
			wWidth, 
			1, 
			CLR_DEFAULT,
			IMAGE_BITMAP,
			LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
		m_wndToolBar.SetImageList(imageList);
	}

	TBBUTTON tbButton = { 0 };
	TBBUTTONINFO tbButtonInfo = { 0 };
	TBREPLACEBITMAP replaceBitmap = { 0 };

	// Add strings to the tool bar
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
		tbButtonInfo.fsStyle |= BTNS_SHOWTEXT | BTNS_AUTOSIZE;
		m_wndToolBar.AddString( tbButton.idCommand );
		m_wndToolBar.SetButtonInfo( tbButton.idCommand, &tbButtonInfo );
	}

	//
	// Modify mirror button as drop down button
	//
	{
		TBBUTTON tb;

		m_wndToolBar.GetButton(
			m_wndToolBar.CommandToIndex(IDM_AGGR_MIRROR), 
			&tb);

		TBBUTTONINFO tbi = {0};
		tbi.cbSize = sizeof(TBBUTTONINFO);
		tbi.dwMask = TBIF_STYLE;
		m_wndToolBar.GetButtonInfo(IDM_AGGR_MIRROR, &tbi);
		tbi.fsStyle |= TBSTYLE_DROPDOWN;
		m_wndToolBar.SetButtonInfo( IDM_AGGR_MIRROR, &tbi);
	}

#define ATL_CUSTOM_REBAR_STYLE \
	((ATL_SIMPLE_REBAR_STYLE & ~RBS_AUTOSIZE) | CCS_NODIVIDER)

	//
	// patria: reason to use ATL_CUSTOM_REBAR_STYLE
	//
	// ATL_SIMPLE_REBAR_STYLE (not a NO_BRODER style) has a problem
	// with theme-enabled Windows XP, 
	// rendering some transparent lines above the rebar.
	// 

	CreateSimpleReBar(ATL_CUSTOM_REBAR_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(m_wndToolBar.m_hWnd, NULL, TRUE);

	CReBarCtrl reBar = m_hWndToolBar;
	DWORD cBands = reBar.GetBandCount();
	for (DWORD i = 0; i < cBands; ++i)
	{
		REBARBANDINFO rbi = {0};
		rbi.cbSize = sizeof(REBARBANDINFO);
		rbi.fMask = RBBIM_STYLE;
		reBar.GetBandInfo(i, &rbi);
		rbi.fStyle |= RBBS_NOGRIPPER;
		reBar.SetBandInfo(i, &rbi);
	} 

// work on status bar, progress bar
	CreateSimpleStatusBar();

	RECT rectRefreshProgress;
	::GetClientRect(m_hWndStatusBar, &rectRefreshProgress);
	rectRefreshProgress.right = 300;
	m_wndRefreshProgress.Create(m_hWndStatusBar, &rectRefreshProgress, NULL, WS_CHILD | WS_VISIBLE);

	m_wndRefreshProgress.SetRange32(0, 100);
	m_wndRefreshProgress.SetPos(50);

	m_wndRefreshProgress.ShowWindow(SW_HIDE);

	m_viewTreeList.Create(
		*this, rcDefault, NULL,
		WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);


	m_viewTreeList.Initialize();

	m_hWndClient = m_viewTreeList;

	UIAddToolBar(m_wndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// TODO : It will be better if we display splash window while
	//		the treeview is initialized

	m_bRefreshing = FALSE;
	::InitializeCriticalSection(&m_csThreadRefreshStatus);
	PostMessage(WM_COMMAND, IDM_AGGR_REFRESH, 0);


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


	int selectedItemData;

	// get selectedItemData
	CRect rect;
	CPoint posInView;
	HTREEITEM hItemSelected;

	// if clicked on tree, we need to change selection
	if (m_viewTreeList.GetWindowRect( rect ) && rect.PtInRect(pos) )
	{
		CTreeViewCtrlEx ctrlTree = m_viewTreeList.GetTreeControl();
		CHeaderCtrl ctrlHeader = m_viewTreeList.GetHeaderControl();

		CRect rectHeader;
		ctrlHeader.GetClientRect(rectHeader);

		// clicked point is inside the tree control
		// Change screen coordinates to client coordinates
		posInView = pos - rect.TopLeft();
		posInView.y -= rectHeader.Height();

		if(NULL == (hItemSelected = ctrlTree.HitTest(posInView, NULL)))
		{
			LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
			return;
		}

		ctrlTree.SelectItem(hItemSelected);
	}

	selectedItemData = m_viewTreeList.GetSelectedItemData();

	// Display context menu
	CMenu menu;
	CMenuHandle subMenu;
	CDiskObjectPtr obj = m_mapObject[selectedItemData];
	ATLASSERT( obj.get() != NULL );
	menu.LoadMenu( MAKEINTRESOURCE(IDR_CONTEXT_MENU) );
	subMenu = menu.GetSubMenu(0);
	CObjectUIHandler::GetUIHandler(obj)->InsertMenu(obj, subMenu);
	ATLTRACE(_T("Menu Count : %d"), subMenu.GetMenuItemCount());
	subMenu.RemoveMenu(IDM_AGGR_PROPERTY, MF_BYCOMMAND);
//		subMenu.RemoveMenu(MF_BYPOSITION, subMenu.GetMenuItemCount());
	subMenu.TrackPopupMenu(
		TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
		pos.x, 
		pos.y, 
		m_hWnd
		);

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
	return;

}

void CMainFrame::RefreshAction()
{
	int iItemSelected;

//	UIEnable(IDM_AGGR_BIND, FALSE);
	UIEnable(IDM_AGGR_UNBIND, FALSE);
	UIEnable(IDM_AGGR_SYNCHRONIZE, FALSE);
	UIEnable(IDM_AGGR_ADDMIRROR, FALSE);

	if (-1 != (iItemSelected = m_viewTreeList.GetSelectedItemData()))
	{
		CDiskObjectPtr obj = m_mapObject[iItemSelected];
		ATLASSERT( obj.get() != NULL );
		CCommandSet cmdSet = 
			CObjectUIHandler::GetUIHandler(obj)->GetCommandSet(obj);
		CCommandSet::iterator itr;
		for ( itr = cmdSet.begin(); itr != cmdSet.end(); itr++ )
		{
			UIEnable( itr->GetID(), !itr->IsDisabled() );
		}
	}
}

LRESULT CMainFrame::OnTreeSelChanged(LPNMHDR lpNLHDR)
{
/*
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);
	if(!lpNLHDR)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return 0;
	}

	NMTREEVIEW *pNMTV = (NMTREEVIEW *)lpNLHDR;

	ATLTRACE(_T("OnTreeSelChanged %d, LV : %d, TV : %d\n"),
		pNMTV->itemNew.lParam,
		m_viewList.GetSelectedItemData(), m_viewTree.GetSelectedItemData());

	if(pNMTV->itemNew.lParam != m_viewList.GetSelectedItemData())
	{
		m_viewList.SelectDiskObject(m_mapObject[pNMTV->itemNew.lParam]);
		RefreshAction();
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
*/
	return 0;
}

LRESULT CMainFrame::OnToolBarDropDown(LPNMHDR lpNMHDR)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);
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
	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
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

	nbbwiz::CWizard dlgBindWizard;
	dlgBindWizard.SetSingleDisks(singleDisks);

	if ( dlgBindWizard.DoModal() == IDOK )
	{
		// AING : Cause dlgBind use ndasop.lib to bind disks, 
		// you can't ensure each disk information is stable after bind process.
		OnRefreshStatus(NULL, NULL, NULL);
	}

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
	return;
}

void CMainFrame::OnSingle(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	WTL::CString strMsg;

	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj, parent;
	obj = m_mapObject[iItemSelected];

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
		strMsg.LoadString(IDS_FAIL_TO_ACCESS_EXCLUSIVELY);
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

	NDASCOMM_CONNECTION_INFO ConnectionInfo;

	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	
	ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	ConnectionInfo.UnitNo = unitDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ConnectionInfo.bWriteAccess = TRUE;
	ConnectionInfo.ui64OEMCode = NULL;
	ConnectionInfo.bSupervisor = FALSE;
	ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(ConnectionInfo.AddressLPX, 
		unitDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		LPXADDR_NODE_LENGTH);

	UINT32 BindResult = NdasOpBind(
		1,
		&ConnectionInfo,
		NMT_SINGLE);

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	if(1 != BindResult)
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
	BOOL bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		NDAS_UNITDEVICE_ID unitDeviceId;
		CopyMemory(unitDeviceId.DeviceId.Node, ConnectionInfo.AddressLPX, 
			sizeof(unitDeviceId.DeviceId.Node));
		unitDeviceId.UnitNo = ConnectionInfo.UnitNo;
		HixChangeNotify.Notify(unitDeviceId);
	}

	OnRefreshStatus(NULL, NULL, NULL);


}

void CMainFrame::OnUnBind(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj, parent;
	obj = m_mapObject[iItemSelected];
	
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
		strMsg.LoadString(IDS_FAIL_TO_ACCESS_EXCLUSIVELY);
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

void CMainFrame::OnRepair(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	BOOL bResults;
	BOOL bReturn = FALSE;

	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}
	
	CDiskObjectPtr obj = m_mapObject[iItemSelected];


	if( dynamic_cast<CDiskObjectComposite *>(obj.get()) == NULL )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectCompositePtr DiskObjectComposite =
		boost::dynamic_pointer_cast<CDiskObjectComposite>(obj);

	if(!((NMT_RAID1 == DiskObjectComposite->GetNDASMediaType() ||
		NMT_RAID4 == DiskObjectComposite->GetNDASMediaType()) &&
		1 == DiskObjectComposite->GetMissingMemberCount()))
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// TODO : No disk is available
		WTL::CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NOT_READY_REPAIR );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONERROR
			);
		return;
	}		


	CSelectDiskDlg dlgSelect(IDD_REPAIR);
	CDiskObjectList singleDisks;
	CFindIfVisitor<FALSE> singleDiskFinder;
	singleDisks = singleDiskFinder.FindIf(m_pRoot, IsWritableUnitDisk);

	if ( singleDisks.size() == 0 )
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		// TODO : No disk is available
		WTL::CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NO_DISK_REPAIR );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	dlgSelect.SetSingleDisks(singleDisks);

	if ( dlgSelect.DoModal() != IDOK )
		return;

	CUnitDiskObjectPtr replaceDisk, sourceDisk;
	sourceDisk = DiskObjectComposite->GetAvailableUnitDisk();
	replaceDisk = dlgSelect.GetSelectedDisk();

	if(NULL == sourceDisk.get())
		return;
	
	NDASCOMM_CONNECTION_INFO ci, ciReplace;

	ZeroMemory(&ci, sizeof(NDASCOMM_CONNECTION_INFO));
	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ci.bWriteAccess = TRUE;
	ci.ui64OEMCode = NULL;
	ci.bSupervisor = FALSE;
	ci.protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(ci.AddressLPX, 
		sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		LPXADDR_NODE_LENGTH);

	ZeroMemory(&ciReplace, sizeof(NDASCOMM_CONNECTION_INFO));
	ciReplace.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ciReplace.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	ciReplace.UnitNo = replaceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ciReplace.bWriteAccess = TRUE;
	ciReplace.ui64OEMCode = NULL;
	ciReplace.bSupervisor = FALSE;
	ciReplace.protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(ciReplace.AddressLPX, 
		replaceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		LPXADDR_NODE_LENGTH);

	bResults = NdasOpRepair(&ci, &ciReplace);

	if(!bResults)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);

		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();
		strMsg.LoadString(IDS_REPAIR_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		CUnitDiskObjectPtr UnitDiskObjectPtr;
		NDAS_UNITDEVICE_ID unitDeviceId;

		CDiskObjectComposite::const_iterator itr;
		for ( itr = DiskObjectComposite->begin(); itr != DiskObjectComposite->end(); ++itr )
		{
			if((dynamic_cast<CEmptyDiskObject*>((*itr).get()) != NULL))
				continue;

			UnitDiskObjectPtr = 
				boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);

			CopyMemory(unitDeviceId.DeviceId.Node, 
				UnitDiskObjectPtr->GetLocation()->GetUnitDiskLocation()->MACAddr, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				UnitDiskObjectPtr->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);
		}

		CopyMemory(unitDeviceId.DeviceId.Node, 
			replaceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr, 
			sizeof(unitDeviceId.DeviceId.Node));
		unitDeviceId.UnitNo = 
			replaceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		HixChangeNotify.Notify(unitDeviceId);
	}


	CRecoverDlg dlgRecover(
		TRUE, 
        (NMT_RAID1 == DiskObjectComposite->GetNDASMediaType()) ?
		IDS_LOGDEV_TYPE_DISK_RAID1 : IDS_LOGDEV_TYPE_DISK_RAID4,
		IDS_RECOVERDLG_TASK_REPAIR);

	dlgRecover.SetMemberDevice(sourceDisk);
	dlgRecover.DoModal();

	OnRefreshStatus(NULL, NULL, NULL);

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
	return;
}

void CMainFrame::OnAddMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	ENTER_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectList singleDisks;
	CUnitDiskObjectPtr  sourceDisk;
	sourceDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(
			m_mapObject[iItemSelected]
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
		CUnitDiskObjectPtr mirDisk = dlgSelDisk.GetSelectedDisk();

		// Bind & Synchronize 
		NDASCOMM_CONNECTION_INFO ConnectionInfo[2];

		ZeroMemory(&ConnectionInfo[0], sizeof(NDASCOMM_CONNECTION_INFO));
		ConnectionInfo[0].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		ConnectionInfo[0].login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		ConnectionInfo[0].UnitNo = sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		ConnectionInfo[0].bWriteAccess = TRUE;
		ConnectionInfo[0].ui64OEMCode = NULL;
		ConnectionInfo[0].bSupervisor = FALSE;
		ConnectionInfo[0].protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(ConnectionInfo[0].AddressLPX, 
				sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			LPXADDR_NODE_LENGTH);

		ZeroMemory(&ConnectionInfo[1], sizeof(NDASCOMM_CONNECTION_INFO));
		ConnectionInfo[1].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		ConnectionInfo[1].login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		ConnectionInfo[1].UnitNo = mirDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		ConnectionInfo[1].bWriteAccess = TRUE;
		ConnectionInfo[1].ui64OEMCode = NULL;
		ConnectionInfo[1].bSupervisor = FALSE;
		ConnectionInfo[1].protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(ConnectionInfo[1].AddressLPX, 
			mirDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			LPXADDR_NODE_LENGTH);

		UINT32 BindResult = NdasOpBind(
			2,
			ConnectionInfo,
			NMT_SAFE_RAID1);

		if(2 != BindResult)
		{
	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);

			WTL :: CString strMsg;

			DWORD dwLastError = ::GetLastError();

			switch(dwLastError)
	{
			case NDASCOMM_ERROR_RW_USER_EXIST:
			case NDASOP_ERROR_ALREADY_USED:
			case NDASOP_ERROR_DEVICE_FAIL:
			case NDASOP_ERROR_NOT_SINGLE_DISK:
			case NDASOP_ERROR_DEVICE_UNSUPPORTED:
				strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, 
					(BindResult == 0) ? sourceDisk->GetTitle() : mirDisk->GetTitle());
				break;
			default:
				strMsg.LoadString(IDS_BIND_FAIL);
				break;
	}

			ShowErrorMessageBox(strMsg);

		return;
	}

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

		CRecoverDlg dlgRecover(FALSE, IDS_LOGDEV_TYPE_DISK_RAID1, IDS_RECOVERDLG_TASK_ADD_MIRROR);

		dlgRecover.SetMemberDevice(mirDisk);
		dlgRecover.DoModal();

		OnRefreshStatus(NULL, NULL, NULL);
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
	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);
		return;
	}

	CDiskObjectPtr obj;
	const CObjectUIHandler *phandler;
	obj = m_mapObject[iItemSelected];
	ATLASSERT( obj.get() != NULL );
	phandler = CObjectUIHandler::GetUIHandler( obj );
	phandler->OnCommand( obj, wID );
	
//	m_viewTreeList.UpdateDiskObject( obj );

	LEAVE_CRITICAL_SECTION(&m_csThreadRefreshStatus);

	OnTreeSelChanged(NULL);
//	OnListItemChanged(NULL);

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
