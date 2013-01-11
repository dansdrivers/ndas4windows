#pragma once

class CNdasAutoRegister :
	public CNdasServiceWorkItem<CNdasAutoRegister,HANDLE>,
	public INdasHeartbeatSink
{
public:

	typedef struct _QUEUE_ENTRY {
		NDAS_DEVICE_ID deviceID;
		ACCESS_MASK	access;
	} QUEUE_ENTRY;

protected:

	CNdasAutoRegScopeData m_data;

	CLock m_queueLock;
	std::queue<QUEUE_ENTRY> m_queue;
	XTL::AutoObjectHandle m_hSemQueue;

	BOOL AddToQueue(const NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);
	BOOL ProcessRegister(const NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);

public:

	CNdasAutoRegister();

	void FinalRelease();

	//
	// Implementation of CTask
	//
	DWORD ThreadStart(HANDLE hStopEvent);

	//
	// Initializer
	//
	bool Initialize();

	STDMETHODIMP_(void) NdasHeartbeatReceived(const NDAS_DEVICE_HEARTBEAT_DATA* Data);
};
