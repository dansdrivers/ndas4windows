#include "lsp_hash.h"
#include "lsp_impl.h"

#define LSP_HASH_KEY_CON0 0x268F2736
#define LSP_HASH_KEY_CON1 0x813A76BC

void 
lsp_call
lsp_hash32to128(
	lsp_uint8* dst,
	const lsp_uint8* src,  /*  32 bits,  4 bytes */
	const lsp_uint8* key)
{
	lsp_uint8 kc0[4] = { 0x36, 0x27, 0x8F, 0x26}; /* 0x268F2736 */
	lsp_uint8 kc1[4] = { 0xBC, 0x76, 0x3A, 0x81}; /* 0x813A76BC */
	dst[ 0] =   key[2] ^ src[1];
	dst[ 1] =   kc0[3] | src[2];
	dst[ 2] =   kc0[2] & src[1];
	dst[ 3] = ~(key[6] ^ src[2]);
	dst[ 4] =   kc1[2] ^ src[3];
	dst[ 5] =   key[0] & src[0];
	dst[ 6] = ~(kc1[0] ^ src[1]);
	dst[ 7] =   key[5] | src[3];
	dst[ 8] =   key[7] & src[2];
	dst[ 9] =   kc0[0] ^ src[1];
	dst[10] =   key[4] ^ src[3];
	dst[11] = ~(kc1[1] ^ src[0]);
	dst[12] =   key[3] ^ src[2];
	dst[13] =   kc0[1] & src[3];
	dst[14] =   key[1] ^ src[3];
	dst[15] =   kc1[3] | src[0];
}

void
lsp_call
lsp_encrypt32(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* key,
	const lsp_uint8* pwd)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; ++i) 
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			  pwd[1] ^ pwd[7] ^ p[3] ^ key[3] ,
			~(pwd[0] ^ pwd[3] ^ key[0] ^ p[0]),
			  pwd[2] ^ pwd[6] ^ p[1] ^ key[2] ,
			~(pwd[4] ^ pwd[5] ^ p[2] ^ key[1]),
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32_fast(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* eir) /* intermediate result */
{
	lsp_uint32 i;
	for (i = 0; i < len/ 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			  eir[0] ^ p[3],
			~(eir[1] ^ p[0]),
			  eir[2] ^ p[1],
			~(eir[3] ^ p[2]),
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32_fast_copy(
	lsp_uint8* dst,
	const lsp_uint8* src, 
	lsp_uint32 len,
	const lsp_uint8* eir) /* intermediate result */
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; ++i)
	{
		dst[i*4 + 0] =   eir[0] ^ src[i*4 + 3];
		dst[i*4 + 1] = ~(eir[1] ^ src[i*4 + 0]);
		dst[i*4 + 2] =   eir[2] ^ src[i*4 + 1];
		dst[i*4 + 3] = ~(eir[3] ^ src[i*4 + 2]);
	}
}

void
lsp_call
lsp_decrypt32(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* key,
	const lsp_uint8* pwd)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; ++i)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] =
		{
			pwd[0] ^ pwd[3] ^ key[0] ^ ~(p[1]),
			pwd[2] ^ pwd[6] ^ p[2] ^ key[2],
			pwd[4] ^ pwd[5] ^ p[3] ^ ~(key[1]),
			pwd[1] ^ pwd[7] ^ p[0] ^ key[3]
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_decrypt32_fast(
	lsp_uint8* buf, 
	lsp_uint32 len,
	const lsp_uint8* dir) /* intermediate result */
{
	lsp_uint32 i;
	for (i = 0; i < len/ 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			dir[3] ^ p[0], 
			dir[0] ^ p[1],
			dir[1] ^ p[2], 
			dir[2] ^ p[3]
		};
		lsp_memcpy(p, res, 4);
	}
}

