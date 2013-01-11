#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <shellapi.h> // required for atlctrlx.h
#include <atlframe.h>
#include <atlwin.h>
#include <atlctl.h>
#include <atlmisc.h>
#include <atlddx.h>
#include <atldlgs.h>
#include <atlddx.h>
#include <atlctrls.h>

#include <atlctrlx.h>
#include <atlctrlw.h>
#include <atlcoll.h>

#include <atlcrack.h>

#include <strsafe.h>

#ifndef COMVERIFY
#ifdef _DEBUG
#define COMVERIFY(x) ATLASSERT(SUCCEEDED(x))
#else  // _DEBUG
#define COMVERIFY(x) (x)
#endif // _DEBUG
#endif // COMVERIFY
