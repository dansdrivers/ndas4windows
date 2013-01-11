#include "stdatl.hpp"
#include <ndas/ndasportioctl.h>
#include <ndas/ndasdiskioctl.h>
#include <initguid.h>
#include <ioevent.h>
#include <winioctl.h>
#include <ndas/ndasataguid.h>
#include <ndas/ndasportguid.h>
#include "eventwnd.hpp"

HRESULT
pRegisterDeviceInterfaceNotification(
	__in HWND hWnd, 
	__in LPCGUID InterfaceGuid,
	__out HDEVNOTIFY* DevNotifyHandle)
{
	HRESULT hr;

	*DevNotifyHandle = NULL;

	HDEVNOTIFY h;
	DEV_BROADCAST_DEVICEINTERFACE deviceInterface = {0};
	deviceInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	deviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	deviceInterface.dbcc_classguid = *InterfaceGuid;
	h = RegisterDeviceNotification(
		hWnd, &deviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (NULL == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}
	*DevNotifyHandle = h;
	return S_OK;
}

HRESULT
pRegisterDeviceHandleNotification(
	__in HWND hWnd, 
	__in HANDLE DeviceHandle,
	__out HDEVNOTIFY* DevNotifyHandle)
{
	HRESULT hr;

	*DevNotifyHandle = NULL;

	HDEVNOTIFY h;
	DEV_BROADCAST_HANDLE handleNotify = {0};
	handleNotify.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
	handleNotify.dbch_devicetype = DBT_DEVTYP_HANDLE;
	handleNotify.dbch_handle = DeviceHandle;
	h = RegisterDeviceNotification(
		hWnd, &handleNotify, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (NULL == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}
	*DevNotifyHandle = h;
	return S_OK;
}

HRESULT
pGetNdasPortHandle(__out HANDLE* NdasportHandle)
{
	HRESULT hr;

	*NdasportHandle = INVALID_HANDLE_VALUE;

	HDEVINFO devInfoSet = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_NDASPORT,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		fprintf(stderr, "SetupDiGetClassDevs failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVICE_INTERFACE_DATA devIntfData;
	devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL success = SetupDiEnumDeviceInterfaces(
		devInfoSet,
		NULL,
		&GUID_DEVINTERFACE_NDASPORT,
		0,
		&devIntfData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		fprintf(stderr, "SetupDiEnumDeviceInterfaces failed, hr=0x%X\n", hr);
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	DWORD devIntfDetailSize = offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA, DevicePath) + 
		sizeof(TCHAR) * MAX_PATH;
	
	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail = 
		static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(devIntfDetailSize));

	if (NULL == devIntfDetail)
	{
		hr = E_OUTOFMEMORY;
		fprintf(stderr, "Memory allocation failed, size=0x%X\n", devIntfDetailSize);
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	devIntfDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	success = SetupDiGetDeviceInterfaceDetail(
		devInfoSet,
		&devIntfData,
		devIntfDetail,
		devIntfDetailSize,
		&devIntfDetailSize,
		NULL);

	if (!success)
	{
		if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
			free(devIntfDetail);
			SetupDiDestroyDeviceInfoList(devInfoSet);
			return hr;
		}

		PVOID p = realloc(devIntfDetail, devIntfDetailSize);

		if (NULL == p)
		{
			hr = E_OUTOFMEMORY;
			fprintf(stderr, "Memory allocation failed, size=0x%X\n", devIntfDetailSize);
			free(devIntfDetail);
			SetupDiDestroyDeviceInfoList(devInfoSet);
			return hr;
		}

		devIntfDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(p);

		success = SetupDiGetDeviceInterfaceDetail(
			devInfoSet,
			&devIntfData,
			devIntfDetail,
			devIntfDetailSize,
			&devIntfDetailSize,
			NULL);
	}

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		fprintf(stderr, "SetupDiGetDeviceInterfaceDetail2 failed, hr=0x%X\n", hr);
		free(devIntfDetail);
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	printf("ndasport: %s\n", devIntfDetail->DevicePath);

	HANDLE h = CreateFile(
		devIntfDetail->DevicePath,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	free(devIntfDetail);
	SetupDiDestroyDeviceInfoList(devInfoSet);

	if (INVALID_HANDLE_VALUE == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		fprintf(stderr, "CreateFile(%s) failed, hr=0x%X\n", hr);
		return hr;
	}

	*NdasportHandle = h;
	return S_OK;
}

BOOL
pIsNdasPortLogicalUnit(HANDLE DeviceHandle)
{
	DWORD bytesReturned;
	GUID ndasportIdentityGuid;

	BOOL success = DeviceIoControl(
		DeviceHandle,
		IOCTL_NDASPORT_LOGICALUNIT_EXIST,
		NULL, 0,
		&ndasportIdentityGuid, sizeof(GUID),
		&bytesReturned,
		NULL);

	if (success)
	{
		if (IsEqualGUID(GUID_NDASPORT_LOGICALUNIT_IDENTITY, ndasportIdentityGuid))
		{
			return TRUE;
		}
	}
	return FALSE;
}

HRESULT
pEnumerateNdasportLogicalUnits(
	__inout CSimpleArray<HDEVNOTIFY>& DevNotifyHandles,
	__in HWND hWnd,
	__in HDEVINFO DevInfoSet,
	__in LPCGUID InterfaceGuid)
{
	HRESULT hr;
	BOOL success;
	DWORD index = 0;
	SP_DEVICE_INTERFACE_DATA devIntfData;

	DWORD devIntfDetailSize = 
		offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA, DevicePath) + 
		sizeof(TCHAR) * MAX_PATH;

	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail = 
		static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(devIntfDetailSize));

	if (NULL == devIntfDetail)
	{
		hr = E_OUTOFMEMORY;
		fprintf(stderr, "Memory allocation failed, size=0x%X\n", devIntfDetailSize);
		return hr;
	}

	for (DWORD index = 0; ; ++index)
	{
		devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		
		success = SetupDiEnumDeviceInterfaces(
			DevInfoSet,
			NULL,
			InterfaceGuid,
			index,
			&devIntfData);

		if (!success)
		{
			if (ERROR_NO_MORE_ITEMS == GetLastError())
			{
				if (0 == index) fprintf(stderr, "No devices\n");
				hr = S_OK;
			}
			else
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				fprintf(stderr, "SetupDiEnumDeviceInterfaces failed, hr=0x%X\n", hr);
			}
			break;
		}

		devIntfDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		success = SetupDiGetDeviceInterfaceDetail(
			DevInfoSet,
			&devIntfData,
			devIntfDetail,
			devIntfDetailSize,
			&devIntfDetailSize,
			NULL);

		if (!success)
		{
			if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				fprintf(stderr, "SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
				continue;
			}

			PVOID p = realloc(devIntfDetail, devIntfDetailSize);

			if (NULL == p)
			{
				hr = E_OUTOFMEMORY;
				fprintf(stderr, "Memory allocation failed, size=0x%X\n", devIntfDetailSize);
				continue;
			}

			devIntfDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(p);

			success = SetupDiGetDeviceInterfaceDetail(
				DevInfoSet,
				&devIntfData,
				devIntfDetail,
				devIntfDetailSize,
				&devIntfDetailSize,
				NULL);
		}

		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail2 failed, hr=0x%X\n", hr);
			continue;
		}

		//
		// Now we have devIntfDetail->DevicePath
		//
		HANDLE h = CreateFile(
			devIntfDetail->DevicePath,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_DEVICE,
			NULL);

		if (INVALID_HANDLE_VALUE == h)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			fprintf(stderr, "CreateFile(%s) failed, hr=0x%X\n", 
				devIntfDetail->DevicePath, hr);
			continue;
		}

		if (!pIsNdasPortLogicalUnit(h))
		{
			ATLVERIFY( CloseHandle(h) );
			continue;
		}

		HDEVNOTIFY devNotifyHandle;

		hr = pRegisterDeviceHandleNotification(hWnd, h, &devNotifyHandle);

		if (FAILED(hr))
		{
			fprintf(stderr, "RegisterDeviceHandleNotification(%s) failed, hr=0x%X\n", 
				devIntfDetail->DevicePath, hr);
		}
		else
		{
			ATLVERIFY( DevNotifyHandles.Add(devNotifyHandle) );
			printf("Registered DevNotifyHandle=%p, Path=%s\n", 
				devNotifyHandle,
				devIntfDetail->DevicePath);
		}

		ATLVERIFY( CloseHandle(h) );
	}

	free(devIntfDetail);
	return S_OK;
}

