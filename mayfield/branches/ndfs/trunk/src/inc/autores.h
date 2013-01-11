#pragma once
#ifndef _AUTORES_H_
#define _AUTORES_H_

// namespace ximeta {

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
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoFileHandleConfig
{
	static HANDLE GetInvalidValue() { return (HANDLE)INVALID_HANDLE_VALUE; }
	static void Release(HANDLE h)
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

template <>
struct AutoResourceConfigT<SC_HANDLE>
{
	static HANDLE GetInvalidValue() { return (SC_HANDLE)NULL; }
	static void Release(SC_HANDLE h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseServiceHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoSCLockConfig
{
	static SC_LOCK GetInvalidValue() { return (SC_LOCK)NULL; }
	static void Release(SC_LOCK lock)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::UnlockServiceDatabase(lock);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoHLOCALConfig
{
	static HLOCAL GetInvalidValue() { return (HLOCAL)NULL; }
	static void Release(HLOCAL p)
	{
		DWORD dwError = ::GetLastError();
		HLOCAL hLocal = ::LocalFree(p);
		_ASSERTE(NULL == hLocal);
		::SetLastError(dwError);
	}
};

struct AutoHKEYConfig
{
	static HKEY GetInvalidValue() { return (HKEY)INVALID_HANDLE_VALUE; }
	static void Release(HKEY hKey) {
		DWORD dwError = ::GetLastError();
		LONG lResult = RegCloseKey(hKey);
		_ASSERTE(ERROR_SUCCESS == lResult);
		::SetLastError(dwError);
	}
};

template <HANDLE HeapHandle, DWORD dwFreeFlags = 0>
struct AutoHeapResourceConfig
{
	static LPVOID GetInvalidValue() { return (LPVOID)0; }
	static void Release(LPVOID p)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::HeapFree(HeapHandle, dwFreeFlags, p);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoProcessHeapResourceConfig {
	static LPVOID GetInvalidValue() { return (LPVOID)0; }
	static void Release(LPVOID p)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::HeapFree(::GetProcessHeap(), 0, p);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoHModuleResourceConfig {
	static HMODULE GetInvalidValue() { return (HMODULE) NULL; }
	static void Release(HMODULE h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::FreeLibrary(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

struct AutoHDeskResourceConfig {
	static HDESK GetInvalidValue() { return (HDESK) NULL; }
	static void Release(HDESK h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseDesktop(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

#ifdef _WINSOCKAPI_
template <>
struct AutoResourceConfigT<SOCKET>
{
	static SOCKET GetInvalidValue() { return (SOCKET)INVALID_SOCKET; }
	static void Release(SOCKET s)
	{
		DWORD dwError = ::WSAGetLastError();
		INT iResult = ::closesocket(s);
		_ASSERTE(ERROR_SUCCESS == iResult);
		::WSASetLastError(dwError);
	}
};

#endif

template <typename T, typename Config = AutoResourceConfigT<T> >
class AutoResourceT
{
public:
	// Takes ownership of passed resource, or initializes internal resource
	// member of invalid value
	//	explicit AutoResourceT(T resource = Config::GetInvalidValue()) :
	//		m_resource(resource)
	//	{
	//	}

	AutoResourceT() :
		m_resource(Config::GetInvalidValue())
	{
	}

	// If owns a valid resource, release it using the Config::Release
	~AutoResourceT()
	{
		if (Config::GetInvalidValue() != m_resource) 
		{
			Config::Release(m_resource);
		}
	}

	AutoResourceT(const T& resource)
	{
		// _ASSERTE(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	void operator =(const T& resource)
	{
		_ASSERTE(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// Returns the owned resource for normal use.
	// Retains ownership.
	operator T()
	{
		return m_resource;
	}

	void Attach(const T& resource)
	{
		_ASSERTE(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// Detaches a resource and returns it, so it is not release automatically
	// when leaving current scope. Note that the resource type must support
	// the assignment operator and must have value semantics.
	T Detach()
	{
		T temp = m_resource;
		m_resource = Config::GetInvalidValue();
		return temp;
	}

	void Release()
	{
		if (Config::GetInvalidValue() != m_resource) 
		{
			Config::Release(m_resource);
			m_resource = Config::GetInvalidValue();
		}
	}

#ifdef USE_AUTORES_EXPERIMENTAL
	  /*
	  T & operator*() const // never throws
	  {
	  _ASSERTE(m_resource != 0);
	  return m_resource;
	  }
	  */

	  T operator->() const // never throws
	  {
		  _ASSERTE(m_resource != 0);
		  return m_resource;
	  }
#endif

private:
	// hide copy constructor
	AutoResourceT(const AutoResourceT &);
	// hide assignment operator
	AutoResourceT& operator = (const AutoResourceT&);

protected:
	T m_resource;
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
		  if (m_resource != Config::GetInvalidValue()) {
			  Config::Release(m_resource);
		  }
	  }

	  AutoArrayResourceT(const T& resource)
	  {
		  m_resource = resource;
	  }

};

typedef AutoResourceT<HANDLE, AutoObjectHandleConfig> AutoObjectHandle;
typedef AutoResourceT<HANDLE, AutoFileHandleConfig> AutoFileHandle;
typedef AutoResourceT<SC_HANDLE> AutoSCHandle;

typedef AutoResourceT<SC_LOCK,AutoSCLockConfig> AutoSCLock;
typedef AutoResourceT<HLOCAL,AutoHLOCALConfig> AutoHLocal;

typedef AutoResourceT<HKEY,AutoHKEYConfig> AutoHKey;

typedef AutoResourceT<HMODULE,AutoHModuleResourceConfig> AutoHModule;
typedef AutoResourceT<HDESK,AutoHDeskResourceConfig> AutoHDesk;

// typedef AutoHeapResourceConfig<ProcessHeapProvider> AutoProcessHeapResourceConfig;
typedef AutoResourceT<LPVOID, AutoProcessHeapResourceConfig> AutoProcessHeap;

#ifdef _WINSOCKAPI_
typedef AutoResourceT<SOCKET> AutoSocket;
#endif


// } // ximeta

#endif
