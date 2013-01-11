#ifndef _SERIAL_H_
#define	_SERIAL_H_

#include	<windows.h>

//#define READONLY_CHAR	0x2f
//#define	READWRITE_CHAR	0xd5

//#define	PASSWORD_CONSTANT_1	0x3e
//#define	PASSWORD_CONSTANT_2	0x5a
//#define	PASSWORD_CONSTANT_3	0xb7

typedef struct _SERIAL_INFO {
	unsigned char	ucAddr[6];
	unsigned char	ucVid;
	unsigned char	reserved[2];
	unsigned char	ucRandom;
	unsigned char	ucKey1[8];
	unsigned char	ucKey2[8];
	char			ucSN[4][5];
	char			ucWKey[5];
	BOOL			bIsReadWrite;
} SERIAL_INFO, *PSERIAL_INFO;

BOOL	EncryptSerial(PSERIAL_INFO pInfo);
BOOL	DecryptSerial(PSERIAL_INFO pInfo);
#endif