#ifndef NDAS_EVENT_PUBLISHER_H
#define NDAS_EVENT_PUBLISHER_H
#pragma once
#include <queue>
#include <vector>
#include "task.h"
#include "ndaseventex.h"

class CNdasEventPublisher :
	public ximeta::CTask
{
	typedef struct _CLIENT_DATA {
		HANDLE hPipe;
		OVERLAPPED overlapped;
		BOOL bConnected;
	} CLIENT_DATA, *PCLIENT_DATA;

	typedef std::vector<CLIENT_DATA> ClientDataVector;
	ClientDataVector m_PipeData;
	std::queue<NDAS_EVENT_MESSAGE> m_EventMessageQueue;
	HANDLE m_hSemQueue;

	DWORD m_dwPeriod;

public:

	CNdasEventPublisher();
	virtual ~CNdasEventPublisher();

	BOOL Initialize();

	virtual DWORD OnTaskStart();

	BOOL AddEvent(PNDAS_EVENT_MESSAGE pEventMessage);

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

	BOOL LogicalDeviceEntryChanged();
	BOOL LogicalDeviceStatusChanged(
		const NDAS_LOGICALDEVICE_ID& logicalDeviceId,
		const NDAS_LOGICALDEVICE_STATUS oldStatus,
		const NDAS_LOGICALDEVICE_STATUS newStatus);

	BOOL LogicalDeviceDisconnected(const NDAS_LOGICALDEVICE_ID& logicalDeviceId);
	BOOL LogicalDeviceReconnecting(const NDAS_LOGICALDEVICE_ID& logicalDeviceId);
	BOOL LogicalDeviceReconnected(const NDAS_LOGICALDEVICE_ID& logicalDeviceId);
	BOOL ServiceTerminating();
};

#endif /* NDAS_EVENT_PUBLISHER_H */
