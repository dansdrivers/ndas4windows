// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__38D6527C_633F_4D8C_A5CC_2E1424926FF2__INCLUDED_)
#define AFX_STDAFX_H__38D6527C_633F_4D8C_A5CC_2E1424926FF2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <WinSock2.h> // Prevent windows.h from include winsock.h
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include "LanScsi.h"
#include "BinParams.h"
#include "Hash.h"
#include "hdreg.h"
#include "socketlpx.h"
#include "emuprocs.h"

// TODO: reference additional headers your program requires here

#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif
#ifndef C_ASSERT_SIZEOF
#define C_ASSERT_SIZEOF(type, size) C_ASSERT(sizeof(type) == size)
#endif
#ifndef C_ASSERT_EQUALSIZE
#define C_ASSERT_EQUALSIZE(t1,t2) C_ASSERT(sizeof(t1) == sizeof(t2))
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__38D6527C_633F_4D8C_A5CC_2E1424926FF2__INCLUDED_)
