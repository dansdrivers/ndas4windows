#pragma once
#include "ndasmgmt.h"

#define LOGDEV_MAKE_ULONG_PTR(s,t,l) \
	(((ULONG_PTR)((WORD)(s))) | \
	((ULONG_PTR)(MAKEWORD(t,l)) << 16))

#define LOGDEV_MAKE_ULONG_PTR_FROM_LDID(id) \
	LOGDEV_MAKE_ULONG_PTR(id.SlotNo, id.TargetId, id.LUN)

#define LOGDEV_ULONG_PTR_TO_SLOTNO(x) \
	((DWORD)(LOWORD((x))))

#define LOGDEV_ULONG_PTR_TO_TARGETID(x) \
	((DWORD)(HIWORD(LOBYTE(x))))

#define LOGDEV_ULONG_PTR_TO_LUN(x) \
	((DWORD)(HIWORD(HIBYTE((x)))))

#define LOGDEV_ULONG_PTR_TO_LDID(u) { \
	LOGDEV_ULONG_PTR_TO_SLOTNO(u), \
	LOGDEV_ULONG_PTR_TO_TARGETID(u), \
	LOGDEV_ULONG_PTR_TO_LUN(u) }

#define UNITDEV_MAKE_ULONG_PTR(devid,un) \
	((ULONG_PTR)(((WORD)(devid)) | (((WORD)(un)) << 16)))

#define UNITDEV_ULONG_PTR_TO_DEVID(p) \
	((DWORD)((LOWORD)(p)))

#define UNITDEV_ULONG_PTR_TO_UNITNO(p) \
	((DWORD)((HIWORD)(p)))


class CNdasDeviceMenu
{
	CWindow m_wnd;
public:

	CNdasDeviceMenu(HWND hWnd) :
		m_wnd(hWnd)
	{
	}

	void AppendUnitDeviceMenuItem(
		ndas::UnitDevice* pUnitDevice,
		HMENU hMenu)
	{
		CMenuHandle menu(hMenu);
		CMenuItemInfo mii;
		WTL::CString strText;
		BOOL fSuccess = FALSE;

		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA | MIIM_STATE;
		mii.fState = MFS_ENABLED;
		mii.dwItemData = 
			((WORD) pUnitDevice->GetSlotNo()) |
			(((WORD) pUnitDevice->GetUnitNo()) << 16);

		NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();

		ndas::LogicalDevice* pLogDev = 
			_pLogDevColl->FindLogicalDevice(logDevId);

		if (NULL == pLogDev) {
			return;
		}

		pLogDev->UpdateStatus();
		pLogDev->UpdateInfo();

		mii.dwItemData = LOGDEV_MAKE_ULONG_PTR_FROM_LDID(
			pLogDev->GetLogicalDeviceId());

		switch (pLogDev->GetStatus()) {
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			{
				// push the status
				UINT fSavedState = mii.fState;

				fSuccess = strText.LoadString(ID_NDD_MOUNT_RO);
				ATLASSERT(fSuccess);
				mii.wID = ID_NDD_MOUNT_RO;
				mii.dwTypeData = LPTSTR(LPCTSTR(strText));
				mii.fState = (pLogDev->GetGrantedAccess() & GENERIC_READ) ?
					MFS_ENABLED : MFS_DISABLED;
				fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
				ATLASSERT(fSuccess);

				fSuccess = strText.LoadString(ID_NDD_MOUNT_RW);
				ATLASSERT(fSuccess);
				mii.wID = ID_NDD_MOUNT_RW;
				mii.dwTypeData = LPTSTR(LPCTSTR(strText));
				mii.fState = (pLogDev->GetGrantedAccess() & GENERIC_WRITE) ?
					MFS_ENABLED : MFS_DISABLED;
				fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
				ATLASSERT(fSuccess);

				// pop the status;
				mii.fState = fSavedState;
			}
			break;
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:

			fSuccess = strText.LoadString(ID_NDD_UNMOUNT);
			ATLASSERT(fSuccess);
			mii.wID = ID_NDD_UNMOUNT;
			mii.dwTypeData = LPTSTR(LPCTSTR(strText));
			fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
			ATLASSERT(fSuccess);

			break;
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
			break;
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
			break;
		case NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED:
			break;
		case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
			break;
		case NDAS_LOGICALDEVICE_STATUS_UNKNOWN:
			break;
		default:
			ATLASSERT(FALSE);
		}

	}

