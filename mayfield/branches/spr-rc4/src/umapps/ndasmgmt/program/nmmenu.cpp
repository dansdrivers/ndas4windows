#include "stdafx.h"
#include "resource.h"
#include "appconf.h"
#include "nmmenu.h"

static
BOOL
pSetDeviceStatusMenuItem(HMENU hMenu, UINT nStringID);

CNdasDeviceMenu::CNdasDeviceMenu(HWND hWnd) :
	m_wnd(hWnd)
{
}

VOID
CNdasDeviceMenu::AppendUnitDeviceMenuItem(
	ndas::UnitDevice* pUnitDevice,
	HMENU hMenu,
	PBYTE pPartStatus)
{
	CMenuHandle menu = hMenu;
	CMenuItemInfo mii;
	CString strText;
	BOOL fSuccess = FALSE;

	CMenuItemInfo sep;
	sep.fMask = MIIM_TYPE;
	sep.fType = MFT_SEPARATOR;

	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA | MIIM_STATE;
	mii.fState = MFS_ENABLED;
	mii.dwItemData = 
		((WORD) pUnitDevice->GetSlotNo()) |
		(((WORD) pUnitDevice->GetUnitNo()) << 16);

	NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();
	ndas::LogicalDevice* pLogDev = _pLogDevColl->FindLogicalDevice(logDevId);

	if (NULL == pLogDev) {
		return;
	}

	pLogDev->UpdateStatus();
	pLogDev->UpdateInfo();

	mii.dwItemData = pLogDev->GetLogicalDeviceId();

	NDAS_LOGICALDEVICE_STATUS lstatus = pLogDev->GetStatus();

	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == lstatus ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == lstatus) 
	{

		if (pLogDev->GetMountedAccess() & GENERIC_WRITE) {
			*pPartStatus = NDSI_PART_MOUNTED_RW;
			pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTED_RW);
		} else {
			*pPartStatus = NDSI_PART_MOUNTED_RO;
			pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTED_RO);
		}

		fSuccess = strText.LoadString(IDR_NDD_UNMOUNT);
		ATLASSERT(fSuccess);
		mii.wID = IDR_NDD_UNMOUNT;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == lstatus) {
			mii.fState = MFS_DISABLED;
		}

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
		ATLASSERT(fSuccess);

	} else if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == lstatus ||
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus) 
	{

		*pPartStatus = NDSI_PART_UNMOUNTED;
		pSetDeviceStatusMenuItem(menu, IDS_DEVMST_CONNECTED);

		// push the status
		UINT fSavedState = mii.fState;

		fSuccess = strText.LoadString(IDR_NDD_MOUNT_RO);
		ATLASSERT(fSuccess);

		mii.wID = IDR_NDD_MOUNT_RO;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDev->GetGrantedAccess() & GENERIC_READ) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus) {
			mii.fState = MFS_DISABLED;
		}

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		fSuccess = strText.LoadString(IDR_NDD_MOUNT_RW);
		ATLASSERT(fSuccess);

		mii.wID = IDR_NDD_MOUNT_RW;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDev->GetGrantedAccess() & GENERIC_WRITE) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus) {
			mii.fState = MFS_DISABLED;
		}

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		// pop the status;
		mii.fState = fSavedState;

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
		ATLASSERT(fSuccess);

	} else if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED == lstatus) {
		// not used
		*pPartStatus = NDSI_PART_UNMOUNTED;
// obsolete
//	} else if (NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE == lstatus) {
//		// not used
//		*pPartStatus = NDSI_PART_UNMOUNTED;
	} else if (NDAS_LOGICALDEVICE_STATUS_UNKNOWN == lstatus) {
		// not used
		*pPartStatus = NDSI_PART_UNMOUNTED;
	} else {
		ATLASSERT(FALSE);
	}
}

