#include "stdafx.h"
#include "resource.h"
#include "appconf.h"
#include "nmmenu.h"

namespace
{
	BOOL 
	pSetDeviceStatusMenuItem(
		HMENU hMenu, 
		UINT nStringID);
	
	HIMAGELIST
	pLoadStatusIndicatorImageList();
	
	HBITMAP
	pCreateStatusIndicatorBitmap(
		HWND hWnd,
		HIMAGELIST hImageList,
		const NDSI_DATA* Data);

}

void
CNdasTaskBarMenu::Init(HWND hwnd)
{
	m_wnd.Attach(hwnd);
}

void 
CNdasTaskBarMenu::ClearItemStringData()
{
	m_itemStringDataVector.clear();
}

void 
CNdasTaskBarMenu::AddItemStringData(CString& str)
{
	m_itemStringDataVector.push_back(str);
}

void
CNdasTaskBarMenu::AppendUnitDeviceMenuItem(
	ndas::UnitDevicePtr pUnitDevice,
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

	NDAS_LOGICALDEVICE_ID logDeviceId = pUnitDevice->GetLogicalDeviceId();
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId)) 
	{
		pSetDeviceStatusMenuItem(menu, IDS_DEVMST_ERROR);
		if (pUnitDevice->GetStatus() == NDAS_UNITDEVICE_STATUS_ERROR)
		{
			*pPartStatus = NDSI_PART_ERROR;
		}
		return;
	}

	pLogDevice->UpdateStatus();
	pLogDevice->UpdateInfo();

	mii.dwItemData = pLogDevice->GetLogicalDeviceId();

	NDAS_LOGICALDEVICE_STATUS lstatus = pLogDevice->GetStatus();
	NDAS_LOGICALDEVICE_ERROR lerror = pLogDevice->GetNdasLogicalUnitError();
	DWORD unitMask = pLogDevice->GetLogicalDrives();

	if (NDAS_LOGICALUNIT_STATUS_MOUNTED == lstatus ||
		NDAS_LOGICALUNIT_STATUS_DISMOUNT_PENDING == lstatus) 
	{

		if (pLogDevice->GetMountedAccess() & GENERIC_WRITE) 
		{
			*pPartStatus = NDSI_PART_MOUNTED_RW;
			if (NDAS_LOGICALUNIT_STATUS_MOUNTED == lstatus)
			{
				pSetDeviceStatusMenuItemWithDrives(menu, IDS_DEVMST_MOUNTED_RW, unitMask);
			}
			else
			{
				pSetDeviceStatusMenuItemWithDrives(menu, IDS_DEVMST_DISMOUNTING_RW, unitMask);
			}
		}
		else 
		{
			*pPartStatus = NDSI_PART_MOUNTED_RO;
			if (NDAS_LOGICALUNIT_STATUS_MOUNTED == lstatus)
			{
				pSetDeviceStatusMenuItemWithDrives(menu, IDS_DEVMST_MOUNTED_RO, unitMask);
			}
			else
			{
				pSetDeviceStatusMenuItemWithDrives(menu, IDS_DEVMST_DISMOUNTING_RO, unitMask);
			}
		}

		if (pLogDevice->IsContentEncrypted())
		{
			*pPartStatus |= NDSI_PART_CONTENT_IS_ENCRYPTED;
		}

		ATLVERIFY(strText.LoadString(IDR_NDD_UNMOUNT));

		mii.wID = IDR_NDD_UNMOUNT;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));

		if (NDAS_LOGICALUNIT_STATUS_DISMOUNT_PENDING == lstatus) 
		{
			mii.fState = MFS_DISABLED;
		}

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &sep));

	}
	else if (NDAS_LOGICALUNIT_STATUS_DISMOUNTED == lstatus ||
		NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING == lstatus) 
	{
		*pPartStatus = NDSI_PART_UNMOUNTED;
		if (pLogDevice->IsContentEncrypted())
		{
			*pPartStatus |= NDSI_PART_CONTENT_IS_ENCRYPTED;
		}

		switch (lstatus)
		{
		case NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING:
			{
				ACCESS_MASK access = pLogDevice->GetMountedAccess();
				if (access & GENERIC_WRITE)
				{
					pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTING_RW);
				}
				else
				{
					pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTING_RO);
				}
			}
			break;
		case NDAS_LOGICALUNIT_STATUS_DISMOUNTED:
			pSetDeviceStatusMenuItem(menu, IDS_DEVMST_CONNECTED);
			break;
		}


		// push the status
		UINT fSavedState = mii.fState;

		ATLVERIFY(strText.LoadString(IDR_NDD_MOUNT_RO));

		mii.wID = IDR_NDD_MOUNT_RO;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDevice->GetGrantedAccess() & GENERIC_READ) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING == lstatus /*||
			(lerror != NDAS_LOGICALDEVICE_ERROR_NONE && 
			lerror !=NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE) */) 
		{
			mii.fState = MFS_DISABLED;
		}

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

		ATLVERIFY(strText.LoadString(IDR_NDD_MOUNT_RW));

		mii.wID = IDR_NDD_MOUNT_RW;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDevice->GetGrantedAccess() & GENERIC_WRITE) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING == lstatus /*||
			(lerror != NDAS_LOGICALDEVICE_ERROR_NONE && 
			lerror !=NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE) */) 
		{
			mii.fState = MFS_DISABLED;
		}

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

		// pop the status;
		mii.fState = fSavedState;

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &sep));

	} 
	else if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED == lstatus) 
	{
		// not used
		*pPartStatus = NDSI_PART_UNMOUNTED;
	}
	else if (NDAS_LOGICALUNIT_STATUS_UNKNOWN == lstatus) 
	{
		// not used
		*pPartStatus = NDSI_PART_UNMOUNTED;
	}
	else 
	{
		ATLASSERT(FALSE);
	}
}

