/*++

Copyright (C) 2005 XIMETA, Inc.
All rights reserved.

Date:   October, 2005
Author: Chesong Lee

--*/
#pragma once
#include "xtldef.h"

namespace XTL
{

template <typename T, typename P, typename U, typename ReturnT>
class ICB1
{
	typedef ReturnT (T::*MemberFuncT)(U);
	T* m_pInstance;
	MemberFuncT m_fn;
	U m_p;
public:
	ICB1(T* pInstance, MemberFuncT fn, U p = U()) : 
		m_pInstance(pInstance), m_fn(fn), m_p(p)
	{}
	static ReturnT CALLBACK CallbackProc(P p)
	{
		ICB1* pT = reinterpret_cast<ICB1*>(p);
		return (pT->m_pInstance->*(pT->m_fn))(pT->m_p);
	}
};

template <typename T, typename P1, typename P, typename U, typename ReturnT>
class ICB2
{
	typedef ReturnT (T::*MemberFuncT)(P1, U);
	T* m_pInstance;
	MemberFuncT m_fn;
	U m_p;
public:
	ICB2(T* pInstance, MemberFuncT fn, U p = U()) : 
		m_pInstance(pInstance), m_fn(fn), m_p(p)
	{}
	static ReturnT CALLBACK CallbackProc(P1 p1, P p)
	{
		ICB2* pT = reinterpret_cast<ICB2*>(p);
		return (pT->m_pInstance->*(pT->m_fn))(p1, pT->m_p);
	}
};

template <typename T, typename P1, typename P2, typename P, typename U, typename ReturnT>
class ICB3
{
	typedef ReturnT (T::*MemberFuncT)(P1, P2, U);
	T* m_pInstance;
	MemberFuncT m_fn;
	U m_p;
public:
	ICB3(T* pInstance, MemberFuncT fn, U p = U()) : 
		m_pInstance(pInstance), m_fn(fn), m_p(p)
	{}
	static ReturnT CALLBACK CallbackProc(P1 p1, P2 p2, P p)
	{
		ICB3* pT = reinterpret_cast<ICB3*>(p);
		return (pT->m_pInstance->*(pT->m_fn))(p1, p2, pT->m_p);
	}
};

template <typename T, typename P1, typename P2, typename P3, typename P, typename U, typename ReturnT>
class ICB4
{
	typedef ReturnT (T::*MemberFuncT)(P1, P2, P3, U);
	T* m_pInstance;
	MemberFuncT m_fn;
	U m_p;
public:
	ICB4(T* pInstance, MemberFuncT fn, U p = U()) : 
		m_pInstance(pInstance), m_fn(fn), m_p(p)
	{}
	static ReturnT CALLBACK CallbackProc(P1 p1, P2 p2, P3 p3, P p)
	{
		ICB4* pT = reinterpret_cast<ICB4*>(p);
		return (pT->m_pInstance->*(pT->m_fn))(p1, p2, p3, pT->m_p);
	}
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P, typename U, typename ReturnT>
class ICB5
{
	typedef ReturnT (T::*MemberFuncT)(P1, P2, P3, P4, U);
	T* m_pInstance;
	MemberFuncT m_fn;
	U m_p;
public:
	ICB5(T* pInstance, MemberFuncT fn, U p = U()) : 
		m_pInstance(pInstance), m_fn(fn), m_p(p)
	{}
	static ReturnT CALLBACK CallbackProc(P1 p1, P2 p2, P3 p3, P4 p4, P p)
	{
		ICB5* pT = reinterpret_cast<ICB5*>(p);
		return (pT->m_pInstance->*(pT->m_fn))(p1, p2, p3, p4, pT->m_p);
	}
};

template <typename T, typename P> class WaitOrTimerCallbackAdapter;

template <typename T, typename P>
struct WaitOrTimerCallback
{
	T* ClassInstance;
	void (T::*MemberFun)(P, BOOLEAN);
	WaitOrTimerCallback() : ClassInstance(0), MemberFun(0) {}
	WaitOrTimerCallback(T* ClassInstance, void (T::*MemberFun)(P, BOOLEAN)) :
		ClassInstance(ClassInstance), MemberFun(MemberFun)
	{}
};

template <typename T, typename P>
class WaitOrTimerCallbackAdapter
{
	typedef WaitOrTimerCallbackAdapter<T,P> ThisClass;
	typedef void (T::*MemberFunT)(P, BOOLEAN);
	ThisClass* m_pThis;
	HANDLE m_hWaitHandle;
	volatile bool m_final;
	WaitOrTimerCallback<T,P> m_cb;
	P m_context;
public:
	static void CALLBACK WaitOrTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
	{
		ThisClass* pT = static_cast<ThisClass*>(lpParameter);
		if (::IsBadWritePtr(pT, sizeof(ThisClass)) ||
			pT->m_pThis != pT)
		{
			::OutputDebugStringA("POST-FINALIZATION CALL\n");
			return;
		}
		::OutputDebugStringA("ENTER>>>WaitOrTimerCallback\n");
		(pT->m_cb.ClassInstance->*(pT->m_cb.MemberFun))(pT->m_context, TimerOrWaitFired);
		::OutputDebugStringA("LEAVE<<<WaitOrTimerCallback\n");
#define XTL_WAIT_DONT_FINALIZE
#if !defined(XTL_WAIT_DONT_FINALIZE)
		if (pT->m_final)
		{
			::OutputDebugStringA("---FINALIZATION---\n");
			XTLVERIFY( ::HeapFree(::GetProcessHeap(), HEAP_ZERO_MEMORY, pT) );
		}
#endif
	}
	static BOOL RegisterWaitForSingleObject(
		WaitOrTimerCallbackAdapter<T,P>** phNewWaitObject, 
		HANDLE hObject, 
		const XTL::WaitOrTimerCallback<T,P>& Callback,
		P Context,
		ULONG dwMilliseconds, 
		ULONG dwFlags = WT_EXECUTEDEFAULT)
	{
		ThisClass* pT = static_cast<ThisClass*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThisClass)));
		if (NULL == pT)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}
		HANDLE hWaitHandle;
		BOOL ret = ::RegisterWaitForSingleObject(
			&hWaitHandle, hObject, 
			ThisClass::WaitOrTimerCallback, pT, 
			dwMilliseconds, dwFlags);
		if (!ret)
		{
			XTLVERIFY( ::HeapFree(::GetProcessHeap(), HEAP_ZERO_MEMORY, pT) );
		}
		else
		{
			pT->m_pThis = pT;
			pT->m_hWaitHandle = hWaitHandle;
			pT->m_cb = Callback;
			pT->m_final = false;
			pT->m_context = Context;
			*phNewWaitObject = pT;
		}
		return ret;
	}
	static BOOL UnregisterWait(WaitOrTimerCallbackAdapter<T,P>* hWaitHandle)
	{
		BOOL ret = ::UnregisterWait(hWaitHandle->m_hWaitHandle);
		if (ret || ERROR_IO_PENDING == ::GetLastError())
		{
			hWaitHandle->m_final = true;
		}
		return ret;
	}
	static BOOL UnregisterWaitEx(WaitOrTimerCallbackAdapter<T,P>* hWaitHandle, HANDLE CompletionEvent)
	{
		BOOL ret = ::UnregisterWaitEx(hWaitHandle->m_hWaitHandle, CompletionEvent);
		if (ret || ERROR_IO_PENDING == ::GetLastError())
		{
			hWaitHandle->m_final = true;
		}
		return ret;
	}
};

