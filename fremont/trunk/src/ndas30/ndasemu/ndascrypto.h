/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2008, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 12/11/2008
 -------------------------------------------------------------------------
*/

#pragma once

#include "stdafx.h"


VOID
AES_cipher (
	UINT8 *pText_in,
	UINT8 *pText_out,
	UINT8 *pKey
	);

VOID
Hash32To128_l (
	UINT8 *pSource,
	UINT8 *pResult,
	UINT8 *pKey
	);

VOID
Encrypt32_l (
	UINT8	*pData,
	UINT32	uiDataLength,
	UINT8	*pKey,
	UINT8	*pPassword
	);

VOID
Decrypt32_l (
	UINT8	*pData,
	UINT32	uiDataLength,
	UINT8	*pKey,
	UINT8	*pPassword
	);

VOID
__stdcall
Encrypt128 (
	UINT8	*pData,
	UINT32	uiDataLength,
	UINT8	*pKey,
	UINT8	*pPassword
	);

VOID
__stdcall
Decrypt128 (
	UINT8	*pData,
	UINT32	uiDataLength,
	UINT8	*pKey,
	UINT8	*pPassword
	);

VOID
__stdcall
CRC32 (
	UINT8	*pData,
	UINT8	*pOutput,
	UINT32	uiDataLength
	);

#define Decrypt32	Decrypt32_l
#define Encrypt32	Encrypt32_l
#define Hash32To128 Hash32To128_l

