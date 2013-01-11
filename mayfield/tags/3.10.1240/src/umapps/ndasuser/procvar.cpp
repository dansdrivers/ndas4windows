#include "stdafx.h"
#include "procvar.h"

static PPROCESS_DATA _pProcessData = NULL;
static CRITICAL_SECTION _csProcData = {0};
static LPCRITICAL_SECTION _pcsProcData = NULL;

BOOL InitProcessData()
{
	if (NULL == _pcsProcData) {
		::ZeroMemory(&_csProcData, sizeof(CRITICAL_SECTION));
		::InitializeCriticalSection(&_csProcData);
		_pcsProcData = &_csProcData;
	}

	if (NULL == _pProcessData) {
		_pProcessData = reinterpret_cast<PPROCESS_DATA>(
			::GlobalAlloc(GPTR, sizeof(PROCESS_DATA)));
		if (NULL == _pProcessData) {
			return FALSE;
		}
	}

	return TRUE;
}

PPROCESS_DATA GetProcessData()
{
	_ASSERTE(NULL != _pProcessData);
	// Do not call get process data without init
	return _pProcessData;
}

VOID CleanupProcessData()
{
	DWORD err = ::GetLastError();

	if (NULL != _pProcessData) {
		HGLOBAL hGlobal = ::GlobalFree(
			reinterpret_cast<HGLOBAL>(_pProcessData));
	}

	if (NULL != _pcsProcData) {
		::DeleteCriticalSection(_pcsProcData);
	}

	::SetLastError(err);
}

VOID LockProcessData()
{
	_ASSERTE(NULL != _pcsProcData);
	::EnterCriticalSection(_pcsProcData);
}

VOID UnlockProcessData()
{
	_ASSERTE(NULL != _pcsProcData);
	::LeaveCriticalSection(_pcsProcData);
}

