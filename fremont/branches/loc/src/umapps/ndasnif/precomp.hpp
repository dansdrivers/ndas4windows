#ifndef UNICODE
#define UNICODE
#endif

//#ifndef _ATL_ATTRIBUTES
//#define _ATL_ATTRIBUTES
//#endif
#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlconv.h>

#ifndef ATLENSURE_SUCCEEDED
#define ATLENSURE_SUCCEEDED(hr) ATLENSURE_THROW(SUCCEEDED(hr), hr)
#endif // ATLENSURE

#ifndef ATLENSURE_RETURN_HR
#define ATLENSURE_RETURN_HR(expr,hr) \
do { \
    int __atl_condVal=!!(expr); \
    ATLASSERT(__atl_condVal && #expr); \
    if(!(__atl_condVal)) return hr; \
} while(0)
#endif

