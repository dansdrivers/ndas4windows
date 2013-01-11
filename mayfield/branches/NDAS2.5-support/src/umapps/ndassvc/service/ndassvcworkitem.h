#pragma once
#include <xtl/xtlthread.h>
#include <xtl/xtlautores.h>

template <typename T, typename P>
struct NdasServiceWorkItemParam
{
	T* Instance;
	HANDLE hStartEvent;
	HANDLE hStopEvent;
	HANDLE hWorkItemSemaphore;
	P UserData;
	NdasServiceWorkItemParam(T* Instance, HANDLE hStartEvent, HANDLE hStopEvent, HANDLE hSem, P p) :
		Instance(Instance),
		hStartEvent(hStartEvent), hStopEvent(hStopEvent), hWorkItemSemaphore(hSem),
		UserData(p)
	{}
};

template <typename T, typename P>
class CNdasServiceWorkItem : 
	public XTL::CQueuedWorker<CNdasServiceWorkItem<T,P>,NdasServiceWorkItemParam<T,P> >
{
	typedef XTL::CQueuedWorker<CNdasServiceWorkItem<T,P>,NdasServiceWorkItemParam<T,P> > BaseClass;

public:

	BOOL QueueUserWorkItemParam(
		T* Instance, HANDLE hStartEvent, HANDLE hStopEvent, HANDLE hWorkItemSem, 
		P p, ULONG Flags = WT_EXECUTELONGFUNCTION)
	{	
		NdasServiceWorkItemParam<T,P> wip(Instance, hStartEvent, hStopEvent, hWorkItemSem, p);
		return BaseClass::QueueUserWorkItemParam(wip, Flags);
	}
	DWORD WorkItemStart(const NdasServiceWorkItemParam<T,P> Param)
	{
		// 'Param' should be a copy of the 'Param' supplied by the caller
		// If the initiating thread has been abandoned already, Param will
		// be invalidated if it is an automatic object.

		// decrement the work item (1) count or 
		// check if stop event is signaled already
		HANDLE waitHandles[] = {Param.hStopEvent, Param.hWorkItemSemaphore};
		const DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);

		DWORD waitResult = ::WaitForMultipleObjects(nWaitHandles, waitHandles, FALSE, INFINITE);
		if (WAIT_OBJECT_0 == waitResult)
		{
			return 0xFFFFFFFF; // stopped without actually starting a work
		}
		else if (WAIT_OBJECT_0 + 1 == waitResult)
		{
			// execute the actual work item
			DWORD ret = Param.Instance->ThreadStart(Param.UserData);
			// increment the work item count (1)
			XTLVERIFY( ::ReleaseSemaphore(Param.hWorkItemSemaphore, 1, NULL) );
			return ret;
		}
		else
		{
			XTLASSERT(FALSE);
			return 0x0000FFFF;
		}
	}
};
