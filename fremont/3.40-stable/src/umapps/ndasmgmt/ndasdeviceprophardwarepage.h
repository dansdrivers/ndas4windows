#pragma once

#include "propertylist.h"

class CNdasDevicePropHardwarePage :
	public CPropertyPageImpl<CNdasDevicePropHardwarePage>
{
public:

	enum { IDD = IDD_DEVPROP_HW };

	BEGIN_MSG_MAP_EX(CNdasDevicePropHardwarePage)
		MSG_WM_INITDIALOG(OnInitDialog)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	CNdasDevicePropHardwarePage();

	void SetDevice(ndas::DevicePtr pDevice);

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

private:

	CPropertyListCtrl m_propList;
	ndas::DevicePtr m_pDevice;

};
