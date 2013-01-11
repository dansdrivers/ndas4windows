#include "stdafx.h"
#include <commctrl.h>
#include <process.h>
#include <regstr.h>
#include <initguid.h>
#include <devguid.h>
#include <winioctl.h>
#include <strsafe.h>
#include <xdbgprn.h>
#include "devupdate.h"
#include "ndasddus.h"

static
LRESULT CALLBACK 
DebugWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static
unsigned int __stdcall DebugWndThreadProc(void * pArg);

static BOOL CALLBACK 
ProcessUnconfigedDevice(LPCTSTR devInstID, LPVOID lpContext);

static BOOL
MonitorNDASKeyChange(HANDLE hKeyChangeEvent, HANDLE hStopEvent);

static BOOL
MonitorEnumKeyChange(HANDLE hKeyChangeEvent, HANDLE hStopEvent);

static BOOL
IsDeviceInstallInProgress(VOID);

//////////////////////////////////////////////////////////////////////////
//
BOOL 
CNdasDDUServiceInstaller::_PostInstall(SC_HANDLE hSCService)
{
	// Post Install Tasks
	return TRUE; // none
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasDDUService Member Function Implementations
//

CNdasDDUService::CNdasDDUService() :
	CService(NDASDEVU_SERVICE_NAME, NDASDEVU_DISPLAY_NAME),
	m_hDevNotify(NULL),
	m_hTaskThread(NULL),
	m_hStopTask(NULL)
{
	CService::s_pServiceInstance = this;
}

CNdasDDUService::~CNdasDDUService()
{
}

unsigned int __stdcall 
CNdasDDUService::_ServiceThreadProc(void* pArg)
{
	CNdasDDUService* pService = reinterpret_cast<CNdasDDUService*>(pArg);
	DWORD dwRet = pService->OnTaskStart();
	_endthreadex(dwRet);
	return dwRet;
}

VOID 
CNdasDDUService::StartTask()
{
	unsigned int threadId;
	m_hTaskThread = (HANDLE) 
		_beginthreadex(NULL, 0, _ServiceThreadProc, this, 0, &threadId);
}

DWORD
CNdasDDUService::OnTaskStart()
{
	//
	// PNP Message DEV_NODECHAGE is not sent to the service
	// We hacks monitoring the registry key change to detect it!
	//

	HANDLE hKeyChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	while (TRUE)
	{
		BOOL fSuccess = MonitorNDASKeyChange(hKeyChangeEvent, m_hStopTask);

		if (WAIT_OBJECT_0 == ::WaitForSingleObject(m_hStopTask, 0))
		{
			if (m_hStopTask) :: CloseHandle(m_hStopTask);
			if (m_hDevNotify) ::UnregisterDeviceNotification(m_hDevNotify);

			DebugPrint(1, _T("Service is stopped\n"));

			(VOID) ReportStatusToSCMgr(SERVICE_STOPPED);
			return 0;
		}

		//
		// Check Unconfigured NDAS Devices
		// Doing this anyway does no harm!
		//

		DebugPrint(1, _T("Finding unconfigured NDAS devices...\n"));

		::NdasDiEnumUnconfigedDevices(
			_T("NDAS\\SCSIAdapter_R01"), 
			ProcessUnconfigedDevice, 
			NULL);
	}

}

VOID 
CNdasDDUService::ServiceMain(DWORD dwArgc, LPTSTR* lpArgs)
{

	ReportStatusToSCMgr(SERVICE_START_PENDING, 0, GetLastError());

#ifdef NDAS_DDUS_USE_DEVICE_EVENTS

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = {0};
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid = StoragePortClassGuid;

#define DEVICE_NOTIFY_ALL_INTERFACE_CLASSES  0x00000004

	m_hDevNotify = ::RegisterDeviceNotification(
		m_sshStatusHandle, 
		&NotificationFilter,
		DEVICE_NOTIFY_SERVICE_HANDLE |
		DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);

	_ASSERTE(m_hDevNotify);

#endif

	m_hStopTask = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	DebugPrint(1, _T("Starting service task thread...\n"));

	StartTask();

	ReportStatusToSCMgr(SERVICE_RUNNING, 0, GetLastError());
}

DWORD
CNdasDDUService::OnServiceShutdown()
{
	(VOID) ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	::SetEvent(m_hStopTask);
	return NO_ERROR;
}

DWORD
CNdasDDUService::OnServiceStop()
{
	(VOID) ReportStatusToSCMgr(SERVICE_STOP_PENDING);
	::SetEvent(m_hStopTask);
	return NO_ERROR;
}

VOID 
CNdasDDUService::ServiceDebug(DWORD dwArgc, LPTSTR* lpArgs)
{
	unsigned int threadId;
	HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, DebugWndThreadProc, this, 0, &threadId);

	unsigned int threadId2;
	m_hTaskThread = (HANDLE) 
		_beginthreadex(NULL, 0, _ServiceThreadProc, this, 0, &threadId2);

	::WaitForSingleObjectEx(hThread, INFINITE, TRUE);
	::SetEvent(m_hStopTask);
	::WaitForSingleObjectEx(m_hTaskThread, INFINITE, TRUE);
}

