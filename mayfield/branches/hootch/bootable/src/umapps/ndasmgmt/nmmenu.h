#pragma once
#include "ndasmgmt.h"
#include "menubitmap.h"

class CNdasDeviceMenu
{
	CWindow m_wnd;

public:

	CNdasDeviceMenu(HWND hWnd);

	void AppendUnitDeviceMenuItem(
		ndas::UnitDevicePtr pUnitDevice,
		HMENU hMenu,
		PBYTE psiPartData);

	void CreateDeviceMenuItem(
		IN ndas::DevicePtr pDevice,
		IN OUT MENUITEMINFO& mii);

	HMENU CreateDeviceSubMenu(
		ndas::DevicePtr pDevice,
		NDSI_DATA* psiData);
};

