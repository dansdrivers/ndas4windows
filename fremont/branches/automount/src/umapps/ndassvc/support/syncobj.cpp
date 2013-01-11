#include "stdafx.h"
#include "syncobj.h"

BOOL ximeta::CCritSecLockGlobal::s_bInit = FALSE;
CRITICAL_SECTION ximeta::CCritSecLockGlobal::m_cs = {0};