DWORD
CNdasDDUService::OnServiceDeviceEvent(
	DWORD dwEventType, 
	LPVOID lpEventData)
{
	LRESULT lResult = OnDeviceEvent((WPARAM)dwEventType, (LPARAM)lpEventData);
	return (lResult == TRUE) ? NO_ERROR : (DWORD)lResult;
}

void CNdasDDUService::OnDevNodesChanged()
{
#ifdef NDAS_DDUS_USE_DEVICE_EVENTS

	if (IsDeviceInstallInProgress())
	{
		DebugPrint(1, _T("Warning! Device Installation is in progress!\n"));
		DebugPrint(1, _T("Sleeping 1 sec.\n"));
		::Sleep(1000);
	}

	::NdasDiEnumUnconfigedDevices(
			_T("NDAS\\SCSIAdapter_R01"), 
			ProcessUnconfigedDevice, 
			NULL);
#endif
}


//////////////////////////////////////////////////////////////////////////

BOOL 
CALLBACK
ProcessUnconfigedDevice(LPCTSTR devInstID, LPVOID lpContext)
{
	DebugPrint(1, _T("Un-configured device: %s\n"), devInstID);

	if (IsDeviceInstallInProgress())
	{
		DebugPrint(1, _T("Warning! Device Installation is in progress!\n"));
		DebugPrint(1, _T("Sleeping 1 sec.\n"));
		::Sleep(1000);
	}

	NdasDiInstallDeviceDriver(
		&GUID_DEVCLASS_SCSIADAPTER, 
		devInstID, 
		_T("C:\\Program Files\\NDAS\\Drivers\\ndasscsi.inf"));

	// We need a gap!
	::Sleep(1000);

	return TRUE;
}

BOOL
MonitorNDASKeyChange(HANDLE hKeyChangeEvent, HANDLE hStopEvent)
{
	LONG lResult;
	BOOL fSuccess;

	HKEY hNDASKey;
	lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		REGSTR_PATH_SYSTEMENUM _T("\\NDAS"),
		0,
		KEY_NOTIFY | KEY_READ | KEY_ENUMERATE_SUB_KEYS,
		&hNDASKey);

	if (ERROR_SUCCESS != lResult)
	{
		// maybe Enum\NDAS is not created yet!
		// wait for ENUM to be changed!
		return MonitorEnumKeyChange(hKeyChangeEvent, hStopEvent);
	}

	::ResetEvent(hKeyChangeEvent);

	lResult = ::RegNotifyChangeKeyValue(
		hNDASKey,
		TRUE, // watch for sub-tree
		REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME,
		hKeyChangeEvent,
		TRUE);

	if (ERROR_SUCCESS != lResult)
	{
		DebugPrintErr(lResult, _T("RegNotifyChangeKeyValue(%s) failed"), 
			REGSTR_PATH_SYSTEMENUM _T("\\NDAS"));
		::RegCloseKey(hNDASKey);
		return FALSE;
	}

	HANDLE hWaitingEvents[] = { hStopEvent, hKeyChangeEvent };
	DWORD dwWaitResult = ::WaitForMultipleObjects(
		RTL_NUMBER_OF(hWaitingEvents),
		hWaitingEvents,
		FALSE,
		INFINITE);

	if (WAIT_OBJECT_0 != dwWaitResult && WAIT_OBJECT_0 + 1 != dwWaitResult) 
	{
		DebugPrintLastErr(_T("Wait failed!"));
		::RegCloseKey(hNDASKey);
		return FALSE;
	}

	DebugPrint(1, _T("Changes in %s notified.\n"),
		REGSTR_PATH_SYSTEMENUM _T("\\NDAS"));

	::RegCloseKey(hNDASKey);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

