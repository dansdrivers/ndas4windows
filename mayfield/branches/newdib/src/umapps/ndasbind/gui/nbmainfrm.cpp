// nbmainfrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"

#include <ndas/ndasop.h>

#include "nbbindwiz.h"
#include "nbdevicelistdlg.h"
#include "nbaboutdlg.h"
#include "autocursor.h"

#include "apperrdlg.h"
#include "appconf.h"

static
VOID CALLBACK 
pNdasEventProc(
			   DWORD dwError, 
			   PNDAS_EVENT_INFO pEventInfo, 
			   LPVOID lpContext);

CMainFrame::~CMainFrame()
{
	ClearDevices();
}

BOOL 
CALLBACK 
EnumUnitDevicesCallBack(PNDASUSER_UNITDEVICE_ENUM_ENTRY pEntry, LPVOID lpContext)
{
	CNBNdasDevice *pDevice = (CNBNdasDevice *)lpContext;

	if(!pDevice)
		return FALSE;

	if(!pDevice->UnitDeviceAdd(pEntry))
		return FALSE;

	return TRUE;
}

BOOL 
CALLBACK 
EnumDevicesCallBack(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, LPVOID lpContext)
{
	NBNdasDevicePtrList *plistDevices = (NBNdasDevicePtrList *)lpContext;
//	CMainFrame *pMainFrame = (CMainFrame *)lpContext;

	if(!plistDevices)
//	if(!pMainFrame)
		return FALSE;

	NBNdasDevicePtrList *plistDevice = (NBNdasDevicePtrList *)lpContext;

	// check if the device already exists

	// check device connection status
	NDAS_DEVICE_STATUS status;
	NDAS_DEVICE_ERROR lastError;

	BOOL fSuccess = ::NdasQueryDeviceStatus(
		lpEnumEntry->SlotNo, 
		&status, 
		&lastError);

	CNBNdasDevice *pNBNdasDevice = new CNBNdasDevice(lpEnumEntry, status);

	if(!pNBNdasDevice)
		return FALSE;

	plistDevices->push_back(pNBNdasDevice);
//	pMainFrame->m_listDevices.push_back(pNBNdasDevice);

	// enum unit devices
	if(!NdasEnumUnitDevices(lpEnumEntry->SlotNo, EnumUnitDevicesCallBack, reinterpret_cast<LPVOID>(pNBNdasDevice)))
		return FALSE;

	return TRUE;
}

void CMainFrame::ClearDevices()
{
	NBLogicalDevicePtrList::iterator itLogicalDevice;
	for(itLogicalDevice = m_listLogicalDevices.begin(); 
		itLogicalDevice != m_listLogicalDevices.end(); 
		++itLogicalDevice)
	{
		if(*itLogicalDevice)
		{
			delete (*itLogicalDevice);
		}
	}
	m_listLogicalDevices.clear();

	NBNdasDevicePtrList::iterator itDevice;
	for(itDevice = m_listDevices.begin();
		itDevice != m_listDevices.end();
		++itDevice)
	{
		if(*itDevice)
		{
			delete (*itDevice);
		}
	}
	m_listDevices.clear();
}

