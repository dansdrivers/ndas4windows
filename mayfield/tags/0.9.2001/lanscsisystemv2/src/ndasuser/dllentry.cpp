#include "stdafx.h"

#define INIT_XDEBUG_MODULE
#include "xdebug.h"
#include "procvar.h"
#include "ndasevtsub.h"

PPROCESS_DATA _pProcessData = NULL;

bool InitProcessData()
{
	_pProcessData = reinterpret_cast<PPROCESS_DATA>(
		::GlobalAlloc(GPTR, sizeof(PROCESS_DATA)));

	if (NULL == _pProcessData) {
		return false;
	}

	return true;
}

bool CleanupProcessData()
{
	HGLOBAL hGlobal = ::GlobalFree(
		reinterpret_cast<HGLOBAL>(_pProcessData));

	if (NULL != hGlobal) {
		return false;
	}

	return true;
}

CRITICAL_SECTION _csProcData = {0};

void LockProcessData()
{
	::EnterCriticalSection(&_csProcData);
}

void UnlockProcessData()
{
	::LeaveCriticalSection(&_csProcData);
}

XDebugConsoleOutput* _PXDbgConsoleOutput = NULL;
XDebugSystemOutput* _PXDbgSystemOutput = NULL;

extern CNdasEventSubscriber* _pEventSubscriber;

BOOL 
APIENTRY 
DllMain(
	HANDLE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		::ZeroMemory(&_csProcData, sizeof(CRITICAL_SECTION));
		::InitializeCriticalSection(&_csProcData);

		XDebugInit(_T("NDASUSER"));
		_PXDbgSystemOutput = new XDebugSystemOutput;
		// _PXDbgConsoleOutput = new XDebugConsoleOutput;
		_PXDebug->dwOutputLevel = XDebug::OL_INFO;
		_PXDebug->Attach(_PXDbgSystemOutput);
//		_PXDebug->Attach(_PXDbgConsoleOutput);
		InitProcessData();
		DPInfo(_FT("NDASUSER.DLL Process Attach\n"));

		_pEventSubscriber = new CNdasEventSubscriber();

		break;

	case DLL_THREAD_ATTACH:
		DPInfo(_FT("NDASUSER.DLL Thread Attach\n"));
		break;
	case DLL_THREAD_DETACH:
		DPInfo(_FT("NDASUSER.DLL Thread Detach\n"));
		break;
	case DLL_PROCESS_DETACH:
		_PXDebug->Detach(_PXDbgSystemOutput);
		_PXDebug->Detach(_PXDbgConsoleOutput);
		delete _PXDbgConsoleOutput;
		delete _PXDbgSystemOutput;
		XDebugCleanup();
		CleanupProcessData();
		DPInfo(_FT("NDASUSER.DLL Process Detach\n"));

		::DeleteCriticalSection(&_csProcData);

		delete _pEventSubscriber;

		break;
	}
    return TRUE;
}
