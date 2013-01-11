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

	// Image List Indexes
	const int nDISCONNECTED = 0;
	const int nCONNECTING = 1;
	const int nCONNECTED = 2;
	const int nMOUNTED_RW = 3;
	const int nMOUNTED_RO = 4;
	const int nERROR = 5;
	const int nDEACTIVATED = 6;
	const int nENCRYPTED = 7;
	const int nOV_ENCRYPTED = 1;
	const int cxStatusBitmap = 14; // 14 by 14 images

	// TODO: This code will not be necessary later
	BOOL 
	RequireVistaMenuBitmapBugWorkaround()
	{
		OSVERSIONINFO osvi = { sizeof(OSVERSIONINFO) };
		ATLVERIFY(GetVersionEx(&osvi));
		return (osvi.dwMajorVersion == 6) &&
			(osvi.dwBuildNumber < 5536);
	}


}

CNdasDeviceMenu::CNdasDeviceMenu(HWND hWnd) :
	m_wnd(hWnd)
{
}

void
CNdasDeviceMenu::AppendUnitDeviceMenuItem(
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
	NDAS_LOGICALDEVICE_ERROR lerror = pLogDevice->GetLastError();
	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == lstatus ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == lstatus) 
	{

		if (pLogDevice->GetMountedAccess() & GENERIC_WRITE) 
		{
			*pPartStatus = NDSI_PART_MOUNTED_RW;
			pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTED_RW);
		}
		else 
		{
			*pPartStatus = NDSI_PART_MOUNTED_RO;
			pSetDeviceStatusMenuItem(menu, IDS_DEVMST_MOUNTED_RO);
		}
		if (pLogDevice->IsContentEncrypted())
		{
			*pPartStatus |= NDSI_PART_CONTENT_IS_ENCRYPTED;
		}

		ATLVERIFY(strText.LoadString(IDR_NDD_UNMOUNT));

		mii.wID = IDR_NDD_UNMOUNT;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));

		if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == lstatus) 
		{
			mii.fState = MFS_DISABLED;
		}

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));
		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &sep));

	}
	else if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == lstatus ||
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus) 
	{
		*pPartStatus = NDSI_PART_UNMOUNTED;
		if (pLogDevice->IsContentEncrypted())
		{
			*pPartStatus |= NDSI_PART_CONTENT_IS_ENCRYPTED;
		}

		pSetDeviceStatusMenuItem(menu, IDS_DEVMST_CONNECTED);

		// push the status
		UINT fSavedState = mii.fState;

		ATLVERIFY(strText.LoadString(IDR_NDD_MOUNT_RO));

		mii.wID = IDR_NDD_MOUNT_RO;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDevice->GetGrantedAccess() & GENERIC_READ) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus ||
			(lerror != NDAS_LOGICALDEVICE_ERROR_NONE && 
			lerror !=NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE)) 
		{
			mii.fState = MFS_DISABLED;
		}

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

		ATLVERIFY(strText.LoadString(IDR_NDD_MOUNT_RW));

		mii.wID = IDR_NDD_MOUNT_RW;
		mii.dwTypeData = LPTSTR(LPCTSTR(strText));
		mii.fState = (pLogDevice->GetGrantedAccess() & GENERIC_WRITE) ?
			MFS_ENABLED : MFS_DISABLED;

		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == lstatus ||
			(lerror != NDAS_LOGICALDEVICE_ERROR_NONE && 
			lerror !=NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE)) 
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
	else if (NDAS_LOGICALDEVICE_STATUS_UNKNOWN == lstatus) 
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
CNdasDeviceMenu::CreateDeviceSubMenu(
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

	if (NDAS_DEVICE_STATUS_DISABLED == status) 
	{
		psiData->Status = NDSI_DISABLED;

		pSetDeviceStatusMenuItem(menu,IDS_DEVMST_DEACTIVATED);

		ATLVERIFY(strText.LoadString(IDS_ACTIVATE_DEVICE));

		mii.wID = IDR_ENABLE_DEVICE;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;

		ATLVERIFY(menu.InsertMenuItem(0xFFFF, TRUE, &mii));

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
	else if (NDAS_DEVICE_STATUS_CONNECTED == status) 
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

			if (NDAS_UNITDEVICE_ERROR_NONE != pUnitDevice->GetLastError())
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
	if (NDAS_DEVICE_ERROR_NONE != pDevice->GetLastError())
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
CNdasDeviceMenu::CreateDeviceMenuItem(
	__in ndas::DevicePtr pDevice,
	__inout MENUITEMINFO& mii)
{
	static BOOL requireVistaMenuBitmapBugWorkaround = 
		RequireVistaMenuBitmapBugWorkaround();

	mii.fMask = 
		MIIM_BITMAP | MIIM_DATA | MIIM_FTYPE | /* MIIM_ID | */ 
		MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;

	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED;
	mii.dwItemData; /* see next */
	mii.dwTypeData = const_cast<LPTSTR>(pDevice->GetName());

	//
	// As we don't want to invert the color of the status indicator
	// we cannot simply create a bitmap here. We should draw it from the callback.
	//
	// Vista 5384 does not accept HBMMENU_CALLBACK
	// (Seems like a bug)
	//
	if (requireVistaMenuBitmapBugWorkaround)
	{
		CImageList imageList = pLoadStatusIndicatorImageList();
		NDSI_DATA ndsiData;
		ATLVERIFY( mii.hSubMenu = CreateDeviceSubMenu(pDevice, &ndsiData) );
		mii.hbmpItem = pCreateStatusIndicatorBitmap(m_wnd, imageList, &ndsiData);
		ATLVERIFY( imageList.Destroy() );
	}
	else
	{
		mii.hbmpItem = HBMMENU_CALLBACK;
		ATLVERIFY(mii.hSubMenu = 
			CreateDeviceSubMenu(
			pDevice, 
			reinterpret_cast<PNDSI_DATA>(&mii.dwItemData)));
	}
}

namespace
{

BOOL
pSetDeviceStatusMenuItem(HMENU hMenu, UINT nStringID)
{
	CMenuHandle menu = hMenu;
	BOOL fShow = TRUE;
	pGetAppConfigValue(_T("ShowDeviceStatusText"), &fShow);
	if (!fShow) 
	{
		return TRUE;
	}

	CMenuItemInfo mii;
	mii.fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE | MIIM_DATA;
	mii.wID = 100; // status text
	mii.fState = MFS_DISABLED;
	mii.fType = MFT_OWNERDRAW;
	mii.dwItemData = nStringID;
	// mii.dwTypeData = (LPTSTR)nStringID;// (LPTSTR)(LPCTSTR)strStatus;

	BOOL fSuccess;

	ATLVERIFY( fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii) );

	return fSuccess;
}

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
				case NDSI_PART_MOUNTED_RW: index = nMOUNTED_RW; break;
				case NDSI_PART_MOUNTED_RO: index = nMOUNTED_RO; break;
				case NDSI_PART_ERROR:      index = nERROR; break;
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

