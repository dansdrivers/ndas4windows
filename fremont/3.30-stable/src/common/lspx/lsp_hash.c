#include <lspx/lsp_hash.h>
#include <lspx/lsp_host.h>

void
lsp_call
lsp_encrypt32_build_combined_key(
	__out lsp_uint32_t* ckey,
	__in const lsp_uint32_t key,
	__in_ecount(8) const lsp_uint8_t* pwd)
{
	lsp_uint8_t key_code[4];

	*(lsp_uint32_t *)key_code = lsp_htobel(key);
	
	*ckey = 
		((pwd[1] ^ pwd[7] ^ key_code[0]) & 0xFF) << 0 |
		((pwd[0] ^ pwd[3] ^ key_code[3]) & 0xFF) << 8 |
		((pwd[2] ^ pwd[6] ^ key_code[1]) & 0xFF) << 16 |
		((pwd[4] ^ pwd[5] ^ key_code[2]) & 0xFF) << 24;
}

void
lsp_call
lsp_decrypt32_build_combined_key(
	__out lsp_uint32_t* ckey,
	__in const lsp_uint32_t key,
	__in_ecount(8) const lsp_uint8_t* pwd)
{
	lsp_uint8_t key_code[4];

	*(lsp_uint32_t *)key_code = lsp_htobel(key);
	
	*ckey = 
		((~(pwd[0] ^ pwd[3] ^   key_code[3])) & 0xFF) << 0 |
		((  pwd[2] ^ pwd[6] ^   key_code[1])  & 0xFF) << 8 |
		((  pwd[4] ^ pwd[5] ^ ~(key_code[2])) & 0xFF) << 16 |
		((  pwd[1] ^ pwd[7] ^   key_code[0])  & 0xFF) << 24;
}

void 
lsp_call
lsp_hash_uint32_to128(
	__out_ecount(16) lsp_uint8_t* dst,
	__in lsp_uint32_t src,
	__in_ecount(8) const lsp_uint8_t* key)
{	
	/* 0x268F2736 */
	static const lsp_uint8_t kc0[4] = { 0x36, 0x27, 0x8F, 0x26};
	/* 0x813A76BC */
	static const lsp_uint8_t kc1[4] = { 0xBC, 0x76, 0x3A, 0x81};
	lsp_uint8_t src_code[4];

	*(lsp_uint32_t *)src_code = lsp_htobel(src);

	dst[ 0] = (lsp_uint8_t)   key[2] ^ src_code[2];
	dst[ 1] = (lsp_uint8_t)   kc0[3] | src_code[1];
	dst[ 2] = (lsp_uint8_t)   kc0[2] & src_code[2];
	dst[ 3] = (lsp_uint8_t) ~(key[6] ^ src_code[1]);
	dst[ 4] = (lsp_uint8_t)   kc1[2] ^ src_code[0];
	dst[ 5] = (lsp_uint8_t)   key[0] & src_code[3];
	dst[ 6] = (lsp_uint8_t) ~(kc1[0] ^ src_code[2]);
	dst[ 7] = (lsp_uint8_t)   key[5] | src_code[0];
	dst[ 8] = (lsp_uint8_t)   key[7] & src_code[1];
	dst[ 9] = (lsp_uint8_t)   kc0[0] ^ src_code[2];
	dst[10] = (lsp_uint8_t)   key[4] ^ src_code[0];
	dst[11] = (lsp_uint8_t) ~(kc1[1] ^ src_code[3]);
	dst[12] = (lsp_uint8_t)   key[3] ^ src_code[1];
	dst[13] = (lsp_uint8_t)   kc0[1] & src_code[0];
	dst[14] = (lsp_uint8_t)   key[1] ^ src_code[0];
	dst[15] = (lsp_uint8_t)   kc1[3] | src_code[3];
}

