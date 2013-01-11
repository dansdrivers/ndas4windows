#pragma once
#ifndef _NDASEVENTMON_H_
#define _NDASEVENTMON_H_

#include <set>
#include <xtl/xtlautores.h>
#include <xtl/xtllock.h>
#include "ndassvcdef.h"
#include "syncobj.h"

class CNdasEventMonitor
{
	XTL::CReaderWriterLock m_NdasDeviceDataLock;
	XTL::CReaderWriterLock m_NdasLogicalUnitDataLock;

	CInterfaceArray<INdasDevice> m_NdasDevices;
	CInterfaceArray<INdasLogicalUnit> m_NdasLogicalUnits;

public:

	static const LONG HEARTBEAT_MONITOR_INTERVAL = 10 * 1000; // 10 sec

protected:

	XTL::AutoObjectHandle m_HeartbeatMonitorTimer;
	XTL::AutoObjectHandle m_NdasLogicalUnitSetChanged;

public:

	CNdasEventMonitor();
	~CNdasEventMonitor();

	bool Initialize();

	void Attach(INdasDevice* pNdasDevice);
	void Detach(INdasDevice* pNdasDevice);

	void Attach(INdasLogicalUnit* pNdasLogicalUnit);
	void Detach(INdasLogicalUnit* pNdasLogicalUnit);

	DWORD ThreadStart(HANDLE hStopEvent);

	void OnLogicalDeviceAlarmedByPnP(INdasLogicalUnit* pNdasLogicalUnit, ULONG LogDeviceStatus);

private:

	void OnLogicalDeviceAlarmed(INdasLogicalUnit* pNdasLogicalUnit);
	void OnLogicalDeviceDisconnected(INdasLogicalUnit* pNdasLogicalUnit);

private:
	// hide copy constructor
	CNdasEventMonitor(const CNdasEventMonitor&);
	// hide assignment operator
	CNdasEventMonitor& operator = (const CNdasEventMonitor&);	
};

#endif
