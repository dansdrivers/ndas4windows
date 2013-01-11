#pragma once
#include "syncobj.h"
#include <set>

namespace ximeta {

class CSubject;
class CObserver;

typedef CSubject* PCSubject;
typedef CObserver* PCObserver;

// __declspec(novtable)
class CObserver
{
public:
	virtual ~CObserver() {}
	virtual VOID Update(PCSubject pChangedSubject) = 0;
};


class CSubject
{
public:

	virtual VOID Attach(PCObserver pObserver)
	{
		CAutoLock autolock(&m_subjectDataLock);
		m_pObservers.insert(pObserver);
	}

	virtual VOID Detach(PCObserver pObserver)
	{
		CAutoLock autolock(&m_subjectDataLock);
		m_pObservers.erase(pObserver);
	}

	virtual VOID Notify()
	{
		CAutoLock autolock(&m_subjectDataLock);
		std::set<PCObserver>::const_iterator itr =
			m_pObservers.begin();

		for (; itr != m_pObservers.end(); itr++) {
			(*itr)->Update(this);
		}
	}

protected:
	CSubject() : m_subjectDataLock(CCritSecLock()) {}

private:
	CCritSecLock m_subjectDataLock;
	std::set<PCObserver> m_pObservers;
};

} // ximeta
