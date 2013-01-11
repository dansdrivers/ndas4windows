#pragma once
#ifndef _AUTORES_H_
#define _AUTORES_H_
//
// Last Revision: July 15, 2005 Chesong Lee
//
//

// Config struct template for pointer-like resources.
// Need to specialize for non-pointers
template <typename T>
struct AutoResourceConfigT
{
	static T GetInvalidValue() 
    { 
        return (T)0; 
    }
	static void Release(T t) 
    { 
        delete t; 
    }
};

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

	explicit AutoResourceT() throw() :
        m_resource(Config::GetInvalidValue())
    {
    }

	  // If owns a valid resource, release it using the Config::Release
    ~AutoResourceT() throw()
    {
        if (m_resource != Config::GetInvalidValue()) 
        {
            Config::Release(m_resource);
        }
    }

    AutoResourceT(const T& resource) throw() :
        m_resource(resource)
    {
    }

    void operator =(const T& resource) throw()
    {
        _ASSERTE(Config::GetInvalidValue() == m_resource);
        m_resource = resource;
    }

    // Returns the owned resource for normal use.
    // Retains ownership.
    operator T() throw()
    {
        return m_resource;
    }

	void Attach(const T& resource) throw()
	{
		_ASSERTE(Config::GetInvalidValue() == m_resource);
		m_resource = resource;
	}

    // Detaches a resource and returns it, so it is not release automatically
    // when leaving current scope. Note that the resource type must support
    // the assignment operator and must have value semantics.
    T Detach() throw()
    {
        T resource = m_resource;
        m_resource = Config::GetInvalidValue();
        return resource;
    }
    
private:
	// hide copy constructor
	AutoResourceT(const AutoResourceT &);
	// hide assignment operator
	AutoResourceT& operator = (const AutoResourceT&);

protected:
	T m_resource;
};

template <typename T>
struct AutoArrayPtrResourceConfigT
{
	static T GetInvalidValue() 
	{ 
		return (T)0; 
	}
	static void Release(T t) 
	{ 
		delete[] t; 
	}
};

template <typename T>
class AutoArrayResourceT : 
	public AutoResourceT<T, AutoArrayPtrResourceConfigT<T> >
{
	typedef AutoResourceT<T, AutoArrayPtrResourceConfigT<T> > BaseT;

public:

    explicit AutoArrayResourceT() throw() : BaseT()
    {}
        
	AutoArrayResourceT(const T& resource) throw() : Base(resource)
    {}

	void operator =(const T& resource) throw() 
	{ BaseT::operator = (resource); }

	operator T() throw() 
	{ return BaseT::operator T(); }

private:
	// hide copy constructor
	AutoArrayResourceT(const AutoArrayResourceT&);
	// hide assignment operator
	AutoArrayResourceT& operator = (const AutoArrayResourceT&);

};

//
// Tweaking Specialization to provide a kind of AutoHeapPtr<char*>.
//
template <typename T>
struct AutoProcessHeapResourceConfig 
{
	static T GetInvalidValue() 
	{ 
		return reinterpret_cast<T>(0); 
	}
	static void Release(T p)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::HeapFree(::GetProcessHeap(), 0, reinterpret_cast<LPVOID>(p));
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

template <typename T>
class AutoProcessHeapPtr : 
	public AutoResourceT<T, AutoProcessHeapResourceConfig<T> >
{
	typedef AutoResourceT<T, AutoProcessHeapResourceConfig<T> > BaseT;

public:

	explicit AutoProcessHeapPtr() throw() : BaseT()
	{}

	AutoProcessHeapPtr(const T& resource) throw() : BaseT(resource) 
	{}

	void operator =(const T& resource) throw() 
	{ BaseT::operator = (resource); }

	operator T() throw() 
	{ return BaseT::operator T(); }

private:
	// hide copy constructor
	AutoProcessHeapPtr(const AutoProcessHeapPtr &);
	// hide assignment operator
	AutoProcessHeapPtr& operator = (const AutoProcessHeapPtr&);
};


// AutoProcessHeapPtr;

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
	static HANDLE GetInvalidValue() 
	{ 
		return (HANDLE)NULL; 
	}
	static void Release(HANDLE h)
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HANDLE, AutoObjectHandleConfig> AutoObjectHandle;

struct AutoFileHandleConfig
{
	static HANDLE GetInvalidValue() 
	{ 
		return (HANDLE)INVALID_HANDLE_VALUE; 
	}
	static void Release(HANDLE h)
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HANDLE, AutoFileHandleConfig> AutoFileHandle;

struct AutoSCHandleConfig
{
	static SC_HANDLE GetInvalidValue() 
	{ 
		return (SC_HANDLE)NULL; 
	}
	static void Release(SC_HANDLE h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseServiceHandle(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<SC_HANDLE, AutoSCHandleConfig> AutoSCHandle;

struct AutoSCLockConfig
{
	static SC_LOCK GetInvalidValue() 
	{ 
		return (SC_LOCK)NULL; 
	}
	static void Release(SC_LOCK lock)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::UnlockServiceDatabase(lock);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<SC_LOCK,AutoSCLockConfig> AutoSCLock;

struct AutoHLOCALConfig
{
	static HLOCAL GetInvalidValue() 
	{ 
		return (HLOCAL)NULL; 
	}
	static void Release(HLOCAL p)
	{
		DWORD dwError = ::GetLastError();
		HLOCAL hLocal = ::LocalFree(p);
		_ASSERTE(NULL == hLocal);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HLOCAL,AutoHLOCALConfig> AutoHLocal;

struct AutoHKEYConfig
{
	static HKEY GetInvalidValue() 
	{
		return (HKEY)INVALID_HANDLE_VALUE; 
	}
	static void Release(HKEY hKey) 
	{
		DWORD dwError = ::GetLastError();
		LONG lResult = RegCloseKey(hKey);
		_ASSERTE(ERROR_SUCCESS == lResult);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HKEY,AutoHKEYConfig> AutoHKey;

struct AutoHModuleResourceConfig 
{
	static HMODULE GetInvalidValue() 
	{ 
		return (HMODULE) NULL; 
	}
	static void Release(HMODULE h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::FreeLibrary(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HMODULE,AutoHModuleResourceConfig> AutoHModule;

struct AutoHDeskResourceConfig 
{
	static HDESK GetInvalidValue() 
	{ 
		return (HDESK) NULL; 
	}
	static void Release(HDESK h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::CloseDesktop(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HDESK,AutoHDeskResourceConfig> AutoHDesk;

#ifdef _WINSOCKAPI_

struct AutoSocketResourceConfig
{
	static SOCKET GetInvalidValue() 
	{
		return (SOCKET) INVALID_SOCKET; 
	}
	static void Release(SOCKET s)
	{
		DWORD dwError = ::WSAGetLastError();
		INT iResult = ::closesocket(s);
		_ASSERTE(0 == iResult);
		::WSASetLastError(dwError);
	}
};

typedef AutoResourceT<SOCKET, AutoSocketResourceConfig> AutoSocket;

#endif

#endif