BOOL
MonitorEnumKeyChange(HANDLE hKeyChangeEvent, HANDLE hStopEvent)
{
	LONG lResult;
	HKEY hEnumKey;
	BOOL fSuccess;

	lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		REGSTR_PATH_SYSTEMENUM,
		0, KEY_NOTIFY, &hEnumKey);

	if (ERROR_SUCCESS != lResult) 
	{
		DebugPrintErr(lResult, _T("RegOpenKeyEx(%s) failed"), REGSTR_PATH_SYSTEMENUM);
		return FALSE;
	}

	fSuccess = ::ResetEvent(hKeyChangeEvent);
	_ASSERTE(fSuccess);

	// Only we monitors the add or delete of ENUM's subkey only.
	lResult = ::RegNotifyChangeKeyValue(
		hEnumKey,
		FALSE,
		REG_NOTIFY_CHANGE_NAME,
		hKeyChangeEvent, 
		TRUE);

	if (ERROR_SUCCESS != lResult)
	{
		DebugPrintErr(lResult, _T("RegNotifyChangeKeyValue(%s) failed"), REGSTR_PATH_SYSTEMENUM);
		::RegCloseKey(hEnumKey);
		return FALSE;
	}

	DebugPrint(1, _T("Waiting for changes in %s.\n"), REGSTR_PATH_SYSTEMENUM);

	HANDLE hWaitingEvents[] = { hStopEvent, hKeyChangeEvent };
	DWORD dwWaitResult = ::WaitForMultipleObjects(
		RTL_NUMBER_OF(hWaitingEvents),
		hWaitingEvents,
		FALSE,
		INFINITE);

	if (WAIT_OBJECT_0 != dwWaitResult && WAIT_OBJECT_0 + 1 != dwWaitResult) 
	{
		DebugPrintErrEx(_T("Wait failed!"));
		::RegCloseKey(hEnumKey);
		return FALSE;
	}

	DebugPrint(1, _T("Changes in %s notified.\n"), REGSTR_PATH_SYSTEMENUM);

	::RegCloseKey(hEnumKey);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

BOOL
IsDeviceInstallInProgress(VOID)
{
	typedef DWORD (WINAPI *CMP_WAITNOPENDINGINSTALLEVENTS_PROC)(IN DWORD dwTimeout);

	CMP_WAITNOPENDINGINSTALLEVENTS_PROC pCMP_WaitNoPendingInstallEvents;

	HMODULE hModule = GetModuleHandle(TEXT("setupapi.dll"));
	if(!hModule)
	{
		// Should never happen since we're linked to SetupAPI, but...
		_ASSERTE(FALSE);
		return FALSE;
	}

	pCMP_WaitNoPendingInstallEvents = 
		(CMP_WAITNOPENDINGINSTALLEVENTS_PROC)GetProcAddress(hModule,
		"CMP_WaitNoPendingInstallEvents");
	if(!pCMP_WaitNoPendingInstallEvents)
	{
		// We're running on a release of the operating system that doesn't supply this function.
		// Trust the operating system to suppress Autorun when appropriate.
		return FALSE;
	}
	return (pCMP_WaitNoPendingInstallEvents(0) == WAIT_TIMEOUT);
}

//////////////////////////////////////////////////////////////////////////


unsigned int _stdcall DebugWndThreadProc(void * pArg)
{
	//
	// A hidden window for device notification
	// (Message-only windows cannot receive WM_BROADCAST)
	//

	CNdasDDUService* pService = reinterpret_cast<CNdasDDUService*>(pArg);

	WNDCLASSEX wcex = {0};
	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)DebugWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= (HINSTANCE) ::GetModuleHandle(NULL);
	wcex.hIcon			= ::LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor		= ::LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH) ::GetStockObject(WHITE_BRUSH);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= _T("NDAS_DDUS_Debug_Window");

	::RegisterClassEx(&wcex);

	HWND hWnd;

	hWnd = ::CreateWindowEx(
		0,
		_T("NDAS_DDUS_Debug_Window"),
		NULL, 
		WS_EX_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		NULL,
		(HINSTANCE) ::GetModuleHandle(NULL),
		NULL);

	DebugPrint(1, _T("Debug Window Created!"));

	if (hWnd == NULL) {
		_ASSERT(FALSE && "Creating Debug Window failed.\n");
	};

#pragma warning(disable: 4244)
	::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pService);
#pragma warning(default: 4244)

	//	::ShowWindow(hWnd, SW_SHOW);
	//	::UpdateWindow(hWnd);

	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	_endthreadex(0);
	return 0;
}

LRESULT CALLBACK 
DebugWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndEdit;

	switch (uMsg) 
	{ 
	case WM_CREATE:
		break;
	case WM_DEVICECHANGE:
		{
#ifdef NDAS_DDUS_USE_DEVICE_EVENTS
			CNdasDDUService* pService = 
				(CNdasDDUService*) ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
			return pService ? pService->OnDeviceEvent(wParam, lParam) : TRUE;
#endif
		}
	} 
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam); 
}
