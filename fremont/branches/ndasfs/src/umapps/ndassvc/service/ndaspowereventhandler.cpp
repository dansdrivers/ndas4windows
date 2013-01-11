#include "stdafx.h"
#include <ndas/ndassvcparam.h>
#include "ndascfg.h"
#include "ndasobjs.h"
#include "ndaslogdev.h"
#include "ndaslogdevman.h"
#include "ndaseventpub.h"
#include "ndaspowereventhandler.hpp"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaspowereventhandler.tmh"
#endif

struct LogicalDeviceIsMounted : 
	std::unary_function<CNdasLogicalDevicePtr,bool> 
{
	bool operator()(const CNdasLogicalDevicePtr& pLogDevice) const 
	{
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		switch (status)
		{
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
			return true;
		}
		return false;
	}
};

CNdasServicePowerEventHandler::CNdasServicePowerEventHandler(
	CNdasService& service) :
	m_service(service)
{
}

bool
CNdasServicePowerEventHandler::Initialize()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_VERBOSE, 
		"CNdasServicePowerEventHandler::Initialize.\n");
	return true;
}

//
// Return TRUE to grant the request to suspend. 
// To deny the request, return BROADCAST_QUERY_DENY.
//
LRESULT 
CNdasServicePowerEventHandler::OnQuerySuspend(DWORD dwFlags)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnQuerySuspend.\n");

	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. All other bit values are reserved. 

	DWORD dwValue = NdasServiceConfig::Get(nscSuspendOptions);

	if (NDASSVC_SUSPEND_ALLOW == dwValue) 
	{
		return TRUE;
	}

	CNdasLogicalDeviceManager& manager = m_service.GetLogicalDeviceManager();

	CNdasLogicalDeviceVector logDevices;

	// do not unlock the logical device collection here
	// we want to make sure that there will be no registration 
	// during mount status check (until the end of this function)

	manager.Lock();
	manager.GetItems(logDevices);
	manager.Unlock();

	bool fMounted = (
		logDevices.end() != 
		std::find_if(
			logDevices.begin(), logDevices.end(), 
			LogicalDeviceIsMounted()));

	//
	// Service won't interact with the user
	// If you want to make this function interactive
	// You should set the NDASSVC_SUSPEND_ALLOW
	// and the UI application should process NDASSVC_SUSPEND by itself
	//
	if (fMounted) 
	{
		if (0x01 == (dwFlags & 0x01)) 
		{
			//
			// Possible to interact with the user
			//
			(void) m_service.GetEventPublisher().SuspendRejected();
			return BROADCAST_QUERY_DENY;
		}
		else 
		{
			//
			// No User interface is available
			//
			(void) m_service.GetEventPublisher().SuspendRejected();
			return BROADCAST_QUERY_DENY;
		}
	}

	return TRUE;
}

void
CNdasServicePowerEventHandler::OnQuerySuspendFailed()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnQuerySuspendFailed.\n");
	return;
}

void
CNdasServicePowerEventHandler::OnSuspend()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnSuspend.\n");
	return;
}

void
CNdasServicePowerEventHandler::OnResumeAutomatic()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeAutomatic.\n");
	return;
}

void
CNdasServicePowerEventHandler::OnResumeCritical()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeCritical.\n");
	return;
}

void
CNdasServicePowerEventHandler::OnResumeSuspend()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeSuspend.\n");
	return;
}

