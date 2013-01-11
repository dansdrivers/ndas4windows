/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#include <windows.h>
#include "xixRawDiskData.h"
#include "md5.h"


#define MD5FUNC1(x, y, z)	( (z) ^  ( (x) &  ((y) ^ (z)) ) )
#define MD5FUNC2(x, y, z)	MD5FUNC1(z, x, y)
#define MD5FUNC3(x, y, z)	((x) ^ (y) ^ (z))
#define MD5FUNC4(x, y, z)	( (y) ^ ((x) | ~(z)) )


#define MD5STEP1(w, x, y, z, in, s) \
	((w) += MD5FUNC1((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define MD5STEP2(w, x, y, z, in, s) \
	((w) += MD5FUNC2((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define MD5STEP3(w, x, y, z, in, s) \
	((w) += MD5FUNC3((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define MD5STEP4(w, x, y, z, in, s) \
	((w) += MD5FUNC4((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )





static 
void 
md5_function(
	uint32 *hash, 
	uint32 const *in)
{
	uint32 a, b, c, d;

	a = hash[0];
	b = hash[1];
	c = hash[2];
	d = hash[3];

	MD5STEP1(a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP1(d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP1(c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP1(b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP1(a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP1(d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP1(c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP1(b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP1(a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP1(d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP1(c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP1(b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP1(a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP1(d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP1(c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP1(b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP2(a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP2(d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP2(c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP2(b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP2(a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP2(d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP2(c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP2(b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP2(a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP2(d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP2(c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP2(b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP2(a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP2(d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP2(c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP2(b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP3(a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP3(d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP3(c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP3(b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP3(a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP3(d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP3(c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP3(b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP3(a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP3(d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP3(c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP3(b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP3(a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP3(d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP3(c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP3(b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP4(a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP4(d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP4(c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP4(b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP4(a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP4(d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP4(c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP4(b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP4(a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP4(d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP4(c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP4(b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP4(a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP4(d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP4(c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP4(b, c, d, a, in[9] + 0xeb86d391, 21);

	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
}

static
void 
md5_transform(
	PMD5DIGEST_CTX ctx
)
{
	md5_function(ctx->hash, ctx->block);
}


static 
void
md5digest_ctx_init(
	PMD5DIGEST_CTX ctx
)
{
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->bytecount = 0;
}


static
void
md5digest_update(
	 PMD5DIGEST_CTX		ctx,
	 const uint8				data[], 
	 uint32					len
)
{
	const uint32	avail = sizeof(ctx->block) - (uint32)(ctx->bytecount & 0x3f);

	ctx->bytecount += len;

	if(avail > len){
		memcpy((uint8 *)ctx->block + ( sizeof(ctx->block) - avail), data, len);
		return;
	}

	memcpy((uint8 *)ctx->block + ( sizeof(ctx->block) - avail), data, avail);


	md5_transform(ctx);
	data += avail;
	len -= avail;

	while( len >= sizeof(ctx->block))
	{
		memcpy(ctx->block, data, sizeof(ctx->block));
		md5_transform(ctx);
		data += sizeof(ctx->block);
		len -= sizeof(ctx->block);
	}

	memcpy(ctx->block, data, len);

}


static 
void 
md5digest_final(
	PMD5DIGEST_CTX		ctx, 
	uint8 out[]
)
{
	const uint32 offset = (uint32)(ctx->bytecount & 0x3f);
	uint8 *p = (uint8 *)ctx->block + offset;
	int32 padding = 56 - (offset + 1);

	*p++ = 0x80;
	if (padding < 0) {
		memset(p, 0x00, padding + sizeof(uint64));
		md5_transform(ctx);
		p = (uint8 *)ctx->block;
		padding = 56;
	}

	memset(p, 0, padding);
	ctx->block[14] = (uint32)(ctx->bytecount << 3);
	ctx->block[15] = (uint32)(ctx->bytecount >> 29);
	
	md5_function(ctx->hash, ctx->block);

	memcpy(out, ctx->hash, sizeof(ctx->hash));
	memset(ctx, 0, sizeof(MD5DIGEST_CTX));
}



void
md5digest_metadata(
	uint8	buffer[],
	uint32	buffer_size,
	uint8	out[]
	)
{
	MD5DIGEST_CTX		ctx;
	
	memset(&ctx, 0, sizeof(MD5DIGEST_CTX));
	

	md5digest_ctx_init(&ctx);
	md5digest_update(&ctx,buffer,buffer_size);
	md5digest_final(&ctx,out);
	return;
}



