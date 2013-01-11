/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once

namespace ximeta {

class CCritSecLock
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
	CCritSecLock()
	{
		if (s_bInit == FALSE) {
			::InitializeCriticalSection(&m_cs);
			s_bInit = TRUE;
		}
		// if (!::InitializeCriticalSectionAndSpinCount(&m_cs,0x80004000)) throw; 
	}

	virtual ~CCritSecLock() 
	{
		// ::DeleteCriticalSection(&m_cs); 
	}

	void Lock() { ::EnterCriticalSection(&m_cs); }
	void Unlock() {	::LeaveCriticalSection(&m_cs); }
};

class CAutoLock
{
protected:
	CCritSecLock* m_pLock;
public:
	CAutoLock(CCritSecLock* plock) : m_pLock(plock) { _ASSERTE(m_pLock); m_pLock->Lock(); }
	~CAutoLock() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }
	VOID Release() { if (m_pLock) { m_pLock->Unlock(); m_pLock = NULL; } }
};


} // ximeta