void
lsp_call
lsp_encrypt32(
	__inout_ecount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t key,
	__in_ecount(8) const lsp_uint8_t* pwd)
{
	lsp_uint32_t i;
	lsp_uint8_t key_code[4];

	*(lsp_uint32_t *)key_code = lsp_htobel(key);

	for (i = 0; i < len / 4; ++i) 
	{
		lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &buf[i*4];
		res[0] = (lsp_uint8_t)   pwd[1] ^ pwd[7] ^ p[3] ^ key_code[0];
		res[1] = (lsp_uint8_t) ~(pwd[0] ^ pwd[3] ^ p[0] ^ key_code[3]);
		res[2] = (lsp_uint8_t)   pwd[2] ^ pwd[6] ^ p[1] ^ key_code[1];
		res[3] = (lsp_uint8_t) ~(pwd[4] ^ pwd[5] ^ p[2] ^ key_code[2]);
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32ex(
	__inout_ecount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	lsp_uint8_t ckey_code[4];

	*(lsp_uint32_t *)ckey_code = lsp_htobel(ckey);

	for (i = 0; i < len / 4; i++)
	{
		lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &buf[i*4];
		res[0] = (lsp_uint8_t) ckey_code[3] ^ p[3];
		res[1] = (lsp_uint8_t) ~(ckey_code[2] ^ p[0]);
		res[2] = (lsp_uint8_t) ckey_code[1] ^ p[1];
		res[3] = (lsp_uint8_t) ~(ckey_code[0] ^ p[2]);
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32exx(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	for (i = 0; i < len / 4; i++)
	{
		lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &buf[i*4];
		res[0] = (lsp_uint8_t) ((ckey >> 0 ) & 0xFF) ^ p[3];
		res[1] = (lsp_uint8_t) ~(((ckey >> 8 ) & 0xFF) ^ p[0]);
		res[2] = (lsp_uint8_t) ((ckey >> 16) & 0xFF) ^ p[1];
		res[3] = (lsp_uint8_t) ~(((ckey >> 24) & 0xFF) ^ p[2]);
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_encrypt32ex_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	lsp_uint8_t ckey_code[4];

	*(lsp_uint32_t *)ckey_code = lsp_htobel(ckey);

	for (i = 0; i < len / 4; ++i)
	{
		dst[i*4 + 0] = (lsp_uint8_t)  ckey_code[3] ^ src[i*4 + 3];
		dst[i*4 + 1] = (lsp_uint8_t) ~(ckey_code[2] ^ src[i*4 + 0]);
		dst[i*4 + 2] = (lsp_uint8_t)  ckey_code[1] ^ src[i*4 + 1];
		dst[i*4 + 3] = (lsp_uint8_t) ~(ckey_code[0] ^ src[i*4 + 2]);
	}
}

void
lsp_call
lsp_encrypt32exx_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	for (i = 0; i < len / 4; ++i)
	{
		dst[i*4 + 0] = (lsp_uint8_t)   ((ckey >> 0 ) & 0xFF) ^ src[i*4 + 3];
		dst[i*4 + 1] = (lsp_uint8_t) ~(((ckey >> 8 ) & 0xFF) ^ src[i*4 + 0]);
		dst[i*4 + 2] = (lsp_uint8_t)   ((ckey >> 16) & 0xFF) ^ src[i*4 + 1];
		dst[i*4 + 3] = (lsp_uint8_t) ~(((ckey >> 24) & 0xFF) ^ src[i*4 + 2]);
	}
}

void
lsp_call
lsp_decrypt32(
	__inout_ecount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in const lsp_uint32_t key,
	__in_ecount(8) const lsp_uint8_t* pwd)
{
	lsp_uint32_t i;
	lsp_uint8_t key_code[4];

	*(lsp_uint32_t *)key_code = lsp_letohl(key);

	for (i = 0; i < len / 4; ++i)
	{
		lsp_uint8_t* p = &buf[i*4];
		lsp_uint8_t  res[4];
		res[0] = pwd[0] ^ pwd[3] ^ ~(p[1]) ^ key_code[0];
		res[1] = pwd[2] ^ pwd[6] ^ p[2] ^ key_code[2];
		res[2] = pwd[4] ^ pwd[5] ^ p[3] ^ ~(key_code[1]);
		res[3] = pwd[1] ^ pwd[7] ^ p[0] ^ key_code[3];
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_decrypt32ex(
	__inout_ecount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in const lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	lsp_uint8_t ckey_code[4];

	*(lsp_uint32_t *)ckey_code = lsp_htobel(ckey);

	for (i = 0; i < len/ 4; i++)
	{
		lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &buf[i*4];
		res[0] = ckey_code[3] ^ p[1];
		res[1] = ckey_code[2] ^ p[2];
		res[2] = ckey_code[1] ^ p[3];
		res[3] = ckey_code[0] ^ p[0];
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_decrypt32exx(
	__inout_ecount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	for (i = 0; i < len / 4; i++)
	{
		lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &buf[i*4];
		res[0] = (lsp_uint8_t) ((ckey >>  0 ) & 0xFF) ^ p[1];
		res[1] = (lsp_uint8_t) ((ckey >>  8 ) & 0xFF) ^ p[2];
		res[2] = (lsp_uint8_t) ((ckey >> 16 ) & 0xFF) ^ p[3];
		res[3] = (lsp_uint8_t) ((ckey >> 24 ) & 0xFF) ^ p[0];
		lsp_memcpy(p, res, 4);
	}
}

void
lsp_call
lsp_decrypt32ex_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	lsp_uint8_t ckey_code[4];

	*(lsp_uint32_t *)ckey_code = lsp_htobel(ckey);

	for (i = 0; i < len/ 4; i++)
	{
		const lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &src[i*4];
		res[0] = ckey_code[3] ^ p[1];
		res[1] = ckey_code[2] ^ p[2];
		res[2] = ckey_code[1] ^ p[3];
		res[3] = ckey_code[0] ^ p[0];
		lsp_memcpy(&dst[i*4], res, 4);
	}
}

void
lsp_call
lsp_decrypt32exx_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey)
{
	lsp_uint32_t i;
	for (i = 0; i < len / 4; i++)
	{
		const lsp_uint8_t* p;
		lsp_uint8_t  res[4];
		p = &src[i*4];
		res[0] = (lsp_uint8_t) ((ckey >>  0 ) & 0xFF) ^ p[1];
		res[1] = (lsp_uint8_t) ((ckey >>  8 ) & 0xFF) ^ p[2];
		res[2] = (lsp_uint8_t) ((ckey >> 16 ) & 0xFF) ^ p[3];
		res[3] = (lsp_uint8_t) ((ckey >> 24 ) & 0xFF) ^ p[0];
		lsp_memcpy(&dst[i*4], res, 4);
	}
}