BOOL CMainFrame::RefreshStatus()
{
	AutoCursor l_auto_cursor(IDC_WAIT);

	m_viewTreeList.GetTreeControl().DeleteAllItems();
	ClearDevices();

	// retrieve all the device & unit device information
	if(!NdasEnumDevices( EnumDevicesCallBack, reinterpret_cast<LPVOID>(&m_listDevices)))
		return FALSE;

	m_wndRefreshProgress.ShowWindow(SW_SHOW);
	m_wndRefreshProgress.SetRange32(0, m_listDevices.size());
	m_wndRefreshProgress.SetStep(1);
	m_wndRefreshProgress.SetPos(0);


	// initialize all the unit devices
	for(NBNdasDevicePtrList::iterator itDevice = m_listDevices.begin();
		itDevice != m_listDevices.end(); itDevice++)
	{
		if(!(*itDevice)->UnitDevicesInitialize())
		{
			// add single empty device
			ATLTRACE(_T("Device not connected : %s\n"), (*itDevice)->GetName());
//			m_wndRefreshProgress.ShowWindow(SW_HIDE);
//			return FALSE;
		}
		m_wndRefreshProgress.StepIt();
	}

	// create logical devices
	CNBUnitDevice *pUnitDevice;
	CNBLogicalDevice *pLogicalDevice;

	m_wndRefreshProgress.SetPos(0);
	for(NBNdasDevicePtrList::iterator itDevice = m_listDevices.begin();
		itDevice != m_listDevices.end(); itDevice++)
	{
		for(UINT32 i = 0; i < (*itDevice)->UnitDevicesCount(); i++)
		{
			// find logical device which has this unit device as member
			pLogicalDevice = NULL;
			pUnitDevice = (*(*itDevice))[i];
			ATLASSERT(pUnitDevice);
			if(!pUnitDevice)
				return FALSE;

			for(NBLogicalDevicePtrList::iterator itLogicalDevice = m_listLogicalDevices.begin();
				itLogicalDevice != m_listLogicalDevices.end(); itLogicalDevice++)
			{
				if((*itLogicalDevice)->IsMember(pUnitDevice))
				{
					// add to this logical device
					pLogicalDevice = *itLogicalDevice;
					ATLTRACE(_T("use CNBLogicalDevice(%p) : %s\n"), pLogicalDevice, pUnitDevice->GetName());
					break;
				}
			}

			if(NULL == pLogicalDevice)
			{
				// create new logical device
				pLogicalDevice = new CNBLogicalDevice();
				m_listLogicalDevices.push_back(pLogicalDevice);
				ATLTRACE(_T("new CNBLogicalDevice(%p) : %s\n"), pLogicalDevice, pUnitDevice->GetName());
			}

			if(!pLogicalDevice->UnitDeviceAdd(pUnitDevice))
			{
				m_wndRefreshProgress.ShowWindow(SW_HIDE);
				return FALSE;			
			}
		}

		m_wndRefreshProgress.StepIt();
	}

	m_viewTreeList.SetDevices(&m_listLogicalDevices);
	m_wndRefreshProgress.ShowWindow(SW_HIDE);
	
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
	PostQuitMessage(0);
}

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

	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH, 0);


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

		if(hItemSelected = ctrlTree.HitTest(posInView, NULL))
		{
			ctrlTree.SelectItem(hItemSelected);
		}
	}

	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	CMenu menu;
	CMenuHandle subMenu;

	menu.LoadMenu(IDR_MAINFRAME);
	subMenu = menu.GetSubMenu(1); // Tool
	
	subMenu.TrackPopupMenu(
		TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
		pos.x, 
		pos.y, 
		m_hWnd
		);
	
	return;

}

#define UIEnableOfDevice(DEVICE_PTR, ID_MENU) \
	UIEnable(ID_MENU, (DEVICE_PTR) ? (DEVICE_PTR)->GetCommandAbility(ID_MENU) : FALSE)

LRESULT CMainFrame::OnTreeSelChanged(LPNMHDR lpNLHDR)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return FALSE;

//	UIEnableOfDevice(pDevice, IDM_TOOL_BIND);
	UIEnableOfDevice(pDevice, IDM_TOOL_UNBIND);
	UIEnableOfDevice(pDevice, IDM_TOOL_ADDMIRROR);
	UIEnableOfDevice(pDevice, IDM_TOOL_MIGRATE);
	UIEnableOfDevice(pDevice, IDM_TOOL_REPLACE_DEVICE);
	UIEnableOfDevice(pDevice, IDM_TOOL_REPLACE_UNIT_DEVICE);
	UIEnableOfDevice(pDevice, IDM_TOOL_SINGLE);
	UIEnableOfDevice(pDevice, IDM_TOOL_SPAREADD);
	UIEnableOfDevice(pDevice, IDM_TOOL_SPAREREMOVE);

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

NBUnitDevicePtrList CMainFrame::GetOperatableSingleDevices()
{
	NBUnitDevicePtrList listUnitDevicesSingle;
	CNBLogicalDevice *pLogicalDevice;

	for(NBLogicalDevicePtrList::iterator itLogicalDevice = m_listLogicalDevices.begin();
		itLogicalDevice != m_listLogicalDevices.end(); itLogicalDevice++)
	{
		pLogicalDevice = *itLogicalDevice;
		if( pLogicalDevice->IsOperatable() &&
			pLogicalDevice->IsHDD() &&
			!pLogicalDevice->IsGroup())
		{
			listUnitDevicesSingle.push_back((*pLogicalDevice)[0]);
		}
	}

	return listUnitDevicesSingle;
}

void CMainFrame::OnBind(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	nbbwiz::CWizard dlgBindWizard;
	dlgBindWizard.SetSingleDisks(GetOperatableSingleDevices());

	if ( dlgBindWizard.DoModal() == IDOK )
	{
		OnRefreshStatus(NULL, NULL, NULL);
	}
}