HMENU 
CNdasTaskBarMenu::CreateDeviceSubMenu(
	__in ndas::DevicePtr pDevice,
	__out NDSI_DATA* psiData)
{
	CMenuHandle menu;
	CString strText;

	CMenuItemInfo sep;
	sep.fMask = MIIM_TYPE;
	sep.fType = MFT_SEPARATOR;

	CMenuItemInfo mii;
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;
	mii.dwItemData = (WORD)pDevice->GetSlotNo();

	ATLVERIFY(menu.CreateMenu());

	NDAS_DEVICE_STATUS status = pDevice->GetStatus();
	// Enable or Disable

	BOOL fShowUnregister = FALSE;
	BOOL fShowReset = FALSE;

	if (NDAS_DEVICE_STATUS_NOT_REGISTERED == status) 
	{
		psiData->Status = NDSI_DISABLED;

		pSetDeviceStatusMenuItem(menu,IDS_DEVMST_DEACTIVATED);

		ATLVERIFY(strText.LoadString(IDS_ACTIVATE_DEVICE));

		mii.wID = IDR_ENABLE_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

		fShowUnregister = TRUE;

	} 
	else if (NDAS_DEVICE_STATUS_OFFLINE == status) 
	{
		//
		// although there is no ERROR status in NDAS device,
		// if the last error is set, we will show the indicator
		// as error
		//
		psiData->Status = 
			(ERROR_SUCCESS == pDevice->GetNdasDeviceError()) ? 
			NDSI_DISCONNECTED : 
			NDSI_ERROR;

		ATLVERIFY(pSetDeviceStatusMenuItem(menu, IDS_DEVMST_DISCONNECTED));
		fShowUnregister = TRUE;
	}
	else if (NDAS_DEVICE_STATUS_UNKNOWN == status) 
	{
		psiData->Status = NDSI_UNKNOWN;
		ATLVERIFY(pSetDeviceStatusMenuItem(menu, IDS_DEVMST_UNKNOWN));
		fShowUnregister = TRUE;
	} 
	else if (NDAS_DEVICE_STATUS_CONNECTING == status)
	{
		psiData->Status = NDSI_CONNECTING;
		ATLVERIFY(pSetDeviceStatusMenuItem(menu, IDS_DEVMST_CONNECTING));
		fShowUnregister = TRUE;
	}
	else if (NDAS_DEVICE_STATUS_ONLINE == status) 
	{
		psiData->Status = NDSI_CONNECTED;

		BOOL fMounted = FALSE;
		BOOL fErrorOnAnyUnitDevice = FALSE;

		ndas::UnitDeviceVector unitDevices = pDevice->GetUnitDevices();
		int i = 0;
		for (ndas::UnitDeviceConstIterator itr = unitDevices.begin();
			itr != unitDevices.end(); ++itr, ++i)
		{
			ndas::UnitDevicePtr pUnitDevice = *itr;

			pUnitDevice->UpdateStatus();

			BYTE bReserved;
			psiData->nParts = min(i + 1, 2);
			AppendUnitDeviceMenuItem(
				pUnitDevice, 
				menu.m_hMenu, 
				(i < 2) ? &psiData->StatusPart[i] : &bReserved);

			// If there is a mounted unit device, DISABLE menu will be disabled.
			if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus()) 
			{
				fMounted = TRUE;
			}

			if (NDAS_UNITDEVICE_ERROR_NONE != pUnitDevice->GetNdasUnitError())
			{
				fErrorOnAnyUnitDevice = TRUE;
			}
		}

		if (!fMounted) 
		{
			fShowUnregister = TRUE;
		}

		if (fErrorOnAnyUnitDevice)
		{
			fShowReset = TRUE;
		}
	}

	//
	// Show 'Re&set' at the NDAS device context menu if the NDAS device is 'Red icon' status.
	//
	if (NDAS_DEVICE_ERROR_NONE != pDevice->GetNdasDeviceError())
	{
		fShowReset = TRUE;
	}

	if (fShowReset)
	{
		ATLVERIFY(strText.LoadString(IDS_RESET_DEVICE));
		mii.wID = IDR_RESET_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &sep));
	}

	//
	// Unregister Device Menu
	//
	if (fShowUnregister) 
	{
		ATLVERIFY(strText.LoadString(IDS_UNREGISTER_DEVICE));
		mii.wID = IDR_UNREGISTER_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &sep));
	}

	ATLVERIFY(strText.LoadString(IDS_SHOW_DEVICE_PROPERTIES));
	mii.wID = IDR_SHOW_DEVICE_PROPERTIES;
	mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
	ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

	return menu.m_hMenu;
}

