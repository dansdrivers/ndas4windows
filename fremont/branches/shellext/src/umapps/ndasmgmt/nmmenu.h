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
		__in ndas::DevicePtr pDevice,
		__inout MENUITEMINFO& mii);

	HMENU CreateDeviceSubMenu(
		__in ndas::DevicePtr pDevice,
		__out NDSI_DATA* psiData);
};

