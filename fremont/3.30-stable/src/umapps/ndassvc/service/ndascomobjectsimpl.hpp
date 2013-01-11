#pragma once
#include "ndascomobjects.hpp"

template <typename T>
class ATL_NO_VTABLE ILockImpl : public T
{
protected:
	CRITICAL_SECTION m_cs;
	// HANDLE m_mutex;
public:

	ILockImpl()
	{
		InitializeCriticalSection(&m_cs);
	}

	~ILockImpl() 
	{
		DeleteCriticalSection(&m_cs); 
	}

	STDMETHOD_(void, LockInstance)()
	{ 
		EnterCriticalSection(&m_cs);
	}

	STDMETHOD_(void, UnlockInstance)()
	{	
		LeaveCriticalSection(&m_cs); 
	}
};

class CLockImpl {};

class CLock : public ILockImpl<CLockImpl> {};

template <typename T = ILock> class ILockImpl;

template <typename T = ILock> class CAutoLock;

template <typename T>
class CAutoLock
{
protected:

	T* m_pLock;

public:

	explicit CAutoLock(T* plock) : m_pLock(plock) 
	{
		_ASSERTE(m_pLock);
		m_pLock->LockInstance(); 
	}
	~CAutoLock()
	{ 
		if (m_pLock) 
		{ 
			m_pLock->UnlockInstance(); m_pLock = NULL; 
		} 
	}
	void Release() 
	{
		if (m_pLock) 
		{ 
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

template <typename T>
class CAutoReadLockT
{
protected:

	T* m_pLock;

public:

	explicit CAutoReadLockT(T* plock) : m_pLock(plock) 
	{
		_ASSERTE(m_pLock);
		m_pLock->ReadLock(); 
	}

	~CAutoReadLockT()
	{ 
		if (m_pLock) 
		{ 
			m_pLock->ReadUnlock(); m_pLock = NULL; 
		} 
	}
	void Release() 
	{
		if (m_pLock) 
		{ 
			m_pLock->ReadUnlock(); m_pLock = NULL; 
		}
	}
};

template <typename T>
class CAutoWriteLockT
{
protected:

	T* m_pLock;

public:

	explicit CAutoWriteLockT(T* plock) : m_pLock(plock) 
	{
		_ASSERTE(m_pLock);
		m_pLock->WriteLock(); 
	}
	~CAutoWriteLockT()
	{ 
		if (m_pLock) 
		{ 
			m_pLock->WriteUnlock(); 
			m_pLock = NULL; 
		} 
	}
	void Release() 
	{
		if (m_pLock) 
		{ 
			m_pLock->WriteUnlock(); 
			m_pLock = NULL; 
		}
	}
};

typedef CAutoReadLockT<CReadWriteLock> CAutoReadLock;
typedef CAutoWriteLockT<CReadWriteLock> CAutoWriteLock;

template <typename ContainerT, typename FunctorT> 
inline FunctorT AtlForEach(const ContainerT& _Container, FunctorT _Functor)
{	
	size_t count = _Container.GetCount();
	// perform function for each element
	for (size_t index = 0; index < count; ++index)
	{
		_Functor(_Container.GetAt(index));
	}
	return (_Functor);
}

template <typename ContainerT, typename FunctorT> 
inline size_t AtlFindIf(const ContainerT& _Container, FunctorT _Functor)
{	
	size_t count = _Container.GetCount();
	// perform function for each element
	for (size_t index = 0; index < count; ++index)
	{
		if (_Functor(_Container.GetAt(index)))
		{
			return index;
		}
	}
	return count;
}

template <typename ContainerT, typename FunctorT> 
inline size_t AtlRemoveIf(const ContainerT& _Container, FunctorT _Functor)
{	
	size_t removed = 0;
	size_t count = _Container.GetCount();
	// perform function for each element
	for (size_t index = 0; index < count; ++index)
	{
		if (_Functor(_Container.GetAt(index)))
		{
			_Container.RemoveAt(index);
			--index; --count;
		}
	}
	return removed;
}

extern const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_WINDOWS_RO;

extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_WINDOWS_RO;
