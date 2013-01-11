#include "stdafx.h"
#include "ndaslogdevmon.h"

CNdasLogicalDeviceMonitor::CNdasLogicalDeviceMonitor() :
	ximeta::CTask(_T("NdasLogDevMon"))
{

}

CNdasLogicalDeviceMonitor::~CNdasLogicalDeviceMonitor()
{
}

DWORD
CNdasLogicalDeviceMonitor::OnTaskStart()
{
	
	return 0;
}