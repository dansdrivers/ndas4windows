#pragma once
#include "observer.h"
#include "task.h"
#include "ndas/ndastypeex.h"
#include "ndas/ndasautoregscope.h"
#include <queue>
#include <vector>

class CNdasAutoRegister :
	public ximeta::CTask,
	public ximeta::CObserver
{
public:

	typedef struct _QUEUE_ENTRY {
		NDAS_DEVICE_ID deviceID;
		ACCESS_MASK	access;
	} QUEUE_ENTRY;

protected:

	CNdasAutoRegScopeData m_data;

	ximeta::CCritSecLock m_queueLock;
	std::queue<QUEUE_ENTRY> m_queue;
	HANDLE m_hSemQueue;

	BOOL AddToQueue(CONST NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);
	BOOL ProcessRegister(CONST NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);

public:
	CNdasAutoRegister();
	virtual ~CNdasAutoRegister();

	//
	// Implementation of CTask
	//
	DWORD OnTaskStart();

	//
	// Initializer
	//
	BOOL Initialize();

	//
	// Implementation of CObserver
	//
	VOID Update(ximeta::CSubject* pChangedSubject);
};
