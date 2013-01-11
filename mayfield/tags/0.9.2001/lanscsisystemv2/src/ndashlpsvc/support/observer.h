#pragma once
#include "syncobj.h"
#include <list>

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
	virtual void Update(PCSubject pChangedSubject) = 0;
};


class CSubject
{
public:

	virtual void Attach(PCObserver pObserver)
	{
		CAutoLock autolock(&m_subjectDataLock);
		m_listObserver.push_back(pObserver);
	}

	virtual void Detach(PCObserver pObserver)
	{
		CAutoLock autolock(&m_subjectDataLock);
		m_listObserver.remove(pObserver);
	}

	virtual void Notify()
	{
		CAutoLock autolock(&m_subjectDataLock);
		std::list<PCObserver>::const_iterator itr =
			m_listObserver.begin();

		for (; itr != m_listObserver.end(); itr++) {
			(*itr)->Update(this);
		}
	}

protected:
	CSubject() : m_subjectDataLock(CCritSecLock()) {}

private:
	CCritSecLock m_subjectDataLock;
	std::list<PCObserver> m_listObserver;
};

} // ximeta
