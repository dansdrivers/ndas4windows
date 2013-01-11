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

namespace
{

void CALLBACK 
NdasEventCallback(DWORD dwError, PNDAS_EVENT_INFO pEventInfo, LPVOID lpContext)
{
	if (NULL == pEventInfo) 
	{
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
	}
}

BOOL CALLBACK 
EnumUnitDevicesCallBack(
	PNDASUSER_UNITDEVICE_ENUM_ENTRY pEntry, 
	LPVOID lpContext)
{
	CNBNdasDevice *pDevice = (CNBNdasDevice *)lpContext;

	if(!pDevice)
	{
		return FALSE;
	}

	if(!pDevice->UnitDeviceAdd(pEntry))
	{
		return FALSE;
	}

	return TRUE;
}

} // local namespace

CMainFrame::~CMainFrame()
{
	ClearDevices();
}

BOOL 
CALLBACK 
CMainFrame::EnumDevicesCallBack(
	PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, 
	LPVOID lpContext)
{
	NBNdasDevicePtrList *plistDevices = (NBNdasDevicePtrList *)lpContext;
//	CMainFrame *pMainFrame = (CMainFrame *)lpContext;

	//	if(!pMainFrame)
	if(!plistDevices)
	{
		return FALSE;
	}

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
	{
		return FALSE;
	}

	plistDevices->push_back(pNBNdasDevice);
//	pMainFrame->m_listDevices.push_back(pNBNdasDevice);

	// enum unit devices
	if(!NdasEnumUnitDevices(
		lpEnumEntry->SlotNo, 
		EnumUnitDevicesCallBack, 
		reinterpret_cast<LPVOID>(pNBNdasDevice)))
	{
		return FALSE;
	}
	if (pNBNdasDevice->UnitDevicesCount() == 0) {
		// No unit is attached. Add empty unit
		NDASUSER_UNITDEVICE_ENUM_ENTRY BaseInfo;
		BaseInfo.UnitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN;
		BaseInfo.UnitNo = 0;
		pNBNdasDevice->UnitDeviceAdd(&BaseInfo, 0, 0);		
	}

	return TRUE;
}

void 
CMainFrame::ClearDevices()
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

	for(itDevice = m_listMissingDevices.begin();
		itDevice != m_listMissingDevices.end();
		++itDevice)
	{
		if(*itDevice)
		{
			delete (*itDevice);
		}
	}
	m_listMissingDevices.clear();
}

