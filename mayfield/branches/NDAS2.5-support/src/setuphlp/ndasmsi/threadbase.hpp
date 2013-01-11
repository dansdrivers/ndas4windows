#pragma once
#include <windows.h>
//
// Thread Base Template Class
//
// This class is a template base for threading classes.
// The implementation class should provide a function:
//
// DWORD ThreadMain();
//
// June 30, 2005 Chesong Lee 
//

class CThreadBaseData
{
public:
	HANDLE m_hThread;
	DWORD m_dwThreadId;
};

template <typename T>
class CWinThreadBase : public CThreadBaseData
{
	static DWORD WINAPI ThreadProc(LPVOID lpContext)
	{
		T* pThis = reinterpret_cast<T*>(lpContext);
		// C++ code should not call ExitThread(ret);
		return pThis->ThreadMain();
	}
public:
	BOOL Create(
		LPSECURITY_ATTRIBUTES lpSecAttr = NULL,
		SIZE_T dwStackSize = 0,
		DWORD dwCreationFlags = 0)
	{
		m_hThread = ::CreateThread(
			lpSecAttr,
			dwStackSize,
			T::ThreadProc,
			this,
			dwCreationFlags,
			&m_dwThreadId);
		return NULL != m_hThread;
	}
};

#ifdef _MT
#include <process.h>
template <typename T>
class CCrtThreadBase : public CThreadBaseData
{
	static unsigned int __stdcall ThreadProc(void* context)
	{
		T* pThis = reinterpret_cast<T*>(context);
		unsigned int ret =  pThis->ThreadMain();
		_endthreadex(ret);
		return ret;
	}
public:
	BOOL Create(
		LPSECURITY_ATTRIBUTES lpSecAttr = NULL,
		SIZE_T dwStackSize = 0,
		DWORD dwCreationFlags = 0)
	{
		unsigned int* pThreadId = reinterpret_cast<unsigned int*>(&m_dwThreadId);
		m_hThread = (HANDLE)::_beginthreadex(
			lpSecAttr,
			(unsigned int) dwStackSize,
			T::ThreadProc,
			this,
			dwCreationFlags,
			pThreadId);
		return 0 != m_hThread;
	}
};
#endif

#ifdef _MT
template <typename T, typename ThreadBaseImpl = CCrtThreadBase<T> >
class CThreadBase : public ThreadBaseImpl
{
};
#else
template <typename T, typename ThreadBaseImpl = CWinThreadBase<T> >
class CThreadBase : public ThreadBaseImpl
{
};
#endif
