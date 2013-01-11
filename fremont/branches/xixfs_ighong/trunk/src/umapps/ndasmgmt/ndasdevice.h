#pragma once
#include <ndas/ndasuser.h>

#if 0
class CDeviceStatusBitmap
{
public:
	static HBITMAP GetStatusBitmap(NDAS_DEVICE_STATUS status)
	{
		static HBITMAP hbdisabled = NULL;
		static HBITMAP hbconnected = NULL;
		static HBITMAP hbdisconnected = NULL;
		static HBITMAP hberror = NULL;

		switch (status) {
		case NDAS_DEVICE_STATUS_DISABLED:
			if (NULL == hbdisabled) {
				hbdisabled = ::LoadBitmap(
					_Module.GetResourceInstance(), 
					MAKEINTRESOURCE(IDB_STATUS_DISABLED));
			}
			return hbdisabled;

		case NDAS_DEVICE_STATUS_CONNECTED:
			if (NULL == hbconnected) {
				hbconnected = ::LoadBitmap(
					_Module.GetResourceInstance(), 
					MAKEINTRESOURCE(IDB_STATUS_ENABLED_RW));
			}
			return hbconnected;

		case NDAS_DEVICE_STATUS_DISCONNECTED:
			if (NULL == hbdisconnected) {
				hbdisconnected = ::LoadBitmap(
					_Module.GetResourceInstance(), 
					MAKEINTRESOURCE(IDB_STATUS_BUSY));
			}
			return hbdisconnected;

		default:
			if (NULL == hberror) {
				hberror = ::LoadBitmap(
					_Module.GetResourceInstance(), 
					MAKEINTRESOURCE(IDB_STATUS_ERROR));
			}
			return hberror;
		}
	}
};

class CNdasDevice
{
	CBitmap m_hStatusBitmap;

public:

	CString m_strDeviceName;
	CString m_strDeviceId;
	CString m_strDeviceKey;
	DWORD m_dwIndex;
	BOOL m_bWriteAccess;

	NDAS_DEVICE_STATUS m_status;
	NDAS_DEVICE_ERROR m_lastError;

	CMenu m_subMenu;

	CNdasDevice(
		DWORD dwIndex, LPCTSTR szDeviceId, 
		LPCTSTR szDeviceName, LPCTSTR szDeviceKey = NULL, BOOL bWriteAccess = FALSE);

	void CreateMenuItem(WORD wId, MENUITEMINFO &mii);

	void GetDeviceStringId(CString& stringId)
	{
		stringId.Format(TEXT("%s-%s-%s-%s"), 
			m_strDeviceId.Mid(0,5),
			m_strDeviceId.Mid(5,5),
			m_strDeviceId.Mid(10,5),
			m_strDeviceId.Mid(15,5));
	}

	BOOL UpdateStatus()
	{
		BOOL fSuccess = ::NdasQueryDeviceStatusByIdW(
			m_strDeviceId, &m_status,);
		ATLTRACE(TEXT("Updating device (%s) status returned %d, status %d\n"),
			m_strDeviceId, fSuccess, m_status);
		if (!fSuccess) {
			return FALSE;
		}

		fSuccess = UpdateUnitDeviceInformation();
		return fSuccess;
	}

	static BOOL CALLBACK UnitDeviceEnumProc(
		PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, 
		LPVOID lpContext)
	{
		CNdasDevice* p = reinterpret_cast<CNdasDevice*>(lpContext);
		ATLTRACE(TEXT("Enumerating unit device from %s - Unit %d, Type %d\n"), 
			p->m_strDeviceId,
            lpEntry->UnitNo,
            lpEntry->UnitDeviceType);
		return TRUE;
	}

	BOOL UpdateUnitDeviceInformation()
	{
		BOOL fSuccess = NdasEnumUnitDevicesById(
			m_strDeviceId, 
			UnitDeviceEnumProc, 
			reinterpret_cast<LPVOID>(this));

		ATLTRACE(TEXT("Enumerating unit devices from (%s) status returned %d\n"),
			m_strDeviceId, fSuccess);
		return fSuccess;
	}

	void UpdateBitmap()
	{
		switch (m_status) {
		case NDAS_DEVICE_STATUS_DISABLED:
			m_hStatusBitmap.LoadBitmap(IDB_STATUS_DISABLED);
			break;
		case NDAS_DEVICE_STATUS_CONNECTED:
			m_hStatusBitmap.LoadBitmap(IDB_STATUS_ENABLED_RW);
			break;
		case NDAS_DEVICE_STATUS_DISCONNECTED:
			m_hStatusBitmap.LoadBitmap(IDB_STATUS_ENABLED_RO);
			break;
		default:
			m_hStatusBitmap.LoadBitmap(IDB_STATUS_ERROR);
			break;
		}
	}

};