BOOL CMainFrame::RefreshStatus()
{
	BOOL bResult;
	CNBUnitDevice *pUnitDevice;
	CNBNdasDevice *pNdasDevice;	
	CNBLogicalDevice *pLogicalDevice;
	NBUnitDevicePtrList AllUnits;
	NBUnitDevicePtrList UnusedUnits;
	
	AutoCursor l_auto_cursor(IDC_WAIT);

	m_viewTreeList.GetTreeControl().DeleteAllItems();
	ClearDevices();

	// Initially, no commands are enabled
	//	UIEnableForDevice(pDevice, IDM_TOOL_BIND);
	UIEnableForDevice(NULL, IDM_TOOL_UNBIND);
	UIEnableForDevice(NULL, IDM_TOOL_ADDMIRROR);
	UIEnableForDevice(NULL, IDM_TOOL_MIGRATE);
	UIEnableForDevice(NULL, IDM_TOOL_REPLACE_DEVICE);
	UIEnableForDevice(NULL, IDM_TOOL_USE_AS_MEMBER);
//	UIEnableForDevice(NULL, IDM_TOOL_SINGLE);
	UIEnableForDevice(NULL, IDM_TOOL_SPAREADD);
//	UIEnableForDevice(NULL, IDM_TOOL_SPAREREMOVE);
	UIEnableForDevice(NULL, IDM_TOOL_RESET_TYPE);
	UIEnableForDevice(NULL, IDM_TOOL_CLEAR_DEFECTIVE);
	UIEnableForDevice(NULL, IDM_TOOL_REMOVE_FROM_RAID);
	UIEnableForDevice(NULL, IDM_TOOL_USE_AS_BASIC);


	UpdateWindow();

	// Summary:
	// Create LogicalDevice list from Device list
	// 


	// retrieve all the device & unit device information
	if(!NdasEnumDevices( EnumDevicesCallBack, reinterpret_cast<LPVOID>(&m_listDevices)))
	{
		return FALSE;
	}

	m_wndRefreshProgress.ShowWindow(SW_SHOW);
	m_wndRefreshProgress.SetRange32(0, m_listDevices.size() * 2 ); // 2 iteration.
	m_wndRefreshProgress.SetStep(1);
	m_wndRefreshProgress.SetPos(0);

	//
	// 1. Find all the unit devices and initialize.
	//
	for(NBNdasDevicePtrList::iterator itDevice = m_listDevices.begin();
		itDevice != m_listDevices.end(); itDevice++)
	{
		for(UINT32 i = 0; i < (*itDevice)->UnitDevicesCount(); i++)
		{
			if ((*(*itDevice))[i]) {
				(*(*itDevice))[i]->Initialize();
				AllUnits.push_back((*(*itDevice))[i]);
			} else {
				// Reported as 2 units device but there may be only one unit.
				// ATLASSERT((*(*itDevice))[i]);
			}
		}

		m_wndRefreshProgress.StepIt();
	}

	//
	// 2. create logical devices that are consisted of multiple disks
	//
	for(NBUnitDevicePtrList::iterator itUnit = AllUnits.begin();
		itUnit != AllUnits.end(); itUnit++) 
	{
		pLogicalDevice = NULL;
		pUnitDevice = *itUnit;
		if (!pUnitDevice)
			continue;

		// Iterate already created logical devices to find logical device which has this unit device as member Iterate 
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
		// We will create logical device for disabled or not connected units later.
		if (!(pUnitDevice->GetStatus() & 
			(NDASBIND_UNIT_DEVICE_STATUS_DISABLED|NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED)) ||
			pUnitDevice->IsGroup()) 
		{
			if(NULL == pLogicalDevice)
			{
				// create new logical device
				pLogicalDevice = new CNBLogicalDevice();
				m_listLogicalDevices.push_back(pLogicalDevice);
				ATLTRACE(_T("new CNBLogicalDevice(%p) : %s\n"), pLogicalDevice, pUnitDevice->GetName());
			}

			if(!pLogicalDevice->UnitDeviceSet(pUnitDevice, pUnitDevice->GetSequence()))
			{
				m_wndRefreshProgress.ShowWindow(SW_HIDE);
				return FALSE;
			}
		} else {
			UnusedUnits.push_back(pUnitDevice);
		}
	}
	
	//
	// 3. Set additional info to unit device and fill empty units of logical devices.
	//
	for(NBLogicalDevicePtrList::iterator itLogicalDevice = m_listLogicalDevices.begin();
		itLogicalDevice != m_listLogicalDevices.end(); itLogicalDevice++)
	{
		CNBLogicalDevice* ld = *itLogicalDevice;
		NDASOP_RAID_INFO* RaidInfo;
		if (!ld->IsGroup()) {
			continue;
		}
		
		bResult = ld->InitRaidInfo();
		if (!bResult) {
			ATLTRACE(_T("Failed to get RAID info for %s\n"), ld->GetName());
			continue;
		}
		RaidInfo = ld->RAID_INFO();
		
		for(UINT32 i =0; i<ld->DevicesTotal();i++) {
			if ((*ld)[i] == NULL) { // Found missing member.
				CNBNdasDevice *pMissingNdasDevice = NULL;
				NDASUSER_UNITDEVICE_ENUM_ENTRY BaseInfo;

				pNdasDevice = NULL;
				pUnitDevice = NULL;
				ATLTRACE(_T("Filling up empty unit\n"));
				//
				// Find device from m_RaidInfo's unit info
				//
				for(NBNdasDevicePtrList::iterator itDevice = m_listDevices.begin();
					itDevice != m_listDevices.end(); itDevice++)
				{
					pNdasDevice = (*itDevice);
					if (pNdasDevice->IsThisDevice(ld->RAID_INFO()->Members[i].DeviceId.Node, ld->RAID_INFO()->Members[i].DeviceId.VID)) 
					{
						//
						// Device is matched but this device's unit is not a member of this logical device.
						// Create virtual NDAS device for presentation.
						//
						pMissingNdasDevice = new CNBNdasDevice((*itDevice), FALSE);
						m_listMissingDevices.push_back(pMissingNdasDevice);
						BaseInfo.UnitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
						BaseInfo.UnitNo = ld->RAID_INFO()->Members[i].UnitNo;
						pUnitDevice = pMissingNdasDevice->UnitDeviceAdd(&BaseInfo, ld->RAID_INFO()->Members[i].Flags, ld->RAID_INFO()->Members[i].DefectCode);
						if (pUnitDevice)
							pUnitDevice->Initialize(TRUE);
						break;
					}
				}
				if (pMissingNdasDevice == NULL) {
					CString str;
					// This device is not registered device. Create one from RAID info
					str.LoadString(IDS_DEV_NAME_NOT_REGISTERED);
					pMissingNdasDevice = new CNBNdasDevice((PCTSTR)str, 
						&ld->RAID_INFO()->Members[i].DeviceId, NDAS_DEVICE_STATUS_NOT_REGISTERED);
					m_listMissingDevices.push_back(pMissingNdasDevice);
					BaseInfo.UnitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
					BaseInfo.UnitNo = ld->RAID_INFO()->Members[i].UnitNo;
					pUnitDevice = pMissingNdasDevice->UnitDeviceAdd(&BaseInfo, 
						ld->RAID_INFO()->Members[i].Flags | NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED);
						//Temp fix: NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED cannot be set by ndasop so set here.
					if (pUnitDevice)
						pUnitDevice->Initialize(TRUE);
				}
				ld->UnitDeviceSet(pUnitDevice, i);
			}  else {
				(*ld)[i]->SetRaidStatus(ld->RAID_INFO()->Members[i].Flags, ld->RAID_INFO()->Members[i].DefectCode);
			}
			// Remove used unit from allunit list
			for(NBUnitDevicePtrList::iterator itUnit = UnusedUnits.begin();
				itUnit != UnusedUnits.end(); itUnit++) 
			{
				pUnitDevice = *itUnit;
				if (pUnitDevice->IsThisUnit(ld->RAID_INFO()->Members[i].DeviceId.Node, 
					ld->RAID_INFO()->Members[i].DeviceId.VID,
					ld->RAID_INFO()->Members[i].UnitNo)) {
					UnusedUnits.erase(itUnit);
					break;
				}
			}
			m_wndRefreshProgress.StepIt();
		}
	}

	// Create logical device for disabled/not-connected devices.
	for(NBUnitDevicePtrList::iterator itUnit = UnusedUnits.begin();
		itUnit != UnusedUnits.end(); itUnit++) {
		pUnitDevice = *itUnit;
		pLogicalDevice = new CNBLogicalDevice();
		m_listLogicalDevices.push_back(pLogicalDevice);
		ATLTRACE(_T("new CNBLogicalDevice(%p) : %s\n"), pLogicalDevice, pUnitDevice->GetName());
		pLogicalDevice->UnitDeviceSet(pUnitDevice, 0);
	}

	m_viewTreeList.SetDevices(&m_listLogicalDevices);
	m_wndRefreshProgress.ShowWindow(SW_HIDE);
	
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
	CString strTitle;
	strTitle.LoadString( IDS_APPLICATION );
	SetWindowText(strTitle);

	//
	// setting up a tool bar
	//
	// patria:
	//
	// We are loading an empty tool bar.
	// If we directly load a tool bar using a bitmap which does not
	// match with windows system palette, the application may crash
	// in Windows 2000.
	// As an workaround, we just create a simple tool bar with
	// an empty bitmap and replace them later.
	//
	HWND hWndToolBar = CreateSimpleToolBarCtrl(
		m_hWnd, 
		IDR_EMPTY_TOOLBAR, 
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
		CString strCommand;

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

	CRect rectPgs;
	::GetClientRect(m_hWndStatusBar, &rectPgs);
	rectPgs.DeflateRect(1,2,1,2);
	rectPgs.right = 300;
	m_wndRefreshProgress.Create(m_hWndStatusBar, &rectPgs, NULL, WS_CHILD | WS_VISIBLE);

	m_wndRefreshProgress.SetRange32(0, 100);
	m_wndRefreshProgress.SetPos(50);

	m_wndRefreshProgress.ShowWindow(SW_HIDE);

	//
	// Comments from the author
	// (http://www.viksoe.dk/code/treelistview.htm)
	//
	// It is wise to add the TVS_DISABLEDRAGDROP and TVS_SHOWSELALWAYS 
	// styles to the tree control for best result.
	//
	m_viewTreeList.Create(
		*this, rcDefault, NULL,
		WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
		TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP /* | TVS_HASLINES */ );


	m_viewTreeList.Initialize();
	m_viewTreeList.GetTreeControl().SetIndent(24);

	m_hWndClient = m_viewTreeList;

	UIAddToolBar(m_wndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// TODO : It will be better if we display splash window while
	//		the treeview is initialized

	PostMessage(WM_COMMAND, IDM_TOOL_REFRESH, 0);


	m_hEventCallback = 
		::NdasRegisterEventCallback(NdasEventCallback,m_hWnd);


	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	// FIXME : We need to remember the window size
	//CRect rectResize;
	//GetClientRect( rectResize );
	//rectResize = CRect( rectResize.TopLeft(), CSize(500, 500) );
	//ClientToScreen( rectResize );
	//MoveWindow( rectResize );
	//CenterWindow();

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
	CNBLogicalDevice * LogDev;
	menu.LoadMenu(IDR_MAINPOPUP);

	LogDev = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if (LogDev) {
		if (!LogDev->IsGroup()) {
			subMenu = menu.GetSubMenu(0);
		} else {
			subMenu = menu.GetSubMenu(1);
		}
	} else if (dynamic_cast<CNBUnitDevice*>(pDevice)) {
		subMenu = menu.GetSubMenu(2);
	}
	
	subMenu.TrackPopupMenu(
		TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
		pos.x, 
		pos.y, 
		m_hWnd);
	
	return;

}

void
CMainFrame::UIEnableForDevice(CNBDevice* pDevice, UINT nMenuID)
{
	UIEnable(nMenuID, pDevice ? pDevice->GetCommandAbility(nMenuID) : FALSE);
}

LRESULT CMainFrame::OnTreeSelChanged(LPNMHDR lpNLHDR)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	// If the selection is empty, all commands should be disabled
	// And it is okay to pass null as pDevice calling UIEnableForDevice
	//if(!pDevice)
	//	return FALSE;

//	UIEnableForDevice(pDevice, IDM_TOOL_BIND);
	UIEnableForDevice(pDevice, IDM_TOOL_UNBIND);
	UIEnableForDevice(pDevice, IDM_TOOL_ADDMIRROR);
	UIEnableForDevice(pDevice, IDM_TOOL_MIGRATE);
	UIEnableForDevice(pDevice, IDM_TOOL_REPLACE_DEVICE);
	UIEnableForDevice(pDevice, IDM_TOOL_USE_AS_MEMBER);
//	UIEnableForDevice(pDevice, IDM_TOOL_SINGLE);
	UIEnableForDevice(pDevice, IDM_TOOL_SPAREADD);
//	UIEnableForDevice(pDevice, IDM_TOOL_SPAREREMOVE);
	UIEnableForDevice(pDevice, IDM_TOOL_RESET_TYPE);
	UIEnableForDevice(pDevice, IDM_TOOL_CLEAR_DEFECTIVE);
	UIEnableForDevice(pDevice, IDM_TOOL_REMOVE_FROM_RAID);
	UIEnableForDevice(pDevice, IDM_TOOL_USE_AS_BASIC);


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

	if (NMT_MIRROR != pLogicalDevice->GetType() &&
		NMT_RAID1 != pLogicalDevice->GetType() &&
		NMT_RAID4 != pLogicalDevice->GetType() &&
		NMT_RAID1R2 != pLogicalDevice->GetType() &&
		NMT_RAID4R2 != pLogicalDevice->GetType())
		return;

	// warning message
	CString strMsg;

	NBUnitDevicePtrList listUnitDevices = pLogicalDevice->GetOperatableDevices();
	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_MIGRATE_DLG_CAPTION, 
		IDS_MIGRATE_DLG_MESSAGE, 
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
	BOOL bResult = NdasOpMigrate(&ci[0]);
	l_auto_cursor.Release();

	DWORD dwLastError = ::GetLastError();

	if(!bResult)
	{
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

// To do: convert to "use as basic disk" command handling.
void CMainFrame::OnSingle(UINT wNotifyCode, int wID, HWND hwndCtl) 
{
	CString strMsg;

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
		CString strTitle;
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
	DWORD UnitDevCount;
	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice)
		return;

	BOOL bUnbindMirror = 
		(NMT_RAID1 == pLogicalDevice->GetType() ||
		NMT_RAID1R2 == pLogicalDevice->GetType() ||
		NMT_RAID1R3 == pLogicalDevice->GetType() ||		
		NMT_MIRROR == pLogicalDevice->GetType());

	// warning message
	CString strMsg;

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
	UnitDevCount = 0;
	for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
		itUnitDevice != listUnitDevices.end(); itUnitDevice++)
	{
		if ((*itUnitDevice)->IsMissingMember()) { 
			// Do not touch missing member because it's not member!
		} else {
			(*itUnitDevice)->InitConnectionInfo(&ci[i], TRUE);
			i++;
		}
	}
	UnitDevCount = i;

	if (pLogicalDevice->DevicesTotal() != UnitDevCount) {
		CString strTitle;
		CString strMsg;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_MISSING_MEMBER);
		int id = MessageBox(
			strMsg,
			strTitle,
			MB_YESNO|MB_ICONEXCLAMATION
			);
		if(IDYES != id) {
			return;
		}
	}

	AutoCursor l_auto_cursor(IDC_WAIT);
	UINT32 BindResult = NdasOpBind(UnitDevCount, ci, NMT_SINGLE, 0);
	l_auto_cursor.Release();

	DWORD dwLastError = ::GetLastError();

	if(UnitDevCount == BindResult)
	{
		CString strTitle;
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
			if(BindResult < UnitDevCount)
			{
				i = 0;
				for(NBUnitDevicePtrList::iterator itUnitDevice = listUnitDevices.begin();
					itUnitDevice != listUnitDevices.end(); itUnitDevice++)
				{
					if(i == BindResult)
						strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, (*itUnitDevice)->GetName());
					if (!(*itUnitDevice)->IsMissingMember()) { 
						i++;
					}
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
		CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NOT_READY_REPAIR );
		CString strTitle;
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
		CString strMsg;
		strMsg.LoadString( IDS_MAINFRAME_NO_DISK_REPAIR );
		CString strTitle;
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
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ci.WriteAccess = TRUE;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(
		ci.DeviceId.Node, 
		sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		sizeof(ci.DeviceId.Node));

	ZeroMemory(&ciReplace, sizeof(NDASCOMM_CONNECTION_INFO));
	ciReplace.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ciReplace.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ciReplace.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ciReplace.UnitNo = replaceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
	ciReplace.WriteAccess = TRUE;
	ciReplace.Protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(
		ciReplace.DeviceId.Node,
		replaceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
		sizeof(ciReplace.DeviceId.Node));

	AutoCursor l_auto_cursor(IDC_WAIT);
	bResults = NdasOpRepair(&ci, &ciReplace);
	l_auto_cursor.Release();

	if(!bResults)
	{
		CString strMsg;

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
			(*pLogicalDevice)[i]->GetLogicalCapacityInByte() > pUnitDevice->GetPhysicalCapacityInByte())
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
			(*pLogicalDevice)[i]->GetLogicalCapacityInByte() > pUnitDevice->GetPhysicalCapacityInByte())
		{
			CString strMsg;
			strMsg.LoadString( IDS_SELECTMIRDLG_SMALLER_DISK );
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
BOOL CALLBACK CheckReplaceDevice(CNBUnitDevice *pUnitDevice, HWND hWnd, LPVOID lpContext)
{
	if(!lpContext)
		return TRUE;

	if(!pUnitDevice)
		return FALSE;

	CNBUnitDevice *pSrcUnitDevice = (CNBUnitDevice *)lpContext;
	CNBLogicalDevice *pLogicalDevice = (CNBLogicalDevice*) pSrcUnitDevice->GetLogicalDevice();

	if (pLogicalDevice->GetLogicalCapacityInByte() > pUnitDevice->GetPhysicalCapacityInByte()) {
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


void CMainFrame::OnSpareAdd(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if(!pLogicalDevice || !pLogicalDevice->IsOperatableAll() || !pLogicalDevice->IsHealthy())
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
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}
#if 0
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
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}
#endif

void CMainFrame::OnReplaceDevice(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	DWORD i;
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();
	BOOLEAN ConvertToBasic;
	DWORD ReplaceIndex;

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pOldUnitDevice = dynamic_cast<CNBUnitDevice*>(pDevice);
	if(!pOldUnitDevice)
		return;
	CNBLogicalDevice *pLogicalDevice = pOldUnitDevice->GetLogicalDevice();
	if(!pLogicalDevice)
		return;
	
	//
	// Check selected device can be replaced.
	//
	// Check RAID is not broken with remaining members.
	//
	if (pLogicalDevice->GetType() == NMT_RAID1R3) {
		BOOLEAN ValidMemberExist = FALSE;
		// Check at least one active member except this one is not out-of-sync.
		for(i=0;i<pLogicalDevice->DevicesInRaid();i++) {
			DWORD UnitRaidStatus;
			if (pOldUnitDevice == (*pLogicalDevice)[i]) {
				continue;
			}
			UnitRaidStatus = (*pLogicalDevice)[i]->GetRaidStatus();

			if (UnitRaidStatus & NDAS_RAID_MEMBER_FLAG_ONLINE) {
				if (!(UnitRaidStatus & (
					NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC |
					NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER))) {
					ValidMemberExist = TRUE;
				}
			}
		}
		if (!ValidMemberExist) {
			CString strMsg;
			strMsg.LoadString(IDS_RAID_MEMBER_UNREPLACEABLE);
			CString strTitle;
			strTitle.LoadString(IDS_APPLICATION);
			MessageBox(
				strMsg,
				strTitle, 
				MB_OK | MB_ICONWARNING
				);
			return;
		}
	}else {
		// Unsupported RAID type.
		return;
	}
	
	//
	// Show warning if any of other member is not online.
	//
	for(i=0;i<pLogicalDevice->DevicesInRaid();i++) {
		DWORD UnitRaidStatus;
		if (pOldUnitDevice == (*pLogicalDevice)[i])
			continue;
		if (((*pLogicalDevice)[i])->IsMissingMember()) {
			CString strTitle;
			CString strMsg;
			strTitle.LoadString(IDS_APPLICATION);
			strMsg.LoadString(IDS_WARNING_MISSING_MEMBER);
			int id = MessageBox(
				strMsg,
				strTitle,
				MB_YESNO|MB_ICONEXCLAMATION
				);
			if(IDYES != id) {
				return;
			}
			break;
		}
	}

	//
	// Choose the replacement disk.
	//
	CNBSelectDeviceDlg dlgSelectDevice(
		IDD_DEVICE_LIST, 
		IDS_REPLACE_DEVICE_DLG_CAPTION, 
		IDS_REPLACE_DEVICE_DLG_MESSAGE, 
		GetOperatableSingleDevices(), 
		1, 
		CheckReplaceDevice, 
		pOldUnitDevice);

	if(dlgSelectDevice.DoModal() != IDOK)
		return;

	if ((pOldUnitDevice->GetRaidStatus() &  NDAS_RAID_MEMBER_FLAG_ONLINE) &&
		!(pOldUnitDevice->GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER)) {
		// Convert to basic disk if this is currently member of this RAID.
		// Otherwise don't touch this disk.
		ConvertToBasic = TRUE;
	} else {
		ConvertToBasic = FALSE;
	}
	CNBUnitDevice *pNewUnitDevice = dlgSelectDevice.GetSelectedDevice();

	// Update DIB & RMD including new Unit
	
	NDASCOMM_CONNECTION_INFO ci, ci_new;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE);
	pNewUnitDevice->InitConnectionInfo(&ci_new, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpReplaceDevice(
		&ci, &ci_new, pNewUnitDevice->GetSequence());
	
	if (!bResults)	{
		l_auto_cursor.Release();
		goto out;
	}
	if (ConvertToBasic) {
		//
		// Convert to basic disk.
		//
		bResults = pOldUnitDevice->InitConnectionInfo(&ci, TRUE);
		if (!bResults) {
			l_auto_cursor.Release();
			goto out;
		}
		NdasOpBind(	1, &ci, NMT_SINGLE,0);
	}
	l_auto_cursor.Release();

out:
	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pOldUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}


void CMainFrame::OnClearDefect(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();
	DWORD i;
	
	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pUnitDevice = dynamic_cast<CNBUnitDevice*>(pDevice);
	if(!pUnitDevice)
		return;
	CNBLogicalDevice *pLogicalDevice = pUnitDevice->GetLogicalDevice();
	if(!pLogicalDevice)
		return;

	//
	// Show warning if any of other member is not online.
	//
	for(i=0;i<pLogicalDevice->DevicesTotal();i++) {
		DWORD UnitRaidStatus;
		if (pUnitDevice == (*pLogicalDevice)[i])
			continue;
		if (((*pLogicalDevice)[i])->IsMissingMember()) {
			CString strTitle;
			CString strMsg;
			strTitle.LoadString(IDS_APPLICATION);
			strMsg.LoadString(IDS_WARNING_MISSING_MEMBER);
			int id = MessageBox(
				strMsg,
				strTitle,
				MB_YESNO|MB_ICONEXCLAMATION
				);
			if(IDYES != id) {
				return;
			}
			break;
		}
	}

	NDASCOMM_CONNECTION_INFO ci;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE); // LogicalDevice will set up-to-date disk's info.

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpClearDefectMark(
		&ci, pUnitDevice->GetSequence());

	l_auto_cursor.Release();

	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

//
// Reconfigure RAID without this disk. If spare disk does not exist, we cannot reconfigure.
//
void CMainFrame::OnRemoveFromRaid(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	DWORD i;
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();
	BOOLEAN ConvertToBasic;

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pOldUnitDevice = dynamic_cast<CNBUnitDevice*>(pDevice);
	if(!pOldUnitDevice)
		return;
	CNBLogicalDevice *pLogicalDevice = pOldUnitDevice->GetLogicalDevice();
	if(!pLogicalDevice)
		return;

	if ((pOldUnitDevice->GetRaidStatus() &  NDAS_RAID_MEMBER_FLAG_ONLINE) &&
		!(pOldUnitDevice->GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER)) {
		// Convert to basic disk if this is currently member of this RAID.
		// Otherwise don't touch this disk.
		ConvertToBasic = TRUE;
	} else {
		ConvertToBasic = FALSE;
	}

	//
	// Check RAID will not be broken with remaining members.
	//

	if (pLogicalDevice->GetType() == NMT_RAID1R3) {
		BOOLEAN ValidMemberExist = FALSE;
		// Check at least one active member except this one is not out-of-sync.
		for(i=0;i<pLogicalDevice->DevicesInRaid();i++) {
			DWORD UnitRaidStatus;
			if (pOldUnitDevice->GetSequence() == i) {
				continue;
			}
			UnitRaidStatus = (*pLogicalDevice)[i]->GetRaidStatus();

			if (UnitRaidStatus & NDAS_RAID_MEMBER_FLAG_ONLINE) {
				if (!(UnitRaidStatus & (
					NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC |
					NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER))) {
					ValidMemberExist = TRUE;
				}
			}
		}
		if (!ValidMemberExist) {
			CString strMsg;
			strMsg.LoadString(IDS_RAID_MEMBER_UNREMOVALBLE);
			CString strTitle;
			strTitle.LoadString(IDS_APPLICATION);
			MessageBox(
				strMsg,
				strTitle, 
				MB_OK | MB_ICONWARNING
				);
			return;
		}
	}else {
		// Unsupported RAID type.
		return;
	}

	//
	// Show error if any of other member is not online.
	//
	for(i=0;i<pLogicalDevice->DevicesInRaid();i++) {
		DWORD UnitRaidStatus;
		if (pOldUnitDevice == (*pLogicalDevice)[i])
			continue;
		if (((*pLogicalDevice)[i])->IsMissingMember()) {
			CString strTitle;
			CString strMsg;
			strTitle.LoadString(IDS_APPLICATION);
			strMsg.LoadString(IDS_ERROR_MISSING_MEMBER);
			int id = MessageBox(
				strMsg,
				strTitle,
				MB_ICONEXCLAMATION
				);
			return;
		}
	}

	// Update DIB & RMD including new Unit
	
	NDASCOMM_CONNECTION_INFO ci;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpRemoveFromRaid(
		&ci, pOldUnitDevice->GetSequence());
	
	if (!bResults)	{
		l_auto_cursor.Release();
		goto out;
	}

	if (ConvertToBasic) {
		//
		// Convert to basic disk.
		//
		bResults = pOldUnitDevice->InitConnectionInfo(&ci, TRUE);
		if (!bResults) {
			l_auto_cursor.Release();
			goto out;
		}
		NdasOpBind(	1, &ci, NMT_SINGLE,0);
	}
	l_auto_cursor.Release();

out:
	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pOldUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
	
}

void CMainFrame::OnUseAsBasic(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	// Just convert to basic disk. Same as OnResetType except warning message
	
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pUnitDevice = dynamic_cast<CNBUnitDevice *>(pDevice);
	if (!pUnitDevice)
		return;

	// Show warning that RAID may be broken.
	{
		CString strTitle;
		CString strMsg;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_RAID_BROKEN);
		int id = MessageBox(
			strMsg,
			strTitle,
			MB_YESNO|MB_ICONEXCLAMATION
			);
		if(IDYES != id) {
			return;
		}
	}

	NDASCOMM_CONNECTION_INFO ci;
	AutoCursor l_auto_cursor(IDC_WAIT);

	//
	// Convert to basic disk.
	//
	BOOL bResults = pUnitDevice->InitConnectionInfo(&ci, TRUE);
	if (!bResults) {
		l_auto_cursor.Release();
		goto out;
	}
	NdasOpBind(	1, &ci, NMT_SINGLE,0);
	l_auto_cursor.Release();
out:
	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);
}

//
// Convert disk type to default even if it is unknown type.
//
void CMainFrame::OnResetType(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBLogicalDevice *pLogicalDevice = dynamic_cast<CNBLogicalDevice *>(pDevice);
	if (!pLogicalDevice)
		return;

	NDASCOMM_CONNECTION_INFO ci;
	AutoCursor l_auto_cursor(IDC_WAIT);

	//
	// Convert to basic disk.
	//
	BOOL bResults = pLogicalDevice->InitConnectionInfo(&ci, TRUE);
	if (!bResults) {
		l_auto_cursor.Release();
		goto out;
	}
	NdasOpBind(	1, &ci, NMT_SINGLE,0);
	l_auto_cursor.Release();
out:
	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		strMsg.LoadString(IDS_OPERATION_FAIL);

		ShowErrorMessageBox(strMsg);

		return;
	}

	OnRefreshStatus(NULL, NULL, NULL);	
}

