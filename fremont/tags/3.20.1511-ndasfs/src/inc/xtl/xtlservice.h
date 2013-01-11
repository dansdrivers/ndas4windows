#pragma once
#include <windows.h>
#include <tchar.h>
#pragma warning(disable: 4995)
#include <shlwapi.h>
#pragma warning(default: 4995)

#include "xtldef.h"
#include "xtlautores.h"

namespace XTL
{

inline BOOL 
XtlInstallService(
	LPCTSTR ServiceName,
	LPCTSTR ServiceDisplayName,
	DWORD ServiceType,
	LPCTSTR BinaryPathName = NULL,
	DWORD StartType = SERVICE_DEMAND_START, 
	DWORD ErrorControl = SERVICE_ERROR_NORMAL,
	LPCTSTR LoadOrderGroup = _T(""),
	LPCTSTR Dependencies = _T(""),
	LPCTSTR ServiceStartName = NULL,
	LPCTSTR Password = _T(""));

inline BOOL 
XtlRemoveService(LPCTSTR ServiceName);

template <DWORD t_dwServiceType, DWORD t_dwControlsAccepted> 
class ServiceTraits
{
public:
	static DWORD GetServiceType()
	{
		return t_dwServiceType;
	}
	static DWORD GetControlsAccepted()
	{
		return t_dwControlsAccepted;
	}
};

// Windows XP or later
#ifndef SERVICE_ACCEPT_SESSIONCHANGE
#define SERVICE_ACCEPT_SESSIONCHANGE 0x00000080
#endif

const DWORD DefaultServiceAccept = 	
	SERVICE_ACCEPT_PARAMCHANGE |
	SERVICE_ACCEPT_PAUSE_CONTINUE |
	SERVICE_ACCEPT_SHUTDOWN |
	SERVICE_ACCEPT_STOP |
	SERVICE_ACCEPT_HARDWAREPROFILECHANGE |
	SERVICE_ACCEPT_POWEREVENT;
	/* SERVICE_ACCEPT_SESSIONCHANGE; */

typedef ServiceTraits<
	SERVICE_WIN32_OWN_PROCESS,
	DefaultServiceAccept> DefaultServiceTraits;

typedef ServiceTraits<
	SERVICE_WIN32_SHARE_PROCESS,
	DefaultServiceAccept> DefaultSharedServiceTraits;

typedef ServiceTraits<
	SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
	DefaultServiceAccept> DefaultInteractiveServiceTraits;

typedef ServiceTraits<
	SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS,
	DefaultServiceAccept> DefaultInteractiveSharedServiceTraits;

template <typename T, class TServiceTraits = DefaultServiceTraits> class CService;

//template <typename T>
//class CServiceHost
//{
//public:
//
//	enum RunMode 
//	{
//		RunModeDebug,
//		RunModeService,
//		RunModeInstall,
//		RunModeRemove
//	};
//
//	RunMode GetRunMode(int argc, TCHAR** argv)
//	{
//		if (argc > 1 && _T('-') == argv[1][0] || _T('/') == argv[1][0])
//		{
//			LPCTSTR option = &argv[1][1];
//			if (0 == ::lstrcmpi("debug", option))
//			{
//				return RunModeDebug;
//			}
//			else if (0 == ::lstrcmpi(_T("install"), option))
//			{
//				return RunModeInstall;
//			}
//			else if (0 == ::lstrcmpi(_T("remove"), option)) ||
//				0 == ::lstrcmpi(_T("uninstall"))
//			{
//				return RunModeRemove;
//			}
//		}
//		return RunModeInstall;
//	}
//
//	public int Run(int argc, TCHAR** argv)
//	{
//		T* pT = static_cast<T*>(this);
//		RunMode runMode = pT->GetRunMode(argc, argv);
//		switch (runMode)
//		{
//		case RunModeInstall: 
//			return pT->ServiceHostInstall(argc, argv);
//		case RunModeRemove: 
//			return pT->ServiceHostUninstall(argc, argv);
//		case RunModeDebug: 
//			return pT->ServiceHostDebug(argc, argv);
//		case RunModeService: 
//		default:
//			return pT->ServiceHostStart(argc, argv);
//		}
//	}
//
//	public int ServiceHostInstall(int argc, TCHAR** argv)
//	{
//
//	}
//	public int ServiceHostUninstall(int argc, TCHAR** argv)
//	{
//
//	}
//	public int ServiceHostDebugStart(int argc, TCHAR** argv)
//	{
//
//	}
//	public int ServiceHostStart(int argc, TCHAR** argv)
//	{
//		T* pService = new T();
//		static const DWORD nServiceClasses = sizeof(ServiceClasses) / sizeof(ServiceClasses[0]) - 1;
//		SERVICE_TABLE_ENTRY ServiceTable[2] = {0};
//		T::GetServiceTableEntry(&ServiceTable[0]);
//		::StartServiceCtrlDispatcher(ServiceTable);
//		delete pService;
//	}
//	public int GetServiceTables()
//	{
//	}
//	typedef (void *GETSERVICETABLEENTRYPROC)(LPSERVICE_TABLE_ENTRY lpEntry);
//	const GETSERVICETABLEENTRYPROC* ServiceClasses
//#define BEGIN_SERVICE_CLASSES() \
//	const GETSERVICETABLEENTRYPROC* ServiceClasses [] = {
//#define SERVICE_CLASS(ClassName) \
//		ClassName::GetServiceTableEntry,
//#define END_SERVICE_CLASSES() \
//	}; \
//	const DWORD NumberOfServiceClasses = 
//};

//class CNdasServiceHost : public XTL::CServiceHost<CNdasServiceHost>
//{
//public:
//	public DWORD GetServiceTables()
//};

template <typename T, class TServiceTraits>
class CService : public TServiceTraits
{
protected:

