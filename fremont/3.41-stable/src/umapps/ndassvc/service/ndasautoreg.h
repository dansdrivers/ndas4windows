#pragma once

class CNdasAutoRegister :
	public CNdasServiceWorkItem<CNdasAutoRegister,HANDLE>,
	public INdasHeartbeatSink /*,
	public IWorkerThreadClient */
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
	VOID ProcessRegister (const NDAS_DEVICE_ID &DeviceId, ACCESS_MASK AutoRegAccess);

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
	HRESULT AutoRegisterInitialize();

	STDMETHODIMP_(void) NdasHeartbeatReceived(const NDAS_DEVICE_HEARTBEAT_DATA* Data);

	//
	// IWorkerThreadClient
	//
	//HRESULT Execute(DWORD_PTR dwParam, HANDLE hObject);
	//HRESULT CloseHandle(HANDLE hHandle);
};