void
CNdasTaskBarMenu::CreateDeviceMenuItem(
	__in ndas::DevicePtr pDevice,
	__inout MENUITEMINFO& mii)
{
	mii.fMask = 
		MIIM_BITMAP | MIIM_DATA | MIIM_FTYPE | /* MIIM_ID | */ 
		MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;

	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED;
	mii.dwItemData; /* see next */
	mii.dwTypeData = const_cast<LPTSTR>(pDevice->GetName());

	mii.hbmpItem = HBMMENU_CALLBACK;
	ATLVERIFY(mii.hSubMenu = 
		CreateDeviceSubMenu(
		pDevice, 
		reinterpret_cast<PNDSI_DATA>(&mii.dwItemData)));
}

void 
CNdasTaskBarMenu::ShowDeviceStatusText(BOOL Show)
{
	m_showDeviceStatusText = Show;
}

BOOL 
CNdasTaskBarMenu::pSetDeviceStatusMenuItemWithDrives(
	HMENU hMenu, UINT nStringID, DWORD UnitMask)
{
	if (!m_showDeviceStatusText) return TRUE;

	CString str = MAKEINTRESOURCE(nStringID);

	if (UnitMask)
	{
		str += _T(" - (");
		for (DWORD index = 0; index < 'Z' - 'A' + 1; ++index)
		{
			if (UnitMask & (1 << index))
			{
				str += static_cast<CHAR>(_T('A') + index);
				str += _T(':');

				UnitMask &= ~(1 << index);

				if (UnitMask)
				{
					str += _T(',');
					str += _T(' ');
				}
			}
		}
		str += _T(")");
	}

	return pSetDeviceStatusMenuItem(hMenu, str);
}

BOOL
CNdasTaskBarMenu::pSetDeviceStatusMenuItem(HMENU hMenu, UINT nStringID)
{
	if (!m_showDeviceStatusText) return TRUE;

	CString str = MAKEINTRESOURCE(nStringID);
	return pSetDeviceStatusMenuItem(hMenu, str);
}

BOOL 
CNdasTaskBarMenu::pSetDeviceStatusMenuItem(HMENU hMenu, CString& str)
{
	if (!m_showDeviceStatusText) return TRUE;

	//
	// Add the reference to the data
	//
	AddItemStringData(str);

	//
	// Insert the menu item
	//
	CMenuItemInfo mii;
	mii.fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE | MIIM_DATA;
	mii.wID = NDSI_StatusTextItemId; // status text
	mii.fState = MFS_DISABLED;
	mii.fType = MFT_OWNERDRAW;
	mii.dwItemData = (ULONG_PTR)(static_cast<LPCTSTR>(str));

	CMenuHandle menu = hMenu;
	BOOL success;
	ATLVERIFY( success = menu.InsertMenuItem(0xFFFF, TRUE, &mii) );
	return success;
}

