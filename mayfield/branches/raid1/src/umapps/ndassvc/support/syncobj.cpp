#include "stdafx.h"
#include "syncobj.h"

BOOL ximeta::CCritSecLock::s_bInit = FALSE;
CRITICAL_SECTION ximeta::CCritSecLock::m_cs;
