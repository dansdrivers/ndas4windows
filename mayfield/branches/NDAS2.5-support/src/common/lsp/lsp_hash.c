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


#define POLY32 0x04c11db7

//
// pResult is Big Endian...
// pSource and pKey are Little Endian.
//

unsigned char aes_sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

unsigned char aes_inv_sbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

void
lsp_call
lsp_aes_cipher(
		   lsp_uint8* pText_in,
		   lsp_uint8* pText_out,
		   lsp_uint8* pKey
		   )
{
	unsigned char	sa[4][4];
	unsigned char	sa_sub[4][4];
	unsigned char	sa_sr[4][4];
	
	unsigned char	w0[11][4];
	unsigned char	w1[11][4];
	unsigned char	w2[11][4];
	unsigned char	w3[11][4];
	unsigned char	rcon[10][4];
	unsigned char	subword[4];

	unsigned char	i, j, k;

	rcon[0][3] = 0x01;
	rcon[1][3] = 0x02;
	rcon[2][3] = 0x04;
	rcon[3][3] = 0x08;
	rcon[4][3] = 0x10;
	rcon[5][3] = 0x20;
	rcon[6][3] = 0x40;
	rcon[7][3] = 0x80;
	rcon[8][3] = 0x1b;
	rcon[9][3] = 0x36;

	for (i = 0; i <= 9; i++) {
		rcon[i][2] = 0;
		rcon[i][1] = 0;
		rcon[i][0] = 0;
	}

	w3[0][0] = pKey[15];
	w3[0][1] = pKey[14];
	w3[0][2] = pKey[13];
	w3[0][3] = pKey[12];
	w2[0][0] = pKey[11];
	w2[0][1] = pKey[10];
	w2[0][2] = pKey[ 9];
	w2[0][3] = pKey[ 8];
	w1[0][0] = pKey[ 7];
	w1[0][1] = pKey[ 6];
	w1[0][2] = pKey[ 5];
	w1[0][3] = pKey[ 4];
	w0[0][0] = pKey[ 3];
	w0[0][1] = pKey[ 2];
	w0[0][2] = pKey[ 1];
	w0[0][3] = pKey[ 0];

	for (i = 1; i <= 10; i++) {
		subword[3] = aes_sbox[w3[i-1][2]];
		subword[2] = aes_sbox[w3[i-1][1]];
		subword[1] = aes_sbox[w3[i-1][0]];
		subword[0] = aes_sbox[w3[i-1][3]];

		for (j = 0; j < 4; j++) {
			w0[i][j] = w0[i-1][j] ^ subword[j] ^ rcon[i-1][j];
			w1[i][j] = w0[i][j] ^ w1[i-1][j];
			w2[i][j] = w1[i][j] ^ w2[i-1][j];
			w3[i][j] = w2[i][j] ^ w3[i-1][j];
		}
	}

//	printf("%2x %2x %2x %2x\n", pText_in[12], pText_in[13], pText_in[14], pText_in[15]);

	sa[3][3] = pText_in[15] ^ w3[0][0];
	sa[2][3] = pText_in[14] ^ w3[0][1];
	sa[1][3] = pText_in[13] ^ w3[0][2];
	sa[0][3] = pText_in[12] ^ w3[0][3];
	sa[3][2] = pText_in[11] ^ w2[0][0];
	sa[2][2] = pText_in[10] ^ w2[0][1];
	sa[1][2] = pText_in[ 9] ^ w2[0][2];
	sa[0][2] = pText_in[ 8] ^ w2[0][3];
	sa[3][1] = pText_in[ 7] ^ w1[0][0];
	sa[2][1] = pText_in[ 6] ^ w1[0][1];
	sa[1][1] = pText_in[ 5] ^ w1[0][2];
	sa[0][1] = pText_in[ 4] ^ w1[0][3];
	sa[3][0] = pText_in[ 3] ^ w0[0][0];
	sa[2][0] = pText_in[ 2] ^ w0[0][1];
	sa[1][0] = pText_in[ 1] ^ w0[0][2];
	sa[0][0] = pText_in[ 0] ^ w0[0][3];

	for (i = 1; i <= 9; i++) {
		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k ++)
				sa_sub[j][k] = aes_sbox[sa[j][k]];
		
		sa_sr[0][0] = sa_sub[0][0];
		sa_sr[0][1] = sa_sub[0][1];
		sa_sr[0][2] = sa_sub[0][2];
		sa_sr[0][3] = sa_sub[0][3];
		sa_sr[1][0] = sa_sub[1][1];
		sa_sr[1][1] = sa_sub[1][2];
		sa_sr[1][2] = sa_sub[1][3];
		sa_sr[1][3] = sa_sub[1][0];
		sa_sr[2][0] = sa_sub[2][2];
		sa_sr[2][1] = sa_sub[2][3];
		sa_sr[2][2] = sa_sub[2][0];
		sa_sr[2][3] = sa_sub[2][1];
		sa_sr[3][0] = sa_sub[3][3];
		sa_sr[3][1] = sa_sub[3][0];
		sa_sr[3][2] = sa_sub[3][1];
		sa_sr[3][3] = sa_sub[3][2];

		sa[0][0] = sa_sr[0][0] ^ w0[i][3];
		sa[0][1] = sa_sr[0][1] ^ w1[i][3];
		sa[0][2] = sa_sr[0][2] ^ w2[i][3];
		sa[0][3] = sa_sr[0][3] ^ w3[i][3];
		sa[1][0] = sa_sr[1][0] ^ w0[i][2];
		sa[1][1] = sa_sr[1][1] ^ w1[i][2];
		sa[1][2] = sa_sr[1][2] ^ w2[i][2];
		sa[1][3] = sa_sr[1][3] ^ w3[i][2];
		sa[2][0] = sa_sr[2][0] ^ w0[i][1];
		sa[2][1] = sa_sr[2][1] ^ w1[i][1];
		sa[2][2] = sa_sr[2][2] ^ w2[i][1];
		sa[2][3] = sa_sr[2][3] ^ w3[i][1];
		sa[3][0] = sa_sr[3][0] ^ w0[i][0];
		sa[3][1] = sa_sr[3][1] ^ w1[i][0];
		sa[3][2] = sa_sr[3][2] ^ w2[i][0];
		sa[3][3] = sa_sr[3][3] ^ w3[i][0];

//		printf("%2x %2x %2x %2x\n", sa[3][3], sa[3][2], sa[3][1], sa[3][0]);

	}

	for (j = 0; j < 4; j++)
		for (k = 0; k < 4; k ++)
			sa_sub[j][k] = aes_sbox[sa[j][k]];

	sa_sr[0][0] = sa_sub[0][0];
	sa_sr[0][1] = sa_sub[0][1];
	sa_sr[0][2] = sa_sub[0][2];
	sa_sr[0][3] = sa_sub[0][3];
	sa_sr[1][0] = sa_sub[1][1];
	sa_sr[1][1] = sa_sub[1][2];
	sa_sr[1][2] = sa_sub[1][3];
	sa_sr[1][3] = sa_sub[1][0];
	sa_sr[2][0] = sa_sub[2][2];
	sa_sr[2][1] = sa_sub[2][3];
	sa_sr[2][2] = sa_sub[2][0];
	sa_sr[2][3] = sa_sub[2][1];
	sa_sr[3][0] = sa_sub[3][3];
	sa_sr[3][1] = sa_sub[3][0];
	sa_sr[3][2] = sa_sub[3][1];
	sa_sr[3][3] = sa_sub[3][2];

	pText_out[ 0] = sa_sr[0][0] ^ w0[10][3];
	pText_out[ 1] = sa_sr[1][0] ^ w0[10][2];
	pText_out[ 2] = sa_sr[2][0] ^ w0[10][1];
	pText_out[ 3] = sa_sr[3][0] ^ w0[10][0];
	pText_out[ 4] = sa_sr[0][1] ^ w1[10][3];
	pText_out[ 5] = sa_sr[1][1] ^ w1[10][2];
	pText_out[ 6] = sa_sr[2][1] ^ w1[10][1];
	pText_out[ 7] = sa_sr[3][1] ^ w1[10][0];
	pText_out[ 8] = sa_sr[0][2] ^ w2[10][3];
	pText_out[ 9] = sa_sr[1][2] ^ w2[10][2];
	pText_out[10] = sa_sr[2][2] ^ w2[10][1];
	pText_out[11] = sa_sr[3][2] ^ w2[10][0];
	pText_out[12] = sa_sr[0][3] ^ w3[10][3];
	pText_out[13] = sa_sr[1][3] ^ w3[10][2];
	pText_out[14] = sa_sr[2][3] ^ w3[10][1];
	pText_out[15] = sa_sr[3][3] ^ w3[10][0];
}

