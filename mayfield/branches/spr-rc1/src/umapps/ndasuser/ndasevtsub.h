#pragma once
#include "ndas/ndasuser.h"
#include "ndas/ndaseventex.h"

typedef struct _NDAS_EVENT_CALLBACK_DATA {
	NDASEVENTPROC EventProc;
	LPVOID lpContext;
} NDAS_EVENT_CALLBACK_DATA, *PNDAS_EVENT_CALLBACK_DATA;

class CNdasEventSubscriber
{
protected:

	static const HNDASEVENTCALLBACK HNDASEVTCB_BASE;

	CRITICAL_SECTION m_cs;

	NDAS_EVENT_CALLBACK_DATA m_CallbackData[MAX_NDAS_EVENT_CALLBACK];

	LONG m_cRef;
	DWORD m_dwThreadId;
	HANDLE m_hThread;
	HANDLE m_hThreadStopEvent;
	HANDLE m_hDataEvent;

	DWORD Run();

	HANDLE WaitServer(BOOL& bStop);

public:

	CNdasEventSubscriber();
	virtual ~CNdasEventSubscriber();

	BOOL Initialize();

	HNDASEVENTCALLBACK AddCallback(
		NDASEVENTPROC lpEventProc, 
		LPVOID lpContext);

	void CallEventProc(DWORD dwError, PNDAS_EVENT_INFO pEventInfo);
	BOOL RemoveCallback(HNDASEVENTCALLBACK hCallback);

	static BOOL IsValidHandle(HNDASEVENTCALLBACK hCallback);

private:

	static DWORD WINAPI ThreadProc(LPVOID lpParam);

};


