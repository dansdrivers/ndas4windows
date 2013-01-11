#pragma once

#ifndef _ASSERTE
#ifdef XLOCK_NO_ASSERT
#define _ASSERTE(x) \
	::Beep(750,300), \
	::OutputDebugStringA(#x "\n")
#else // XLOCK_NO_ASSERT
#error _ASSERTE is not defined. Include crtdbg.h first, or define XLOCK_NO_ASSERT to prevent this error.
#endif // XLOCK_NO_ASSERT
#endif

template <bool t_fUseSpinCount = true, DWORD t_dwSpinCount = 0x80004000>
class CCritSecLockBase
{
public:

	CRITICAL_SECTION m_cs;
	BOOL m_init;

	CCritSecLockBase() : m_init(FALSE)
	{
		::ZeroMemory(&m_cs, sizeof(CRITICAL_SECTION));
	}

	~CCritSecLockBase()
	{
		if (m_init)
		{
			::DeleteCriticalSection(&m_cs); 
		}
	}

	BOOL 
	Initialize()
	{
		if (m_init)
		{
			_ASSERTE(FALSE && "Lock already initialized");
			return TRUE;
		}

		if (t_fUseSpinCount)
		{
			m_init = ::InitializeCriticalSectionAndSpinCount(&m_cs,t_dwSpinCount);
			return m_init;
		}
		else
		{
			//
			// In low memory situations, InitializeCriticalSection can 
			// raise a STATUS_NO_MEMORY exception.
			//
			__try
			{
				::InitializeCriticalSection(&m_cs);
			}
			__except (::GetExceptionCode() == STATUS_NO_MEMORY ?
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				::SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}
			return m_init = TRUE;
		}
	}

	void 
	Lock() 
	{ 
		//
		// The lock without spin count is subject to the exception
		// EXCEPTION_POSSIBLE_DEADLOCK
		//
		// Windows 2000/NT: In low memory situation, also it may
		// raise a STATUS_NO_MEMORY exception.
		//
		if (m_init)
		{
			::EnterCriticalSection(&m_cs); 
		}
		else
		{
			_ASSERTE(FALSE && "Not not initialized");
		}
	}

	void 
	Unlock() 
	{	
		if (m_init)
		{
			::LeaveCriticalSection(&m_cs); 
		}
		else
		{
			_ASSERTE(FALSE && "Not not initialized");
		}
	}
};

typedef CCritSecLockBase<false,0> CCritSecLock;
typedef CCritSecLockBase<true,0x80004000> CCritSecLockWithSpinCount;

template <typename T>
class CAutoLock
{
protected:

	T* m_pLock;

public:

	CAutoLock(T* plock) : 
		m_pLock(plock) 
	{ 
		_ASSERTE(m_pLock); 
		m_pLock->Lock(); 
	}

	~CAutoLock() 
	{ 
		if (m_pLock) 
		{ 
			m_pLock->Unlock(); m_pLock = NULL; 
		} 
	}

	void 
	Release() 
	{ 
		if (m_pLock) 
		{ 
			m_pLock->Unlock(); 
			m_pLock = NULL; 
		} 
	}

};

typedef CAutoLock<CCritSecLock> CAutoCritSecLock;