void
lsp_call
lsp_aes_cipher_dummy(
		   lsp_uint8* pText_in,
		   lsp_uint8* pText_out,
		   lsp_uint8* pKey
		   )
{
	unsigned char i;

	for (i = 0; i < 16; i++) {
		pText_out[i] = pText_in[i] ^ pKey[i];
//		printf("%x %x %x\n", pText_in[i], pKey[i], pText_out[i]);
	}
}

void
lsp_call
lsp_encrypt128(
		   unsigned char	*pData,
		   unsigned _int32	uiDataLength,
		   unsigned char	*pKey,
		   unsigned char	*pPassword
		   )
{
	unsigned char	sa[4][4];
	unsigned char	sa_sub[4][4];
	unsigned char	sa_sr[4][4];
	
	unsigned char	w0[11][4];
	unsigned char	w1[11][4];
	unsigned char	w2[11][4];
	unsigned char	w3[11][4];
	unsigned char	rcon[10][4];
	unsigned char	subword[4];

	unsigned	 	i, j, k, l;

	rcon[0][3] = 0x01;
	rcon[1][3] = 0x02;
	rcon[2][3] = 0x04;
	rcon[3][3] = 0x08;
	rcon[4][3] = 0x10;
	rcon[5][3] = 0x20;
	rcon[6][3] = 0x40;
	rcon[7][3] = 0x80;
	rcon[8][3] = 0x1b;
	rcon[9][3] = 0x36;

	for (i = 0; i <= 9; i++) {
		rcon[i][2] = 0;
		rcon[i][1] = 0;
		rcon[i][0] = 0;
	}

	w3[0][0] = pKey[15] ^ pPassword[15];
	w3[0][1] = pKey[14] ^ pPassword[14];
	w3[0][2] = pKey[13] ^ pPassword[13];
	w3[0][3] = pKey[12] ^ pPassword[12];
	w2[0][0] = pKey[11] ^ pPassword[11];
	w2[0][1] = pKey[10] ^ pPassword[10];
	w2[0][2] = pKey[ 9] ^ pPassword[ 9];
	w2[0][3] = pKey[ 8] ^ pPassword[ 8];
	w1[0][0] = pKey[ 7] ^ pPassword[ 7];
	w1[0][1] = pKey[ 6] ^ pPassword[ 6];
	w1[0][2] = pKey[ 5] ^ pPassword[ 5];
	w1[0][3] = pKey[ 4] ^ pPassword[ 4];
	w0[0][0] = pKey[ 3] ^ pPassword[ 3];
	w0[0][1] = pKey[ 2] ^ pPassword[ 2];
	w0[0][2] = pKey[ 1] ^ pPassword[ 1];
	w0[0][3] = pKey[ 0] ^ pPassword[ 0];

	for (i = 1; i <= 10; i++) {
		subword[3] = aes_sbox[w3[i-1][2]];
		subword[2] = aes_sbox[w3[i-1][1]];
		subword[1] = aes_sbox[w3[i-1][0]];
		subword[0] = aes_sbox[w3[i-1][3]];

		for (j = 0; j < 4; j++) {
			w0[i][j] = w0[i-1][j] ^ subword[j] ^ rcon[i-1][j];
			w1[i][j] = w0[i][j] ^ w1[i-1][j];
			w2[i][j] = w1[i][j] ^ w2[i-1][j];
			w3[i][j] = w2[i][j] ^ w3[i-1][j];
		}
	}

	for (l = 0; l < (uiDataLength + 15) / 16; l++) {
		sa[3][3] = pData[l*16+15] ^ w3[0][0];
		sa[2][3] = pData[l*16+14] ^ w3[0][1];
		sa[1][3] = pData[l*16+13] ^ w3[0][2];
		sa[0][3] = pData[l*16+12] ^ w3[0][3];
		sa[3][2] = pData[l*16+11] ^ w2[0][0];
		sa[2][2] = pData[l*16+10] ^ w2[0][1];
		sa[1][2] = pData[l*16+ 9] ^ w2[0][2];
		sa[0][2] = pData[l*16+ 8] ^ w2[0][3];
		sa[3][1] = pData[l*16+ 7] ^ w1[0][0];
		sa[2][1] = pData[l*16+ 6] ^ w1[0][1];
		sa[1][1] = pData[l*16+ 5] ^ w1[0][2];
		sa[0][1] = pData[l*16+ 4] ^ w1[0][3];
		sa[3][0] = pData[l*16+ 3] ^ w0[0][0];
		sa[2][0] = pData[l*16+ 2] ^ w0[0][1];
		sa[1][0] = pData[l*16+ 1] ^ w0[0][2];
		sa[0][0] = pData[l*16+ 0] ^ w0[0][3];

		for (i = 1; i <= 9; i++) {
			for (j = 0; j < 4; j++)
				for (k = 0; k < 4; k ++)
					sa_sub[j][k] = aes_sbox[sa[j][k]];
		
			sa_sr[0][0] = sa_sub[0][0];
			sa_sr[0][1] = sa_sub[0][1];
			sa_sr[0][2] = sa_sub[0][2];
			sa_sr[0][3] = sa_sub[0][3];
			sa_sr[1][0] = sa_sub[1][1];
			sa_sr[1][1] = sa_sub[1][2];
			sa_sr[1][2] = sa_sub[1][3];
			sa_sr[1][3] = sa_sub[1][0];
			sa_sr[2][0] = sa_sub[2][2];
			sa_sr[2][1] = sa_sub[2][3];
			sa_sr[2][2] = sa_sub[2][0];
			sa_sr[2][3] = sa_sub[2][1];
			sa_sr[3][0] = sa_sub[3][3];
			sa_sr[3][1] = sa_sub[3][0];
			sa_sr[3][2] = sa_sub[3][1];
			sa_sr[3][3] = sa_sub[3][2];

			sa[0][0] = sa_sr[0][0] ^ w0[i][3];
			sa[0][1] = sa_sr[0][1] ^ w1[i][3];
			sa[0][2] = sa_sr[0][2] ^ w2[i][3];
			sa[0][3] = sa_sr[0][3] ^ w3[i][3];
			sa[1][0] = sa_sr[1][0] ^ w0[i][2];
			sa[1][1] = sa_sr[1][1] ^ w1[i][2];
			sa[1][2] = sa_sr[1][2] ^ w2[i][2];
			sa[1][3] = sa_sr[1][3] ^ w3[i][2];
			sa[2][0] = sa_sr[2][0] ^ w0[i][1];
			sa[2][1] = sa_sr[2][1] ^ w1[i][1];
			sa[2][2] = sa_sr[2][2] ^ w2[i][1];
			sa[2][3] = sa_sr[2][3] ^ w3[i][1];
			sa[3][0] = sa_sr[3][0] ^ w0[i][0];
			sa[3][1] = sa_sr[3][1] ^ w1[i][0];
			sa[3][2] = sa_sr[3][2] ^ w2[i][0];
			sa[3][3] = sa_sr[3][3] ^ w3[i][0];
		}

		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k ++)
				sa_sub[j][k] = aes_sbox[sa[j][k]];

		sa_sr[0][0] = sa_sub[0][0];
		sa_sr[0][1] = sa_sub[0][1];
		sa_sr[0][2] = sa_sub[0][2];
		sa_sr[0][3] = sa_sub[0][3];
		sa_sr[1][0] = sa_sub[1][1];
		sa_sr[1][1] = sa_sub[1][2];
		sa_sr[1][2] = sa_sub[1][3];
		sa_sr[1][3] = sa_sub[1][0];
		sa_sr[2][0] = sa_sub[2][2];
		sa_sr[2][1] = sa_sub[2][3];
		sa_sr[2][2] = sa_sub[2][0];
		sa_sr[2][3] = sa_sub[2][1];
		sa_sr[3][0] = sa_sub[3][3];
		sa_sr[3][1] = sa_sub[3][0];
		sa_sr[3][2] = sa_sub[3][1];
		sa_sr[3][3] = sa_sub[3][2];

		pData[16*l+ 0] = sa_sr[0][0] ^ w0[10][3];
		pData[16*l+ 1] = sa_sr[1][0] ^ w0[10][2];
		pData[16*l+ 2] = sa_sr[2][0] ^ w0[10][1];
		pData[16*l+ 3] = sa_sr[3][0] ^ w0[10][0];
		pData[16*l+ 4] = sa_sr[0][1] ^ w1[10][3];
		pData[16*l+ 5] = sa_sr[1][1] ^ w1[10][2];
		pData[16*l+ 6] = sa_sr[2][1] ^ w1[10][1];
		pData[16*l+ 7] = sa_sr[3][1] ^ w1[10][0];
		pData[16*l+ 8] = sa_sr[0][2] ^ w2[10][3];
		pData[16*l+ 9] = sa_sr[1][2] ^ w2[10][2];
		pData[16*l+10] = sa_sr[2][2] ^ w2[10][1];
		pData[16*l+11] = sa_sr[3][2] ^ w2[10][0];
		pData[16*l+12] = sa_sr[0][3] ^ w3[10][3];
		pData[16*l+13] = sa_sr[1][3] ^ w3[10][2];
		pData[16*l+14] = sa_sr[2][3] ^ w3[10][1];
		pData[16*l+15] = sa_sr[3][3] ^ w3[10][0];
	}
}
			
