/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <atlexcept.h>

namespace ximeta {

struct ILock
{
	virtual void LockInstance() = 0;
	virtual void UnlockInstance() = 0;
	virtual bool IsInstanceLocked() = 0;
};

class CLockImpl : public ILock
{
protected:
	// CRITICAL_SECTION m_cs;
	HANDLE m_mutex;
public:

	CLockImpl() : m_mutex(CreateMutex(NULL, FALSE, NULL))
	{
		if (NULL == m_mutex)
		{
			AtlThrow(HRESULT_FROM_WIN32(GetLastError()));
		}
		//::InitializeCriticalSection(&m_cs);
	}

	~CLockImpl() 
	{
		ATLVERIFY(CloseHandle(m_mutex));
		//::DeleteCriticalSection(&m_cs); 
	}

	void LockInstance()
	{ 
		ATLVERIFY(WAIT_OBJECT_0 == WaitForSingleObject(m_mutex, INFINITE));
		// ::EnterCriticalSection(&m_cs);
	}

	void UnlockInstance()
	{	
		ATLVERIFY(ReleaseMutex(m_mutex));
		// ::LeaveCriticalSection(&m_cs); 
	}

	bool IsInstanceLocked()
	{
		DWORD waitResult = WaitForSingleObject(m_mutex, 0);
		if (WAIT_OBJECT_0 == waitResult)
		{
			ATLVERIFY(ReleaseMutex(m_mutex));
			return true;
		}
		ATLVERIFY(WAIT_TIMEOUT == waitResult);
		return false;
	}
};


template <typename T>
class CAutoLock
{
protected:

	T* m_pLock;

public:

	CAutoLock(T* plock) : m_pLock(plock) {
		_ASSERTE(m_pLock);
		m_pLock->LockInstance(); 
	}
#ifdef BOOST_SHARED_PTR_HPP_INCLUDED
	CAutoLock(boost::shared_ptr<T> p) : m_pLock(p.get()) {
		m_pLock->LockInstance(); 
	}
#endif
	~CAutoLock() { 
		if (m_pLock) { 
			m_pLock->UnlockInstance(); m_pLock = NULL; 
		} 
	}
	void Release() {
		if (m_pLock) { 
			m_pLock->UnlockInstance(); m_pLock = NULL; 
		}
	}

};

//
// This is a modified version of RWMonitor from 
// http://www.viksoe.dk/wtldoc/
//

class CReadWriteLock
{
private:

	volatile LONG m_nReaders;
	volatile LONG m_nWriters;

public:

	CReadWriteLock() : m_nReaders(0), m_nWriters(0)
	{
		m_nReaders = 0;
		m_nWriters = 0;
	}

	void ReadLock()
	{
		while (TRUE)
		{
			::InterlockedIncrement(&m_nReaders);
			if(0 == m_nWriters)
			{
				return;
			}
			::InterlockedDecrement(&m_nReaders);
			::Sleep(0);
		}
	}

	void WriteLock()
	{
		while (TRUE) 
		{
			while (TRUE) 
			{
				if( ::InterlockedExchange( &m_nWriters, 1 ) == 1 ) 
				{
					::Sleep(0);
				}
				else 
				{
					while( m_nReaders != 0 )
					{
						::Sleep(0);
					}
					return;
				}
			}
		}
	}

	void ReadUnlock()
	{
		LONG readers = ::InterlockedDecrement(&m_nReaders);
		_ASSERTE(readers >= 0);
	}

	void WriteUnlock()
	{
		LONG writers = ::InterlockedDecrement(&m_nWriters);
		_ASSERTE(0 == writers);
	}
};


} // ximeta