HMENU 
CNdasDeviceMenu::CreateDeviceSubMenu(
	ndas::Device* pDevice,
	NDSI_DATA* psiData)
{
	BOOL fSuccess(FALSE);
	CMenuHandle menu;
	CMenuItemInfo mii;
	CMenuItemInfo sep;
	CString strText;
	sep.fMask = MIIM_TYPE;
	sep.fType = MFT_SEPARATOR;

	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;
	mii.dwItemData = (WORD)pDevice->GetSlotNo();

	fSuccess = menu.CreateMenu();
	ATLASSERT(fSuccess);

	NDAS_DEVICE_STATUS status = pDevice->GetStatus();
	// Enable or Disable

	BOOL fShowDisable = FALSE;
	BOOL fShowUnregister = FALSE;

	if (NDAS_DEVICE_STATUS_DISABLED == status) 
	{

		psiData->Status = NDSI_DISABLED;

		pSetDeviceStatusMenuItem(menu,IDS_DEVMST_DEACTIVATED);

		fSuccess = strText.LoadString(IDS_ACTIVATE_DEVICE);
		ATLASSERT(fSuccess);

		mii.wID = IDR_ENABLE_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		fShowUnregister = TRUE;

	} 
	else if (NDAS_DEVICE_STATUS_DISCONNECTED == status) 
	{
		//
		// although there is no ERROR status in NDAS device,
		// if the last error is set, we will show the indicator
		// as error
		//
		psiData->Status = 
			(ERROR_SUCCESS == pDevice->GetLastError()) ? 
			NDSI_DISCONNECTED : 
			NDSI_ERROR;

		pSetDeviceStatusMenuItem(menu, IDS_DEVMST_DISCONNECTED);
		fShowDisable = TRUE;
		fShowUnregister = TRUE;

	}
	else if (NDAS_DEVICE_STATUS_UNKNOWN == status) 
	{

		psiData->Status = NDSI_UNKNOWN;
		pSetDeviceStatusMenuItem(menu, IDS_DEVMST_UNKNOWN);
		fShowDisable = TRUE;
		fShowUnregister = TRUE;

	} 
	else if (NDAS_DEVICE_STATUS_CONNECTED == status) 
	{

		psiData->Status = NDSI_CONNECTED;

		BOOL fMounted = FALSE;
		DWORD nDevices = pDevice->GetUnitDeviceCount();

		for (DWORD i = 0; i < nDevices; ++i) {

			ndas::UnitDevice* pUnitDevice = pDevice->GetUnitDevice(i);

			pUnitDevice->UpdateStatus();

			BYTE bReserved;
			psiData->nParts = min(i + 1, 2);
			AppendUnitDeviceMenuItem(
				pUnitDevice, 
				menu.m_hMenu, 
				(i < 2) ? &psiData->StatusPart[i] : &bReserved);

			// If there is a mounted unit device, DISABLE menu will be disabled.
			if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus()) {
				fMounted = TRUE;
			}

			pUnitDevice->Release();
		}

		if (!fMounted) 
		{
			fShowDisable = TRUE;
			fShowUnregister = TRUE;
		}


	}

	//
	// Unregister Device Menu
	//
	if (fShowUnregister) {

		fSuccess = strText.LoadString(IDS_UNREGISTER_DEVICE);
		ATLASSERT(fSuccess);

		mii.wID = IDR_UNREGISTER_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
		ATLASSERT(fSuccess);
	}

	//
	// We are not using DISABLE in the menu anymore
	//
#if USE_DISABLE_COMMAND_IN_MENU
	if (fShowDisable) {
		fSuccess = strText.LoadString(IDS_DISABLE_DEVICE);
		ATLASSERT(fSuccess);

		mii.wID = IDR_DISABLE_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
		ATLASSERT(fSuccess);
	}
#endif

	fSuccess = strText.LoadString(IDS_SHOW_DEVICE_PROPERTIES);
	ATLASSERT(fSuccess);
	mii.wID = IDR_SHOW_DEVICE_PROPERTIES;
	mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
	fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
	ATLASSERT(fSuccess);

	return menu.m_hMenu;
}

VOID
CNdasDeviceMenu::CreateDeviceMenuItem(
	IN ndas::Device* pDevice,
	IN OUT MENUITEMINFO& mii)
{
	mii.fMask = 
		MIIM_STATE | MIIM_FTYPE | MIIM_BITMAP |
		MIIM_ID | MIIM_STRING | MIIM_SUBMENU | MIIM_DATA;

	mii.hbmpItem = HBMMENU_CALLBACK;
	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED;
	mii.dwTypeData = (LPTSTR)pDevice->GetName();
	mii.dwItemData = 0;
	PNDSI_DATA psiData = (PNDSI_DATA)&mii.dwItemData;
	mii.hSubMenu = CreateDeviceSubMenu(pDevice, psiData);
}

BOOL
pSetDeviceStatusMenuItem(HMENU hMenu, UINT nStringID)
{
	BOOL fSuccess;
	CMenuHandle menu = hMenu;
	BOOL fShow = TRUE;
	pGetAppConfigValue(_T("ShowDeviceStatusText"), &fShow);
	if (!fShow) {
		return TRUE;
	}

	CMenuItemInfo mii;
	mii.fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE | MIIM_DATA;
	mii.wID = 100; // status text
	mii.fState = MFS_DISABLED;
	mii.fType = MFT_OWNERDRAW;
	mii.dwItemData = nStringID;
	// mii.dwTypeData = (LPTSTR)nStringID;// (LPTSTR)(LPCTSTR)strStatus;
	fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
	ATLASSERT(fSuccess);

	return fSuccess;
}

