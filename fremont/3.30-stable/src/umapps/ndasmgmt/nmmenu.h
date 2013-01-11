#pragma once
#include "ndasmgmt.h"
#include "menubitmap.h"

class CNdasTaskBarMenu
{
public:

	void Init(HWND hWnd);

	void ClearItemStringData();
	void AddItemStringData(CString& str);

	void AppendUnitDeviceMenuItem(
		ndas::UnitDevicePtr pUnitDevice,
		HMENU hMenu,
		PBYTE psiPartData);

	void CreateDeviceMenuItem(
		__in ndas::DevicePtr pDevice,
		__inout MENUITEMINFO& mii);

	HMENU CreateDeviceSubMenu(
		__in ndas::DevicePtr pDevice,
		__out NDSI_DATA* psiData);

	void ShowDeviceStatusText(BOOL Show);

private:

	CWindow m_wnd;
	std::vector<CString> m_itemStringDataVector;
	BOOL m_showDeviceStatusText;

	BOOL pSetDeviceStatusMenuItem(HMENU hMenu, CString& str);
	BOOL pSetDeviceStatusMenuItem(HMENU hMenu, UINT nStringID);
	BOOL pSetDeviceStatusMenuItemWithDrives(HMENU hMenu, UINT nStringID, DWORD UnitMask);

};