void
lsp_call
lsp_decrypt128(
		   unsigned char	*pData,
		   unsigned _int32	uiDataLength,
		   unsigned char	*pKey,
		   unsigned char	*pPassword
		   )
{
	unsigned char	sa[4][4];
	unsigned char	sa_sub[4][4];
	unsigned char	sa_sr[4][4];
	
	unsigned char	w0[11][4];
	unsigned char	w1[11][4];
	unsigned char	w2[11][4];
	unsigned char	w3[11][4];
	unsigned char	rcon[10][4];
	unsigned char	subword[4];

	unsigned	 	i, j, k, l;

	rcon[0][3] = 0x01;
	rcon[1][3] = 0x02;
	rcon[2][3] = 0x04;
	rcon[3][3] = 0x08;
	rcon[4][3] = 0x10;
	rcon[5][3] = 0x20;
	rcon[6][3] = 0x40;
	rcon[7][3] = 0x80;
	rcon[8][3] = 0x1b;
	rcon[9][3] = 0x36;

	for (i = 0; i <= 9; i++) {
		rcon[i][2] = 0;
		rcon[i][1] = 0;
		rcon[i][0] = 0;
	}

	w3[10][0] = pKey[15] ^ pPassword[15];
	w3[10][1] = pKey[14] ^ pPassword[14];
	w3[10][2] = pKey[13] ^ pPassword[13];
	w3[10][3] = pKey[12] ^ pPassword[12];
	w2[10][0] = pKey[11] ^ pPassword[11];
	w2[10][1] = pKey[10] ^ pPassword[10];
	w2[10][2] = pKey[ 9] ^ pPassword[ 9];
	w2[10][3] = pKey[ 8] ^ pPassword[ 8];
	w1[10][0] = pKey[ 7] ^ pPassword[ 7];
	w1[10][1] = pKey[ 6] ^ pPassword[ 6];
	w1[10][2] = pKey[ 5] ^ pPassword[ 5];
	w1[10][3] = pKey[ 4] ^ pPassword[ 4];
	w0[10][0] = pKey[ 3] ^ pPassword[ 3];
	w0[10][1] = pKey[ 2] ^ pPassword[ 2];
	w0[10][2] = pKey[ 1] ^ pPassword[ 1];
	w0[10][3] = pKey[ 0] ^ pPassword[ 0];

	for (i = 1; i <= 10; i++) {
		subword[3] = aes_sbox[w3[11-i][2]];
		subword[2] = aes_sbox[w3[11-i][1]];
		subword[1] = aes_sbox[w3[11-i][0]];
		subword[0] = aes_sbox[w3[11-i][3]];

		for (j = 0; j < 4; j++) {
			w0[10-i][j] = w0[11-i][j] ^ subword[j] ^ rcon[i-1][j];
			w1[10-i][j] = w0[10-i][j] ^ w1[11-i][j];
			w2[10-i][j] = w1[10-i][j] ^ w2[11-i][j];
			w3[10-i][j] = w2[10-i][j] ^ w3[11-i][j];
		}
	}

	for (l = 0; l < (uiDataLength + 15) / 16; l++) {
		sa[3][3] = pData[l*16+15] ^ w3[0][0];
		sa[2][3] = pData[l*16+14] ^ w3[0][1];
		sa[1][3] = pData[l*16+13] ^ w3[0][2];
		sa[0][3] = pData[l*16+12] ^ w3[0][3];
		sa[3][2] = pData[l*16+11] ^ w2[0][0];
		sa[2][2] = pData[l*16+10] ^ w2[0][1];
		sa[1][2] = pData[l*16+ 9] ^ w2[0][2];
		sa[0][2] = pData[l*16+ 8] ^ w2[0][3];
		sa[3][1] = pData[l*16+ 7] ^ w1[0][0];
		sa[2][1] = pData[l*16+ 6] ^ w1[0][1];
		sa[1][1] = pData[l*16+ 5] ^ w1[0][2];
		sa[0][1] = pData[l*16+ 4] ^ w1[0][3];
		sa[3][0] = pData[l*16+ 3] ^ w0[0][0];
		sa[2][0] = pData[l*16+ 2] ^ w0[0][1];
		sa[1][0] = pData[l*16+ 1] ^ w0[0][2];
		sa[0][0] = pData[l*16+ 0] ^ w0[0][3];

		for (i = 1; i <= 9; i++) {
			sa_sr[0][0] = sa[0][0];
			sa_sr[0][1] = sa[0][1];
			sa_sr[0][2] = sa[0][2];
			sa_sr[0][3] = sa[0][3];
			sa_sr[1][0] = sa[1][3];
			sa_sr[1][1] = sa[1][0];
			sa_sr[1][2] = sa[1][1];
			sa_sr[1][3] = sa[1][2];
			sa_sr[2][0] = sa[2][2];
			sa_sr[2][1] = sa[2][3];
			sa_sr[2][2] = sa[2][0];
			sa_sr[2][3] = sa[2][1];
			sa_sr[3][0] = sa[3][1];
			sa_sr[3][1] = sa[3][2];
			sa_sr[3][2] = sa[3][3];
			sa_sr[3][3] = sa[3][0];

			for (j = 0; j < 4; j++)
				for (k = 0; k < 4; k ++)
					sa_sub[j][k] = aes_inv_sbox[sa_sr[j][k]];

			sa[0][0] = sa_sub[0][0] ^ w0[i][3];
			sa[0][1] = sa_sub[0][1] ^ w1[i][3];
			sa[0][2] = sa_sub[0][2] ^ w2[i][3];
			sa[0][3] = sa_sub[0][3] ^ w3[i][3];
			sa[1][0] = sa_sub[1][0] ^ w0[i][2];
			sa[1][1] = sa_sub[1][1] ^ w1[i][2];
			sa[1][2] = sa_sub[1][2] ^ w2[i][2];
			sa[1][3] = sa_sub[1][3] ^ w3[i][2];
			sa[2][0] = sa_sub[2][0] ^ w0[i][1];
			sa[2][1] = sa_sub[2][1] ^ w1[i][1];
			sa[2][2] = sa_sub[2][2] ^ w2[i][1];
			sa[2][3] = sa_sub[2][3] ^ w3[i][1];
			sa[3][0] = sa_sub[3][0] ^ w0[i][0];
			sa[3][1] = sa_sub[3][1] ^ w1[i][0];
			sa[3][2] = sa_sub[3][2] ^ w2[i][0];
			sa[3][3] = sa_sub[3][3] ^ w3[i][0];
		}

		sa_sr[0][0] = sa[0][0];
		sa_sr[0][1] = sa[0][1];
		sa_sr[0][2] = sa[0][2];
		sa_sr[0][3] = sa[0][3];
		sa_sr[1][0] = sa[1][3];
		sa_sr[1][1] = sa[1][0];
		sa_sr[1][2] = sa[1][1];
		sa_sr[1][3] = sa[1][2];
		sa_sr[2][0] = sa[2][2];
		sa_sr[2][1] = sa[2][3];
		sa_sr[2][2] = sa[2][0];
		sa_sr[2][3] = sa[2][1];
		sa_sr[3][0] = sa[3][1];
		sa_sr[3][1] = sa[3][2];
		sa_sr[3][2] = sa[3][3];
		sa_sr[3][3] = sa[3][0];

		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k ++)
				sa_sub[j][k] = aes_inv_sbox[sa_sr[j][k]];

		pData[16*l+ 0] = sa_sub[0][0] ^ w0[10][3];
		pData[16*l+ 1] = sa_sub[1][0] ^ w0[10][2];
		pData[16*l+ 2] = sa_sub[2][0] ^ w0[10][1];
		pData[16*l+ 3] = sa_sub[3][0] ^ w0[10][0];
		pData[16*l+ 4] = sa_sub[0][1] ^ w1[10][3];
		pData[16*l+ 5] = sa_sub[1][1] ^ w1[10][2];
		pData[16*l+ 6] = sa_sub[2][1] ^ w1[10][1];
		pData[16*l+ 7] = sa_sub[3][1] ^ w1[10][0];
		pData[16*l+ 8] = sa_sub[0][2] ^ w2[10][3];
		pData[16*l+ 9] = sa_sub[1][2] ^ w2[10][2];
		pData[16*l+10] = sa_sub[2][2] ^ w2[10][1];
		pData[16*l+11] = sa_sub[3][2] ^ w2[10][0];
		pData[16*l+12] = sa_sub[0][3] ^ w3[10][3];
		pData[16*l+13] = sa_sub[1][3] ^ w3[10][2];
		pData[16*l+14] = sa_sub[2][3] ^ w3[10][1];
		pData[16*l+15] = sa_sub[3][3] ^ w3[10][0];
	}
}

void
lsp_call
lsp_crc32(
	  lsp_uint8* pData,
	  lsp_uint8* Out,
	  unsigned		uiDataLength
	  )
{
	unsigned _int32	crc;
	unsigned		i, j;

	crc = 0x52325032;
	for (i = 0; i < uiDataLength; i++) {
		crc = crc ^ (pData[i] << 24);
		for (j = 0; j < 8; j++) {
			if (crc & 0x80000000) crc = (crc << 1) ^ POLY32;
			else crc = crc << 1;
		}
	}
	Out[0] = (crc >> 24) & 0xff;
	Out[1] = (crc >> 16) & 0xff;
	Out[2] = (crc >> 8) & 0xff;
	Out[3] = crc & 0xff;
/*
	pData[uiDataLength] = (crc >> 24) & 0xff;
	pData[uiDataLength+1] = (crc >> 16) & 0xff;
	pData[uiDataLength+2] = (crc >> 8) & 0xff;
	pData[uiDataLength+3] = crc & 0xff;
*/	
}

		


