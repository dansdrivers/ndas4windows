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
#ifndef __XIXCORE_MD5_H__
#define __XIXCORE_MD5_H__



#define MD5DIGEST_HASH_WORD		4
#define MD5DIGEST_BLOCK_WORD	16
#define MD5DIGEST_SIZE			16

typedef struct _MD5DIGEST_CTX {
	uint32 hash[MD5DIGEST_HASH_WORD];
	uint32 block[MD5DIGEST_BLOCK_WORD];
	uint64 bytecount;
}MD5DIGEST_CTX, *PMD5DIGEST_CTX;


void
md5digest_metadata(
	uint8 buffer[],
	uint32	buffer_size,
	uint8	out[]
);



#endif // #ifndef __XIXCORE_MD5_H__

