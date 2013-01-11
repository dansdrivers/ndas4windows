#ifndef _HASH_H_
#define _HASH_H_

#define HASH_KEY_USER			0x1f4a50731530eabb
#define HASH_KEY_SUPER1			0x3e2b321a4750131e
#define HASH_KEY_SUPER			HASH_KEY_SUPER1


//#define HASH_KEY_USER			0x0001020304050607
//#define HASH_KEY_SUPER1			0x0F0E0D0304050607
//#define HASH_KEY_SUPER			HASH_KEY_USER


#define KEY_CON0					0x268F2736
#define KEY_CON1					0x813A76BC

#ifdef __cplusplus
extern "C"
{
#endif

void
Hash32To128(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			);

void
Encrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
Decrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

#ifdef __cplusplus
}
#endif

#endif