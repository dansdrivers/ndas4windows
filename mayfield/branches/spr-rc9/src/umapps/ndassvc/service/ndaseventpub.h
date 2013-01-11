#ifndef NDAS_EVENT_PUBLISHER_H
#define NDAS_EVENT_PUBLISHER_H
#pragma once
#include <queue>
#include <vector>
#include "task.h"
#include "syncobj.h"
#include "ndas/ndaseventex.h"

//
// Event publisher
//
// Events are published to the \\.\pipe\ndas\event
//

class CNdasEventPublisher :
	public ximeta::CTask
{
	typedef struct _CLIENT_DATA {
		HANDLE hPipe;
		OVERLAPPED overlapped;
		BOOL bConnected;
	} CLIENT_DATA, *PCLIENT_DATA;

	typedef std::vector<CLIENT_DATA*> ClientDataVector;
	ClientDataVector m_PipeData;
	std::queue<NDAS_EVENT_MESSAGE> m_EventMessageQueue;
	HANDLE m_hSemQueue;

	ximeta::CCritSecLock m_queueLock;

	DWORD m_dwPeriod;

public:

	CNdasEventPublisher();
	virtual ~CNdasEventPublisher();

	BOOL Initialize();

	virtual DWORD OnTaskStart();

	BOOL AddEvent(const NDAS_EVENT_MESSAGE& pEventMessage);

	void Publish(const PNDAS_EVENT_MESSAGE pEventMessage);

	BOOL SendVersionInfo(HANDLE hPipe, LPOVERLAPPED lpOverlapped);

	BOOL AcceptNewConnection();
	BOOL CleanupConnection(PCLIENT_DATA pClientData);

	static HANDLE CreatePipeInstance(LPOVERLAPPED lpOverlapped);

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
	// -- Replaced with LogicalDeviceAlarmed
	// BOOL LogicalDeviceReconnecting(NDAS_LOGICALDEVICE_ID logicalDeviceId);
	// -- Replaced with LogicalDeviceAlarmed
	// BOOL LogicalDeviceReconnected(NDAS_LOGICALDEVICE_ID logicalDeviceId);
	BOOL LogicalDeviceAlarmed(NDAS_LOGICALDEVICE_ID logicalDeviceId, ULONG ulStatus);
	// -- Replaced with LogicalDeviceAlarmed
	// BOOL LogicalDeviceEmergency(NDAS_LOGICALDEVICE_ID logicalDeviceId);	
	BOOL LogicalDevicePropertyChanged(NDAS_LOGICALDEVICE_ID logicalDeviceId);
	BOOL LogicalDeviceRelationChanged(NDAS_LOGICALDEVICE_ID logicalDeviceId);

	BOOL SurrenderRequest(
		DWORD SlotNo, 
		DWORD UnitNo, 
		LPCGUID lpRequestHostGuid,
		DWORD RequestFlags);

	BOOL SuspendRejected();

	BOOL ServiceTerminating();
};

#endif /* NDAS_EVENT_PUBLISHER_H */
