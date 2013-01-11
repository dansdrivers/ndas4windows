/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASSVC_H_
#define _NDASSVC_H_
#include <xtl/xtlthread.h>
#include <xtl/xtlservice.h>
#include <xtl/xtlautores.h>
#include <xtl/xtlservice.h>
//#include <boost/shared_ptr.hpp>
#include "svchelp.h"

class CNdasService;

class CNdasDeviceHeartbeatListener;
class CNdasAutoRegister;
class CNdasDeviceRegistrar;
class CNdasLogicalUnitManager;
class CNdasEventMonitor;
class CNdasEventPublisher;
class CNdasServiceDeviceEventHandler;
class CNdasServicePowerEventHandler;

__interface INdasDeviceRegistrar;
__interface INdasLogicalUnitManager;

//////////////////////////////////////////////////////////////////////////

class CNdasService : 
	public XTL::CService<CNdasService>
{
private:

	static CNdasService* instance;

	CNdasService();
	~CNdasService();

public:

	XTL_DECLARE_SERVICE_NAME("ndassvc")
	XTL_DECLARE_SERVICE_DISPLAY_NAME("NDAS Service")

	BOOL ServiceStart(DWORD dwArgc, LPTSTR* lpArgs);
	DWORD OnServiceStop();
	DWORD OnServiceShutdown();
	DWORD OnServicePause();
	DWORD OnServiceResume();
	DWORD OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData);
	DWORD OnServicePowerEvent(DWORD dwEventType, LPVOID lpEventData);

	// singleton
	static CNdasService* Instance();
	static CNdasService* CreateInstance();
	static void DestroyInstance(CNdasService* pService);

public:

	CNdasDeviceHeartbeatListener& GetDeviceHeartbeatListener();
	CNdasAutoRegister& GetAutoRegisterHandler();

	HRESULT GetDeviceRegistrar(__deref_out INdasDeviceRegistrar** ppNdasDeviceRegistrar);
	HRESULT GetLogicalUnitManager(__deref_out INdasLogicalUnitManager** ppManager);

	CNdasEventMonitor& GetEventMonitor();
	CNdasEventPublisher& GetEventPublisher();
	CNdasServiceDeviceEventHandler& GetDeviceEventHandler();

	const CNdasDeviceHeartbeatListener& GetDeviceHeartbeatListener() const;
	const CNdasAutoRegister& GetAutoRegisterHandler() const;
	const CNdasEventMonitor& GetEventMonitor() const;
	const CNdasEventPublisher& GetEventPublisher() const;
	const CNdasServiceDeviceEventHandler& GetDeviceEventHandler() const;

	const BOOL NdasPortExists() const;
	const DWORD GetNdasPortNumber() const;

protected:

	class Impl;
	boost::shared_ptr<Impl> m_pImpl;
	friend class Impl;
};

BOOL IsNdasPortMode();

#endif // _NDASSVC_H_
