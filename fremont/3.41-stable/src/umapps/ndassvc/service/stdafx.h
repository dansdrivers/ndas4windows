// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _DEBUG
// #define _ATL_DEBUG_INTERFACES
#endif

//#define _WIN32_WINNT 0x0500 // Windows 2000 or later
#define STRICT
#include <winsock2.h>
#include <windows.h>
#include <basetsd.h>
#include <tchar.h>
#include <crtdbg.h>
#include <regstr.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <winioctl.h>
#include <setupapi.h>
#include <ntddscsi.h>
#include <diskguid.h>

#include <sddl.h>

#include <shlwapi.h>
#include <strsafe.h>

#include <list>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <bitset>
#include <algorithm>
#include <functional>

#include <atlbase.h>
#include <atlapp.h>
#include <atlcoll.h>

extern CAppModule _Module;

#include <atlwin.h>
#include <atlsiface.h>
#include <atlutil.h>

#include <boost/shared_ptr.hpp>
#include <boost/mem_fn.hpp>

#include <ndas/ndasuidebug.h>

#include <scrc32.h>

#ifndef COMVERIFY
#define COMVERIFY(x) ATLVERIFY(SUCCEEDED(x))
#endif

#ifndef COMASSERT
#define COMASSERT(x) ATLASSERT(SUCCEEDED(x))
#endif

class CCoInitialize
{
public:
	HRESULT m_hr;
	CCoInitialize(DWORD CoInit = COINIT_MULTITHREADED) :
		m_hr(CoInitializeEx(0, CoInit))
	{
		if (FAILED(m_hr)) AtlThrow(m_hr);
	}
	~CCoInitialize()
	{
		if (SUCCEEDED(m_hr))
		{
			CoUninitialize();
		}
	}
};


template <typename T>
const T* ByteOffset(const void* buffer, size_t offset)
{
	return reinterpret_cast<T*>(reinterpret_cast<const char*>(buffer) + offset);
}

template <typename T>
T* ByteOffset(void* buffer, size_t offset)
{
	return reinterpret_cast<T*>(reinterpret_cast<char*>(buffer) + offset);
}

