#pragma once
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <crtdbg.h>

struct Win32APIErrorHandler
{
	static BOOL ModuleNotLoaded(LPCTSTR /* ModuleName */)
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	static BOOL ProcNotFound(LPCSTR /* ProcName */)
	{
		// implicitly error ERROR_PROC_NOT_FOUND is set by the caller;
		return FALSE;
	}
};

struct NTAPIErrorHandler
{
	static UINT ModuleNotLoaded(LPCTSTR /* ModuleName */)
	{
		return ERROR_INVALID_HANDLE;
	}
	static UINT ProcNotFound(LPCSTR /* ProcName */)
	{
		return ERROR_PROC_NOT_FOUND;
	}
};

#ifdef DLOAD_USE_SEH

#define DLOAD_EXCEPTION_PROC_NOT_FOUND    0x0000C000
#define DLOAD_EXCEPTION_MODULE_NOT_LOADED 0x0000C001

#define DLOAD_THROW_PROC_NOT_FOUND    RaiseException(DLOAD_EXCEPTION_PROC_NOT_FOUND, 0, 0, NULL)
#define DLOAD_THROW_MODULE_NOT_LOADED RaiseException(DLOAD_EXCEPTION_MODULE_NOT_LOADED, 0, 0, NULL)

#else

class DelayedLoaderException
{
};

class ProcNotFoundException : public DelayedLoaderException
{
};

class ModuleNotLoadedException : public DelayedLoaderException
{
};

#define DLOAD_THROW_PROC_NOT_FOUND    throw ProcNotFoundException()
#define DLOAD_THROW_MODULE_NOT_LOADED throw ModuleNotLoadedException()

#endif

template <typename RT = BOOL>
struct DelayedLoaderExceptionThrower;

template <typename RT>
struct DelayedLoaderExceptionThrower
{
	static RT ModuleNotLoaded(LPCTSTR ModuleName)
	{
		throw ModuleNotLoadedException(ModuleName);
	}
	static RT ProcNotFound(LPCSTR ProcName)
	{
		throw ProcNotFoundException(ProcName);
	}
};

//
// Delayed DLL Loader
//
// DerivedT requires the following function which returns the module name
//
// static LPCTSTR GetModuleName();
//

template <typename DerivedT, typename ErrorT = DelayedLoaderExceptionThrower<BOOL> >
class DelayedLoader;

template <typename DerivedT, typename ErrorT>
class DelayedLoader
{
public:

	HMODULE m_hModule;

	DelayedLoader() : m_hModule(NULL)
	{
	}

	DelayedLoader(HMODULE hModule) : m_hModule(hModule)
	{
	}

	//BOOL Load() throw()
	//{
	//	m_hModule = ::LoadLibrary(DerivedT::GetModuleName());
	//	return (NULL != m_hModule);
	//}

	//BOOL Unload()
	//{
	//	if (NULL != m_hModule)
	//	{
	//		return ::FreeLibrary(m_hModule);
	//	}
	//	else
	//	{
	//		::SetLastError(ERROR_INVALID_HANDLE);
	//		return FALSE;
	//	}
	//}

	BOOL IsModuleLoaded() throw()
	{
		return (NULL != m_hModule);
	}

	BOOL IsProcAvailable(LPCSTR ProcName)
	{
		if (!IsModuleLoaded())
		{
			::SetLastError(ERROR_INVALID_HANDLE);
			return FALSE;
		}
		FARPROC pfn = ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			return FALSE;
		}
		return TRUE;
	}

	template <typename RT>
	RT Invoke(LPCSTR ProcName)
	{
		typedef RT (WINAPI* pfn_t)(void);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn();
	}

	template <typename P1, typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1)
	{
		typedef RT (WINAPI* pfn_t)(P1 p1);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1);
	}

	template <typename P1, typename P2, typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2)
	{
		typedef RT (WINAPI* pfn_t)(P1 p1, P2 p2);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2);
	}

	template <typename P1, typename P2, typename P3, 
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3)
	{
		typedef RT (WINAPI* pfn_t)(P1 p1, P2 p2, P3 p3);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3);
	}

	template <typename P1, typename P2, typename P3, typename P4,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7, typename P8,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7, P8);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7, p8);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7, typename P8, 
			  typename P9,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7, P8, P9);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7, p8, p9);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7, typename P8, 
			  typename P9, typename P10,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7, typename P8, 
			  typename P9, typename P10, typename P11,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10, P11 p11)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
	}

	template <typename P1, typename P2, typename P3, typename P4, 
			  typename P5, typename P6, typename P7, typename P8, 
			  typename P9, typename P10, typename P11, typename P12,
			  typename RT>
	RT Invoke(LPCSTR ProcName, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9, P10 p10, P11 p11, P12 p12)
	{
		typedef RT (WINAPI* pfn_t)(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
		if (!IsModuleLoaded())
		{
			DLOAD_THROW_MODULE_NOT_LOADED;
		}
		pfn_t pfn = (pfn_t) ::GetProcAddress(m_hModule, ProcName);
		if (NULL == pfn)
		{
			DLOAD_THROW_PROC_NOT_FOUND;
		}
		return pfn(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
	}

};