template <typename T, typename P> 
class XtlWaitObject : private WaitOrTimerCallbackAdapter<T,P> {};

template <typename T, typename P>
inline 
BOOL 
XtlRegisterWaitForSingleObject(
	XtlWaitObject<T,P>** phNewWaitObject, 
	HANDLE hObject, 
	const WaitOrTimerCallback<T,P>& Callback,
	P Context,
	ULONG dwMilliseconds, 
	ULONG dwFlags = WT_EXECUTEDEFAULT)
{
	if (0 == phNewWaitObject || ::IsBadWritePtr(phNewWaitObject, sizeof(XtlWaitObject<T,P>*)))
	{
		XTLASSERT(FALSE && "NULL Pointer");
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>** ppWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>**>(phNewWaitObject);
	return WaitOrTimerCallbackAdapter<T,P>::RegisterWaitForSingleObject(
		ppWaitObject, hObject, Callback, Context, dwMilliseconds, dwFlags);
}

template <typename T, typename P>
inline 
BOOL
XtlUnregisterWaitEx(XtlWaitObject<T,P>* hWaitHandle, HANDLE CompletionEvent)
{
	if (::IsBadWritePtr(hWaitHandle, sizeof(XtlWaitObject<T,P>)))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>* pWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>*>(hWaitHandle);
	return WaitOrTimerCallbackAdapter<T,P>::UnregisterWaitEx(pWaitObject, CompletionEvent);
}

template <typename T, typename P>
inline 
BOOL
XtlUnregisterWait(XtlWaitObject<T,P>* hWaitHandle)
{
	if (::IsBadWritePtr(hWaitHandle, sizeof(XtlWaitObject<T,P>)))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>* pWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>*>(hWaitHandle);
	return WaitOrTimerCallbackAdapter<T,P>::UnregisterWait(pWaitObject);
}


template <typename T, typename P>
struct APCProcContext
{
	T* ClassInstance;
	void (T::*MemberFunc)(P);
	P Data;
	APCProcContext();
	APCProcContext(T* ClassInstance, void (T::*MemberFunc)(P), P Data) :
		ClassInstance(ClassInstance), MemberFunc(MemberFunc), Data(Data) 
	{}
};

template <typename T, typename P>
class APCProcAdapter
{
	typedef APCProcAdapter<T,P> ThisClass;
	typedef void (T::*MemberFunT)(P);
	APCProcContext<T,P> m_context;
public:
	static void CALLBACK APCProc(ULONG_PTR dwParam)
	{
		ThisClass* pT = reinterpret_cast<ThisClass*>(dwParam);
		(pT->m_context->ClassInstance>*(pT->m_context->MemberFunc))(pT->m_context->Data);
		XTLVERIFY( ::HeapFree(::GetProcessHeap(), 0, pT) );
	}
	static DWORD QueueUserAPC(
		HANDLE hThread,
		const APCProcContext<T,P>& Context)
	{
		ThisClass* pT = static_cast<ThisClass*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThisClass)));
		if (NULL == pT)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}
		pT->m_context = Context;
		DWORD ret = ::QueueUserAPC(ThisClass::APCProc, hThread, (ULONG_PTR)(pT));
		if (0 == ret)
		{
			XTLVERIFY( ::HeapFree(::GetProcessHeap(), 0, pT) );
		}
		return ret;
	}
};

template <typename T, typename P>
inline 
DWORD 
XtlQueueUserAPC(
	HANDLE hThread,
	T* ClassInstance,
	const APCProcContext<T,P>& Context)
{
	return APCProcAdapter<T,P>::QueueUserAPC(hThread, Context);
}


} // namespace XTL
