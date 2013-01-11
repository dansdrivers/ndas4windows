/*++

Copyright (C) 2005 XIMETA, Inc.
All rights reserved.

Date:   October, 2005
Author: Chesong Lee

--*/
#pragma once
#ifndef _XTLRES_H_
#define _XTLRES_H_
#include <windows.h>
#include "xtldef.h"

namespace XTL 
{


template <typename T, typename Config = AutoResourceConfigT<T> > class AutoResourceT;
template <typename T, typename Config> class AutoCleanupT;

template <typename T, typename Config>
class AutoCleanupT
{
public:

	AutoCleanupT() throw() : m_value(Config::GetInvalidValue()) {}
	AutoCleanupT(const T& value) throw() : m_value(value) {}
	~AutoCleanupT() { Config::Release(m_value); }

	bool IsInvalid() throw()
	{
		return (Config::GetInvalidValue() == m_value);
	}

	operator T() throw()
	{
		return m_value;
	}
	
private:
	// hide copy constructor
	AutoCleanupT(const AutoCleanupT &);
	// hide assignment operator
	AutoCleanupT& operator = (const AutoCleanupT&);
protected:
	T m_value;
};

template <typename T, typename Config>
class AutoResourceT
{
public:
	// Takes ownership of passed resource, or initializes internal resource
	// member of invalid value
	//	explicit AutoResourceT(T resource = Config::GetInvalidValue()) :
	//		m_resource(resource)
	//	{
	//	}

	AutoResourceT() throw() :
		m_resource(Config::GetInvalidValue())
	{
	}

	AutoResourceT(const T& resource) throw()
	{
		// XTLASSERT(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// If owns a valid resource, release it using the Config::Release
	~AutoResourceT()
	{
		if (!IsInvalid())
		{
			Config::Release(m_resource);
		}
	}

	bool IsInvalid() const throw()
	{
		return (Config::GetInvalidValue() == m_resource);
	}

	T* operator&() throw()
	{
		XTLASSERT(m_resource == Config::GetInvalidValue());
		return &m_resource;
	}
	
	const T* operator&() const throw()
	{
		XTLASSERT(m_resource == Config::GetInvalidValue());
		return &m_resource;
	}

	void operator=(const T& resource) throw()
	{
		XTLASSERT(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// Returns the owned resource for normal use.
	// Retains ownership.
	operator const T() const throw()
	{
		return m_resource;
	}

	void Attach(const T& resource) throw()
	{
		XTLASSERT(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// Detaches a resource and returns it, so it is not release automatically
	// when leaving current scope. Note that the resource type must support
	// the assignment operator and must have value semantics.
	T Detach() throw()
	{
		T temp = m_resource;
		m_resource = Config::GetInvalidValue();
		return temp;
	}

	void Release() throw()
	{
		if (Config::GetInvalidValue() != m_resource) 
		{
			Config::Release(m_resource);
			m_resource = Config::GetInvalidValue();
		}
	}

private:
	// hide copy constructor
	AutoResourceT(const AutoResourceT &);
	// hide assignment operator
	AutoResourceT& operator = (const AutoResourceT&);

protected:
	T m_resource;
};

// Config struct template for pointer-like resources.
// Need to specialize for non-pointers
template <typename T>
struct AutoResourceConfigT
{
	static T GetInvalidValue() { return (T)0; }
	static void Release(T t) { delete t; }
};

template <typename T>
struct AutoArrayPtrResourceConfigT
{
	static T GetInvalidValue() { return (T)0; }
	static void Release(T t) { delete[] t; }
};

template <typename T>
class AutoArrayResourceT : 
	public AutoResourceT<T, AutoArrayPtrResourceConfigT<T> >
{
	typedef AutoArrayPtrResourceConfigT<T> Config;

public:

	AutoArrayResourceT() :
		m_resource(Config::GetInvalidValue())
	{
	}

	// If owns a valid resource, release it using the Config::Release
	~AutoArrayResourceT()
	{
		if (m_resource != Config::GetInvalidValue()) 
		{
			Config::Release(m_resource);
		}
	}

	AutoArrayResourceT(const T& resource)
	{
		m_resource = resource;
	}

};

//
// AutoFileHandle are AutoObjectHandle are different in its use.
//
// Use AutoFileHandle for handles created with CreateFile,
// which returns INVALID_HANDLE_VALUE for an invalid handle
// For other handles, where NULL is returned for an invalid handle
// such as CreateEvent, CreateMutex, etc. use AutoObjectHandle
//

struct AutoObjectHandleConfig
{
	static HANDLE GetInvalidValue() { return (HANDLE)NULL; }
	static void Release(HANDLE h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::CloseHandle(h));
	}
};

struct AutoFileHandleConfig
{
	static HANDLE GetInvalidValue() { return (HANDLE)INVALID_HANDLE_VALUE; }
	static void Release(HANDLE h)
	{ 
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::CloseHandle(h));
	}
};

template <>
struct AutoResourceConfigT<SC_HANDLE>
{
	static HANDLE GetInvalidValue() { return (SC_HANDLE)NULL; }
	static void Release(SC_HANDLE h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::CloseServiceHandle(h));
	}
};

struct AutoSCLockConfig
{
	static SC_LOCK GetInvalidValue() { return (SC_LOCK)NULL; }
	static void Release(SC_LOCK lock)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::UnlockServiceDatabase(lock));
	}
};

struct AutoHLOCALConfig
{
	static HLOCAL GetInvalidValue() { return (HLOCAL)NULL; }
	static void Release(HLOCAL p)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(NULL == ::LocalFree(p));
	}
};

