////////////////////////////////////////////////////////////////////////////
//
// classes for observer pattern
//  for more information about observer pattern 
//  see http://www.dofactory.com/Patterns/PatternObserver.aspx
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASOBSERVER_H_
#define _NDASOBSERVER_H_

#include <list>
#include <boost/shared_ptr.hpp>

typedef struct _NDAS_SYNC_REPORT
{
	UINT	nSize;	// Size of the structure
} NDAS_SYNC_REPORT;

class CSubject;

class CObserver
{
public:
	virtual void Update(CSubject *pSubject) = 0;
};

class CSubject
{
protected:
	typedef std::list<CObserver*> CObserverList;
	std::list<CObserver*> m_listObserver;
public:
	void Attach(CObserver *pObserver)
	{
		m_listObserver.push_back( pObserver );
	}
	void Notify()
	{
		CObserverList::iterator itr;
		for ( itr = m_listObserver.begin(); itr != m_listObserver.end(); ++itr )
		{
			(*itr)->Update( this );
		}
	}
	virtual const NDAS_SYNC_REPORT *GetReport() = 0;
};

//
// An abstract observer class used when the notification should be 
// sent to different thread. When notification comes, event is fired after
// _Update method is called. 
// Since the working thread can fire event more than once before
// the observer thread gets it, be sure to make report not to
// be overwritten by other notification.
//
class CMultithreadedObserver : public CObserver
{
protected:
	CRITICAL_SECTION	m_csSync;
	HANDLE				m_hSyncEvent;
	UINT				m_nReportSize;
	boost::shared_ptr<BYTE>	m_pbReport;
public:
	CMultithreadedObserver();
	virtual ~CMultithreadedObserver();
	virtual void Update(CSubject *pSubject);
	virtual UINT GetReportSize();
	virtual void GerReport(NDAS_SYNC_REPORT *pReportBuffer);
};

#endif // _NDASOBSERVER_H_