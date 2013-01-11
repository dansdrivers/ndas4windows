#ifndef NDAS_EVENT_PUBLISHER_H
#define NDAS_EVENT_PUBLISHER_H
#pragma once
#include <queue>
#include <vector>
#include <xtl/xtlautores.h>
#include "ndas/ndaseventex.h"
#include "ndascomobjectsimpl.hpp"

//
// Event publisher
//
// Events are published to the \\.\pipe\ndas\event
//

typedef std::queue<NDAS_EVENT_MESSAGE> NdasEventMessageQueue;

class CNdasEventPublisher
{
	NdasEventMessageQueue m_EventMessageQueue;

	XTL::AutoObjectHandle m_hSemQueue;
	CLock m_queueLock;
	const DWORD PeriodicEventInterval;

public:

	CNdasEventPublisher();

	HRESULT Initialize();

	DWORD ThreadStart(HANDLE hStopEvent);

	BOOL AddEvent(const NDAS_EVENT_MESSAGE& EventMessage);

	BOOL DeviceEntryChanged();
	BOOL DeviceStatusChanged(
		const DWORD slotNo, 
		const NDAS_DEVICE_STATUS oldStatus,
		const NDAS_DEVICE_STATUS newStatus);

	BOOL DevicePropertyChanged(DWORD slotNo);
	BOOL UnitDevicePropertyChanged(DWORD slotNo, DWORD unitNo);

	BOOL LogicalDeviceEntryChanged();

	BOOL LogicalDeviceStatusChanged(
		NDAS_LOGICALDEVICE_ID logicalDeviceId,
		NDAS_LOGICALDEVICE_STATUS oldStatus,
		NDAS_LOGICALDEVICE_STATUS newStatus);

	BOOL LogicalDeviceDisconnected(NDAS_LOGICALDEVICE_ID logicalDeviceId);
	BOOL LogicalDeviceAlarmed(NDAS_LOGICALDEVICE_ID logicalDeviceId, ULONG ulStatus);
	BOOL LogicalDevicePropertyChanged(NDAS_LOGICALDEVICE_ID logicalDeviceId);
	BOOL LogicalDeviceRelationChanged(NDAS_LOGICALDEVICE_ID logicalDeviceId);

	BOOL SurrenderRequest(
		DWORD SlotNo, 
		DWORD UnitNo, 
		LPCGUID lpRequestHostGuid,
		DWORD RequestFlags);

	BOOL SuspendRejected();

	BOOL NdasAutoPnp (TCHAR *NdasStringId);

};

#endif /* NDAS_EVENT_PUBLISHER_H */