struct AutoHGLOBALConfig
{
	static HGLOBAL GetInvalidValue() { return (HGLOBAL)NULL; }
	static void Release(HGLOBAL p)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(NULL == ::GlobalFree(p));
	}
};

struct AutoHKEYConfig
{
	static HKEY GetInvalidValue() { return (HKEY)INVALID_HANDLE_VALUE; }
	static void Release(HKEY hKey) 
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(ERROR_SUCCESS == ::RegCloseKey(hKey));
	}
};

template <HANDLE HeapHandle, DWORD dwFreeFlags = 0>
struct AutoHeapResourceConfig
{
	static LPVOID GetInvalidValue() { return (LPVOID)0; }
	static void Release(LPVOID p)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::HeapFree(HeapHandle, dwFreeFlags, p));
	}
};

template <typename T = void>
struct AutoProcessHeapResourceConfig;

template <typename T>
struct AutoProcessHeapResourceConfig {
	static T* GetInvalidValue() { 
		return (T*)0; 
	}
	static void Release(T* p) {
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, p));
	}
};

struct AutoHModuleResourceConfig {
	static HMODULE GetInvalidValue() { 
		return (HMODULE) NULL; 
	}
	static void Release(HMODULE h) {
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::FreeLibrary(h));
	}
};

struct AutoHDeskResourceConfig 
{
	static HDESK GetInvalidValue() { return (HDESK) NULL; }
	static void Release(HDESK h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::CloseDesktop(h));
	}
};

struct AutoHWINSTAResourceConfig 
{
	static HWINSTA GetInvalidValue() { return (HWINSTA) NULL; }
	static void Release(HWINSTA h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::CloseWindowStation(h));
	}
};

#ifdef _OBJBASE_H_
struct AutoComCleanupConfig
{
	static HRESULT GetInvalidValue() { return E_FAIL; }
	static void Release(HRESULT hr)
	{
		XTL_SAVE_LAST_ERROR();
		if (S_OK == hr || S_FALSE == hr)
		{
			(void) ::CoUninitialize();
		}
	}
};
#endif

#ifdef _WINSOCKAPI_
template <>
struct AutoResourceConfigT<SOCKET> 
{
	static SOCKET GetInvalidValue() { return (SOCKET)INVALID_SOCKET; }
	static void Release(SOCKET s)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(0 == ::closesocket(s));		
	}
};

