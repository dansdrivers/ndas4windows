#include "stdafx.h"
#include "ndasmgmt.h"
#include "devpropsh.h"

CNdasDevicePropSheet::CNdasDevicePropSheet(
	_U_STRINGorID title,
	UINT uStartPage,
	HWND hWndParent) : 
	CPropertySheetImpl<CNdasDevicePropSheet>(title, uStartPage, hWndParent),
	m_bCentered(FALSE),
	m_pspHostStats(NULL)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_USEPAGELANG;
	AddPage(m_pspGeneral);
}

void 
CNdasDevicePropSheet::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
	m_pspGeneral.SetDevice(pDevice);

	//
	// Hardware Page shows only if the device is connected
	//
	if (NDAS_DEVICE_STATUS_ONLINE == pDevice->GetStatus()) 
	{
		m_pspHardware.SetDevice(pDevice);
		AddPage(m_pspHardware);
	}

	m_pspHostStats.clear();

	ndas::UnitDeviceVector unitDevices = pDevice->GetUnitDevices();
	if (!unitDevices.empty()) 
	{
		DWORD nPages = unitDevices.size();
		for (DWORD i = 0; i < nPages; ++i) 
		{
			HostStatPagePtr pPage(new CNdasDevicePropHostStatPage());
			pPage->SetUnitDevice(unitDevices[i]);
			m_pspHostStats.push_back(pPage);
			AddPage(*pPage.get());
		}
	}

	m_pspAdvanced.SetDevice(pDevice);
	AddPage(m_pspAdvanced);
}

void 
CNdasDevicePropSheet::OnShowWindow(BOOL bShow, UINT nStatus)
{
	ATLASSERT(NULL != m_pDevice.get()); 
	if (bShow && !m_bCentered) 
	{
		// Center Windows only once!
		m_bCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