//
// Reconfigure RAID set using this disk.
//
void CMainFrame::OnUseAsMember(UINT wNotifyCode, int wID, HWND hwndCtl)
{
	CNBDevice *pDevice = m_viewTreeList.GetSelectedDevice();

	if(!pDevice)
		return;

	if(!pDevice->GetCommandAbility(wID))
		return;

	CNBUnitDevice *pUnitDevice = dynamic_cast<CNBUnitDevice *>(pDevice);
	if (!pUnitDevice)
		return;
	
	CNBLogicalDevice *pLogicalDevice = pUnitDevice->GetLogicalDevice();
	if (!pLogicalDevice)
		return;

	//
	// Check this unit device has enough space.
	//
	if (pLogicalDevice->GetLogicalCapacityInByte() > pUnitDevice->GetPhysicalCapacityInByte()) {
		CString strTitle, strMsg;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_RAID_MEMBER_UNREPLACEABLE_SMALL_SIZE);
		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONERROR
			);
		return;
	}

	//
	// Show warning that this disk's data will be destroyed or RAID will be broken if this disk is member of another RAID.
	//
	{
		CString strTitle;
		CString strMsg;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_DATA_LOST_AFTER_USE_AS_MEMBER);
		int id = MessageBox(
			strMsg,
			strTitle,
			MB_YESNO|MB_ICONEXCLAMATION
			);
		if(IDYES != id) {
			return;
		}
	}

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ci, ci_replace;
	pLogicalDevice->InitConnectionInfo(&ci, TRUE); // use write access. this function does not support run time replace yet.
	pUnitDevice->InitConnectionInfo(&ci_replace, TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);

	BOOL bResults = NdasOpReplaceDevice(
		&ci, &ci_replace, pUnitDevice->GetSequence());

	l_auto_cursor.Release();

	pLogicalDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());

	if(!bResults)
	{
		CString strMsg;

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
	// Remove self from device list to show.
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

	CNBUnitDevice *pUnitDeviceToAdd = dlgSelectDevice.GetSelectedDevice();
	CNBUnitDevice *pUnitDevice = pUnitDevice = (*pLogicalDevice)[0];

	// Bind & Synchronize 
	NDASCOMM_CONNECTION_INFO ConnectionInfo[2];
	pUnitDevice->InitConnectionInfo(&ConnectionInfo[0], TRUE);
	pUnitDeviceToAdd->InitConnectionInfo(&ConnectionInfo[1], TRUE);

	AutoCursor l_auto_cursor(IDC_WAIT);
	UINT32 BindResult = NdasOpBind(
		2,
		ConnectionInfo,
		NMT_SAFE_RAID1,
		0);
	l_auto_cursor.Release();

	if(2 != BindResult)
	{


		CString strMsg;

		DWORD dwLastError = ::GetLastError();

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
			strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, 
				(BindResult == 0) ? pUnitDevice->GetName() : pUnitDeviceToAdd->GetName());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);

		return;
	}

	pUnitDevice->HixChangeNotify(pGetNdasHostGuid());
	pUnitDeviceToAdd->HixChangeNotify(pGetNdasHostGuid());

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

void 
CMainFrame::OnInterAppMsg(WPARAM wParam, LPARAM lParam)
{
	if (NDASBIND_INSTMSG_POPUP == wParam)
	{
		::SetForegroundWindow(m_hWnd);
	}
}