struct AutoWinsockConfig
{
	static int GetInvalidValue() { return SOCKET_ERROR; }
	static void Release(int ret)
	{
		XTL_SAVE_LAST_ERROR();
		if (ret == 0)
		{
			XTLVERIFY(0 == ::WSACleanup());
		}
	}
};
#endif

typedef AutoResourceT<HANDLE,AutoObjectHandleConfig> AutoObjectHandle;
typedef AutoResourceT<HANDLE,AutoFileHandleConfig> AutoFileHandle;
typedef AutoResourceT<SC_HANDLE> AutoServiceConfigHandle;
typedef AutoServiceConfigHandle AutoSCHandle;
typedef AutoResourceT<SC_LOCK,AutoSCLockConfig> AutoServiceConfigLock;
typedef AutoServiceConfigLock AutoSCLock;
typedef AutoResourceT<HKEY,AutoHKEYConfig> AutoKeyHandle;
typedef AutoResourceT<HMODULE,AutoHModuleResourceConfig> AutoModuleHandle;
typedef AutoResourceT<HDESK,AutoHDeskResourceConfig> AutoDesktopHandle;
typedef AutoResourceT<HWINSTA,AutoHWINSTAResourceConfig> AutoWindowStationHandle;

// typedef AutoHeapResourceConfig<ProcessHeapProvider> AutoProcessHeapResourceConfig;
typedef AutoResourceT<LPVOID,AutoProcessHeapResourceConfig<VOID> > AutoProcessHeap;
typedef AutoResourceT<HLOCAL,AutoHLOCALConfig> AutoLocalHandle;
typedef AutoResourceT<HGLOBAL,AutoHGLOBALConfig> AutoGlobalHandle;

template <typename T>
class AutoProcessHeapPtr : public AutoResourceT<T*,AutoProcessHeapResourceConfig<T> > {
	typedef AutoResourceT<T*,AutoProcessHeapResourceConfig<T> > BaseT;
public:
	AutoProcessHeapPtr() throw() : BaseT() {}
	AutoProcessHeapPtr(T* resource) throw() : BaseT(resource) {}
	void operator=(T* resource) throw() {
		BaseT::operator=(resource);
	}
	T** operator&() throw() {
		AutoProcessHeapResourceConfig<T>::Release(m_resource);
		return BaseT::operator&();
	}
	const T** operator&() const throw() {
		AutoProcessHeapResourceConfig<T>::Release(m_resource);
		C_ASSERT(FALSE);
		return BaseT::operator&();
	}
	T* operator->() throw() {
		return m_resource;
	}
	const T* operator->() const throw() {
		return m_resource;
	}
};

#ifdef _OBJBASE_H_
typedef AutoResourceT<HRESULT,AutoComCleanupConfig> AutoComCleanup;
#endif // _OBJBASE_H_

#ifdef _WINSOCKAPI_
typedef AutoResourceT<SOCKET> AutoSocket;
typedef AutoCleanupT<int,AutoWinsockConfig> AutoWSA;
typedef AutoWSA AutoWinsockStartup;
#endif // _WINSOCKAPI_

struct AutoHDEVNOTIFYConfig
{
	static HDEVNOTIFY GetInvalidValue() { return (HDEVNOTIFY)NULL; }
	static void Release(HDEVNOTIFY h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::UnregisterDeviceNotification(h));
	}
};

struct AutoEventSourceHandleConfig
{
	static HANDLE GetInvalidValue() { return (HANDLE)NULL; }
	static void Release(HANDLE h)
	{
		XTL_SAVE_LAST_ERROR();
		XTLVERIFY(::DeregisterEventSource(h));
	}
};

typedef AutoResourceT<HDEVNOTIFY,AutoHDEVNOTIFYConfig> AutoDeviceNotifyHandle;
typedef AutoResourceT<HANDLE, AutoEventSourceHandleConfig> AutoEventSourceHandle;

} // namespace XTL

#endif