void CMainFrame::OnSingle(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	WTL::CString strMsg;

	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pUnitDevice;
	if(dynamic_cast<CNBLogicalDevice *>(pDevice))
	{
		CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
		if(!pLogicalDevice || !pLogicalDevice->IsOperatableAll())
			return;

		pUnitDevice = (*pLogicalDevice)[0];
	}
	else
	{
		pUnitDevice = dynamic_cast<CNBUnitDevice *>(pDevice);
		if(!pUnitDevice || !pUnitDevice->IsOperatable())
			return;
	}

	if(!pUnitDevice)
		return;

	NBUnitDevicePtrList listUnitDevices;
	listUnitDevices.push_back(pUnitDevice);

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_SINGLE_DLG_CAPTION, 
		IDS_SINGLE_DLG_MESSAGE, 
		listUnitDevices, 
		0, 
		NULL, 
		NULL);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;


	NDASCOMM_CONNECTION_INFO ConnectionInfo;
	if(!pUnitDevice->InitConnectionInfo(&ConnectionInfo, TRUE))
	{
		
		// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
		strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
			pUnitDevice->GetName()
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

	AutoCursor l_auto_cursor(IDC_WAIT);
	UINT32 BindResult = NdasOpBind(
		1,
		&ConnectionInfo,
		NMT_SINGLE,
		0);
	l_auto_cursor.Release();

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
			strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, pUnitDevice->GetName());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);
	}

	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnUnBind(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice)
		return;

	BOOL bUnbindMirror = 
		(NMT_RAID1 == pLogicalDevice->GetType() ||
		NMT_MIRROR == pLogicalDevice->GetType());

	// warning message
	WTL::CString strMsg;

	NBUnitDevicePtrList listUnitDevices = pLogicalDevice->GetOperatableDevices();
	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_UNBIND_DLG_CAPTION, 
		(bUnbindMirror) ? IDS_WARNING_UNBIND_MIR : IDS_WARNING_UNBIND, 
		listUnitDevices, 
		0, 
		NULL, 
		NULL);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	NDASCOMM_CONNECTION_INFO *ci = new NDASCOMM_CONNECTION_INFO[listUnitDevices.size()];

	UINT32 i = 0;

	for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
		itUnitDevice != listUnitDevices.end(); i++, itUnitDevice++)
	{
		(*itUnitDevice)->InitConnectionInfo(&ci[i], TRUE);
	}


	AutoCursor l_auto_cursor(IDC_WAIT);
	UINT32 BindResult = NdasOpBind(listUnitDevices.size(), ci, NMT_SINGLE, 0);
	l_auto_cursor.Release();

	DWORD dwLastError = ::GetLastError();

	if(i == BindResult)
	{
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);

		strMsg.LoadString(
			(bUnbindMirror) ? IDS_WARNING_UNBIND_AFTER_MIR : 
		IDS_WARNING_UNBIND_AFTER);

		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONINFORMATION
			);
	}
	else
	{

		::SetLastError(dwLastError);

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error
			if(BindResult < listUnitDevices.size())
			{
				i = 0;
				for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
					itUnitDevice != listUnitDevices.end(); i++, itUnitDevice++)
				{
					if(i == BindResult)
						strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, (*itUnitDevice)->GetName());
				}
				
			}
			else
				strMsg.LoadString(IDS_BIND_FAIL);
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}
		ShowErrorMessageBox(IDS_MAINFRAME_SINGLE_ACCESS_FAIL);
	}

	for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
		itUnitDevice != listUnitDevices.end(); i++, itUnitDevice++)
	{
		(*itUnitDevice)->HixChangeNotify(pGetNdasHostGuid());
	}

	delete [] ci;

	OnRefreshStatus(NULL, NULL, NULL);

	return;
}