	HMENU CreateDeviceSubMenu(ndas::Device* pDevice)
	{
		BOOL fSuccess(FALSE);
		CMenuHandle menu;
		CMenuItemInfo mii;
		CMenuItemInfo sep;
		WTL::CString strText;
		sep.fMask = MIIM_TYPE;
		sep.fType = MFT_SEPARATOR;

		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;
		mii.dwItemData = (WORD)pDevice->GetSlotNo();

		fSuccess = menu.CreateMenu();
		ATLASSERT(fSuccess);

		NDAS_DEVICE_STATUS status = pDevice->GetStatus();
		// Enable or Disable
		if (NDAS_DEVICE_STATUS_DISABLED == status) {

			fSuccess = strText.LoadString(IDS_ENABLE_DEVICE);
			ATLASSERT(fSuccess);
			mii.wID = ID_ENABLE_DEVICE;
			mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
			fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
			ATLASSERT(fSuccess);

			fSuccess = strText.LoadString(IDS_UNREGISTER_DEVICE);
			ATLASSERT(fSuccess);
			mii.wID = ID_UNREGISTER_DEVICE;
			mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
			fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
			ATLASSERT(fSuccess);

			fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
			ATLASSERT(fSuccess);

		} else {

			BOOL fMounted = FALSE;
			DWORD nDevices = pDevice->GetUnitDeviceCount();

			for (DWORD i = 0; i < nDevices; ++i) {
				ndas::UnitDevice* pUnitDevice = pDevice->GetUnitDevice(i);
				AppendUnitDeviceMenuItem(pUnitDevice, menu.m_hMenu);
				pUnitDevice->Release();

				fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &sep);
				ATLASSERT(fSuccess);

				if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus()) {
					fMounted = TRUE;
				}
			}

			if (!fMounted) {
				fSuccess = strText.LoadString(IDS_DISABLE_DEVICE);
				ATLASSERT(fSuccess);
				mii.wID = ID_DISABLE_DEVICE;
				mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
				fSuccess = menu.InsertMenuItem(0, TRUE, &mii);
				ATLASSERT(fSuccess);

				fSuccess = menu.InsertMenuItem(1, TRUE, &sep);
				ATLASSERT(fSuccess);
			}


		}

		fSuccess = strText.LoadString(IDS_SHOW_DEVICE_PROPERTIES);
		ATLASSERT(fSuccess);
		mii.wID = ID_SHOW_DEVICE_PROPERTIES;
		mii.dwTypeData = (LPTSTR)(LPCTSTR)strText;
		fSuccess = menu.InsertMenuItem(0xFFFF, TRUE, &mii);
		ATLASSERT(fSuccess);

		return menu.m_hMenu;
	}

	void CreateDeviceMenuItem(
		IN ndas::Device* pDevice,
		IN OUT MENUITEMINFO& mii)
	{
		mii.fMask = 
			MIIM_STATE | MIIM_FTYPE | MIIM_BITMAP |
			MIIM_ID | MIIM_STRING | MIIM_SUBMENU | MIIM_DATA;

		mii.dwItemData = (ULONG_PTR) pDevice;
		mii.hbmpItem = HBMMENU_CALLBACK;
		mii.fType = MFT_STRING;
		mii.fState = MFS_ENABLED;
		mii.dwTypeData = (LPTSTR)pDevice->GetName();
		mii.hSubMenu = 	CreateDeviceSubMenu(pDevice);
	}
};

template <typename WindowT>
class CNdasDeviceMenuHandler :
	// Derive from the CMessageMap to receive dynamically
	// chained messages
	public CMessageMap
{
public:
	BEGIN_MSG_MAP_EX(CNdasDeviceMenuHandler)
	// Handle messages from the view and the main frame
		COMMAND_ID_HANDLER_EX(ID_NDD_MOUNT_RO, OnMountRO)
		COMMAND_ID_HANDLER_EX(ID_NDD_MOUNT_RW, OnMountRW)
		COMMAND_ID_HANDLER_EX(ID_NDD_UNMOUNT, OnUnmount)
	END_MSG_MAP()

	void OnMountRO(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnMountRO: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}

	void OnMountRW(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnMountRW: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}

	void OnUnmount(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnUnmount: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}
};

