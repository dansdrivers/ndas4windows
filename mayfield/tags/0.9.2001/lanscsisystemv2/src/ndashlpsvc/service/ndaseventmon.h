#pragma once
#ifndef _NDASEVENTMON_H_
#define _NDASEVENTMON_H_

#include <set>
#include "task.h"
#include "syncobj.h"

class CNdasEventMonitor;
typedef CNdasEventMonitor* PCNdasEventMonitor;

class CNdasDevice;
typedef CNdasDevice* PCNdasDevice;

class CNdasEventMonitor :
	public ximeta::CTask,
	public ximeta::CCritSecLock
{
	typedef std::set<PCNdasDevice> PCNdasDeviceSet;
	typedef std::vector<PCNdasLogicalDevice> PCNdasLogicalDeviceVector;

	PCNdasDeviceSet m_hbMonDevices;
	PCNdasLogicalDeviceVector m_vLogDevices;

public:

	static const LONG HEARTBEAT_MONITOR_INTERVAL = 10 * 1000; // 10 sec

protected:

	HANDLE m_hHeartbeatMonitorTimer;
	HANDLE m_hLogDeviceSetChangeEvent;
	BOOL m_bIterating;

public:
	CNdasEventMonitor();
	virtual ~CNdasEventMonitor();

	BOOL Initialize();
	VOID Attach(const PCNdasDevice pDevice);
	VOID Detach(const PCNdasDevice pDevice);

	VOID Attach(const PCNdasLogicalDevice pLogDevice);
	VOID Detach(const PCNdasLogicalDevice pLogDevice);

	virtual DWORD OnTaskStart();

private:
	// hide copy constructor
	CNdasEventMonitor(const CNdasEventMonitor&);
	// hide assignment operator
	CNdasEventMonitor& operator = (const CNdasEventMonitor&);
	
};

#endif