void CMainFrame::OnRepair(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
#ifdef __NEVER_DEFINED__
	BOOL bResults;
	BOOL bReturn = FALSE;

	int iItemSelected = m_viewTreeList.GetSelectedItemData();
	if (-1 == iItemSelected)
	{
		
		return;
	}
	
	CDiskObjectPtr obj = m_mapObject[iItemSelected];


	if( dynamic_cast<CDiskObjectComposite *>(obj.get()) == NULL )
	{
		
		return;
	}

	CDiskObjectCompositePtr DiskObjectComposite =
		boost::dynamic_pointer_cast<CDiskObjectComposite>(obj);

	if(!((NMT_RAID1 == DiskObjectComposite->GetNDASMediaType() ||
		NMT_RAID4 == DiskObjectComposite->GetNDASMediaType()) &&
		1 == DiskObjectComposite->GetMissingMemberCount()))
	{
		
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

	AutoCursor l_auto_cursor(IDC_WAIT);
	bResults = NdasOpRepair(&ci, &ciReplace);
	l_auto_cursor.Release();

	if(!bResults)
	{
		

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
#endif
	
	return;
}

BOOL CALLBACK CheckCapacityForSpare(CNBUnitDevice *pUnitDevice, HWND hWnd, LPVOID lpContext)
{
	if(!lpContext)
		return TRUE;

	if(!pUnitDevice)
		return FALSE;

	CNBLogicalDevice *pLogicalDevice = (CNBLogicalDevice *)lpContext;

	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if ((*pLogicalDevice)[i] && 
			(*pLogicalDevice)[i]->GetCapacityInByte() > pUnitDevice->GetCapacityInByte())
		{
			WTL::CString strMsg;
			strMsg.LoadString( IDS_SPARE_ADD_DLG_SIZE_FAIL );
			WTL::CString strTitle;
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

BOOL CALLBACK CheckCapacityForMirror(CNBUnitDevice *pUnitDevice, HWND hWnd, LPVOID lpContext)
{
	if(!lpContext)
		return TRUE;

	if(!pUnitDevice)
		return FALSE;

	CNBLogicalDevice *pLogicalDevice = (CNBLogicalDevice *)lpContext;

	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if ((*pLogicalDevice)[i] && 
			(*pLogicalDevice)[i]->GetCapacityInByte() > pUnitDevice->GetCapacityInByte())
		{
			WTL::CString strMsg;
			strMsg.LoadString( IDS_SELECTMIRDLG_SMALLER_DISK );
			WTL::CString strTitle;
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

void CMainFrame::OnSpareAdd(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice || !pLogicalDevice->IsOperatableAll())
		return;

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_SPARE_ADD_DLG_CAPTION, 
		IDS_SPARE_ADD_DLG_MESSAGE, 
		GetOperatableSingleDevices(), 
		1, 
		CheckCapacityForSpare, 
		pLogicalDevice);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	CNBUnitDevice *pUnitDevice = dlgSelectDevice.GetSelectedDevice();

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ci, ci_spare;
	(*pLogicalDevice)[0]->InitConnectionInfo(&ci, TRUE);
	pUnitDevice->InitConnectionInfo(&ci_spare, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);
	BOOL bResults = NdasOpSpareAdd(
		&ci, &ci_spare);
	l_auto_cursor.Release();

	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnSpareRemove(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pUnitDevice = dynamic_cast<CNBUnitDevice *>(pDevice);
	if(!pUnitDevice || !pUnitDevice->GetLogicalDevice()->IsOperatableAll())
		return;

	NBUnitDevicePtrList listUnitDevices;
	listUnitDevices.push_back(pUnitDevice);

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_SPARE_REMOVE_DLG_CAPTION, 
		IDS_SPARE_REMOVE_DLG_MESSAGE, 
		listUnitDevices, 
		0, 
		NULL, 
		NULL);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	NDASCOMM_CONNECTION_INFO ci;
	pUnitDevice->InitConnectionInfo(&ci, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);
	BOOL bResults = NdasOpSpareRemove(&ci);
	l_auto_cursor.Release();

	pUnitDevice->GetLogicalDevice()->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnReplaceDevice(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice)
		return;

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_REPLACE_DEVICE_DLG_CAPTION, 
		IDS_REPLACE_DEVICE_DLG_MESSAGE, 
		GetOperatableSingleDevices(), 
		1, 
		CheckCapacityForSpare, 
		pLogicalDevice);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	CNBUnitDevice *pUnitDevice = dlgSelectDevice.GetSelectedDevice();

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ci, ci_spare;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE);
	pUnitDevice->InitConnectionInfo(&ci_spare, TRUE);

	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if (!(*pLogicalDevice)[i])
		{
			break;
		}
	}

	if(pLogicalDevice->DevicesTotal() == i)
	{
		// failed to find missing device
		return;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpReplaceDevice(
		&ci, &ci_spare, i);
	l_auto_cursor.Release();

	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnReplaceUnitDevice(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice)
		return;

	// find the missing device

	CNBLogicalDevice *pLogicalDeviceReplace = NULL;
	CNBUnitDevice *pUnitDeviceReplace = NULL;
	NBUnitDevicePtrList listUnitDevicesReplace;
	NBLogicalDevicePtrList::iterator itLogicalDevice;
	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if(!(*pLogicalDevice)[i])
		{
			for(itLogicalDevice = m_listLogicalDevices.begin(); 
				itLogicalDevice != m_listLogicalDevices.end(); 
				++itLogicalDevice)
			{
				pLogicalDeviceReplace = *itLogicalDevice;
				if(pLogicalDeviceReplace)
				{
					if (pLogicalDeviceReplace->IsHDD() &&
						!pLogicalDeviceReplace->IsGroup() &&
						pLogicalDeviceReplace->IsOperatable())
					{
						pUnitDeviceReplace = (*pLogicalDeviceReplace)[0];
						if (pUnitDeviceReplace->IsThis(
							pLogicalDevice->DIB()->UnitDisks[i].MACAddr,
							pLogicalDevice->DIB()->UnitDisks[i].UnitNumber))
						{
							// ok we found replacable HDD
							listUnitDevicesReplace.push_back(pUnitDeviceReplace);

						}
					}
				}
			}
		}
	}

	if(!listUnitDevicesReplace.size())
		return;

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_REPLACE_UNIT_DEVICE_DLG_CAPTION, 
		IDS_REPLACE_UNIT_DEVICE_DLG_MESSAGE, 
		listUnitDevicesReplace, 
		0, 
		CheckCapacityForSpare, 
		pLogicalDevice);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	// dlgSelectDevice is not selective dialog. get unit device from listUnitDevicesReplace
	CNBUnitDevice *pUnitDevice = *(listUnitDevicesReplace.begin());

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ci, ci_replace;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE); // use write access. this function does not support run time replace yet.
	pUnitDevice->InitConnectionInfo(&ci_replace, TRUE);

	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if (!(*pLogicalDevice)[i])
		{
			break;
		}
	}

	if(pLogicalDevice->DevicesTotal() == i)
	{
		// failed to find missing device
		return;
	}

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpReplaceUnitDevice(
		&ci, &ci_replace, i);
	l_auto_cursor.Release();

	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnMigrate(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice)
		return;

	NBUnitDevicePtrList listUnitDevicesReplace;

	for(UINT32 i = 0; i < pLogicalDevice->DevicesTotal(); i++)
	{
		if ((*pLogicalDevice)[i])
			listUnitDevicesReplace.push_back((*pLogicalDevice)[i]);
	}

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_MIGRATE_DLG_CAPTION, 
		IDS_MIGRATE_DLG_MESSAGE, 
		listUnitDevicesReplace, 
		0, 
		NULL, 
		NULL);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	NDASCOMM_CONNECTION_INFO ci;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);
	BOOL bResults = NdasOpMigrate(&ci);
	l_auto_cursor.Release();

	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		WTL :: CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

