#include "stdafx.h"
#include <ndas/ndassvcparam.h>
#include "ndascfg.h"
#include "ndasobjs.h"
#include "ndasdevid.h"
#include "ndaseventpub.h"
#include "ndaspowereventhandler.hpp"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaspowereventhandler.tmh"
#endif

struct LogicalDeviceIsMounted : 
	std::unary_function<INdasLogicalUnit*,bool> 
{
	bool operator()(INdasLogicalUnit* pNdasLogicalUnit) const 
	{
		NDAS_LOGICALDEVICE_STATUS status;
		COMVERIFY(pNdasLogicalUnit->get_Status(&status));
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
	HRESULT hr;

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

	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));

	CInterfaceArray<INdasLogicalUnit> ndasLogicalUnits;

	pManager->get_NdasLogicalUnits(NDAS_ENUM_DEFAULT, ndasLogicalUnits);

	bool mounted = false;
	size_t count = ndasLogicalUnits.GetCount();
	for (size_t index = 0; index < count; ++index)
	{
		INdasLogicalUnit* pNdasLogicalUnit = ndasLogicalUnits.GetAt(index);
		if (LogicalDeviceIsMounted()(pNdasLogicalUnit))
		{
			mounted = true;
			break;
		}
	}

	//
	// Service won't interact with the user
	// If you want to make this function interactive
	// You should set the NDASSVC_SUSPEND_ALLOW
	// and the UI application should process NDASSVC_SUSPEND by itself
	//
	if (mounted) 
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

