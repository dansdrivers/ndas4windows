////////////////////////////////////////////////////////////////////////////
//
// classes for observer pattern
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasobserver.h"

CMultithreadedObserver::CMultithreadedObserver()
: m_nReportSize(0)
{
	m_hSyncEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	::InitializeCriticalSection( &m_csSync );
}

CMultithreadedObserver::~CMultithreadedObserver()
{
	::CloseHandle( m_hSyncEvent );
}

void CMultithreadedObserver::Update(CSubject *pSubject)
{
	::EnterCriticalSection( &m_csSync );
	const NDAS_SYNC_REPORT *pReport = pSubject->GetReport();
	if ( m_nReportSize < pReport->nSize )
	{
		m_nReportSize = pReport->nSize;
		m_pbReport = boost::shared_ptr<BYTE>( new BYTE[m_nReportSize] );
	}

	::CopyMemory( m_pbReport.get(), pReport, pReport->nSize );
	::LeaveCriticalSection( &m_csSync );
	::SetEvent( m_hSyncEvent );
}

UINT CMultithreadedObserver::GetReportSize()
{
	return m_nReportSize;
}

void CMultithreadedObserver::GerReport(NDAS_SYNC_REPORT *pReportBuffer)
{
	ATLASSERT( pReportBuffer->nSize >= m_nReportSize );
	::EnterCriticalSection( &m_csSync );
	::CopyMemory( pReportBuffer, m_pbReport.get(), m_nReportSize );
	::LeaveCriticalSection( &m_csSync );
}
