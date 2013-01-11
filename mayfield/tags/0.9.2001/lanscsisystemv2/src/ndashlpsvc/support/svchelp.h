/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _XIMETA_SVCHELP_H_
#define _XIMETA_SVCHELP_H_

namespace ximeta {

//////////////////////////////////////////////////////////////////////////

class CService;
typedef CService *PCService;

class CServiceInstaller
{
private:
	// No-instance class
	CServiceInstaller() {}
public:

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
		LPCTSTR lpPassword = NULL);

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
		LPCTSTR lpPassword = NULL);

	static BOOL RemoveService(
		LPCTSTR lpServiceName);

	static BOOL RemoveService(
		LPCTSTR lpMachineName,
		LPCTSTR lpServiceName);

};

//////////////////////////////////////////////////////////////////////////

class CService
{
protected:

	static PCService s_pServiceInstance;

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
		LPVOID lpEventData, 
		LPVOID lpContext);

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

	virtual ReportStatusToSCMgr(
		DWORD dwCurrentState, 
		DWORD dwWaitHint = 3000, 
		DWORD dwWin32ExitCode = NO_ERROR);

	VOID ServiceMain_(DWORD argc, LPTSTR *argv);
	LPSERVICE_TABLE_ENTRY GetDispatchTableEntry();

	//
	// CService instance is limited to one per process 
	//

	static PCService Instance();
	static VOID S_ServiceMain(DWORD argc, LPTSTR *argv);
	static DWORD S_ServiceCtrlHandlerEx(
		DWORD dwControl, 
		DWORD dwEventType, 
		LPVOID lpEventData, 
		LPVOID lpContext);
};

} // ximeta

#endif // _XIMETA_SVCHELP_H_
