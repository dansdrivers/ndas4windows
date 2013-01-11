/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASINSTMAN_H_
#define _NDASINSTMAN_H_

//
// Forward declarations for classes
//
class CNdasDeviceHeartbeatListener;
class CNdasDeviceRegistrar;
class CNdasLogicalDeviceManager;
class CNdasCommandServer;
class CNdasEventMonitor;
class CNdasEventPublisher;
class CNdasServiceDeviceEventHandler;
class CNdasServicePowerEventHandler;

class CNdasInstanceManager;

class CNdasInstanceManager
{
	static CNdasInstanceManager* s_pInstance;

	CNdasDeviceHeartbeatListener* m_pHBListener;
	CNdasDeviceRegistrar* m_pRegistrar;
	CNdasLogicalDeviceManager* m_pLogDevMan;
	CNdasCommandServer* m_pCommandServer;
	CNdasEventMonitor* m_pEventMonitor;
	CNdasEventPublisher* m_pEventPublisher;
	CNdasServiceDeviceEventHandler* m_pDeviceEventHandler;
	CNdasServicePowerEventHandler* m_pPowerEventHandler;

	// Real initialization function
	BOOL Initialize_();

public:

	CNdasInstanceManager();
	virtual ~CNdasInstanceManager();

	// Native class member functions
	CNdasDeviceHeartbeatListener* GetHBListener();
	CNdasDeviceRegistrar* GetRegistrar();
	CNdasLogicalDeviceManager* GetLogDevMan();
	CNdasEventMonitor* GetEventMonitor();
	CNdasEventPublisher* GetEventPublisher();
	CNdasCommandServer* GetCommandServer();

	CNdasServiceDeviceEventHandler* 
		CreateDeviceEventHandler(HANDLE hRecipient, DWORD dwReceptionFlag);
	CNdasServiceDeviceEventHandler* GetDeviceEventHandler();

	CNdasServicePowerEventHandler* CreatePowerEventHandler();
	CNdasServicePowerEventHandler* GetPowerEventHandler();

	// Singleton
	static BOOL Initialize();
	static VOID Cleanup();
	static CNdasInstanceManager* Instance();
};

#endif // _NDASINSTMAN_H_
