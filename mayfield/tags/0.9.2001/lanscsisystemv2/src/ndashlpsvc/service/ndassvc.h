/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASSVC_H_
#define _NDASSVC_H_

#include "svchelp.h"
#include "task.h"

class CNdasService;
typedef CNdasService *PCNdasService;

#define NDAS_SERVICE_NAME _T("ndashelper")
#define NDAS_SERVICE_DISPLAY_NAME _T("NDAS Helper Service")

//////////////////////////////////////////////////////////////////////////

class CNdasService : 
	public ximeta::CTask, 
	public ximeta::CService
{
private:

	CNdasService();

public:

	static LPCTSTR SERVICE_NAME;
	static LPCTSTR DISPLAY_NAME;

	virtual ~CNdasService();

	// Implementation of sys::CService

	virtual VOID ServiceMain(DWORD dwArgc, LPTSTR* lpArgs);
	virtual VOID ServiceDebug(DWORD dwArgc, LPTSTR* lpArgs);

	virtual DWORD OnServiceStop();
	virtual DWORD OnServiceShutdown();
	virtual DWORD OnServicePause();
	virtual DWORD OnServiceResume();
	virtual DWORD OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData);
	virtual DWORD OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData);

	// Implementation of ximeta::CTask

	virtual DWORD OnTaskStart();

	// singleton
	static PCNdasService Instance();

protected:

	HDEVNOTIFY m_hVolumeDeviceNotify;
	HDEVNOTIFY m_hStoragePortDeviceNotify;
	HDEVNOTIFY m_hDiskDeviceNotify;

};

//////////////////////////////////////////////////////////////////////////

class CNdasServiceInstaller
{
public:
	static LPCTSTR SERVICE_NAME;
	static LPCTSTR DISPLAY_NAME;

	static const DWORD SERVICE_TYPE = SERVICE_WIN32_OWN_PROCESS;
	static const DWORD START_TYPE = SERVICE_DEMAND_START;
	static const DWORD ERROR_CONTROL = SERVICE_ERROR_NORMAL;

	static BOOL Install(LPCTSTR lpBinaryPathName);
	//
	// Remove NDAS Helper Service
	//
	// When removing the service, if there is a running instance,
	// the service entry will not be deleted until the service is
	// stopped. Only "MarkedForDeletion" flag will be set to 
	// Service Database
	// 
	static BOOL Remove();
};


#endif // _NDASSVC_H_
