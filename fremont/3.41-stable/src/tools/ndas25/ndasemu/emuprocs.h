#ifdef __cplusplus
extern "C"
{
#endif

//
// From encdec.cpp
//
void
AES_cipher_dummy(
		   unsigned char	*Text_in,
		   unsigned char	*pText_out,
		   unsigned char	*pKey
		   );

void
AES_cipher(
		   unsigned char	*Text_in,
		   unsigned char	*pText_out,
		   unsigned char	*pKey
		   );

void
__stdcall
Encrypt128(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
Decrypt128(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
CRC32(
	  unsigned char	*pData,
	  unsigned char	*pOutput,
	  unsigned		uiDataLength
	  );



#ifdef __cplusplus
}
#endif
