/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _XIMETA_SVCHELP_H_
#define _XIMETA_SVCHELP_H_

//////////////////////////////////////////////////////////////////////////

class CService;

template <typename T>
class CServiceInstallerT
{
public:

	static BOOL _PostInstall(SC_HANDLE hSCService)
	{
		return TRUE;
	}

	static BOOL InstallService(
		LPCTSTR lpMachineName,
		LPCTSTR lpServiceName,
		LPCTSTR lpDisplayName,
		DWORD dwDesiredAccess,
		DWORD dwServiceType,
		DWORD dwStartType,
		DWORD dwErrorControl,
		LPCTSTR lpBinaryPathName,
		LPCTSTR lpLoadOrderGroup = NULL,
		LPDWORD lpdwTagId = NULL,
		LPCTSTR lpDependencies = NULL,
		LPCTSTR lpServiceStartName = NULL,
		LPCTSTR lpPassword = NULL)
	{
		SC_HANDLE hSCManager = ::OpenSCManager(
			lpMachineName,
			SERVICES_ACTIVE_DATABASE,
			SC_MANAGER_CREATE_SERVICE | SC_MANAGER_LOCK);

		if (NULL == hSCManager) 
		{
			return FALSE;
		}

		SC_LOCK hSCLock = ::LockServiceDatabase(hSCManager);

		if (NULL == hSCLock) 
		{
			::CloseServiceHandle(hSCManager);
			return FALSE;
		}

		SC_HANDLE hSCService = ::CreateService(
			hSCManager, lpServiceName, lpDisplayName,
			dwDesiredAccess, dwServiceType, dwStartType, dwErrorControl, 
			lpBinaryPathName, lpLoadOrderGroup, lpdwTagId, lpDependencies,
			lpServiceStartName, lpPassword);

		if (NULL == hSCService) 
		{
			::UnlockServiceDatabase(hSCLock);
			::CloseServiceHandle(hSCManager);
			return FALSE;
		}

		BOOL fSuccess = T::_PostInstall(hSCService);

		::CloseServiceHandle(hSCService);
		::UnlockServiceDatabase(hSCLock);
		::CloseServiceHandle(hSCManager);

		return fSuccess;
	}

	static BOOL InstallService(
		LPCTSTR lpServiceName,
		LPCTSTR lpDisplayName,
		DWORD dwDesiredAccess,
		DWORD dwServiceType,
		DWORD dwStartType,
		DWORD dwErrorControl,
		LPCTSTR lpBinaryPathName,
		LPCTSTR lpLoadOrderGroup = NULL,
		LPDWORD lpdwTagId = NULL,
		LPCTSTR lpDependencies = NULL,
		LPCTSTR lpServiceStartName = NULL,
		LPCTSTR lpPassword = NULL)
	{
		return InstallService(
			NULL, lpServiceName, lpDisplayName, 
			dwDesiredAccess, dwServiceType, dwStartType, dwErrorControl, 
			lpBinaryPathName, lpLoadOrderGroup, lpdwTagId, lpDependencies, 
			lpServiceStartName, lpPassword);
	}

	static BOOL RemoveService(
		LPCTSTR lpServiceName)
	{
		return RemoveService(NULL, lpServiceName);
	}

	static BOOL RemoveService(
		LPCTSTR lpMachineName,
		LPCTSTR lpServiceName)
	{
		SC_HANDLE hSCManager = ::OpenSCManager(
			lpMachineName,
			SERVICES_ACTIVE_DATABASE,
			DELETE | SC_MANAGER_LOCK);

		if (NULL == hSCManager) return FALSE;

		SC_LOCK hSCLock = ::LockServiceDatabase(hSCManager);

		if (NULL == hSCLock) 
		{
			::CloseServiceHandle(hSCManager);
			return FALSE;
		}

		SC_HANDLE hSCService = ::OpenService(
			hSCManager,
			lpServiceName,
			DELETE);

		if (NULL == hSCService) 
		{
			::UnlockServiceDatabase(hSCLock); 
			::CloseServiceHandle(hSCManager);
			return FALSE;
		}

		BOOL fSuccess = ::DeleteService(hSCService);

		::CloseServiceHandle(hSCService); 
		::UnlockServiceDatabase(hSCLock); 
		::CloseServiceHandle(hSCManager);

		return fSuccess;
	}
};

//////////////////////////////////////////////////////////////////////////

class CService
{
protected:

	static CService* s_pServiceInstance;

	SERVICE_TABLE_ENTRY m_dispatchTableEntry;

	CService(LPCTSTR szServiceName, LPCTSTR szServiceDisplayName);

	BOOL m_bDebugMode;
	SERVICE_STATUS			m_ssStatus;
	SERVICE_STATUS_HANDLE   m_sshStatusHandle;

	LPCTSTR	m_szServiceName;
	LPCTSTR m_szServiceDisplayName;

	// Message-only Windows Handle for Debugging
	HWND m_hWndDebug;

public:
	virtual ~CService();

	virtual DWORD ServiceCtrlHandlerEx(
		DWORD dwControl, 
		DWORD dwEventType, 
		LPVOID lpEventData);

	virtual VOID ServiceMain(DWORD dwArgc, LPTSTR* lpArgs) = 0;
	virtual VOID ServiceDebug(DWORD dwArgc, LPTSTR* lpArgs) = 0;

	virtual DWORD OnServiceStop();
	virtual DWORD OnServiceShutdown();
	virtual DWORD OnServicePause();
	virtual DWORD OnServiceResume();
	virtual DWORD OnServiceInterrogate();
	virtual DWORD OnServiceParamChange();

	virtual DWORD OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData);
	virtual DWORD OnServiceHardwareProfileChange(DWORD dwEventType, LPVOID lpEventData);
	virtual DWORD OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData);
	virtual DWORD OnServiceSessionChange(DWORD dwEventType, LPVOID lpEventData);

	virtual DWORD OnServiceUserControl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData);

	virtual BOOL ReportStatusToSCMgr(
		DWORD dwCurrentState, 
		DWORD dwWaitHint = 3000, 
		DWORD dwWin32ExitCode = NO_ERROR);

	VOID ServiceMain_(DWORD argc, LPTSTR *argv);
	LPSERVICE_TABLE_ENTRY GetDispatchTableEntry();

	//
	// CService instance is limited to one per process 
	//

	static CService* Instance();
	static VOID S_ServiceMain(DWORD argc, LPTSTR *argv);
	static DWORD S_ServiceCtrlHandlerEx(
		DWORD dwControl, 
		DWORD dwEventType, 
		LPVOID lpEventData, 
		LPVOID lpContext);
};

#endif // _XIMETA_SVCHELP_H_
