#pragma once

#include "ndasdevicepropgeneralpage.h"
#include "ndasdeviceprophardwarepage.h"
#include "ndasdeviceprophoststatpage.h"
#include "ndasdevicepropadvancedpage.h"

typedef boost::shared_ptr<CNdasDevicePropHostStatPage> HostStatPagePtr;
typedef std::vector<HostStatPagePtr> HostStatPageVector;

class CNdasDevicePropSheet :
	public CPropertySheetImpl<CNdasDevicePropSheet>
{
public:

	BEGIN_MSG_MAP_EX(CNdasDevicePropSheet)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<CNdasDevicePropSheet>)
	END_MSG_MAP()

	CNdasDevicePropSheet(
		_U_STRINGorID title = (LPCTSTR) NULL,
		UINT uStartPage = 0,
		HWND hWndParent = NULL);
	
	void SetDevice(ndas::DevicePtr pDevice);
	void OnShowWindow(BOOL bShow, UINT nStatus);

private:

	BOOL m_bCentered;
	ndas::DevicePtr m_pDevice;
	CNdasDevicePropGeneralPage m_pspGeneral;
	CNdasDevicePropHardwarePage m_pspHardware;
	HostStatPageVector m_pspHostStats;
	CNdasDevicePropAdvancedPage m_pspAdvanced;

};