	SERVICE_STATUS_HANDLE m_hService;
	SERVICE_STATUS m_Status;

	HANDLE m_hServiceCleanup;

	// Message-only Windows Handle for Debugging
	HWND m_hDebugWnd;
	bool m_bDebugMode;
	AutoObjectHandle m_hDebugWindowCreated;
	AutoObjectHandle m_hServiceStopped;

public:

	typedef CService<T, TServiceTraits> ThisClass;

	CService() : m_hService(0), m_bDebugMode(false), m_hDebugWnd(NULL), m_hServiceCleanup(NULL) {}
	~CService() {}

	DWORD ServiceCtrlHandlerEx(
		DWORD dwControl, 
		DWORD dwEventType, 
		LPVOID lpEventData);


	BOOL SetServiceStatus(LPSERVICE_STATUS lpServiceStatus)
	{
		if (m_bDebugMode) return TRUE;
		return ::SetServiceStatus(m_hService, lpServiceStatus);
	}

	BOOL SetServiceBits(DWORD dwServiceBits, BOOL bSetBitsOn, BOOL bUpdateImmediately)
	{
		if (m_bDebugMode) return TRUE;
		return ::SetServiceBits(m_hService, dwServiceBits, bSetBitsOn, bUpdateImmediately);
	}

	//
	// Report Functions
	//
	BOOL ReportServicePaused()
	{
		T* pT = static_cast<T*>(this);
		m_Status.dwCurrentState = SERVICE_PAUSED;
		m_Status.dwWaitHint = 0;
		m_Status.dwCheckPoint = 0;
		return pT->SetServiceStatus(&m_Status);
	}

	BOOL ReportServiceRunning()
	{
		T* pT = static_cast<T*>(this);
		m_Status.dwCurrentState = SERVICE_RUNNING;
		m_Status.dwCheckPoint = 0;
		m_Status.dwWaitHint = 0;
		return pT->SetServiceStatus(&m_Status);
	}

	BOOL ReportServiceStopped(DWORD Win32ExitCode = NO_ERROR, DWORD ServiceExitCode = NO_ERROR)
	{
		XTLASSERT(
			ServiceExitCode == NO_ERROR ||
			(ServiceExitCode != NO_ERROR && Win32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR));
		T* pT = static_cast<T*>(this);
		m_Status.dwCurrentState = SERVICE_STOPPED;
		m_Status.dwWaitHint = 0;
		m_Status.dwCheckPoint = 0;
		m_Status.dwWin32ExitCode = Win32ExitCode;
		m_Status.dwServiceSpecificExitCode = ServiceExitCode;
		BOOL ret = pT->SetServiceStatus(&m_Status);
		if (ret)
		{
			XTLVERIFY( ::SetEvent(m_hServiceStopped) );
		}
		return ret;
	}

