#ifndef _LSP_HASH_H_
#define _LSP_HASH_H_

#include <lsp_type.h>

void 
lsp_call
lsp_hash32to128(
	lsp_uint8* dst,
	const lsp_uint8* src,
	const lsp_uint8* key);

void
lsp_call
lsp_encrypt32(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* key,
	const lsp_uint8* pwd);

void
lsp_call
lsp_encrypt32_fast(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* eir); /* intermediate result */

void
lsp_call
lsp_encrypt32_fast_copy(
	lsp_uint8* dst,
	const lsp_uint8* src, 
	lsp_uint32 len,
	const lsp_uint8* eir); /* intermediate result */

void
lsp_call
lsp_decrypt32(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* key,
	const lsp_uint8* pwd);

void
lsp_call
lsp_decrypt32_fast(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* dir); /* intermediate result */


void
lsp_call
lsp_aes_cipher(
		   lsp_uint8* pText_in,
		   lsp_uint8* pText_out,
		   lsp_uint8* pKey
		   );

void
lsp_call
lsp_aes_cipher_dummy(
		   lsp_uint8* pText_in,
		   lsp_uint8* pText_out,
		   lsp_uint8* pKey
		   );

void
lsp_call
lsp_encrypt128(
		   unsigned char	*pData,
		   unsigned _int32	uiDataLength,
		   unsigned char	*pKey,
		   unsigned char	*pPassword
		   );

void
lsp_call
lsp_decrypt128(
		   unsigned char	*pData,
		   unsigned _int32	uiDataLength,
		   unsigned char	*pKey,
		   unsigned char	*pPassword
		   );		   

void
lsp_call
lsp_crc32(
	  lsp_uint8* pData,
	  lsp_uint8* Out,
	  unsigned	 uiDataLength
	  );  

#endif /* _LSP_HASH_H_ */
