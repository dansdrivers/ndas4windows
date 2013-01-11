/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "svchelp.h"
#include "xdebug.h"

namespace ximeta {

//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////

BOOL 
CServiceInstaller::
InstallService(
	LPCTSTR lpServiceName,
	LPCTSTR lpDisplayName,
	DWORD dwDesiredAccess,
	DWORD dwServiceType,
	DWORD dwStartType,
	DWORD dwErrorControl,
	LPCTSTR lpBinaryPathName,
	LPCTSTR lpLoadOrderGroup /* = NULL */,
	LPDWORD lpdwTagId /* = NULL */,
	LPCTSTR lpDependencies /* = NULL */,
	LPCTSTR lpServiceStartName /* = NULL */,
	LPCTSTR lpPassword /* = NULL */)
{
	return InstallService(
		NULL,
		lpServiceName, 
		lpDisplayName, 
		dwDesiredAccess, 
		dwServiceType, 
		dwStartType, 
		dwErrorControl, 
		lpBinaryPathName, 
		lpLoadOrderGroup, 
		lpdwTagId, 
		lpDependencies, 
		lpServiceStartName, 
		lpPassword);
}

BOOL 
CServiceInstaller::
InstallService(
	LPCTSTR lpMachineName,
	LPCTSTR lpServiceName,
	LPCTSTR lpDisplayName,
	DWORD dwDesiredAccess,
	DWORD dwServiceType,
	DWORD dwStartType,
	DWORD dwErrorControl,
	LPCTSTR lpBinaryPathName,
	LPCTSTR lpLoadOrderGroup /* = NULL */,
	LPDWORD lpdwTagId /* = NULL */,
	LPCTSTR lpDependencies /* = NULL */,
	LPCTSTR lpServiceStartName /* = NULL */,
	LPCTSTR lpPassword /* = NULL */)
{
	SC_HANDLE hSCManager = ::OpenSCManager(
		lpMachineName,
		SERVICES_ACTIVE_DATABASE,
		SC_MANAGER_CREATE_SERVICE | SC_MANAGER_LOCK);

	if (NULL == hSCManager) {
		return FALSE;
	}

	SC_LOCK hSCLock = ::LockServiceDatabase(hSCManager);

	if (NULL == hSCLock) {
		::CloseServiceHandle(hSCManager);
		return FALSE;
	}

	SC_HANDLE hSCService = ::CreateService(
		hSCManager,
		lpServiceName,
		lpDisplayName,
		dwDesiredAccess,
		dwServiceType,
		dwStartType,
		dwErrorControl,
		lpBinaryPathName,
		lpLoadOrderGroup,
		lpdwTagId,
		lpDependencies,
		lpServiceStartName,
		lpPassword);

	if (NULL == hSCService) {
		::UnlockServiceDatabase(hSCLock);
		::CloseServiceHandle(hSCManager);
		return FALSE;
	}

	::CloseServiceHandle(hSCService);
	::UnlockServiceDatabase(hSCLock);
	::CloseServiceHandle(hSCManager);

	return TRUE;
}

BOOL
CServiceInstaller::
RemoveService(
	LPCTSTR lpServiceName)
{
	return RemoveService(NULL, lpServiceName);
}

BOOL
CServiceInstaller::
RemoveService(
	LPCTSTR lpMachineName,
	LPCTSTR lpServiceName)
{
	SC_HANDLE hSCManager = ::OpenSCManager(
		lpMachineName,
		SERVICES_ACTIVE_DATABASE,
		DELETE | SC_MANAGER_LOCK);

	if (NULL == hSCManager) {
		return FALSE;
	}


	SC_LOCK hSCLock = ::LockServiceDatabase(hSCManager);

	if (NULL == hSCLock) {
		::CloseServiceHandle(hSCManager);
		return FALSE;
	}

    SC_HANDLE hSCService = ::OpenService(
		hSCManager,
		lpServiceName,
		DELETE);

    if (NULL == hSCService) {
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

//////////////////////////////////////////////////////////////////////////

//
// Static Functions
//

CService*
CService::
s_pServiceInstance = NULL;

CService*
CService::
Instance()
{
	return s_pServiceInstance;
}

VOID
CService::
S_ServiceMain(DWORD argc, LPTSTR *argv)
{
	CService* pServiceInstance = CService::Instance();
	pServiceInstance->ServiceMain_(argc, argv);
}

DWORD
CService::
S_ServiceCtrlHandlerEx(
	DWORD dwControl, 
	DWORD dwEventType, 
	LPVOID lpEventData, 
	LPVOID lpContext)
{
	CService* pService = reinterpret_cast<CService*>(lpContext);
	return pService->ServiceCtrlHandlerEx(dwControl, dwEventType, lpEventData);
}

//
// Member functions
//

CService::
CService(LPCTSTR szServiceName, LPCTSTR szServiceDisplayName) : 
	m_szServiceName(szServiceName),
	m_szServiceDisplayName(szServiceDisplayName),
	m_bDebugMode(FALSE),
	m_hWndDebug(NULL),
	m_sshStatusHandle(NULL)
{
	::ZeroMemory(&m_ssStatus, sizeof(SERVICE_STATUS));
	m_dispatchTableEntry.lpServiceName = const_cast<LPTSTR>(m_szServiceName);
	m_dispatchTableEntry.lpServiceProc = CService::S_ServiceMain;
}

CService::
~CService()
{
}

LPSERVICE_TABLE_ENTRY
CService::
GetDispatchTableEntry()
{
	return &m_dispatchTableEntry;
}

VOID
CService::
ServiceMain_(DWORD argc,LPTSTR *argv)
{
	//
	// The Service Control Manager (SCM) waits until the service reports 
	// a status of SERVICE_RUNNING. It is recommended that the service 
	// reports this status as quickly as possible, as other components in 
	// the system that require interaction with SCM will be blocked during
	// this time. Some functions may require interaction with the SCM 
	// either directly or indirectly. 
	//
	// The SCM locks the service control database during initialization, 
	// so if a service attempts to call StartService during initialization, 
	// the call will block. When the service reports to the SCM that it has 
	// successfully started, it can call StartService. If the service 
	// requires another service to be running, the service should set 
	// the required dependencies.
	//
	// Furthermore, you should not call any system functions during service 
	// initialization. The service code should call system functions only 
	// after it reports a status of SERVICE_RUNNING.
	//
	m_sshStatusHandle = 
		::RegisterServiceCtrlHandlerEx(
		m_szServiceName, 
		(LPHANDLER_FUNCTION_EX) CService::S_ServiceCtrlHandlerEx,
		(LPVOID) this);

	if (!m_sshStatusHandle)
	{
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, GetLastError());
		return;
	}

	// SERVICE_STATUS members that don't change in example
	//
	m_ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_ssStatus.dwServiceSpecificExitCode = 0;

	// report the status to the service control manager.
	//
	if (!ReportStatusToSCMgr(SERVICE_START_PENDING))
	{
		ReportStatusToSCMgr(SERVICE_STOPPED, 0, GetLastError());
		return;
	}

	// Call ServiceMain of the instance
	ServiceMain(argc, argv);

	return;
}

DWORD
CService::
ServiceCtrlHandlerEx(
	DWORD dwControl, 
	DWORD dwEventType, 
	LPVOID lpEventData)
{
	switch (dwControl) {
	// Standard Handlers
	case SERVICE_CONTROL_CONTINUE:
		return OnServiceResume();
	case SERVICE_CONTROL_INTERROGATE:
		return OnServiceInterrogate();
	case SERVICE_CONTROL_PARAMCHANGE:
		return OnServiceParamChange();
	case SERVICE_CONTROL_PAUSE:
		return OnServicePause();
	case SERVICE_CONTROL_SHUTDOWN:
		return OnServiceShutdown();
	case SERVICE_CONTROL_STOP:
		return OnServiceStop();

	// Extended Handlers
	case SERVICE_CONTROL_DEVICEEVENT:
		return OnServiceDeviceEvent(dwEventType, lpEventData);
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		return OnServiceHardwareProfileChange(dwEventType, lpEventData);
	case SERVICE_CONTROL_POWEREVENT:
		return OnServicePowerEvent(dwEventType, lpEventData);
#if (WINVER >= 0x510)
	case SERVICE_CONTROL_SESSIONCHANGE:
		return OnServiceSessionChange(dwEventType, lpEventData);
#endif

	// This control code has been deprecated. use PnP functionality instead
	case SERVICE_CONTROL_NETBINDADD:
	case SERVICE_CONTROL_NETBINDDISABLE:
	case SERVICE_CONTROL_NETBINDENABLE:
	case SERVICE_CONTROL_NETBINDREMOVE:
		return ERROR_CALL_NOT_IMPLEMENTED;

	default: // User-defined control code
		if (dwControl >= 128 && dwControl <= 255) {
			return OnServiceUserControl(dwControl, dwEventType, lpEventData);
		}
		else {
			return ERROR_CALL_NOT_IMPLEMENTED;
		}
	}
}

DWORD
CService::
OnServiceInterrogate()
{
	(VOID) ReportStatusToSCMgr(m_ssStatus.dwCurrentState);
	return NO_ERROR;
}

DWORD 
CService::
OnServiceStop()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServiceShutdown()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServicePause()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServiceResume()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServiceParamChange()
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// SERVICE_CONTROL_DEVICEEVENT should return
	// NO_ERROR to grant the request 
	// and an error code to deny the quest
	//
	return NO_ERROR;
}

DWORD 
CService::
OnServiceHardwareProfileChange(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// SERVICE_CONTROL_HARDWAREPROFILE_CHANGE should return
	// NO_ERROR to grant the request 
	// and an error code to deny the quest
	//
	return NO_ERROR;
}

DWORD 
CService::
OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData)
{
	//
	// SERVICE_CONTROL_HARDWAREPROFILE_CHANGE should return
	// NO_ERROR to grant the request 
	// and an error code to deny the quest
	//
	return NO_ERROR;
}