	BOOL ReportServiceContinuePending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1)
	{
		T* pT = static_cast<T*>(this);
		if (m_Status.dwCurrentState != SERVICE_CONTINUE_PENDING)
		{
			m_Status.dwCheckPoint = 0;
		}
		m_Status.dwCurrentState = SERVICE_CONTINUE_PENDING;
		m_Status.dwCheckPoint += CheckPointIncrement;
		m_Status.dwWaitHint = WaitHint;
		return pT->SetServiceStatus(&m_Status);
	}

	BOOL ReportServicePausePending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1)
	{
		T* pT = static_cast<T*>(this);
		if (m_Status.dwCurrentState != SERVICE_PAUSE_PENDING)
		{
			m_Status.dwCheckPoint = 0;
		}
		m_Status.dwCurrentState = SERVICE_PAUSE_PENDING;
		m_Status.dwCheckPoint += CheckPointIncrement;
		m_Status.dwWaitHint = WaitHint;
		return pT->SetServiceStatus(&m_Status);
	}

	BOOL ReportServiceStartPending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1)
	{
		T* pT = static_cast<T*>(this);
		if (m_Status.dwCurrentState != SERVICE_START_PENDING)
		{
			m_Status.dwCheckPoint = 0;
		}
		m_Status.dwCurrentState = SERVICE_START_PENDING;
		m_Status.dwCheckPoint += CheckPointIncrement;
		m_Status.dwWaitHint = WaitHint;
		return pT->SetServiceStatus(&m_Status);
	}

	BOOL ReportServiceStopPending(DWORD WaitHint = 1000, DWORD CheckPointIncrement = 1)
	{
		T* pT = static_cast<T*>(this);
		if (m_Status.dwCurrentState != SERVICE_STOP_PENDING)
		{
			m_Status.dwCheckPoint = 0;
		}
		m_Status.dwCurrentState = SERVICE_STOP_PENDING;
		m_Status.dwCheckPoint += CheckPointIncrement;
		m_Status.dwWaitHint = WaitHint;
		return pT->SetServiceStatus(&m_Status);
	}

	//
	// Use this function to get ServiceTableEntry
	//
	static void GetServiceTableEntry(LPSERVICE_TABLE_ENTRY lpEntry)
	{
		lpEntry->lpServiceName = const_cast<LPTSTR>(T::GetServiceName());
		lpEntry->lpServiceProc = T::ServiceMain;
	}
	static DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
	{
		T* pT = static_cast<T*>(lpContext);
		return pT->ServiceCtrlHandlerEx(dwControl, dwEventType, lpEventData);
	}
	static T* CreateInstance()
	{
		static T staticInstance;
		return &staticInstance;
	}
	static void DestroyInstance(T* /*pInstance*/)
	{
		return;
	}
	void ServiceCleanup(BOOLEAN TimerOrWaitFired)
	{
		XTLASSERT(!TimerOrWaitFired); TimerOrWaitFired;
		XTLASSERT(m_hServiceCleanup != NULL);
		HANDLE hWaitHandle = m_hServiceCleanup;
		T* pT = static_cast<T*>(this);
		T::DestroyInstance(pT);
		XTLVERIFY( ::UnregisterWaitEx(hWaitHandle, NULL) ||
			ERROR_IO_PENDING == ::GetLastError());
	}
	static void CALLBACK ServiceCleanupProc(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
	{
		T* pT = static_cast<T*>(lpParameter);
		pT->ServiceCleanup(TimerOrWaitFired);
	}
	static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
	{
		T* pT = T::CreateInstance();
		if (!pT)
		{
			return;
		}
		pT->m_hServiceStopped = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (pT->m_hServiceStopped.IsInvalid())
		{
			return;
		}
		// The service status handle does not have to be closed.
		SERVICE_STATUS_HANDLE ssh = ::RegisterServiceCtrlHandlerEx(
			T::GetServiceName(), 
			T::HandlerEx,
			pT);
		if (NULL == ssh)
		{
			return;
		}
		// SERVICE_STATUS_HANDLE initialization
		pT->m_hService = ssh;
		// SERVICE_STATUS initialization
		pT->m_Status.dwServiceType = TServiceTraits::GetServiceType();
		pT->m_Status.dwControlsAccepted = TServiceTraits::GetControlsAccepted();
		pT->m_Status.dwCurrentState = SERVICE_START_PENDING;
		pT->m_Status.dwWin32ExitCode = NO_ERROR;
		pT->m_Status.dwServiceSpecificExitCode = NO_ERROR;
		pT->m_Status.dwCheckPoint = 0;
		pT->m_Status.dwWaitHint = 0;
		// Initial service status (SERVICE_START_PENDING)
		pT->SetServiceStatus(&pT->m_Status);
		if (pT->ServiceStart(argc, argv))
		{
			// ServiceStart returns immediately after starting the service
			XTLVERIFY( ::RegisterWaitForSingleObject(
				&pT->m_hServiceCleanup, 
				pT->m_hServiceStopped, 
				ServiceCleanupProc, 
				pT, 
				INFINITE, 
				WT_EXECUTEONLYONCE) );
		}
		else
		{
			pT->ReportServiceStopped(::GetLastError());
		}
	}

	static int WINAPI ServiceDebugMain(DWORD argc, LPTSTR* argv)
	{
		// XTLCALLTRACE();
		T* pT = T::CreateInstance();
		if (!pT)
		{
			return ::GetLastError();
		}
		pT->m_hServiceStopped = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (pT->m_hServiceStopped.IsInvalid())
		{
			return ::GetLastError();
		}

		pT->m_bDebugMode = true;
		pT->m_hService = NULL;
		pT->m_Status.dwServiceType = TServiceTraits::GetServiceType();
		pT->m_Status.dwControlsAccepted = TServiceTraits::GetControlsAccepted();
		pT->m_Status.dwCurrentState = SERVICE_START_PENDING;
		pT->m_Status.dwWin32ExitCode = NO_ERROR;
		pT->m_Status.dwServiceSpecificExitCode = NO_ERROR;
		pT->m_Status.dwCheckPoint = 0;
		pT->m_Status.dwWaitHint = 0;

		XTLVERIFY(::SetConsoleCtrlHandler(T::ConsoleDebugCtrlHandler, TRUE));

		Debug::SetServiceInstance(pT);

		pT->m_hDebugWindowCreated = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		XTLASSERT(!pT->m_hDebugWindowCreated.IsInvalid());

		DWORD dwThreadId;
		AutoObjectHandle hThread = ::CreateThread(0,0,Debug::DebugThreadStart,pT,0,&dwThreadId);
		XTLASSERT(!hThread.IsInvalid());

		if (pT->ServiceStart(argc, argv))
		{
			XTLVERIFY(::SetConsoleCtrlHandler(T::ConsoleDebugCtrlHandler, FALSE));

			if (WAIT_OBJECT_0 == WaitForSingleObject(pT->m_hServiceStopped, INFINITE))
			{
				// ignore thread posting error
				::PostThreadMessage(dwThreadId, WM_QUIT, 0, 0);
				while (WAIT_TIMEOUT == ::WaitForSingleObject(hThread, 0))
				{
					// XTLTRACE("Waiting for the thread\n");
					::Sleep(0);
				}
			}
		}

		int ret = (pT->m_Status.dwWin32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR) ?
			pT->m_Status.dwServiceSpecificExitCode :
			pT->m_Status.dwWin32ExitCode;

		T::DestroyInstance(pT);

		return ret;
	}
	static BOOL WINAPI ConsoleDebugCtrlHandler(DWORD dwCtrlType)
	{
		// Only CTRL-C signals the service to stop
		T* pT = Debug::GetServiceInstance();
		switch (dwCtrlType)
		{
		case CTRL_C_EVENT:
			{
				DWORD ret = pT->OnServiceStop();
				// If the service cannot accept stop, we will force 
				// the program to stop
				if (NO_ERROR == ret)
				{
					XTLVERIFY(::FlushConsoleInputBuffer(::GetStdHandle(STD_INPUT_HANDLE)));
					return TRUE;
				}
			}
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		// these signal is received only by services
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
		default:
			return FALSE;
		}
	}

	//
	// Derived class should implement the following functions
	//
	BOOL ServiceStart(DWORD argc, LPTSTR* argv)
	{
		XTLC_ASSERT(FALSE && "Derived class should implement ServiceStart");
	}

	static LPCTSTR GetServiceName()
	{
		return _T("");
	}

	static LPCTSTR GetServiceDisplayName()
	{
		return GetServiceName();
	}

	static LPCTSTR GetServiceDescription()
	{
		return GetServiceDisplayName();
	}

	//
	// Standard Handlers
	//
	DWORD OnServiceContinue() { return ERROR_CALL_NOT_IMPLEMENTED; }

	//
	// All services must accept and process the SERVICE_CONTROL_INTERROGATE control code.
	//
	DWORD OnServiceInterrogate() 
	{
		T* pT = static_cast<T*>(this);
		// Report the current service status
		return pT->SetServiceStatus(&m_Status);
	}
	
	DWORD OnServiceParamChange() { return ERROR_CALL_NOT_IMPLEMENTED; }
	DWORD OnServicePause() { return ERROR_CALL_NOT_IMPLEMENTED; }
	DWORD OnServiceShutdown() { return ERROR_CALL_NOT_IMPLEMENTED; }
	DWORD OnServiceStop() { return ERROR_CALL_NOT_IMPLEMENTED; }

	// NETBIND handlers are not supported.
	// Applications should use Plug and Play functionality instead.
	// SERVICE_CONTROL_NETBINDADD
	// SERVICE_CONTROL_NETBINDDISABLE
	// SERVICE_CONTROL_NETBINDENABLE
	// SERVICE_CONTROL_NETBINDREMOVE

	//
	// Extended Control Handlers
	//
	DWORD OnServiceDeviceEvent(DWORD /*dwEventType*/, LPVOID /*lpEventData*/) { return NO_ERROR; }
	DWORD OnServiceHardwareProfileChange(DWORD /*dwEventType*/, LPVOID /*lpEventData*/) { return NO_ERROR; }
	DWORD OnServicePowerEvent(DWORD /*dwEventType*/, LPVOID /*lpEventData*/) { return NO_ERROR; }
	DWORD OnServiceSessionChange(DWORD /*dwEventType*/, LPVOID /*lpEventData*/) { return NO_ERROR; }

	//
	// User-defined Control Handler
	//
	DWORD OnServiceUserControl(DWORD /*dwControl*/, DWORD /*dwEventType*/, LPVOID /*lpEventData*/) 
	{ return ERROR_CALL_NOT_IMPLEMENTED; }

public:

	static BOOL InstallService()
	{
		return XtlInstallService(
			T::GetServiceName(), 
			T::GetServiceDisplayName(), 
			T::GetServiceType());
	}

	static BOOL RemoveService()
	{
		return XtlRemoveService(T::GetServiceName());
	}

private:
	//
	// Do not use this class directly
	// This inner class is to support debugging routines
	//
	class Debug
	{
		// Singleton instance pointer holder
		static void ServiceInstance(T*& pServiceInstance, bool bSet)
		{
			static T* ServiceInstance = NULL;
			if (bSet)
			{
				XTLASSERT(NULL == ServiceInstance);
				ServiceInstance = pServiceInstance;
			}
			else
			{
				XTLASSERT(NULL != ServiceInstance);
				pServiceInstance = ServiceInstance;
			}
		}
	public:
		static void SetServiceInstance(T* pServiceInstance)
		{
			ServiceInstance(pServiceInstance, true);
		}
		static T* GetServiceInstance()
		{
			T* pServiceInstance;
			ServiceInstance(pServiceInstance, false);
			return pServiceInstance;
		}
		static DWORD WINAPI 
		DebugThreadStart(LPVOID pArg)
		{
			//
			// A hidden window for device notification
			// (Message-only windows cannot receive WM_BROADCAST)
			//
			WNDCLASSEX wcex = {0};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_HREDRAW | CS_VREDRAW; 
			wcex.hInstance = ::GetModuleHandle(NULL);
			// wcex.hIcon = ::LoadIcon(NULL, IDI_APPLICATION); 
			wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW); 
			wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); // ::GetStockObject(WHITE_BRUSH));
			wcex.lpfnWndProc	= DebugWndProc;
			wcex.lpszClassName	= TEXT("XTLServiceDebugWindowClass");

			ATOM atCls = ::RegisterClassEx(&wcex);
			XTLASSERT(0 != atCls);

			T* pT = static_cast<T*>(pArg);

			pT->m_hDebugWnd = ::CreateWindowEx(
				0,
				MAKEINTATOM(atCls),
				_T("XTLServiceDebugWindow"), 
				WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
				CW_USEDEFAULT, 0,
				400, 300,
				NULL, NULL, NULL, pT);

			XTLASSERT(NULL != pT->m_hDebugWnd);

			::ShowWindow(pT->m_hDebugWnd, SW_SHOW);
			::UpdateWindow(pT->m_hDebugWnd);

			MSG msg = {0};
			BOOL ret;
			while ((ret = ::GetMessage(&msg, NULL, 0, 0)) != 0) 
			{
				if (-1 == ret)
				{
					XTLASSERT(FALSE && "GetMessage Error");
				}
				else
				{
					::TranslateMessage(&msg);

					if (msg.hwnd == pT->m_hDebugWnd && msg.message == WM_DESTROY)
					{
					}

					::DispatchMessage(&msg);
				}
				if (msg.hwnd == pT->m_hDebugWnd && msg.message == WM_CREATE)
				{
				}
			}
			OutputDebugStringA("WindowThreadStopped\n");
			return 0;
		}
		static HFONT GetSystemFont()
		{    
			NONCLIENTMETRICS ncmetrics = {0};
			ncmetrics.cbSize = sizeof(NONCLIENTMETRICS);    
			// Retrieves the metrics associated with the nonclient area of    
			// nonminimized windows. The pvParam parameter must point to a    
			// NONCLIENTMETRICS structure that receives the information. Set    
			// the cbSize member of this structure and the uiParam parameter    
			// to "sizeof(NONCLIENTMETRICS)".    
			BOOL fSuccess = ::SystemParametersInfo(
				SPI_GETNONCLIENTMETRICS, 0, &ncmetrics, 0);
			if (!fSuccess)
			{        
				return NULL;
			}  
			HFONT hFont = CreateFontIndirect(&ncmetrics.lfMessageFont);
			return hFont;
		}
		static LRESULT CALLBACK 
		DebugWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			static HWND hWndListBox = NULL;
			static HFONT hFont = NULL;
			static T* pT = NULL;
			switch (message) 
			{
			case WM_CREATE: 
				// Initialize the window. 
				{
					LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
					pT = static_cast<T*>(lpcs->lpCreateParams);
					XTLASSERT(NULL != pT);

					hFont = GetSystemFont();
					hWndListBox = ::CreateWindowEx(
						WS_EX_CLIENTEDGE, 
						_T("LISTBOX"), 
						NULL, 
						WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | LBS_DISABLENOSCROLL, 
						CW_USEDEFAULT, 0, 
						CW_USEDEFAULT, 0,
						hWnd, NULL, NULL, NULL);
					::SendMessage(hWndListBox, WM_SETFONT, (WPARAM)hFont, TRUE);
					::SendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("Debug Windows Created"));
				}
				XTLVERIFY( ::SetEvent(pT->m_hDebugWindowCreated) );
				return 0; 
			case WM_SIZE:
				{
					RECT rc; 
					XTLVERIFY(::GetClientRect(hWnd, &rc));
					rc.left += 10; rc.top += 10; rc.right -= 10; rc.bottom -= 10;
					::MoveWindow(hWndListBox, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
				}
				return 0;
			case WM_CLOSE:
				::DestroyWindow(hWnd);
				return 0;
			case WM_DESTROY:
				XTLASSERT(NULL != pT);
				if (pT)
				{
					if (WAIT_TIMEOUT == ::WaitForSingleObject(pT->m_hServiceStopped, 0))
					{
						pT->OnServiceStop();
					}
				}
				pT->m_hDebugWnd = NULL;
				::PostQuitMessage(0);
				return 0; 
			case WM_DEVICECHANGE:
				{
					::SendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("WM_DEVICECHANGE"));
					LRESULT count = ::SendMessage(hWndListBox, LB_GETCOUNT, 0, 0);
					::SendMessage(hWndListBox, LB_SETCURSEL, count - 1, 0); 
				}
				{
					if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
					DWORD dwEventType = static_cast<DWORD>(wParam);
					LPVOID lpEventData = static_cast<LPVOID>(ULongToPtr(lParam));
					DWORD ret = pT->OnServiceDeviceEvent(dwEventType, lpEventData);
					return (NO_ERROR == ret) ? TRUE : BROADCAST_QUERY_DENY;
				}
			case WM_POWERBROADCAST:
				{
					::SendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("WM_POWERBROADCAST"));
					LRESULT count = ::SendMessage(hWndListBox, LB_GETCOUNT, 0, 0);
					::SendMessage(hWndListBox, LB_SETCURSEL, count - 1, 0); 
				}
				{
					if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
					DWORD dwEventType = static_cast<DWORD>(wParam);
					LPVOID lpEventData = static_cast<LPVOID>(ULongToPtr(lParam));
					DWORD ret = pT->OnServicePowerEvent(dwEventType, lpEventData);
					return (NO_ERROR == ret) ? TRUE : BROADCAST_QUERY_DENY;
				}
			case WM_ENDSESSION:
				{
					::SendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("WM_ENDSESSION"));
					LRESULT count = ::SendMessage(hWndListBox, LB_GETCOUNT, 0, 0);
					::SendMessage(hWndListBox, LB_SETCURSEL, count - 1, 0); 
				}
				if (wParam)
				{
					if (lParam & ENDSESSION_LOGOFF)
					{
						if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
						XTLVERIFY(NOERROR == pT->OnServiceShutdown());
					}
					else
					{
						if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
						XTLVERIFY(NOERROR == pT->OnServiceShutdown());
					}
				}
				return 0;
			default:
				return ::DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
	};

};

