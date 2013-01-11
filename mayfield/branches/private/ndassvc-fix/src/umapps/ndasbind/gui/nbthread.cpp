#include "StdAfx.h"
#include ".\nbthread.h"

CWorkThread::CWorkThread(void)
: m_hThread(INVALID_HANDLE_VALUE)
{
	::InitializeCriticalSection( &m_csSync );
}

CWorkThread::~CWorkThread(void)
{
	::DeleteCriticalSection( &m_csSync );
}

DWORD WINAPI CWorkThread::ThreadStart(LPVOID pParam)
{
	reinterpret_cast<CWorkThread*>(pParam)->Run();
	return 0;
}

void CWorkThread::Execute()
{
	m_bStopped = FALSE;
	m_hThread = ::CreateThread( 
					NULL, 
					0, 
					CWorkThread::ThreadStart,
					reinterpret_cast<LPVOID>(this),
					0, 
					NULL
					);
}

CWorkThread::operator HANDLE() const
{
	return m_hThread;
}

void CWorkThread::Stop()
{
	::EnterCriticalSection( &m_csSync );
	m_bStopped = TRUE;
	::LeaveCriticalSection( &m_csSync );
}

BOOL CWorkThread::IsStopped()
{
	BOOL bStopped;
	::EnterCriticalSection( &m_csSync );
	bStopped = m_bStopped;
	::LeaveCriticalSection( &m_csSync );
	return bStopped;
}
