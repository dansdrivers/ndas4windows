#pragma once
#include <windows.h>

namespace
{

__forceinline BOOL IsValidReadPtr(CONST VOID* lp, UINT_PTR ucb)
{ return !IsBadReadPtr(lp, ucb); }

__forceinline BOOL IsValidWritePtr(LPVOID lp, UINT_PTR ucb)
{ return !IsBadWritePtr(lp, ucb); }

__forceinline BOOL IsValidStringPtrW(LPCWSTR lpsz, UINT_PTR ucchMax)
{ return !IsBadStringPtrW(lpsz, ucchMax); }

__forceinline BOOL IsValidStringPtrA(LPCSTR lpsz, UINT_PTR ucchMax)
{ return !IsBadStringPtrA(lpsz, ucchMax); }

#ifdef UNICODE
#define IsValidStringPtr IsValidStringPtrW
#else
#define IsValidStringPtr IsValidStringPtrA
#endif


} // namespace
