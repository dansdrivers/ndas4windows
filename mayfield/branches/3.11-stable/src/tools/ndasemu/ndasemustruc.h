/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#if _MSC_VER <= 1000
#error "Out of date Compiler"
#endif

#pragma once



//////////////////////////////////////////////////////////////////////////
//
// ATA disk
//




//////////////////////////////////////////////////////////////////////////
//
//	Time machine disk
//


typedef struct _TMD_CONTEXT {

	int		FileHandle;
	UINT64	FileBlockSize;

} TMD_CONTEXT, *PTMD_CONTEXT;

//////////////////////////////////////////////////////////////////////////
//
//	LanScsi Protocol
//

typedef struct _ENCRYPTION_INFO {

	UINT32	CHAP_I;
	UINT32	CHAP_C;
	UINT64	Password64;

	BOOL	HeaderEncryptAlgo;
	BOOL	BodyEncryptAlgo;

} ENCRYPTION_INFO, *PENCRYPTION_INFO;

typedef struct _NDASDIGEST_INFO {

	BOOL	HeaderDigestAlgo;
	BOOL	BodyDigestAlgo;

} NDASDIGEST_INFO, *PNDASDIGEST_INFO;