BOOL
pIsNdasPortLogicalUnitInterfaceGuid(LPCGUID DeviceInterfaceGuid)
{
	static const LPCGUID guidClasses[] = {
		&GUID_DEVINTERFACE_CDROM,
		&GUID_DEVINTERFACE_DISK,
		&GUID_DEVINTERFACE_TAPE,
		&GUID_DEVINTERFACE_WRITEONCEDISK,
		&GUID_DEVINTERFACE_MEDIUMCHANGER,
		&GUID_DEVINTERFACE_CDCHANGER
	};

	for (int i = 0; i < RTL_NUMBER_OF(guidClasses); ++i)
	{
		if (IsEqualGUID(*guidClasses[i], *DeviceInterfaceGuid))
		{
			return TRUE;
		}
	}
	return FALSE;
}

HRESULT
pEnumerateNdasportLogicalUnits(
	__inout CSimpleArray<HDEVNOTIFY>& DevNotifyHandles,
	__in HWND hWnd)
{
	HRESULT hr;

	HDEVINFO devInfoSet = SetupDiGetClassDevsW(
		NULL,
		NULL,
		NULL,
		DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		fprintf(stderr, "SetupDiGetClassDevs failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVINFO_DATA devInfoData;

	for (DWORD index = 0; ; ++index)
	{
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL success = SetupDiEnumDeviceInfo(devInfoSet, index, &devInfoData);
		if (!success)
		{
			if (ERROR_NO_MORE_ITEMS != GetLastError())
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				fprintf(stderr, "SetupDiEnumDeviceInfo failed, hr=0x%X\n", hr);
			}
			break;
		}
		devInfoData.ClassGuid;
	}

	fprintf(stdout, "Enumerating DISK...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_DISK);

	fprintf(stdout, "Enumerating CDROM...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_CDROM);

	fprintf(stdout, "Enumerating TAPE...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_TAPE);

	fprintf(stdout, "Enumerating WRITEONCEDISK...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_WRITEONCEDISK);

	fprintf(stdout, "Enumerating MEDIUMCHANGER...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_MEDIUMCHANGER);

	fprintf(stdout, "Enumerating CDCHANGER...\n");

	hr = pEnumerateNdasportLogicalUnits(
		DevNotifyHandles, 
		hWnd, 
		devInfoSet, 
		&GUID_DEVINTERFACE_CDCHANGER);

	ATLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );

	return S_OK;
}

void 
CPnpEventConsumerWindow::OnNdasPortEvent(
	PNDASPORT_PNP_NOTIFICATION NdasPortNotification)
{
	LPCTSTR event = NULL;
	switch (NdasPortNotification->Type)
	{
	case NdasPortLogicalUnitIsReady: event = _T("Ready"); break;
	case NdasPortLogicalUnitIsRemoved:  event = _T("Removed"); break;
	}
	if (NULL == event)
	{
		fprintf(stdout, "NdasPort LogicalUnit %08X - Event %X\n", 
			NdasPortNotification->LogicalUnitAddress,
			NdasPortNotification->Type);
	}
	else
	{
		fprintf(stdout, "NdasPort LogicalUnit %08X - %s\n", 
			NdasPortNotification->LogicalUnitAddress,
			event);
	}
}
void 
CPnpEventConsumerWindow::OnNdasAtaLinkEvent(
	PNDAS_ATA_LINK_EVENT NdasAtaLinkEvent)
{
	LPCTSTR event = NULL;
	switch (NdasAtaLinkEvent->EventType)
	{
	case NDAS_ATA_LINK_UP: event = _T("Link is up"); break;
	case NDAS_ATA_LINK_DOWN:  event = _T("Link is down"); break;
	case NDAS_ATA_LINK_CONNECTING:  event = _T("Connecting"); break;
	}
	if (NULL == event)
	{
		fprintf(stdout, "NdasAta LogicalUnit %08X - Event %X\n", 
			NdasAtaLinkEvent->LogicalUnitAddress,
			NdasAtaLinkEvent->EventType);
	}
	else
	{
		fprintf(stdout, "NdasAta LogicalUnit %08X - %s\n", 
			NdasAtaLinkEvent->LogicalUnitAddress,
			event);
	}
}


LRESULT
CPnpEventConsumerWindow::OnCreate(LPCREATESTRUCT lpcs)
{
	HRESULT hr;
	HANDLE ndasportHandle;
	HDEVNOTIFY devNotifyHandle;

	hr = pRegisterDeviceInterfaceNotification(
		m_hWnd, &GUID_DEVINTERFACE_NDASPORT, &devNotifyHandle);

	if (FAILED(hr))
	{
		fprintf(stderr, "RegisterDeviceInterfaceNotification(NDASPORT) failed, hr=0x%x\n", hr);
	}
	else
	{
		ATLVERIFY(m_DevNotifyHandles.Add(devNotifyHandle));
		printf("Registered DevNotifyHandle=%p\n", devNotifyHandle);
	}


	hr = pRegisterDeviceInterfaceNotification(
		m_hWnd, &GUID_DEVINTERFACE_DISK, &devNotifyHandle);

	if (FAILED(hr))
	{
		fprintf(stderr, "RegisterDeviceInterfaceNotification(DISK) failed, hr=0x%x\n", hr);
	}
	else
	{
		ATLVERIFY(m_DevNotifyHandles.Add(devNotifyHandle));
		printf("Registered DevNotifyHandle=%p\n", devNotifyHandle);
	}


	hr = pGetNdasPortHandle(&ndasportHandle);

	if (FAILED(hr))
	{
		fprintf(stderr, "GetNdasPortHandle failed, hr=0x%x\n", hr);
	}
	else
	{
		hr = pRegisterDeviceHandleNotification(
			m_hWnd, ndasportHandle, &devNotifyHandle);

		if (FAILED(hr))
		{
			fprintf(stderr, "pRegisterDeviceHandleNotification failed, hr=0x%x\n", hr);
		}
		else
		{
			ATLVERIFY(m_DevNotifyHandles.Add(devNotifyHandle));
			printf("Registered DevNotifyHandle=%p\n", devNotifyHandle);
		}

		CloseHandle(ndasportHandle);
	}

	pEnumerateNdasportLogicalUnits(
		m_DevNotifyHandles,
		m_hWnd);

	return TRUE;
}

void
CPnpEventConsumerWindow::OnDestroy()
{
	int count = m_DevNotifyHandles.GetSize();
	for (int i = 0; i < count; ++i)
	{
		printf("Unregistering DevNotifyHandle=%p\n", m_DevNotifyHandles[i]);
		ATLVERIFY( UnregisterDeviceNotification(m_DevNotifyHandles[i]) );
	}
	SetMsgHandled(FALSE);
	PostQuitMessage(0);
}

void 
CPnpEventConsumerWindow::ReportDeviceChange(PDEV_BROADCAST_HDR DevBroadcast)
{
	switch (DevBroadcast->dbch_devicetype)
	{
	case DBT_DEVTYP_DEVICEINTERFACE:
		{
			printf(" DBT_DEVTYP_DEVICEINTERFACE - ");
			OLECHAR guidString[64] = {0};
			PDEV_BROADCAST_DEVICEINTERFACE bcastDeviceInterface;
			bcastDeviceInterface = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(DevBroadcast);
			printf("%s", bcastDeviceInterface->dbcc_name);
			StringFromGUID2(
				bcastDeviceInterface->dbcc_classguid,
				guidString,
				64);
			printf(" - %ls ", guidString);
		}
		break;
	case DBT_DEVTYP_HANDLE:
		{
			printf("\tDBT_DEVTYP_HANDLE\n");
			PDEV_BROADCAST_HANDLE bcastHandle;
			bcastHandle = reinterpret_cast<PDEV_BROADCAST_HANDLE>(DevBroadcast);
		}
		break;
	case DBT_DEVTYP_OEM:
		{
			printf("\tDBT_DEVTYP_OEM\n");
			PDEV_BROADCAST_OEM bcastOem;
			bcastOem = reinterpret_cast<PDEV_BROADCAST_OEM>(DevBroadcast);
		}
		break;
	case DBT_DEVTYP_PORT:
		{
			printf("\tDBT_DEVTYP_PORT\n");
			PDEV_BROADCAST_PORT bcastPort;
			bcastPort = reinterpret_cast<PDEV_BROADCAST_PORT>(DevBroadcast);
		}
		break;
	case DBT_DEVTYP_VOLUME:
		{
			printf("\tDBT_DEVTYP_VOLUME\n");
			PDEV_BROADCAST_VOLUME bcastVolume;
			bcastVolume = reinterpret_cast<PDEV_BROADCAST_VOLUME>(DevBroadcast);
		}
		break;
	}
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, PDEV_BROADCAST_DEVICEINTERFACE Dbcc)
{
	switch (EventType)
	{
	case DBT_DEVICEARRIVAL:
		{
			if (IsEqualGUID(GUID_DEVINTERFACE_NDASPORT, Dbcc->dbcc_classguid) ||
				pIsNdasPortLogicalUnitInterfaceGuid(&Dbcc->dbcc_classguid))
			{
				HANDLE h = CreateFile(
					Dbcc->dbcc_name,
					GENERIC_READ,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_DEVICE,
					NULL);
				if (INVALID_HANDLE_VALUE == h)
				{
					fprintf(stderr, "CreateFile(%s) failed, error=0x%X\n", 
						Dbcc->dbcc_name,
						GetLastError());
				}
				else
				{
					if (IsEqualGUID(GUID_DEVINTERFACE_NDASPORT, Dbcc->dbcc_classguid) ||
						pIsNdasPortLogicalUnit(h))
					{
						HDEVNOTIFY devNotifyHandle;
						HRESULT hr = pRegisterDeviceHandleNotification(m_hWnd, h, &devNotifyHandle);
						if (FAILED(hr))
						{
							fprintf(stderr, "pRegisterDeviceHandleNotification failed, error=0x%X\n", GetLastError());
						}
						else
						{
							ATLVERIFY( m_DevNotifyHandles.Add(devNotifyHandle) );
							printf("Registered DevNotifyHandle=%p, %s\n", 
								devNotifyHandle,
								Dbcc->dbcc_name);
						}
					}
					CloseHandle(h);
				}
			}
		}
	}
	return TRUE;
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, PDEV_BROADCAST_HANDLE Dbch)
{
	switch (EventType)
	{
	case DBT_DEVICEREMOVECOMPLETE:
		{
			ATLVERIFY( m_DevNotifyHandles.Remove(Dbch->dbch_hdevnotify) );
			printf("Unregistering DevNotifyHandle=%p\n", Dbch->dbch_hdevnotify);
			ATLVERIFY( UnregisterDeviceNotification(Dbch->dbch_hdevnotify) );
		}
		break;
	case DBT_CUSTOMEVENT:

		if (IsEqualGUID(Dbch->dbch_eventguid, GUID_NDASPORT_PNP_NOTIFICATION))
		{
			PNDASPORT_PNP_NOTIFICATION ndasportNotification = 
				reinterpret_cast<PNDASPORT_PNP_NOTIFICATION>(Dbch->dbch_data);
			OnNdasPortEvent(ndasportNotification);
		}
		else if (IsEqualGUID(Dbch->dbch_eventguid, GUID_NDAS_ATA_LINK_EVENT))
		{
			PNDAS_ATA_LINK_EVENT ndasAtaLinkEvent =
				reinterpret_cast<PNDAS_ATA_LINK_EVENT>(Dbch->dbch_data);
			OnNdasAtaLinkEvent(ndasAtaLinkEvent);
		}
		else if (IsEqualGUID(Dbch->dbch_eventguid, GUID_IO_VOLUME_MOUNT))
		{
			// printf("Volume is mounted, handle=%p\n", Dbch->dbch_handle);
		}
		else
		{
			OLECHAR guidString[64] = {0};
			StringFromGUID2(
				Dbch->dbch_eventguid,
				guidString,
				64);

			if (-1 != Dbch->dbch_nameoffset)
			{
				printf("%s (%ls)",
					((PUCHAR)Dbch) + Dbch->dbch_nameoffset,
					guidString);
			}
			else
			{
				printf("%ls", guidString);
			}
		}
	}
	return TRUE;
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, PDEV_BROADCAST_OEM Dbco)
{
	return TRUE;
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, PDEV_BROADCAST_PORT Dbcp)
{
	return TRUE;
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, PDEV_BROADCAST_VOLUME Dbcv)
{
	return TRUE;
}

BOOL 
CPnpEventConsumerWindow::OnDeviceChange(
	UINT EventType, _DEV_BROADCAST_USERDEFINED* Dbcu)
{
	return TRUE;
}

BOOL
CPnpEventConsumerWindow::OnDeviceChange(UINT EventType, LPVOID EventData)
{
	switch (EventType)
	{
	case DBT_CONFIGCHANGECANCELED:
		printf("DBT_CONFIGCHANGECANCELED\n");
		// no data
		break;
	case DBT_CONFIGCHANGED:
		printf("DBT_CONFIGCHANGED\n");
		// no data
		break;
	case DBT_DEVNODES_CHANGED:
		// printf("DBT_DEVNODES_CHANGED\n");
		// no data
		break;
	case DBT_QUERYCHANGECONFIG:
		printf("DBT_QUERYCHANGECONFIG\n");
		// no data
		break;
	case DBT_CUSTOMEVENT:
	case DBT_DEVICEARRIVAL:
	case DBT_DEVICEQUERYREMOVE:
	case DBT_DEVICEQUERYREMOVEFAILED:
	case DBT_DEVICEREMOVECOMPLETE:
	case DBT_DEVICEREMOVEPENDING:
	case DBT_DEVICETYPESPECIFIC:
		{
			PDEV_BROADCAST_HDR dbc = static_cast<PDEV_BROADCAST_HDR>(EventData);
			switch (dbc->dbch_devicetype)
			{
			case DBT_DEVTYP_DEVICEINTERFACE:
				{
					PDEV_BROADCAST_DEVICEINTERFACE dbcc = 
						reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(dbc);
					return OnDeviceChange(EventType, dbcc);
				}
			case DBT_DEVTYP_HANDLE:
				{
					PDEV_BROADCAST_HANDLE dbch = 
						reinterpret_cast<PDEV_BROADCAST_HANDLE>(dbc);
					return OnDeviceChange(EventType, dbch);
				}
			case DBT_DEVTYP_OEM:
				{
					PDEV_BROADCAST_OEM dbco = 
						reinterpret_cast<PDEV_BROADCAST_OEM>(dbc);
					return OnDeviceChange(EventType, dbco);
				}
			case DBT_DEVTYP_PORT:
				{
					PDEV_BROADCAST_PORT dbcp = 
						reinterpret_cast<PDEV_BROADCAST_PORT>(dbc);
					return OnDeviceChange(EventType, dbcp);
				}
			case DBT_DEVTYP_VOLUME:
				{
					PDEV_BROADCAST_VOLUME dbcv = 
						reinterpret_cast<PDEV_BROADCAST_VOLUME>(dbc);
					return OnDeviceChange(EventType, dbcv);
				}
			}
		}
		break;
	case DBT_USERDEFINED:
		{
			_DEV_BROADCAST_USERDEFINED* dbcu = 
				static_cast<_DEV_BROADCAST_USERDEFINED*>(EventData);
			return OnDeviceChange(EventType, dbcu);
		}
		break;
	default:
		printf("unknown event\n");
	}
	return TRUE;
}