void CMainFrame::OnAddMirror(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice || pLogicalDevice->IsGroup())
		return;

	NBUnitDevicePtrList listUnitDevices = GetOperatableSingleDevices();
	// remove self
	listUnitDevices.remove((*pLogicalDevice)[0]);

	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_MIRROR_ADD_DLG_CAPTION, 
		IDS_MIRROR_ADD_DLG_MESSAGE, 
		listUnitDevices, 
		1, 
		CheckCapacityForMirror, 
		pLogicalDevice);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	CNBUnitDevice *pUnitDeviceAdd = dlgSelectDevice.GetSelectedDevice();
	CNBUnitDevice *pUnitDevice = pUnitDevice = (*pLogicalDevice)[0];

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ConnectionInfo[2];
	pUnitDevice->InitConnectionInfo(&ConnectionInfo[0], TRUE);
	pUnitDeviceAdd->InitConnectionInfo(&ConnectionInfo[1], TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);
	UINT32 BindResult = NdasOpBind(
		2,
		ConnectionInfo,
		NMT_SAFE_RAID1,
		0);
	l_auto_cursor.Release();

	if(2 != BindResult)
	{


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
				(BindResult == 0) ? pUnitDevice->GetName() : pUnitDeviceAdd->GetName());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);

		return;
	}

	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDeviceAdd->HixChangeNotify(pGetNdasHostGuid());

/*
	CRecoverDlg dlgRecover(FALSE, IDS_LOGDEV_TYPE_DISK_RAID1, IDS_RECOVERDLG_TASK_ADD_MIRROR);

	ATLASSERT(FALSE);
//		dlgRecover.SetMemberDevice(pUnitDeviceAdd);
	dlgRecover.DoModal();
*/

	OnRefreshStatus(NULL, NULL, NULL);
}


void CMainFrame::OnRefreshStatus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	RefreshStatus();
}

void CMainFrame::OnCommand(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	return;
}

VOID CMainFrame::OnNdasDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH);
}
VOID CMainFrame::OnNdasLogDevEntryChanged()
{
	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH);
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
