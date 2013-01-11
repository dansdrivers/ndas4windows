/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once

namespace ximeta {

struct ILock
{
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
};

class CCritSecLockObject
{
public:
	CRITICAL_SECTION cs;
	CCritSecLockObject() { ::InitializeCriticalSection(&cs); }
	~CCritSecLockObject() { ::DeleteCriticalSection(&cs); }
};

class CCritSecLockGlobal : public ILock
{
protected:

	//
	// BUG:
	//
	// What happened to this lock?
	// Seems that the whole lock is shared.
	// Yes. Right, at this time,
	// The implementation is Dead-lock prone,
	// and it should be solved. Then this will be fixed.
	// 

	static BOOL s_bInit;
	static CRITICAL_SECTION m_cs;

public:
	CCritSecLockGlobal()
	{
		if (s_bInit == FALSE) {
			::InitializeCriticalSection(&m_cs);
			s_bInit = TRUE;
		}
		// if (!::InitializeCriticalSectionAndSpinCount(&m_cs,0x80004000)) throw; 
	}

	virtual ~CCritSecLockGlobal() 
	{
		// ::DeleteCriticalSection(&m_cs); 
	}

	void Lock() { ::EnterCriticalSection(&m_cs); }
	void Unlock() {	::LeaveCriticalSection(&m_cs); }
};

#ifdef DEADLOCK_FIXED
class CCritSecLock : public ILock
{
protected:

	CRITICAL_SECTION m_cs;

public:

	CCritSecLock()
	{
		::InitializeCriticalSection(&m_cs);
		// if (!::InitializeCriticalSectionAndSpinCount(&m_cs,0x80004000)) throw; 
	}

	virtual ~CCritSecLock() 
	{
		::DeleteCriticalSection(&m_cs); 
	}

	void Lock() { ::EnterCriticalSection(&m_cs); }
	void Unlock() {	::LeaveCriticalSection(&m_cs); }
};
#else
typedef CCritSecLockGlobal CCritSecLock;
#endif

class CAutoLock
{
protected:

	ILock* m_pLock;

public:

	CAutoLock(ILock* plock) : m_pLock(plock) { _ASSERTE(m_pLock); m_pLock->Lock(); }
	~CAutoLock() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }
	void Release() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }

};


} // ximeta
