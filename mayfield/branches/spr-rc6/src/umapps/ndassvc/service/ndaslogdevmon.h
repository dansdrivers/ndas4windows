#pragma once

#include "task.h"
#include "ndaslogdev.h"

//
// A task class for monitoring 
// logical devices
//
// - Clearing the mount flags
// - Reconcile logical device status
//

class CNdasLogicalDeviceMonitor :
	ximeta::CTask
{
public:
	CNdasLogicalDeviceMonitor();
	virtual ~CNdasLogicalDeviceMonitor();

	virtual DWORD OnTaskStart();
};
