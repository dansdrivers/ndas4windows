#pragma once

#ifndef XTLASSERT
#ifdef _ASSERTE
#define XTLASSERT _ASSERTE
#else
#pragma message(__FILE__ ": _ASSERTE is not defined, assertion is disabled")
#define XTLASSERT(x) (x)
#endif
#endif

#ifndef XTLC_ASSERT
#define XTLC_ASSERT C_ASSERT
#endif

#ifndef XTLC_ASSERT_EQUAL_SIZE
#define XTLC_ASSERT_EQUAL_SIZE(x,y) XTLC_ASSERT(sizeof(x) == sizeof(y))
#endif

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#ifndef XTLVERIFY
#ifdef _DEBUG
#define XTLVERIFY(expr) XTLASSERT(expr)
#else
#define XTLVERIFY(expr) (expr)
#endif // DEBUG
#endif // XTLVERIFY

/* Used inside API functions that do not want to throw */
#ifndef XTLENSURE_RETURN_T
#define XTLENSURE_RETURN_T(expr, v) \
	do { \
	int __xtl_condVal=!!(expr); \
	XTLASSERT(__xtl_condVal); \
	if (!(__xtl_condVal)) return v; \
	} while(0)
#endif

/* Used inside API functions that do not want to throw */
#ifndef XTLENSURE_RETURN_BOOL
#define XTLENSURE_RETURN_BOOL(expr) XTLENSURE_RETURN_T(expr, FALSE)
#endif

#ifndef XTLENSURE_RETURN
#define XTLENSURE_RETURN(expr) XTLENSURE_RETURN_T(expr, E_FAIL)
#endif 

namespace XTL
{
	
#define XTL_SAVE_LAST_ERROR() XTL::AutoWin32ErrorRecover _error_holder_
struct AutoWin32ErrorRecover
{
	DWORD ErrorCode;
	AutoWin32ErrorRecover(DWORD ErrorCode = ::GetLastError()) throw() : 
		ErrorCode(ErrorCode) 
	{}
	~AutoWin32ErrorRecover()
	{
		::SetLastError(ErrorCode);
	}
};

}
