#pragma once
#include "observer.h"
#include <ndas/ndastypeex.h>
#include <ndas/ndasautoregscope.h>
#include <queue>
#include <vector>
#include <xtl/xtlautores.h>
#include <xtl/xtllock.h>
#include "ndassvcworkitem.h"

class CNdasService;

class CNdasAutoRegister :
	public CNdasServiceWorkItem<CNdasAutoRegister,HANDLE>,
	public ximeta::CObserver
{
public:

	typedef struct _QUEUE_ENTRY {
		NDAS_DEVICE_ID deviceID;
		ACCESS_MASK	access;
	} QUEUE_ENTRY;

protected:

	CNdasService& m_service;
	CNdasAutoRegScopeData m_data;

	ximeta::CCritSecLock m_queueLock;
	std::queue<QUEUE_ENTRY> m_queue;
	XTL::AutoObjectHandle m_hSemQueue;

	BOOL AddToQueue(const NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);
	BOOL ProcessRegister(const NDAS_DEVICE_ID& deviceId, ACCESS_MASK autoRegAccess);

public:

	CNdasAutoRegister(CNdasService& service);
	virtual ~CNdasAutoRegister();

	//
	// Implementation of CTask
	//
	DWORD ThreadStart(HANDLE hStopEvent);

	//
	// Initializer
	//
	bool Initialize();

	//
	// Implementation of CObserver
	//
	VOID Update(ximeta::CSubject* pChangedSubject);
};
