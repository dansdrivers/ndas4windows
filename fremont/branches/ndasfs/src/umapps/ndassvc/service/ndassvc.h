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
#include <boost/shared_ptr.hpp>
#include "svchelp.h"

class CNdasService;

class CNdasDeviceHeartbeatListener;
class CNdasAutoRegister;
class CNdasDeviceRegistrar;
class CNdasLogicalDeviceManager;
class CNdasEventMonitor;
class CNdasEventPublisher;
class CNdasServiceDeviceEventHandler;
class CNdasServicePowerEventHandler;

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
	CNdasDeviceRegistrar& GetDeviceRegistrar();
	CNdasLogicalDeviceManager& GetLogicalDeviceManager();
	CNdasEventMonitor& GetEventMonitor();
	CNdasEventPublisher& GetEventPublisher();
	CNdasServiceDeviceEventHandler& GetDeviceEventHandler();

	const CNdasDeviceHeartbeatListener& GetDeviceHeartbeatListener() const;
	const CNdasAutoRegister& GetAutoRegisterHandler() const;
	const CNdasDeviceRegistrar& GetDeviceRegistrar() const;
	const CNdasLogicalDeviceManager& GetLogicalDeviceManager() const;
	const CNdasEventMonitor& GetEventMonitor() const;
	const CNdasEventPublisher& GetEventPublisher() const;
	const CNdasServiceDeviceEventHandler& GetDeviceEventHandler() const;

	const BOOL CNdasService::NdasPortExists() const;
	const DWORD CNdasService::GetNdasPortNumber() const;

protected:

	class Impl;
	boost::shared_ptr<Impl> m_pImpl;
	friend class Impl;
};

#endif // _NDASSVC_H_
