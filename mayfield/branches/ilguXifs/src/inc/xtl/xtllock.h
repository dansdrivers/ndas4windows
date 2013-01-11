#pragma once
#include "xtldef.h"
#include "xtltrace.h"
#include "xtlautores.h"

namespace XTL
{

#ifndef XTL_DEBUG_READERWRITERLOCK
#define XTL_DEBUG_READERWRITERLOCK 1
#endif

class CReaderWriterLock
{
public:
	CReaderWriterLock();
	bool Initialize();
	bool Uninitialize();

	void LockReader();
	void UnlockReader();
	void LockWriter();
	void UnlockWriter();
private:
	AutoObjectHandle m_hReaderLock;
	AutoObjectHandle m_hWriterSem;
	LONG m_nReaders;
#if XTL_DEBUG_READERWRITERLOCK
	DWORD m_writerThreadId;
#endif
};

inline
CReaderWriterLock::CReaderWriterLock() :
	m_hReaderLock(NULL),
	m_hWriterSem(NULL),
	m_nReaders(0),
#if XTL_DEBUG_READERWRITERLOCK
	m_writerThreadId(0)
#endif
{
}

inline 
bool 
CReaderWriterLock::Initialize()
{
	AutoObjectHandle hReaderLock = ::CreateMutex(NULL, FALSE, NULL);
	if (hReaderLock.IsInvalid())
	{
		return false;
	}
	AutoObjectHandle hWriterSem = ::CreateSemaphore(NULL, 1, 1, NULL);
	if (hWriterSem.IsInvalid())
	{
		return false;
	}
	m_hReaderLock.Attach(hReaderLock.Detach());
	m_hWriterSem.Attach(hWriterSem.Detach());
	return true;
}

inline 
bool
CReaderWriterLock::Uninitialize()
{
	m_hReaderLock.Release();
	m_hWriterSem.Release();
}

inline 
void
CReaderWriterLock::LockReader()
{
//	XTLCALLTRACE();
	XTLVERIFY( ::WaitForSingleObject(m_hReaderLock, INFINITE) == WAIT_OBJECT_0);
	if (1 == ::InterlockedIncrement(&m_nReaders))
	{
		// First Reader
		XTLVERIFY(::WaitForSingleObject(m_hWriterSem, INFINITE) == WAIT_OBJECT_0);
	}
	XTLVERIFY( ::ReleaseMutex(m_hReaderLock) );
}

inline
void
CReaderWriterLock::UnlockReader()
{
//	XTLCALLTRACE();
	LONG nNewReaders = ::InterlockedDecrement(&m_nReaders);
	if (0 == nNewReaders)
	{
		// Last Reader
		XTLVERIFY(::ReleaseSemaphore(m_hWriterSem,1,NULL));
	}
}

inline
void
CReaderWriterLock::LockWriter()
{
//	XTLCALLTRACE();
	XTLVERIFY(::WaitForSingleObject(m_hWriterSem,INFINITE) == WAIT_OBJECT_0);
#if XTL_DEBUG_READERWRITERLOCK
	XTLASSERT(0 == m_writerThreadId);
	m_writerThreadId = ::GetCurrentThreadId();
#endif
}

inline 
void
CReaderWriterLock::UnlockWriter()
{
//	XTLCALLTRACE();
#if XTL_DEBUG_READERWRITERLOCK
	XTLASSERT(::GetCurrentThreadId() == m_writerThreadId);
	m_writerThreadId = 0;
#endif
	XTLVERIFY(::ReleaseSemaphore(m_hWriterSem, 1, NULL));
}

template <typename T>
class CReaderLockHolderT
{
public:
	CReaderLockHolderT(T* pInstance) : m_pInstance(pInstance)
	{
		m_pInstance->LockReader();
	}
	CReaderLockHolderT(T& instance) : m_pInstance(&instance)
	{
		m_pInstance->LockReader();
	}
	~CReaderLockHolderT()
	{
		m_pInstance->UnlockReader();
	}
protected:
	T* const m_pInstance;
private:
	CReaderLockHolderT();
	CReaderLockHolderT(const CReaderLockHolderT&);
	const CReaderLockHolderT& operator=(const CReaderLockHolderT&);
};

template <typename T>
class CWriterLockHolderT
{
public:
	CWriterLockHolderT(T* pInstance) : m_pInstance(pInstance)
	{
		m_pInstance->LockWriter();
	}
	CWriterLockHolderT(T& instance) : m_pInstance(&instance)
	{
		m_pInstance->LockWriter();
	}
	~CWriterLockHolderT()
	{
		m_pInstance->UnlockWriter();
	}
protected:
	T* const m_pInstance;
private:
	CWriterLockHolderT();
	CWriterLockHolderT(const CWriterLockHolderT&);
	const CWriterLockHolderT& operator=(const CWriterLockHolderT&);
};

class CReaderLockHolder : public CReaderLockHolderT<CReaderWriterLock> 
{
public:
	CReaderLockHolder(CReaderWriterLock* pInstance) : 
		CReaderLockHolderT<CReaderWriterLock>(pInstance) {}
	CReaderLockHolder(CReaderWriterLock& instance) : 
		CReaderLockHolderT<CReaderWriterLock>(instance) {}
};
class CWriterLockHolder : public CWriterLockHolderT<CReaderWriterLock> 
{
public:
	CWriterLockHolder(CReaderWriterLock* pInstance) : 
		CWriterLockHolderT<CReaderWriterLock>(pInstance) {}
	CWriterLockHolder(CReaderWriterLock& instance) : 
		CWriterLockHolderT<CReaderWriterLock>(instance) {}
};

}
