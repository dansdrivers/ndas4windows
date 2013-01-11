/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <sddl.h>
#include <ndas/ndascmd.h>
#include "ndascmdserver.h"
#include "ndascmdproc.h"

#include "transport.h"
#include "ndascfg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndascmdserver.tmh"
#endif

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

CNdasCommandServer::CNdasCommandServer() :
	MaxPipeInstances(NdasServiceConfig::Get(nscCommandPipeInstancesMax)),
	PipeTimeout(NdasServiceConfig::Get(nscCommandPipeTimeout))
{
	NdasServiceConfig::Get(nscCommandPipeName, PipeName, MAX_PATH);
}

CNdasCommandServer::~CNdasCommandServer()
{
}

HRESULT
CNdasCommandServer::Initialize()
{
	m_hProcSemaphore = ::CreateSemaphore(NULL, 0, MaxPipeInstances, NULL);
	if (m_hProcSemaphore.IsInvalid())
	{
		HRESULT hr = AtlHresultFromLastError();
		return hr;
	}
	return S_OK;
}

DWORD 
CNdasCommandServer::ThreadStart(LPVOID lpParam)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

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
		BOOL success = p->QueueUserWorkItemEx(
			this, 
			&CNdasCommandServer::CommandProcessStart, 
			hStopEvent, 
			WT_EXECUTELONGFUNCTION);
		if (success)
		{
			++nWorkItems;
		}
		else
		{
			XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
				"Starting work item (%d/%d) failed, error=0x%X\n",
				i + 1, MaxPipeInstances, GetLastError());
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
			XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_INFORMATION,
				"Command Process work item finished (%d/%d).\n", 
				finished + 1, nWorkItems);
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

BOOL
CreateNdasCommandServerDefaultDACL(
	__inout LPSECURITY_ATTRIBUTES SecurityAttributes)
{
#define NDASSVC_ALLOW_INTERACTIVE_USERS
// Anonymous in SDDL in Windows 2000 does not work
// #define NDASSVC_DENY_ANONYMOUS

	// Define the SDDL for the DACL. This example sets 
	// the following access:
	//     Built-in guests are denied all access.
	//     Anonymous Logon is denied all access.
	//     Authenticated Users are allowed read/write/execute access.
	//     Administrators are allowed full control.
	// Modify these values as needed to generate the proper
	// DACL for your application. 
#if 0
	LPCWSTR securityDescriptor = 
		L"D:"                   // Discretionary ACL
		L"(D;OICI;GA;;;BG)"     // Deny access to Built-in Guests
		L"(D;OICI;GA;;;AN)"     // Deny access to Anonymous Logon
		L"(A;OICI;GRGWGX;;;AU)" // Allow read/write/execute to Authenticated Users
		L"(A;OICI;GA;;;BA)";    // Allow full control to Administrators
#else
	LPCTSTR securityDescriptor = 
		SDDL_DACL 
		SDDL_DELIMINATOR
		// Deny access to Built-in Guests
		SDDL_ACE_BEGIN
			SDDL_ACCESS_DENIED SDDL_SEPERATOR
			SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR
			SDDL_GENERIC_ALL SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_BUILTIN_GUESTS
		SDDL_ACE_END
		//
		// Anonymous DOES NOT work in Windows 2000
		//
#ifdef NDASSVC_DENY_ANONYMOUS
		// Deny access to Anonymous
		SDDL_ACE_BEGIN
			SDDL_ACCESS_DENIED SDDL_SEPERATOR
			SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR
			SDDL_GENERIC_ALL SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_ANONYMOUS
		SDDL_ACE_END
#endif
#ifdef NDASSVC_ALLOW_INTERACTIVE_USERS
		// Allow RWX to Interactive Users
		SDDL_ACE_BEGIN
			SDDL_ACCESS_ALLOWED SDDL_SEPERATOR
			SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR
			SDDL_GENERIC_READ SDDL_GENERIC_WRITE SDDL_GENERIC_EXECUTE SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_INTERACTIVE
		SDDL_ACE_END
#endif
		// Allow Full Control to Administrators
		SDDL_ACE_BEGIN
			SDDL_ACCESS_ALLOWED SDDL_SEPERATOR
			SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR
			SDDL_GENERIC_ALL SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_BUILTIN_ADMINISTRATORS
		SDDL_ACE_END
		// Allow Full Control to Local System
		SDDL_ACE_BEGIN
			SDDL_ACCESS_ALLOWED SDDL_SEPERATOR
			SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR
			SDDL_GENERIC_ALL SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_SEPERATOR
			SDDL_LOCAL_SYSTEM
		SDDL_ACE_END;
#endif
	if (NULL == SecurityAttributes)
	{
		XTLASSERT(FALSE);
		return FALSE;
	}

	BOOL success;

	XTLVERIFY( 
	success = ConvertStringSecurityDescriptorToSecurityDescriptor(
			securityDescriptor,
			SDDL_REVISION_1,
			&(SecurityAttributes->lpSecurityDescriptor),
			NULL) );

	return success;
}

DWORD
CNdasCommandServer::ServiceCommand(HANDLE hStopEvent)
{

	// Named Pipe Connection Event
	XTL::AutoObjectHandle hConnectEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	if (hConnectEvent.IsInvalid()) 
	{
		// Log Error Here
		XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
		 "Creating ndascmd connection event failed, error=0x%X\n",
		 GetLastError());
		return 1;
	}

	// Default Security Settings
	SECURITY_ATTRIBUTES securityAttributes;
	BOOL success = CreateNdasCommandServerDefaultDACL(&securityAttributes);
	if (!success)
	{
		XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
			"CreateNdasCommandServerDefaultDACL failed, error=0x%X\n",
			GetLastError());
		return 2;
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
		&securityAttributes);

	if (hPipe.IsInvalid()) 
	{
		// Log Error Here
		XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
			"Creating pipe server failed, error=0x%X\n", GetLastError());
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

		BOOL success = pTransport->Accept(&overlapped);
		if (!success) 
		{ 
			XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
				"Transport Accept (Named Pipe) failed, error=0x%X\n", GetLastError());
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
				CNdasCommandProcessor processor(pTransport);
				if (!processor.Process())
				{
					XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
						"CommandProcess failed, error=0x%X\n", GetLastError());
				}
				// After processing the request, reset the pipe instance
				XTLVERIFY( ::FlushFileBuffers(hPipe) );
				XTLVERIFY( ::DisconnectNamedPipe(hPipe) );
			}			
			break;
		default:
			XTLTRACE2(NDASSVC_CMDSERVER, TRACE_LEVEL_ERROR,
				"WaitForMultipleObjects failed, waitResult=0x%X error=0x%X\n",
				waitResult, GetLastError());
			XTLASSERT(FALSE);
		}
	}

	return 0;
}