#define XTL_DECLARE_SERVICE_NAME_RESOURCE(uID) \
	static LPCTSTR GetServiceName() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_DISPLAY_NAME_RESOURCE(uID) \
	static LPCTSTR GetServiceDisplayName() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_DESCRIPTION_RESOURCE(uID) \
	static LPCTSTR GetServiceDescription() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_NAME(ServiceName) \
	static LPCTSTR GetServiceName() { return _T(ServiceName); }
#define XTL_DECLARE_SERVICE_DISPLAY_NAME(DisplayName) \
	static LPCTSTR GetServiceDisplayName() { return _T(DisplayName); }
#define XTL_DECLARE_SERVICE_DESCRIPTION(Description) \
	static LPCTSTR GetServiceDescription() { return _T(Description); }

template <typename T, class TServiceTraits>
inline DWORD
CService<T,TServiceTraits>::ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
{
	T* pT = static_cast<T*>(this);
	switch (dwControl) 
	{
	// Standard Handlers
	case SERVICE_CONTROL_CONTINUE:
		return pT->OnServiceContinue();
	case SERVICE_CONTROL_INTERROGATE:
		return pT->OnServiceInterrogate();
	case SERVICE_CONTROL_PARAMCHANGE:
		return pT->OnServiceParamChange();
	case SERVICE_CONTROL_PAUSE:
		return pT->OnServicePause();
	case SERVICE_CONTROL_SHUTDOWN:
		return pT->OnServiceShutdown();
	case SERVICE_CONTROL_STOP:
		return pT->OnServiceStop();

	// Extended Handlers
	case SERVICE_CONTROL_DEVICEEVENT:
		return pT->OnServiceDeviceEvent(dwEventType, lpEventData);
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		return pT->OnServiceHardwareProfileChange(dwEventType, lpEventData);
	case SERVICE_CONTROL_POWEREVENT:
		return pT->OnServicePowerEvent(dwEventType, lpEventData);
	case SERVICE_CONTROL_SESSIONCHANGE:
		return pT->OnServiceSessionChange(dwEventType, lpEventData);

	// This control code has been deprecated. use PnP functionality instead
	case SERVICE_CONTROL_NETBINDADD:
	case SERVICE_CONTROL_NETBINDDISABLE:
	case SERVICE_CONTROL_NETBINDENABLE:
	case SERVICE_CONTROL_NETBINDREMOVE:
		return ERROR_CALL_NOT_IMPLEMENTED;

	default: // User-defined control code
		if (dwControl >= 128 && dwControl <= 255) 
		{
			return pT->OnServiceUserControl(dwControl, dwEventType, lpEventData);
		}
		else 
		{
			return ERROR_CALL_NOT_IMPLEMENTED;
		}
	}
}

