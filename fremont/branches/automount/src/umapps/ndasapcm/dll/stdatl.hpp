#pragma once

#include <atlbase.h>
#include <atlcom.h>

using namespace ATL;

#ifndef _VERIFY
#ifdef _DEBUG
#define _VERIFY(x) _ASSERT(x)
#else
#define _VERIFY(x) (x)
#endif
#endif

