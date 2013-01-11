////////////////////////////////////////////////////////////////////////////
//
// Exception classes used for ndasbind
//
// @file	ndasexception.h
// @author	Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASEXCEPTION_H_
#define _NDASEXCEPTION_H_

//namespace ximeta
//{
#include <list>
#include <utility>
#include "ndasutil.h"
#define DEBUG_BUFFER_LENGTH 256

static TCHAR DebugBuffer[DEBUG_BUFFER_LENGTH + 1];

void DbgPrint(LPCTSTR DebugMessage, ... );

///////////////////////////////////////////////////////////////////////////////
// Macros for throwing exceptions
///////////////////////////////////////////////////////////////////////////////
#define NDAS_EXCEPTION_POINT		__FILE__, __FUNCTION__, __LINE__
#define NDAS_EXCEPTION_POINT_PARAM	LPCSTR szFileName, LPCSTR szFuncName, int nLine
#define NDAS_EXCEPTION_POINT_ARG	szFileName, szFuncName, nLine
#define NDAS_THROW_EXCEPTION(theClass, code) throw theClass(NDAS_EXCEPTION_POINT, code)
#define NDAS_THROW_EXCEPTION_STR(theClass, code, str) throw theClass(NDAS_EXCEPTION_POINT, code, str)
#define NDAS_THROW_EXCEPTION_EXT(theClass, code, ext) throw theClass(NDAS_EXCEPTION_POINT, code, ext)
#define NDAS_THROW_EXCEPTION_CHAIN(theClass, code, catchedException) \
	throw theClass(NDAS_EXCEPTION_POINT, code, _T(""), catchedException)
#define NDAS_THROW_EXCEPTION_CHAIN_STR(theClass, code, str, catchedException) \
	throw theClass(NDAS_EXCEPTION_POINT, code, str, catchedException)

//
// Class that represents a point in the program source.
// This class is used to display information about the point
// where the exception is thrown.
class CNDASExceptionPoint
{
public:
	WTL::CString m_strFileName;
	WTL::CString m_strFuncName;
	int			 m_nLine;
	CNDASExceptionPoint(LPCSTR szFileName, LPCSTR szFuncName, int nLine)
		: m_strFileName(szFileName), m_strFuncName(szFuncName), m_nLine(nLine)
	{
	}
	WTL::CString GetName() { return m_strFuncName; }
	WTL::CString ToString() { 
		WTL::CString strMsg;
		strMsg.Format(_T("%s(%s:%d)"), m_strFuncName, m_strFileName, m_nLine);
		return strMsg;
	}
};

class CNDASException
{
protected:
	typedef std::list< CNDASException > CallStackList;
	CallStackList m_callStack;
	int			  m_nCode;
	WTL::CString  m_strError;
	CNDASExceptionPoint m_ptException;
public:
	CNDASException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError)
		: m_nCode(nCode), m_strError(strError), m_ptException(NDAS_EXCEPTION_POINT_ARG)
	{
	}

	CNDASException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError, CNDASException &e)
		: m_nCode(nCode), m_strError(strError), m_ptException(NDAS_EXCEPTION_POINT_ARG)
	{
		m_callStack.push_back(e);
	}

	void PrintStackTrace()
	{
#if defined(DEBUG) || defined(_DEBUG)
		::DbgPrint(
			_T("Exception thrown at %s : %s, Error Code : %d(0x%08X)\n"),
			m_ptException.ToString(),
			m_strError, m_nCode, m_nCode);
		if ( !m_callStack.empty() )
		{
			::DbgPrint(_T("Caused by\n"));
			m_callStack.front().PrintStackTrace();
		}
#endif // defined(DEBUG) || defined(_DEBUG)
	}
	int GetCode() { return m_nCode; }
};

class CNetworkException :
	public CNDASException
{
public:
	enum {
		ERROR_UNSPECIFIED = -1000,
		ERROR_NETWORK_FAIL_NOT_CONNECTED,		// Fail to connect the NDAS device or called before connection
		ERROR_NETWORK_FAIL_TO_CREATE_SOCKET,	// Cannot create socket(System failure)
		ERROR_NETWORK_FAIL_TO_GET_NIC_LIST,		// Fail to get NIC list
		ERROR_NETWORK_FAIL_TO_FIND_NDAS,		// Fail to find the NDAS device from all NIC
		ERROR_NETWORK_FAIL_TO_CONNECT,			// Fail to connect the NDAS device
		ERROR_NETWORK_FAIL_TO_LOGIN,			// Fail to login to the NDAS device
		ERROR_FAIL_TO_GET_DISKINFO,				// Fail to get information about disks in the device
		ERROR_FAIL_TO_WRITE,					// Fail to write data
		ERROR_FAIL_TO_READ,						// Fail to read data
	};
	CNetworkException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError = _T(""))
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, strError)
	{
	}
	CNetworkException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError, CNDASException &e)
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, strError, e)
	{
	}
	CNetworkException(NDAS_EXCEPTION_POINT_PARAM, int nCode, const BYTE *pbNode)
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, AddrToString(pbNode))
	{
	}

};

class CDiskException :
	public CNDASException
{
public:
	enum {
		ERROR_UNSPECIFIED = -1100,
		ERROR_UNSUPPORTED_DISK_TYPE,			// Unsupported type of disk
		ERROR_FAIL_TO_INITIALIZE, 
		ERROR_FAIL_TO_MARK_BITMAP,
		ERROR_FAIL_TO_DISCOVER_DEVICEINFO,
		ERROR_FAIL_TO_GET_DISKINFO, 
	};

	CDiskException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError = _T(""))
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, strError)
	{
	}
	CDiskException(NDAS_EXCEPTION_POINT_PARAM, int nCode, WTL::CString strError, CNDASException &e)
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, strError, e)
	{
	}
};	

//
// Exceptions caused by service
//
class CServiceException :
	public CNDASException
{
protected:
	WTL::CString FormatMessage(int nCode);
public:
	CServiceException(NDAS_EXCEPTION_POINT_PARAM, int nCode)
		: CNDASException(NDAS_EXCEPTION_POINT_ARG, nCode, FormatMessage(nCode) )
	{
		
	}
};
//}

#endif // _NDASEXCEPTION_H_