inline BOOL 
XtlInstallService(
	LPCTSTR ServiceName,
	LPCTSTR ServiceDisplayName,
	DWORD ServiceType,
	LPCTSTR BinaryPathName,
	DWORD StartType, 
	DWORD ErrorControl,
	LPCTSTR LoadOrderGroup,
	LPCTSTR Dependencies,
	LPCTSTR ServiceStartName,
	LPCTSTR Password)
{
	TCHAR szPath[MAX_PATH];
	if (NULL == BinaryPathName)
	{
		DWORD nPath = ::GetModuleFileName(NULL, szPath, RTL_NUMBER_OF(szPath));
		XTLENSURE_RETURN_BOOL(0 != nPath);
	}
	else
	{
		::lstrcpyn(szPath, BinaryPathName, RTL_NUMBER_OF(szPath));
	}

	::PathQuoteSpaces(szPath);

	AutoSCHandle hSCManager = ::OpenSCManager(
		NULL,
		SERVICES_ACTIVE_DATABASE,
		SC_MANAGER_CREATE_SERVICE | SC_MANAGER_LOCK);
	if (hSCManager.IsInvalid()) 
	{
		return FALSE;
	}

	AutoSCLock hSCLock = ::LockServiceDatabase(hSCManager);
	if (hSCLock.IsInvalid())
	{
		return FALSE;
	}

	AutoSCHandle hSCService = ::CreateService(
		hSCManager,
		ServiceName,
		ServiceDisplayName,
		SERVICE_CHANGE_CONFIG,
		ServiceType,
		StartType,
		ErrorControl,
		szPath,
		LoadOrderGroup,
		NULL,
		Dependencies,
		ServiceStartName,
		Password);
	if (hSCService.IsInvalid()) 
	{
		return FALSE;
	}
	return TRUE;
}

