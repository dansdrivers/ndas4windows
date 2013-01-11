#ifndef _HASH_H_
#define _HASH_H_

#define HASH_KEY_USER			0x1F4A50731530EABB
#define HASH_KEY_SAMPLE			0x0001020304050607
#define HASH_KEY_DLINK			0xCE00983088A66118
#define HASH_KEY_RUTTER			0x1F4A50731530EABB
#define HASH_KEY_SUPER1			0x0F0E0D0304050607

#define HASH_KEY_SUPER			HASH_KEY_USER

#define KEY_CON0					0x268F2736
#define KEY_CON1					0x813A76BC

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