inline CNdasDevice::CNdasDevice(
	DWORD dwIndex, LPCTSTR szDeviceId, 
	LPCTSTR szDeviceName, LPCTSTR szDeviceKey,
	BOOL bWriteAccess) :
	m_dwIndex(dwIndex),
	m_strDeviceName(szDeviceName),
	m_strDeviceId(szDeviceId),
	m_strDeviceKey(szDeviceKey),
	m_bWriteAccess(bWriteAccess)
{}

inline void CNdasDevice::CreateMenuItem(WORD wId, MENUITEMINFO& mii)
{
	BOOL fSuccess(FALSE);
	CMenuItemInfo subMenuItemInfo;
	CString strMenuText;

	CMenuItemInfo sep;
	sep.fMask = MIIM_TYPE;
	sep.fType = MFT_SEPARATOR;

	UpdateBitmap();

	m_subMenu.CreateMenu();
	CMenu subMenu;

	subMenuItemInfo.fMask = MIIM_ID | MIIM_STRING;

	// Enable or Disable
	if (m_status == NDAS_DEVICE_STATUS_DISABLED) {
		strMenuText.LoadString(IDS_ENABLE_DEVICE);
	} else {
		strMenuText.LoadString(IDS_DISABLE_DEVICE);
	}

	subMenuItemInfo.wID = IDR_ENABLE_DEVICE + m_dwIndex;
	subMenuItemInfo.dwTypeData = (LPTSTR)(LPCTSTR)strMenuText;
	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);

	if (m_status == NDAS_DEVICE_STATUS_DISABLED) 
	{
		strMenuText.LoadString(IDS_UNREGISTER_DEVICE);
		subMenuItemInfo.wID = IDR_UNREGISTER_DEVICE + m_dwIndex;
		subMenuItemInfo.dwTypeData = (LPTSTR)(LPCTSTR)strMenuText;
		m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);
	}

	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &sep);

	strMenuText.LoadString(IDR_NDD_MOUNT_RO);
	subMenuItemInfo.fMask |= MIIM_DATA;
	subMenuItemInfo.dwItemData = m_dwIndex;
	subMenuItemInfo.wID = IDR_NDD_MOUNT_RO;
	subMenuItemInfo.dwTypeData = LPTSTR(LPCTSTR(strMenuText));
	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);
	subMenuItemInfo.fMask &= ~(MIIM_DATA);

	strMenuText.LoadString(IDR_NDD_MOUNT_RW);
	subMenuItemInfo.wID = IDR_NDD_MOUNT_RW;
	subMenuItemInfo.dwTypeData = LPTSTR(LPCTSTR(strMenuText));
	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);

	strMenuText.LoadString(IDR_NDD_UNMOUNT);
	subMenuItemInfo.wID = IDR_NDD_UNMOUNT;
	subMenuItemInfo.dwTypeData = LPTSTR(LPCTSTR(strMenuText));
	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);

	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &sep);
	
	strMenuText.LoadString(IDS_SHOW_DEVICE_PROPERTIES);
	subMenuItemInfo.wID = IDR_SHOW_DEVICE_PROPERTIES + m_dwIndex;
	subMenuItemInfo.dwTypeData = (LPTSTR)(LPCTSTR)strMenuText;
	m_subMenu.InsertMenuItem(0xFFFF, TRUE, &subMenuItemInfo);

	mii.fMask = MIIM_STATE | MIIM_FTYPE | MIIM_CHECKMARKS |
		MIIM_ID | MIIM_STRING | MIIM_SUBMENU;
	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED | MFS_UNCHECKED;
	mii.hSubMenu = m_subMenu;
	mii.wID = wId;
	mii.dwTypeData = (LPTSTR)((LPCTSTR)m_strDeviceName);
//	mii.wID = IDR_NDAS_DEVICE_NONE + 1;
	// mii.hbmpUnchecked = m_hStatusBitmap;
	mii.hbmpUnchecked = CDeviceStatusBitmap::GetStatusBitmap(m_status);
//	mii.hbmpItem = m_hStatusBitmap;
	// mii.hbmpChecked = m_hBmpDisabled;

}

#endif 
