/*++

Copyright (C) 2005 XIMETA, Inc.
All rights reserved.

Date:   October, 2005
Author: Chesong Lee

--*/
#pragma once
#include <windows.h>
#include <process.h>
#include "xtldef.h"

namespace XTL 
{

template <typename T>
class CRTThreadTraits
{
public:
	static HANDLE CreateThread(
		T* ClassInstance,
		LPSECURITY_ATTRIBUTES Security, 
		DWORD StackSize,
		DWORD Flags,
		LPDWORD ThreadId)
	{
		return reinterpret_cast<HANDLE>(
			::_beginthreadex(
				Security, StackSize, 
				ThreadStartProc, ClassInstance,
				Flags, reinterpret_cast<unsigned int*>(ThreadId)));
	}
	static HANDLE CreateThreadEx(
		T* ClassInstance,
		DWORD (T::*MemberFunc)(LPVOID),
		LPVOID FuncParam,
		LPSECURITY_ATTRIBUTES Security, 
		DWORD StackSize,
		DWORD Flags,
		LPDWORD ThreadId) throw()
	{
		ThreadParamEx* pex = new ThreadParamEx(ClassInstance, MemberFunc, FuncParam);
		if (NULL == pex)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return NULL;
		}
		HANDLE h = reinterpret_cast<HANDLE>(
			::_beginthreadex(
                Security, StackSize, 
				ThreadStartProcEx, pex, Flags, 
				reinterpret_cast<unsigned int*>(ThreadId)));
		if (NULL == h)
		{
			delete pex;
			return NULL;
		}
		return h;
	}
private:
	static unsigned int __stdcall ThreadStartProc(void* pArgs)
	{
		T* ClassInstance = static_cast<T*>(pArgs);
		return ClassInstance->ThreadStart(NULL);
	}
	static unsigned int __stdcall ThreadStartProcEx(void* pArgs)
	{
		ThreadParamEx* pex = static_cast<ThreadParamEx*>(pArgs);
		DWORD ret = (pex->ClassInstance->*(pex->MemberFunc))(pex->FuncParam);
		delete pex;
		return static_cast<int>(ret);
	}
	struct ThreadParamEx
	{
		T* ClassInstance;
		DWORD (T::*MemberFunc)(LPVOID);
		LPVOID FuncParam;
		ThreadParamEx(T* ClassInstance, DWORD (T::*MemberFunc)(LPVOID), LPVOID FuncParam) throw():
			ClassInstance(ClassInstance), MemberFunc(MemberFunc), FuncParam(FuncParam)
		{}
	};
};

template <typename T>
class Win32ThreadTraits
{
public:
	static HANDLE CreateThread(
		T* ClassInstance,
		LPSECURITY_ATTRIBUTES Security, 
		DWORD StackSize,
		DWORD Flags,
		LPDWORD ThreadId)
	{
		return ::CreateThread(Security, StackSize, ThreadStartProc, ClassInstance,	Flags, ThreadId);
	}
	static HANDLE CreateThreadEx(
		T* ClassInstance,
		DWORD (T::*MemberFunc)(LPVOID),
		LPVOID FuncParam,
		LPSECURITY_ATTRIBUTES Security, 
		DWORD StackSize,
		DWORD Flags,
		LPDWORD ThreadId) throw()
	{
		ThreadParamEx* pex = static_cast<ThreadParamEx*>(::HeapAlloc(::GetProcessHeap(), 0, sizeof(ThreadParamEx)));
		if (NULL == pex)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return NULL;
		}
		pex->ClassInstance = ClassInstance;
		pex->MemberFunc = MemberFunc;
		pex->FuncParam = FuncParam;
		HANDLE h = ::CreateThread(Security, StackSize, ThreadStartProcEx, pex, Flags, ThreadId);
		if (NULL == h)
		{
			XTLVERIFY( ::HeapFree(::GetProcessHeap(), 0, pex) );
			return NULL;
		}
		return h;
	}
private:
	static DWORD WINAPI ThreadStartProc(LPVOID pArgs)
	{
		T* ClassInstance = static_cast<T*>(pArgs);
		return ClassInstance->ThreadStart(NULL);
	}
	static DWORD WINAPI ThreadStartProcEx(LPVOID pArgs)
	{
		ThreadParamEx* pex = static_cast<ThreadParamEx*>(pArgs);
		DWORD ret = (pex->ClassInstance->*(pex->MemberFunc))(pex->FuncParam);
		XTLVERIFY( ::HeapFree(::GetProcessHeap(), 0, pex) );	
		return static_cast<int>(ret);
	}
	struct ThreadParamEx
	{
		T* ClassInstance;
		DWORD (T::*MemberFunc)(LPVOID);
		LPVOID FuncParam;
	};
};

#if !defined(_XTL_MIN_CRT) && defined(_MT)
template <typename T> class DefaultThreadTraits : public CRTThreadTraits<T> {};
#else
template <typename T> class DefaultThreadTraits : public Win32ThreadTraits<T> {};
#endif

template <typename T, typename ThreadType = DefaultThreadTraits<T>, bool managed = true> class CThreadT;
template <typename T, typename ThreadType = DefaultThreadTraits<T> > class CThread;
template <typename T, typename ThreadType = DefaultThreadTraits<T> > class CThreadHandle;
template <typename T> class CCRTThread;
template <typename T> class CCRTThreadHandle;
template <typename T> class CWin32Thread;
template <typename T> class CWin32ThreadHandle;

template <typename T, typename ThreadTraits, bool managed>
class CThreadT
{
public:

	CThreadT() throw():
		m_hThread(NULL)
	{

	}

