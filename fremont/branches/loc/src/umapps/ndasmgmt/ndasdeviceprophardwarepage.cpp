#include "stdafx.h"
#include "ndasmgmt.h"
#include "resource.h"
#include <xtl/xtlautores.h>
#include <boost/mem_fn.hpp>

#include "propertylist.h"
#include <boost/shared_ptr.hpp>
#include <xtl/xtlautores.h>
#include "devpropsh.h"
#include "confirmdlg.h"
#include "propertylist.h"
#include "ndastypestr.h"
#include "apperrdlg.h"
#include "waitdlg.h"
#include "exportdlg.h"
#include "ndasdevicerenamedlg.h"
#include "ndasdeviceaddwritekeydlg.h"

#include "ndasdeviceprophardwarepage.h"

namespace
{
	bool IsNullNdasDeviceId(const NDAS_DEVICE_ID& deviceId)
	{
		return (deviceId.Node[0] == 0x00 &&
			deviceId.Node[1] == 0x00 &&
			deviceId.Node[2] == 0x00 &&
			deviceId.Node[3] == 0x00 &&
			deviceId.Node[4] == 0x00 &&
			deviceId.Node[5] == 0x00);
	}
}

CNdasDevicePropHardwarePage::CNdasDevicePropHardwarePage()
{
}

LRESULT 
CNdasDevicePropHardwarePage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice.get());
	if (NULL == m_pDevice.get()) 
	{
		return 0;
	}

	m_propList.SubclassWindow(GetDlgItem(IDC_PROPLIST));
	m_propList.SetExtendedListStyle(PLS_EX_CATEGORIZED);


	//
	// Device Hardware Information
	//
	boost::shared_ptr<const NDAS_DEVICE_HARDWARE_INFO> pDevHWInfo = 
		m_pDevice->GetHardwareInfo();

	CString strValue = (LPCTSTR) IDS_DEVPROP_CATEGORY_HARDWARE;
	m_propList.AddItem(PropCreateCategory(strValue));

	if (0 == pDevHWInfo.get()) 
	{
		CString str = MAKEINTRESOURCE(IDS_DEVICE_HARDWARE_INFO_NA);
		m_propList.AddItem(PropCreateSimple(str, _T("")));
		return 0;
	}

	CString strType;

	// Version
	strType.LoadString(IDS_DEVPROP_HW_VERSION);
	pHWVersionString(strValue, pDevHWInfo.get());
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Revision
	if (pDevHWInfo->HardwareRevision != 0)
	{
		strType.LoadString(IDS_DEVPROP_HW_REVISION);
		strValue.Format(_T("%X"), pDevHWInfo->HardwareRevision);
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	// Max Request Blocks
	strType.LoadString(IDS_DEVPROP_HW_MAX_REQUEST_BLOCKS);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumTransferBlocks);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Slots
	strType.LoadString(IDS_DEVPROP_HW_SLOT_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->NumberOfCommandProcessingSlots);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Targets
	//strType.LoadString(IDS_DEVPROP_HW_TARGET_COUNT);
	//strValue.Format(_T("%d"), pDevHWInfo->NumberOfTargets);
	//m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max Targets
	strType.LoadString(IDS_DEVPROP_HW_MAX_TARGET_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumNumberOfTargets);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max LUs
	strType.LoadString(IDS_DEVPROP_HW_MAX_LU_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumNumberOfLUs);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	boost::shared_ptr<const NDAS_DEVICE_HARDWARE_INFO> phwi = m_pDevice->GetHardwareInfo();
	if (NULL != phwi.get() && 
		!IsNullNdasDeviceId(phwi->NdasDeviceId))
	{
		strType.LoadString(IDS_DEVPRO_HW_MAC_ADDRESS);
		strValue.Format(_T("%02X:%02X:%02X:%02X:%02X:%02X"),
			phwi->NdasDeviceId.Node[0],
			phwi->NdasDeviceId.Node[1],
			phwi->NdasDeviceId.Node[2],
			phwi->NdasDeviceId.Node[3],
			phwi->NdasDeviceId.Node[4],
			phwi->NdasDeviceId.Node[5]);
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	//
	// Unit Device Hardware Information
	//

	const ndas::UnitDeviceVector& unitDevices = m_pDevice->GetUnitDevices();
	for (DWORD i = 0; i < unitDevices.size(); ++i) 
	{
		ndas::UnitDevicePtr pUnitDevice = unitDevices.at(i);

		const NDAS_UNITDEVICE_HARDWARE_INFO* pHWI = pUnitDevice->GetHWInfo();

		strValue.FormatMessage(IDS_DEVPROP_UNITDEV_TITLE_FMT, i + 1);

		m_propList.AddItem(PropCreateCategory(strValue));

		if (NULL == pHWI) 
		{
			// "Not available"
			CString str = MAKEINTRESOURCE(IDS_UNITDEVICE_HARDWARE_INFO_NA);
			m_propList.AddItem(PropCreateSimple(str, _T("")));
			continue;
		}

		// Media Type
		strType.LoadString(IDS_DEVPROP_UNITDEV_DEVICE_TYPE);
		pUnitDeviceMediaTypeString(strValue, pHWI->MediaType);
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		CString modeDelimiter(MAKEINTRESOURCE(IDS_MODE_DELIMITER));

		// Transfer Mode
		strType.LoadString(IDS_DEVPROP_UNITDEV_TRANSFER_MODE);
		strValue = _T("");
		{
			const struct { 
				const BOOL& Mode;
				UINT ResID;
			} TransferModeStrings[] = {
				pHWI->PIO, IDS_TRANSFER_MODE_PIO,
				pHWI->DMA, IDS_TRANSFER_MODE_DMA,
				pHWI->UDMA, IDS_TRANSFER_MODE_UDMA
			};

			bool multipleModes = false;
			for (int i = 0; i < RTL_NUMBER_OF(TransferModeStrings); ++i)
			{
				if (TransferModeStrings[i].Mode)
				{
					if (multipleModes) strValue += modeDelimiter;
					strValue += CString(MAKEINTRESOURCE(TransferModeStrings[i].ResID));
					multipleModes = true;
				}
			}
			m_propList.AddItem(PropCreateSimple(strType, strValue));
		}


		// LBA support?
		strType.LoadString(IDS_DEVPROP_UNITDEV_LBA_MODE);
		strValue = _T("");
		{
			const struct { 
				const BOOL& Mode;
				UINT ResID;
			} LbaModeStrings[] = {
				pHWI->LBA, IDS_LBA_MODE_LBA,
				pHWI->LBA48, IDS_LBA_MODE_LBA48
			};

			bool multipleModes = false;
			for (int i = 0; i < RTL_NUMBER_OF(LbaModeStrings); ++i)
			{
				if (LbaModeStrings[i].Mode)
				{
					if (multipleModes) strValue += modeDelimiter;
					strValue += CString(MAKEINTRESOURCE(LbaModeStrings[i].ResID));
					multipleModes = true;
				}
			}
			m_propList.AddItem(PropCreateSimple(strType, strValue));
		}

		// Model
		strType.LoadString(IDS_DEVPROP_UNITDEV_MODEL);
		strValue = pHWI->Model;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// FWRev
		strType.LoadString(IDS_DEVPROP_UNITDEV_FWREV);
		strValue = pHWI->FirmwareRevision;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// Serial No
		strType.LoadString(IDS_DEVPROP_UNITDEV_SERIALNO);
		strValue = pHWI->SerialNumber;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	return 0;
}

void 
CNdasDevicePropHardwarePage::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
}
