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

	//
	// 3 min
	//
	static const DWORD LOGICALDEVICE_RISK_MOUNT_INTERVAL = 3 * 60 * 1000;
	static const LONG HEARTBEAT_MONITOR_INTERVAL = 10 * 1000; // 10 sec

protected:

	HANDLE m_hHeartbeatMonitorTimer;
	HANDLE m_hLogDeviceSetChangeEvent;
	BOOL m_bIterating;

public:
	CNdasEventMonitor();
	virtual ~CNdasEventMonitor();

	BOOL Initialize();
	VOID Attach(CNdasDevice* pDevice);
	VOID Detach(CNdasDevice* pDevice);

	VOID Attach(CNdasLogicalDevice* pLogDevice);
	VOID Detach(CNdasLogicalDevice* pLogDevice);

	DWORD OnTaskStart();

	BOOL OnLogicalDeviceAlarmed(DWORD nWaitIndex);
	BOOL OnLogicalDeviceDisconnected(DWORD nWaitIndex);
//	BOOL OnLogicalDeviceSetChange();

private:
	// hide copy constructor
	CNdasEventMonitor(const CNdasEventMonitor&);
	// hide assignment operator
	CNdasEventMonitor& operator = (const CNdasEventMonitor&);
	
};

#endif