	~CThreadT()
	{
#pragma warning(disable: 4127)
		if (m_hThread && managed)
		{
			::CloseHandle(m_hThread);
		}
#pragma warning(default: 4127)
	}

	BOOL CreateThread(
	    T* ClassInstance,
		LPSECURITY_ATTRIBUTES Security = 0, 
		DWORD StackSize = 0, 
		DWORD Flags = 0)
	{
		XTLASSERT(NULL == m_hThread);
		DWORD dwThreadId;
		m_hThread = ThreadTraits::CreateThread(
			ClassInstance, Security, StackSize, Flags, &dwThreadId);
		return (NULL != m_hThread);
	}

	BOOL CreateThreadParam(
	    T* ClassInstance,
		LPVOID FuncParam,
		LPSECURITY_ATTRIBUTES Security = 0, 
		DWORD StackSize = 0, 
		DWORD Flags = 0)
	{
		XTLASSERT(NULL == m_hThread);
		return CreateThreadEx(
			ClassInstance, &T::ThreadStart, p,
			Security, StackSize, Flags);
	}

	BOOL CreateThreadEx(
		T* ClassInstance,
		DWORD (T::*MemberFunc)(LPVOID),
		LPVOID FuncParam,
		LPSECURITY_ATTRIBUTES Security = 0, 
		DWORD StackSize = 0, 
		DWORD Flags = 0)
	{
		XTLASSERT(NULL == m_hThread);
		DWORD dwThreadId;
		m_hThread = ThreadTraits::CreateThreadEx(
			ClassInstance, MemberFunc, FuncParam, 
			Security, StackSize, Flags, &dwThreadId);
		return (NULL != m_hThread);
	}

	HANDLE GetThreadHandle() throw()
	{
		return m_hThread;
	}

	DWORD GetThreadId() throw()
	{
		return ::GetThreadId(m_hThread);
	}

	operator HANDLE() throw()
	{
		return GetThreadHandle();
	}

	DWORD_PTR SetThreadAffinityMask(DWORD_PTR ThreadAffinityMask)
	{
		return ::SetThreadAffinityMask(m_hThread, ThreadAffinityMask);
	}

	BOOL SetThreadPriority(int nPriority)
	{
		return ::SetThreadPriority(m_hThread, nPriority);
	}

	BOOL SetThreadPriorityBoost(BOOL DisablePriorityBoost)
	{
		return ::SetThreadPriorityBoost(m_hThread, DisablePriorityBoost);
	}

	BOOL SetThreadIdealProcessor(DWORD IdealProcessor)
	{
		return ::SetThreadIdealProcessor(m_hThread, IdealProcessor);
	}

	DWORD Wait(DWORD dwMilliseconds = INFINITE)
	{
		return ::WaitForSingleObject(m_hThread, dwMilliseconds);
	}

protected:

	HANDLE m_hThread;
};

template <typename T, typename ThreadTraits>
class CThread : public CThreadT<T, ThreadTraits, true>
{
};

template <typename T, typename ThreadTraits>
class CThreadHandle : public CThreadT<T, ThreadTraits, false>
{
};

template <typename T>
class CWin32Thread : public CThreadT<T, Win32ThreadTraits<T>, true >
{
};

template <typename T>
class CWin32ThreadHandle : public CThreadT<T, Win32ThreadTraits<T>, false>
{
};

template <typename T>
class CCRTThread : public CThreadT<T, CRTThreadTraits<T>,  true>
{
};

template <typename T>
class CCRTThreadHandle : public CThreadT<T, CRTThreadTraits<T>,  false>
{
};

template <typename T, typename ThreadTraits = DefaultThreadTraits<T> > class CThreadedWorker;
template <typename T> class CWin32ThreadedWorker : public CThreadedWorker<T, Win32ThreadTraits<T> > {};
template <typename T> class CCRTThreadedWorker : public CThreadedWorker<T, CRTThreadTraits<T> > {};

template <typename T, typename ThreadTraits>
class CThreadedWorker : public CThread<T, ThreadTraits>
{
	typedef CThread<T, ThreadTraits> BaseClass;

	DWORD ThreadStart(LPVOID Param)
	{
		XTLC_ASSERT(FALSE && "Derived class should implement this function");
	}

public:

	BOOL CreateThread(
		LPSECURITY_ATTRIBUTES Security = 0, 
		DWORD StackSize = 0, 
		DWORD Flags = 0)
	{
		T* pT = static_cast<T*>(this);
		return BaseClass::CreateThread(pT, Security, StackSize, Flags);
	}

	BOOL CreateThreadParam(
		LPVOID Param,
		LPSECURITY_ATTRIBUTES Security = 0,
		DWORD StackSize = 0,
		DWORD Flags = 0)
	{
		T* pT = static_cast<T*>(this);
		return BaseClass::CreateThreadEx(pT, &T::ThreadStart, Param, Security, StackSize, Flags);
	}

	BOOL CreateThreadEx(
		DWORD (T::*MemberFunc)(LPVOID),
		LPVOID FuncParam,
		LPSECURITY_ATTRIBUTES Security = 0, 
		DWORD StackSize = 0, 
		DWORD Flags = 0)
	{
		T* pT = static_cast<T*>(this);
		return BaseClass::CreateThreadEx(pT, MemberFunc, Param, Security, StackSize, Flags);
	}
};

template <typename T, typename P>
class Win32UserWorkItemTraits
{
public:
	static BOOL 
	QueueUserWorkItemEx(
		T* ClassInstance,
		DWORD (T::*MemberFunc)(P),
		P FuncParam,
		ULONG Flags) throw()
	{
		WorkItemParamEx* pex = s