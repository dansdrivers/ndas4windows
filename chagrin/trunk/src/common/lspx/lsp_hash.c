#include "lsp_hash.h"
#include "lsp_impl.h"

#define LSP_HASH_KEY_CON0 0x268F2736
#define LSP_HASH_KEY_CON1 0x813A76BC

void 
lsp_call
lsp_hash32to128(
	__out_ecount(16) lsp_uint8* dst,
	__in_ecount(4) const lsp_uint8* src,  /*  32 bits,  4 bytes */
	__in_ecount(8) const lsp_uint8* key)
{
	/* 0x268F2736 */
	static const lsp_uint8 kc0[4] = { 0x36, 0x27, 0x8F, 0x26};
	/* 0x813A76BC */
	static const lsp_uint8 kc1[4] = { 0xBC, 0x76, 0x3A, 0x81};
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
	__inout_ecount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_ecount(4) const lsp_uint8* key,
	__in_ecount(8) const lsp_uint8* pwd)
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
lsp_encrypt32ex(
	__inout_ecount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_ecount(4) const lsp_uint8* ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			  ckey[0] ^ p[3],
			~(ckey[1] ^ p[0]),
			  ckey[2] ^ p[1],
			~(ckey[3] ^ p[2]),
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32exx(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			  ((ckey >> 0 ) & 0xFF) ^ p[3],
			~(((ckey >> 8 ) & 0xFF) ^ p[0]),
			  ((ckey >> 16) & 0xFF) ^ p[1],
			~(((ckey >> 24) & 0xFF) ^ p[2]),
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32ex_copy(
	__out_ecount(len) lsp_uint8* dst,
	__in_ecount(len) const lsp_uint8* src, 
	__in lsp_uint32 len,
	__in_ecount(4) const lsp_uint8* ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; ++i)
	{
		dst[i*4 + 0] =   ckey[0] ^ src[i*4 + 3];
		dst[i*4 + 1] = ~(ckey[1] ^ src[i*4 + 0]);
		dst[i*4 + 2] =   ckey[2] ^ src[i*4 + 1];
		dst[i*4 + 3] = ~(ckey[3] ^ src[i*4 + 2]);
	}
}

void
lsp_call
lsp_encrypt32exx_copy(
	__out_ecount(len) lsp_uint8* dst,
	__in_ecount(len) const lsp_uint8* src, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len / 4; ++i)
	{
		dst[i*4 + 0] =   ((ckey >> 0 ) & 0xFF) ^ src[i*4 + 3];
		dst[i*4 + 1] = ~(((ckey >> 8 ) & 0xFF) ^ src[i*4 + 0]);
		dst[i*4 + 2] =   ((ckey >> 16) & 0xFF) ^ src[i*4 + 1];
		dst[i*4 + 3] = ~(((ckey >> 24) & 0xFF) ^ src[i*4 + 2]);
	}
}

void
lsp_call
lsp_decrypt32(
	__inout_ecount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_ecount(4) const lsp_uint8* key,
	__in_ecount(8) const lsp_uint8* pwd)
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
lsp_encrypt32_build_combined_key(
	__out lsp_uint32* ckey,
	__in_ecount(4) const lsp_uint8* key,
	__in_ecount(8) const lsp_uint8* pwd)
{
	*ckey = 
		((pwd[1] ^ pwd[7] ^ key[3]) & 0xFF) << 0 |
		((pwd[0] ^ pwd[3] ^ key[0]) & 0xFF) << 8 |
		((pwd[2] ^ pwd[6] ^ key[2]) & 0xFF) << 16 |
		((pwd[4] ^ pwd[5] ^ key[1]) & 0xFF) << 24;
}

void
lsp_call
lsp_decrypt32_build_combined_key(
	__out lsp_uint32* ckey,
	__in_ecount(4) const lsp_uint8* key,
	__in_ecount(8) const lsp_uint8* pwd)
{
	*ckey = 
		((~(pwd[0] ^ pwd[3] ^   key[0])) & 0xFF) << 0 |
		((  pwd[2] ^ pwd[6] ^   key[2])  & 0xFF) << 8 |
		((  pwd[4] ^ pwd[5] ^ ~(key[1])) & 0xFF) << 16 |
		((  pwd[1] ^ pwd[7] ^   key[3])  & 0xFF) << 24;
}

void
lsp_call
lsp_decrypt32ex(
	__inout_ecount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_ecount(4) const lsp_uint8* ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len/ 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			ckey[0] ^ p[1],
			ckey[1] ^ p[2], 
			ckey[2] ^ p[3],
			ckey[3] ^ p[0] 
		};
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_decrypt32exx(
	__inout_ecount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey)
{
	lsp_uint32 i;
	for (i = 0; i < len/ 4; i++)
	{
		lsp_uint8* p = &buf[i*4];
		lsp_uint8  res[4] = 
		{
			((ckey >>  0 ) & 0xFF) ^ p[1],
			((ckey >>  8 ) & 0xFF) ^ p[2], 
			((ckey >> 16 ) & 0xFF) ^ p[3],
			((ckey >> 24 ) & 0xFF) ^ p[0] 
		};
		lsp_memcpy(p, res, 4);
	}
}
