/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "ndasdevhb.h"
#include "ndasdevreg.h"
#include "ndascmdserver.h"
#include "ndaspnp.h"
#include "ndaseventmon.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_INSTMAN
#include "xdebug.h"

PCNdasInstanceManager
CNdasInstanceManager::
s_pInstance = NULL;

CNdasInstanceManager::
CNdasInstanceManager() :
	m_pHBListener(NULL),
	m_pRegistrar(NULL),
	m_pLogDevMan(NULL),
	m_pCommandServer(NULL),
	m_pDeviceEventHandler(NULL),
	m_pPowerEventHandler(NULL),
	m_pEventMonitor(NULL),
	m_pEventPublisher(NULL)
{
}

CNdasInstanceManager::
~CNdasInstanceManager()
{
	if (m_pRegistrar) delete m_pRegistrar;
	if (m_pLogDevMan) delete m_pLogDevMan;
	if (m_pCommandServer) delete m_pCommandServer;
	if (m_pHBListener) delete m_pHBListener;
	if (m_pDeviceEventHandler) delete m_pDeviceEventHandler;
	if (m_pPowerEventHandler) delete m_pPowerEventHandler;
	if (m_pEventMonitor) delete m_pEventMonitor;
	if (m_pEventPublisher) delete m_pEventPublisher;
}

PCNdasInstanceManager 
CNdasInstanceManager::
Instance()
{
	// You should never call this without initialization
	_ASSERTE(s_pInstance != NULL && "You should call Instance() after Initialize()");
	return s_pInstance;
}

BOOL 
CNdasInstanceManager::
Initialize()
{
	// You should never call this after initialization
	_ASSERT(s_pInstance == NULL);

	s_pInstance = new CNdasInstanceManager;
	BOOL fSuccess = s_pInstance->Initialize_();
	if (!fSuccess)  {
		delete s_pInstance;
		s_pInstance = NULL;
		return FALSE;
	}

	return TRUE;
}

VOID
CNdasInstanceManager::
Cleanup()
{
	delete s_pInstance;
	s_pInstance = NULL;
}

BOOL 
CNdasInstanceManager::
Initialize_()
{
	BOOL fSuccess;
	
	if (NULL == m_pHBListener) {
		m_pHBListener = new CNdasDeviceHeartbeatListener();

		if (NULL == m_pHBListener) {
			DPErrorEx(_FT("Creating an instance of Heartbeat Listener failed: "));
			return FALSE;
		}
	}

	fSuccess = m_pHBListener->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Initializing Heartbeat Listener failed: "));
		return FALSE;
	}

	if (NULL == m_pRegistrar) {
        m_pRegistrar = new CNdasDeviceRegistrar();

		if (NULL == m_pRegistrar) {
			DPErrorEx(_FT("Creating an instance of Device Registrar failed: "));
			return FALSE;
		}
	}


	if (NULL == m_pLogDevMan) {
		m_pLogDevMan = new CNdasLogicalDeviceManager(m_pRegistrar);

		if (NULL == m_pLogDevMan) {
			DPErrorEx(_FT("Creating an instance of Logical Device Manager failed: "));
			return FALSE;
		}
	}

	if (NULL == m_pCommandServer) {
		m_pCommandServer = new CNdasCommandServer();
		if (NULL == m_pCommandServer) {
			DPErrorEx(_FT("Creating an instance of Command Server failed: "));
			return FALSE;
		}
	}

	fSuccess = m_pCommandServer->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Initializing Command Server failed: "));
		return FALSE;
	}

	if (NULL == m_pEventMonitor) {
		m_pEventMonitor = new CNdasEventMonitor();
		if (NULL == m_pEventMonitor) {
			DPErrorEx(_FT("Creating an instance of Event Monitor failed: "));
			return FALSE;
		}
	}

	fSuccess = m_pEventMonitor->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Initializing Event Monitor failed: "));
		return FALSE;
	}

	if (NULL == m_pEventPublisher) {
		m_pEventPublisher = new CNdasEventPublisher();
		if (NULL == m_pEventPublisher) {
			DPErrorEx(_FT("Creating an instance of Event Publisher failed: "));
			return FALSE;
		}
	}

	fSuccess = m_pEventPublisher->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Initializing Event Publisher failed: "));
		return FALSE;
	}

	return TRUE;
}

CNdasDeviceHeartbeatListener*
CNdasInstanceManager::
GetHBListener()
{
	return m_pHBListener;
}

CNdasDeviceRegistrar*
CNdasInstanceManager::
GetRegistrar()
{
	return m_pRegistrar;
}

CNdasLogicalDeviceManager*
CNdasInstanceManager::
GetLogDevMan()
{
	return m_pLogDevMan;
}

CNdasCommandServer*
CNdasInstanceManager::
GetCommandServer()
{
	return m_pCommandServer;
}

CNdasEventMonitor*
CNdasInstanceManager::
GetEventMonitor()
{
	return m_pEventMonitor;
}

CNdasServiceDeviceEventHandler* 
CNdasInstanceManager::
CreateDeviceEventHandler(HANDLE hRecipient, DWORD dwReceptionFlag)
{
	return m_pDeviceEventHandler = 
		new CNdasServiceDeviceEventHandler(hRecipient, dwReceptionFlag);
}

CNdasServiceDeviceEventHandler* 
CNdasInstanceManager::
GetDeviceEventHandler()
{
	return m_pDeviceEventHandler;
}

CNdasServicePowerEventHandler* 
CNdasInstanceManager::
CreatePowerEventHandler()
{
	return m_pPowerEventHandler =
		new CNdasServicePowerEventHandler();
}

CNdasServicePowerEventHandler* 
CNdasInstanceManager::
GetPowerEventHandler()
{
	return m_pPowerEventHandler;
}

CNdasEventPublisher*
CNdasInstanceManager::
GetEventPublisher()
{
	return m_pEventPublisher;
}

