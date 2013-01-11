/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <ndas/ndascmd.h>
#include <xtl/xtltrace.h>
#include "ndascmdserver.h"
#include "ndascmdproc.h"

#include "transport.h"

#include "ndascfg.h"

CNdasCommandServer::
CNdasCommandServer(CNdasService& service) :
	m_service(service),
	MaxPipeInstances(NdasServiceConfig::Get(nscCommandPipeInstancesMax)),
	PipeTimeout(NdasServiceConfig::Get(nscCommandPipeTimeout))
{
	NdasServiceConfig::Get(nscCommandPipeName, PipeName, MAX_PATH);
}

CNdasCommandServer::
~CNdasCommandServer()
{
}

bool
CNdasCommandServer::
Initialize()
{
	m_hProcSemaphore = ::CreateSemaphore(NULL, 0, MaxPipeInstances, NULL);
	if (m_hProcSemaphore.IsInvalid())
	{
		return false;
	}
	return true;
}

namespace
{
	typedef XTL::CUserWorkItem<CNdasCommandServer,HANDLE> CmdWorkItem;
	typedef boost::shared_ptr<CmdWorkItem> CmdWorkItemPtr;
	typedef std::vector<CmdWorkItemPtr> CmdWorkItemVector;
	CmdWorkItemPtr pWorkItemPtrGenerator()
	{
		CmdWorkItemPtr p (new CmdWorkItem());
		return p;
	}
}


DWORD 
CNdasCommandServer::
ThreadStart(LPVOID lpParam)
{
	HANDLE hStopEvent = static_cast<HANDLE>(lpParam);

	CmdWorkItemVector workItems;
	workItems.reserve(MaxPipeInstances);

	size_t size = workItems.size();
	XTLASSERT(0 == size);
	std::generate_n(
		std::back_inserter(workItems), 
		MaxPipeInstances, 
		pWorkItemPtrGenerator);
	size = workItems.size();
	XTLASSERT(MaxPipeInstances == size);

	DWORD nWorkItems = 0;

	for (DWORD i = 0; i < MaxPipeInstances; ++i)
	{
		CmdWorkItemPtr p = workItems[i];
		BOOL fSuccess = p->QueueUserWorkItemEx(
			this, 
			&CNdasCommandServer::CommandProcessStart, 
			hStopEvent, 
			WT_EXECUTELONGFUNCTION);
		if (fSuccess)
		{
			++nWorkItems;
		}
		else
		{
			XTLTRACE_ERR("Starting work item (%d/%d) failed.\n", i + 1, MaxPipeInstances);
		}
	}

	// Release semaphore to start workers (semaphore increment)
	LONG prev;
	XTLVERIFY( ::ReleaseSemaphore(m_hProcSemaphore, nWorkItems, &prev) );
	XTLASSERT( 0 == prev );

	// Wait for stop event
	XTLVERIFY(WAIT_OBJECT_0 == ::WaitForSingleObject(hStopEvent, INFINITE));

	// Stopped and waits for user work items
	DWORD finished = 0;
	while (finished < nWorkItems)
	{
		::Sleep(0);
		DWORD waitResult = ::WaitForSingleObject(m_hProcSemaphore, 0);
		if (waitResult == WAIT_OBJECT_0)
		{
			XTLTRACE("Command Process work item finished (%d/%d).\n", finished + 1, nWorkItems);
			++finished;
		}
		XTLVERIFY(WAIT_OBJECT_0 == waitResult || WAIT_TIMEOUT == waitResult);
	}

	// Now Finally this thread can stop
	return 0;
}

DWORD 
CNdasCommandServer::CommandProcessStart(HANDLE hStopEvent)
{
	// 'Param' should be a copy of the 'Param' supplied by the caller
	// If the initiating thread has been abandoned already, Param will
	// be invalidated if it is an automatic object.

	// increment the work item count or 
	// check if stop event is signaled already
	HANDLE waitHandles[] = {hStopEvent, m_hProcSemaphore};
	const DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);

	DWORD waitResult = ::WaitForMultipleObjects(nWaitHandles, waitHandles, FALSE, INFINITE);
	if (WAIT_OBJECT_0 == waitResult)
	{
		return 0xFFFFFFFF; // stopped without actually starting a work
	}
	else if (WAIT_OBJECT_0 + 1 == waitResult)
	{
		// incremented work item count
		// execute the actual work item
		DWORD ret = ServiceCommand(hStopEvent);
		// decrement the work item count
		XTLVERIFY( ::ReleaseSemaphore(m_hProcSemaphore, 1, NULL) );
		return ret;
	}
	else
	{
		XTLASSERT(FALSE);
		return 0x0000FFFF;
	}
}


DWORD 
CNdasCommandServer::ServiceCommand(HANDLE hStopEvent)
{

	// Named Pipe Connection Event
	XTL::AutoObjectHandle hConnectEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	if (hConnectEvent.IsInvalid()) 
	{
		// Log Error Here
		XTLTRACE_ERR("Creating ndascmd connection event failed.\n");
		return 1;
	}

	// Create a named pipe instance
	XTL::AutoFileHandle hPipe = ::CreateNamedPipe(
		PipeName,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // | FILE_FLAG_FIRST_PIPE_INSTANCE,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		MaxPipeInstances, 
		0,
		0,
		PipeTimeout,
		NULL);

	if (hPipe.IsInvalid()) 
	{
		// Log Error Here
		XTLTRACE_ERR("Creating pipe server failed.\n");
		return 2;
	}

	HANDLE waitHandles[] = { hStopEvent, hConnectEvent };
	const DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);

	while (TRUE) 
	{
		// Initialize overlapped structure
		OVERLAPPED overlapped = {0};
		overlapped.hEvent = hConnectEvent;
		XTLVERIFY( ::ResetEvent(hConnectEvent) );

		CNamedPipeTransport namePipeTransport(hPipe);
		CNamedPipeTransport* pTransport = &namePipeTransport;

		BOOL fSuccess = pTransport->Accept(&overlapped);
		if (!fSuccess) 
		{ 
			XTLTRACE_ERR("Transport Accept (Named Pipe) failed.\n");
			break;
		}

		DWORD waitResult = ::WaitForMultipleObjects(
			nWaitHandles, waitHandles, FALSE, INFINITE);

		switch (waitResult) 
		{
		case WAIT_OBJECT_0: // Terminate Thread Event
			return 0;
		case WAIT_OBJECT_0 + 1: // Connect Event
			//
			// Process the request
			// (Safely ignore error)
			//
			{
				CNdasCommandProcessor processor(m_service, pTransport);
				if (!processor.Process())
				{
					XTLTRACE_ERR("CommandProcess failed.\n");
				}
				// After processing the request, reset the pipe instance
				XTLVERIFY( ::FlushFileBuffers(hPipe) );
				XTLVERIFY( ::DisconnectNamedPipe(hPipe) );
			}			
			break;
		default:
			XTLTRACE_ERR("Wait failed.\n");
			XTLASSERT(FALSE);
		}
	}

	return 0;
}