DWORD 
CService::
OnServiceSessionChange(DWORD dwEventType, LPVOID lpEventData)
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
CService::
OnServiceUserControl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
{
	return ERROR_CALL_NOT_IMPLEMENTED;
}

//
//  FUNCTION: ReportStatusToSCMgr()
//
//  PURPOSE: Sets the current status of the service and
//           reports it to the Service Control Manager
//
//  PARAMETERS:
//    dwCurrentState - the state of the service
//    dwWin32ExitCode - error code to report
//    dwWaitHint - worst case estimate to next checkpoint
//
//  RETURN VALUE:
//    TRUE  - success
//    FALSE - failure
//
//  COMMENTS:
//

BOOL 
CService::
ReportStatusToSCMgr(
	DWORD dwCurrentState,
	DWORD dwWaitHint /* = 3000 */,
	DWORD dwWin32ExitCode /* = NO_ERROR */)
{
	static DWORD dwCheckPoint = 1;
	BOOL fSuccess= TRUE;

	if (m_bDebugMode) {
		// when debugging we don't report to the SCM
		return fSuccess;
	}

	if (dwCurrentState == SERVICE_START_PENDING) {
		m_ssStatus.dwControlsAccepted = 0;
	} else {
		m_ssStatus.dwControlsAccepted = 
			SERVICE_ACCEPT_STOP |
			SERVICE_ACCEPT_SHUTDOWN |
			SERVICE_ACCEPT_POWEREVENT;
	}

	m_ssStatus.dwCurrentState = dwCurrentState;
	m_ssStatus.dwWin32ExitCode = dwWin32ExitCode;
	m_ssStatus.dwWaitHint = dwWaitHint;

	if ( ( dwCurrentState == SERVICE_RUNNING ) ||
		( dwCurrentState == SERVICE_STOPPED ) )
	{
		m_ssStatus.dwCheckPoint = 0;
	} else { 
		m_ssStatus.dwCheckPoint = static_cast<DWORD>(
		InterlockedIncrement(reinterpret_cast<LPLONG>(&dwCheckPoint)));
	}

	//
	// Report the status of the service to the service control manager.
	//
	fSuccess = ::SetServiceStatus( m_sshStatusHandle, &m_ssStatus);
	if (!fSuccess)
	{
		DPErrorEx(_FT("Setting Service Status to %d failed:"), m_ssStatus);
		// ReportEventError(TEXT("SetServiceStatus"));
	}

	return fSuccess;
}

//////////////////////////////////////////////////////////////////////////

} // ximeta