inline BOOL 
XtlRemoveService(LPCTSTR ServiceName)
{
	AutoSCHandle hSCManager = ::OpenSCManager(
		NULL,
		SERVICES_ACTIVE_DATABASE,
		DELETE | SC_MANAGER_LOCK);
	if (hSCManager.IsInvalid()) 
	{
		return FALSE;
	}

	AutoSCLock hSCLock = ::LockServiceDatabase(hSCManager);
	if (hSCLock.IsInvalid()) 
	{
		return FALSE;
	}

    AutoSCHandle hSCService = ::OpenService(hSCManager, ServiceName, DELETE);

    if (hSCService.IsInvalid()) 
	{
		return FALSE;
	}

	BOOL fSuccess = ::DeleteService(hSCService);
	
	return fSuccess;
	
}

//
// There are problems with the following example advised from the Platform SDK. 
// The service instance is allocated at the stack of ServiceMain
// or ServiceDebugMain. If this thread terminates immediately,
// ServiceMain will also terminate, which subsequently
// delete the instance of the service in the stack.
//
// Maybe if you really want to use thread pool routine for the 
// service cleanup, you should create an instance of the service
// in the heap by overriding ServiceMain and ServiceDebugMain.
// And cleanup routine shall clean the instance later.
//
//
#if 0
	ServiceStart(DWORD argc, LPTSTR* argv) {
		// invoke worker threads
		myworker.Run(m_hServiceStopEvent);
		myworker2.Run(m_hServiceStopEvent);
		BOOL fSuccess = ::RegisterWaitForSingleObject(
			&m_hRegWait,
			m_hServiceStopEvent,
			ServiceCleanupProc,
			this,
			INFINITE,
			WT_EXECUTEONLYONCE | WT_EXECUTELONGFUNCTION);
		if (!fSuccess)
		{
			ReportServiceStopped(2);
		}
	}
#endif


} // namespace XTL
