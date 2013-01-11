/*++

Copyright (C) 2005 XIMETA, Inc.
All rights reserved.

Date:   October, 2005
Author: Chesong Lee

--*/
#pragma once
#include <windows.h>
#include <tchar.h>
#include "xtlmemcallback.h"

namespace XTL
{

template <typename T, typename P>
BOOL XtlEnumWindows(T* pInstance, BOOL (T::*pfn)(HWND, P), P p = P())
{
	typedef XTL::ICB2<T, HWND, LPARAM, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumWindows(
		AdapterT::CallbackProc,
		(LPARAM)&adapter);
	
}

template <typename T, typename P>
BOOL XtlEnumDesktops(HWINSTA hwinsta, T* pInstance, BOOL (T::*pfn)(LPTSTR, P), P p = P())
{
	typedef XTL::ICB2<T, LPTSTR, LPARAM, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumDesktops(
        hwinsta,					  
		AdapterT::CallbackProc,
		(LPARAM)&adapter);
}

template <typename T, typename P>
BOOL XtlEnumWindowStations(T* pInstance, BOOL (T::*pfn)(LPTSTR, P), P p = P())
{
	typedef XTL::ICB2<T, LPTSTR, LPARAM, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumWindowStations(
		AdapterT::CallbackProc,
		(LPARAM)&adapter);
	
}

template <typename T, typename P>
BOOL XtlEnumThreadWindows(DWORD dwThreadId, T* pInstance, BOOL (T::*pfn)(HWND, P), P p = P())
{
	typedef XTL::ICB2<T, HWND, LPARAM, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumThreadWindows(
        dwThreadId,
		AdapterT::CallbackProc,
		(LPARAM)&adapter);
}

template <typename T, typename P>
BOOL
XtlEnumChildWindows(HWND hWndParent, T* pInstance, BOOL (T::*pfn)(HWND, P), P p = P())
{
	typedef XTL::ICB2<T, HWND, LPARAM, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumChildWindows(
        hWndParent,
		AdapterT::CallbackProc,
		(LPARAM)&adapter);
}

template <typename T, typename P>
BOOL 
XtlEnumResourceTypes(
	HMODULE hModule,
	T* pInstance, BOOL (T::*pfn)(HMODULE, LPTSTR, P), P p = P())
{
	typedef XTL::ICB3<T, HMODULE, LPTSTR, LONG_PTR, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumResourceTypes(
		hModule,
		AdapterT::CallbackProc,
		(LONG_PTR)&adapter);
}

template <typename T, typename P>
BOOL 
XtlEnumResourceNames(
	HMODULE hModule, LPCTSTR lpszType,
	T* pInstance, BOOL (T::*pfn)(HMODULE, LPCTSTR, LPTSTR, P), P p = P())
{
	typedef XTL::ICB4<T, HMODULE, LPCTSTR, LPTSTR, LONG_PTR, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumResourceNames(
		hModule, lpszType, 
		AdapterT::CallbackProc,
		(LONG_PTR)&adapter);
}

template <typename T, typename P>
BOOL 
XtlEnumResourceLanguages(
	HMODULE hModule, LPCTSTR lpszType, LPCTSTR lpszName, 
	T* pInstance, BOOL (T::*pfn)(HMODULE, LPCTSTR, LPCTSTR, WORD, P), P p = P())
{
	typedef XTL::ICB5<T, HMODULE, LPCTSTR, LPCTSTR, WORD, LONG_PTR, P, BOOL> AdapterT;
	AdapterT adapter(pInstance, pfn, p);
	return ::EnumResourceLanguages(
		hModule, lpszType, lpszName,
		AdapterT::CallbackProc,
		(LONG_PTR)&adapter);
}

} // namespace XTL