namespace
{

HIMAGELIST
pLoadStatusIndicatorImageList()
{
	BOOL fSuccess;

	CImageList imageList;

	ATLVERIFY( fSuccess = imageList.Create(
		cxStatusBitmap, cxStatusBitmap, ILC_COLOR32 | ILC_MASK, 0, 8) );

	if (!fSuccess) return NULL;

	CBitmapHandle bitmap;
	
	ATLVERIFY( bitmap = AtlLoadBitmapImage(IDB_STATUS, LR_CREATEDIBSECTION | LR_SHARED) );
	ATLVERIFY( imageList.Add(bitmap, (COLORREF)CLR_DEFAULT) != -1);
	ATLVERIFY( imageList.SetOverlayImage(nENCRYPTED, nOV_ENCRYPTED) );

	return imageList;
}

HBITMAP
pCreateStatusIndicatorBitmap(
	HWND hWnd,
	HIMAGELIST hImageList,
	const NDSI_DATA* Data)
{
	//
	// Create a DC compatible with the desktop window's DC
	//
	CWindowDC windowDC = ::GetDesktopWindow();
	CDC dc;
	ATLVERIFY( dc.CreateCompatibleDC(windowDC) );

	int cx = cxStatusBitmap, cy = cxStatusBitmap;

	if (Data->Status == NDSI_CONNECTED && Data->nParts > 0)
	{
		cx *= Data->nParts;
	}

	CBitmapHandle bitmap; 
	ATLVERIFY( bitmap.CreateCompatibleBitmap(windowDC, cx, cy) );
	HBITMAP oldBitmap = dc.SelectBitmap(bitmap);

	dc.PatBlt(0, 0, cx, cy, WHITENESS);

	WTL::CImageList imageList = hImageList;

	int x = 0, y = 0;

	switch (Data->Status) 
	{
	case NDSI_UNKNOWN:
		ATLVERIFY( imageList.Draw(dc, nERROR, x, y, ILD_NORMAL));
		break;
	case NDSI_ERROR:
		ATLVERIFY( imageList.Draw(dc, nERROR, x, y, ILD_NORMAL));
		break;
	case NDSI_DISABLED:
		ATLVERIFY( imageList.Draw(dc, nDEACTIVATED, x, y, ILD_NORMAL));
		break;
	case NDSI_DISCONNECTED:
		ATLVERIFY( imageList.Draw(dc, nDISCONNECTED, x, y, ILD_NORMAL));
		break;
	case NDSI_CONNECTING:
		ATLVERIFY( imageList.Draw(dc, nCONNECTING, x, y, ILD_NORMAL));
		break;
	case NDSI_CONNECTED:
		ATLASSERT(Data->nParts <= 2);
		if (0 == Data->nParts)
		{
			ATLVERIFY( imageList.Draw(dc, nCONNECTED, x, y, ILD_NORMAL));
		}
		else
		{
			for (int i = 0; i < Data->nParts && i < 2; ++i)
			{
				int index = nCONNECTED;
				switch (Data->StatusPart[i] & 0x0F) 
				{
				case NDSI_PART_MOUNTED_RW: 
					index = nMOUNTED_RW; 
					break;
				case NDSI_PART_MOUNTED_RO: 
					index = nMOUNTED_RO; 
					break;
				case NDSI_PART_ERROR:      
					index = nERROR; 
					break;
				case NDSI_PART_UNMOUNTED:
				default:
					break;
				}
				UINT style = ILD_NORMAL;
				if ((Data->StatusPart[i] & NDSI_PART_CONTENT_IS_ENCRYPTED))
				{
					style |= INDEXTOOVERLAYMASK(nOV_ENCRYPTED);
				}
				ATLVERIFY( imageList.Draw(dc, index, x, y, style));
				x += cxStatusBitmap;
			}
		}
		break;
	default:
		ATLASSERT(FALSE);
	}

	// windowDC.BitBlt(0, 0, cx, cy, dc, cx, cy, SRCCOPY);
	dc.SelectBitmap(oldBitmap);

	return bitmap;
}

} // namespace

