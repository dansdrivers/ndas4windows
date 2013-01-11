#pragma once
#ifndef _NDASEVENTMON_H_
#define _NDASEVENTMON_H_

#include <set>
#include <xtl/xtlautores.h>
#include <xtl/xtllock.h>
#include "ndassvcdef.h"
#include "syncobj.h"

class CNdasEventMonitor;

class CNdasDevice;

class CNdasEventMonitor
{
	XTL::CReaderWriterLock m_deviceDataLock;
	XTL::CReaderWriterLock m_logDeviceDataLock;

	CNdasDeviceVector m_devices;
	CNdasLogicalDeviceVector m_logDevices;

public:

	static const LONG HEARTBEAT_MONITOR_INTERVAL = 10 * 1000; // 10 sec

protected:

	XTL::AutoObjectHandle m_hHeartbeatMonitorTimer;
	XTL::AutoObjectHandle m_hLogDeviceSetChangeEvent;

public:

	CNdasEventMonitor();
	~CNdasEventMonitor();

	bool Initialize();

	void Attach(CNdasDevicePtr pDevice);
	void Detach(CNdasDevicePtr pDevice);

	void Attach(CNdasLogicalDevicePtr pLogDevice);
	void Detach(CNdasLogicalDevicePtr pLogDevice);

	DWORD ThreadStart(HANDLE hStopEvent);

private:

	void OnLogicalDeviceAlarmed(CNdasLogicalDevicePtr pLogDevice);
	void OnLogicalDeviceDisconnected(CNdasLogicalDevicePtr pLogDevice);

private:
	// hide copy constructor
	CNdasEventMonitor(const CNdasEventMonitor&);
	// hide assignment operator
	CNdasEventMonitor& operator = (const CNdasEventMonitor&);	
};

#endif
