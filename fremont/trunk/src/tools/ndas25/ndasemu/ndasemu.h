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
#include "..\inc\lsprotospec.h"
#include "..\inc\lsprotoidespec.h"
//#include "LanScsi.h"
#include "..\inc\BinParams.h"
#include "..\inc\Hash.h"
#include "..\inc\hdreg.h"
#include "..\inc\socketlpx.h"
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


#define	_LPX_

#define MB (1024 * 1024)

#define PASSWORD_LENGTH_V1 8
#define PASSWORD_LENGTH 16

#define HTONS2(Data)	(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))
#define NTOHS2(Data)	(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))

#define HTONL2(Data)	( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))
#define NTOHL2(Data)	( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))

#define HTONLL2(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))

#define NTOHLL2(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))


#if 0

#define HTONS(Data)		( HTONS2(Data) )
#define NTOHS(Data)		( NTOHS2(Data) )


#define HTONL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, HTONL2(Data) )
#define NTOHL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, NTOHL2(Data) )

#define HTONLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, HTONLL2(Data) )
#define NTOHLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, NTOHLL2(Data) )

#else

#define HTONS(Data)		HTONS2(Data)
#define NTOHS(Data)		NTOHS2(Data)


#define HTONL(Data)		HTONL2(Data)
#define NTOHL(Data)		NTOHL2(Data)

#define HTONLL(Data)	HTONLL2(Data)
#define NTOHLL(Data)	NTOHLL2(Data)

#endif

void
Hash32To128_l(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			);

void
Encrypt32_l(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
Decrypt32_l(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

#endif // !defined(AFX_STDAFX_H__38D6527C_633F_4D8C_A5CC_2E1424926FF2__INCLUDED_)
