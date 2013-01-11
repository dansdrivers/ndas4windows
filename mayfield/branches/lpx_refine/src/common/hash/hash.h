#ifndef _HASH_H_
#define _HASH_H_

#define HASH_KEY_LENGTH			8

/*
	Depreciated.

//
//	Normal user password
//

#define HASH_KEY_USER			0x1F4A50731530EABB
#define HASH_KEY_SAMPLE			0x0001020304050607
#define HASH_KEY_DLINK			0xCE00983088A66118
#define HASH_KEY_RUTTER			0x1F4A50731530EABB


//
//	Super user password
//

#define HASH_KEY_SUPER1			0x0F0E0D0304050607
#define HASH_KEY_SUPER_V1		0x3e2b321a4750131e

*/

#define KEY_CON0					0x268F2736
#define KEY_CON1					0x813A76BC

//
//	Cipher-specific instance information
//
typedef struct _CIPHER_HASH {

	UCHAR	HashKey[HASH_KEY_LENGTH];

} CIPHER_HASH, *PCIPHER_HASH;

//
//	Cipher-specific key information
//
typedef struct _CIPHER_HASH_KEY {

	UCHAR	CntEcr_IR[HASH_KEY_LENGTH];
	UCHAR	CntDcr_IR[HASH_KEY_LENGTH];

} CIPHER_HASH_KEY, *PCIPHER_HASH_KEY;


#ifdef __cplusplus
extern "C"
{
#endif

void
__stdcall
Hash32To128(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			);

void
__stdcall
Encrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
Decrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
Encrypt32SP(
			unsigned char		*pData,
			unsigned _int32		uiDataLength,
			unsigned _int8		*pEncryptIR
			);

void
__stdcall
Encrypt32SPAndCopy(
				   unsigned char		*pDestinationData,
				   unsigned char		*pSourceData,
				   unsigned _int32		uiDataLength,
				   unsigned _int8		*pEncryptIR
				   );
void
__stdcall
Decrypt32SP(
		  unsigned char		*pData,
		  unsigned _int32	uiDataLength,
		  unsigned _int8	*pDecryptIR
		  );

#ifdef __cplusplus
}
#endif

#endif