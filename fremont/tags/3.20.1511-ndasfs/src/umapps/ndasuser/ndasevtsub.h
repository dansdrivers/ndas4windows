#pragma once
#include <ndas/ndasuser.h>
#include <ndas/ndaseventex.h>
#include <xtl/xtlautores.h>
#include <xtl/xtlthread.h>

typedef struct _NDAS_EVENT_CALLBACK_DATA {
	NDASEVENTPROC EventProc;
	LPVOID lpContext;
} NDAS_EVENT_CALLBACK_DATA, *PNDAS_EVENT_CALLBACK_DATA;

class CNdasEventSubscriber : 
	public XTL::CQueuedWorker<CNdasEventSubscriber, LPVOID>
{
protected:

	static const DWORD MAX_WORKITEMS = 1;

	LONG m_cRef;

	CRITICAL_SECTION m_cs;

	XTL::AutoObjectHandle m_hThreadStopEvent;
	XTL::AutoObjectHandle m_hWorkItemSemaphore;
	XTL::AutoObjectHandle m_hDataEvent;

	NDAS_EVENT_CALLBACK_DATA m_CallbackData[MAX_NDAS_EVENT_CALLBACK];

	DWORD m_nWorkItemStarted;
	bool m_csInit;

public:

	CNdasEventSubscriber();
	~CNdasEventSubscriber();

	bool Initialize();
	bool Finalize();
	bool IsFinal();

	HNDASEVENTCALLBACK AddCallback(NDASEVENTPROC lpEventProc, LPVOID lpContext);
	BOOL RemoveCallback(HNDASEVENTCALLBACK hCallback);
	DWORD WorkItemStart(LPVOID);

private:

	bool IsThreadStopRequested(DWORD Timeout = 0)
	{
		DWORD waitResult = ::WaitForSingleObject(m_hThreadStopEvent, Timeout);
		XTLASSERT(WAIT_TIMEOUT  == waitResult || WAIT_OBJECT_0 == waitResult);
		return waitResult == WAIT_OBJECT_0;
	}

	HANDLE WaitForEventPipe(DWORD Interval);
	bool ReadEventData(HANDLE hPipe, NDAS_EVENT_MESSAGE& message);
	void PublishServiceEvent(const NDAS_EVENT_MESSAGE& message);
	void PublishConnecting();
	void PublishConnectionFailed(DWORD Error = ::GetLastError());
	void PublishConnected();
	void CallEventProc(DWORD Error, const NDAS_EVENT_INFO& EventInfo);

public:
	void* operator new(size_t size)
	{
		return ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	}

	void operator delete(void * p)
	{
		XTLVERIFY(::HeapFree(::GetProcessHeap(), HEAP_ZERO_MEMORY, p));
	}
};
