/*
 *	Copyright (C) 2001-2004 XIMETA, Inc. All rights reserved.
 */

#pragma once
#ifndef _NDAS_ID_ENC_H_
#define	_NDAS_ID_ENC_H_

//#define   READONLY_CHAR	0x2f
//#define   READWRITE_CHAR	0xd5

//#define   PASSWORD_CONSTANT_1	0x3e
//#define   PASSWORD_CONSTANT_2	0x5a
//#define   PASSWORD_CONSTANT_3	0xb7

typedef struct _NDAS_ID_KEY_INFO {
	BYTE address[6];
	BYTE vid;
	BYTE reserved[2];
	BYTE random;
	BYTE key1[8];
	BYTE key2[8];
	CHAR serialNo[4][5];
	CHAR writeKey[5];
	BOOL writable;
} NDAS_ID_KEY_INFO, *PNDAS_ID_KEY_INFO;

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI NdasIdKey_Encrypt(PNDAS_ID_KEY_INFO pInfo);
BOOL WINAPI NdasIdKey_Decrypt(PNDAS_ID_KEY_INFO pInfo);

#ifdef __cplusplus
}
#endif

#endif /* _NDAS_STRING_ID_H_ */