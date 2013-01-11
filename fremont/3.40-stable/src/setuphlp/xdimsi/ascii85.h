/*
	ASCII85 Encoding

	Copyright 2006 Chesong Lee <patria@enterprisent.com>

	http://www.chesong.com/sw/ascii85

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
#ifndef _ASCII85_H_
#define _ASCII85_H_
#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif
#include <stdlib.h>

#ifdef  __cplusplus
extern "C"
{
#endif

/* Implementation of ASCII85 Encoding */

/* decode output callback function */
typedef void (*decode85_callback_t)(
	const unsigned char* buf, 
	size_t len, 
	void* context);

/* encode output callback function */
typedef void (*encode85_callback_t)(
	const char* buf, 
	size_t len, 
	void* context);

/* Returns 0 on success.
 * On error, returns the first character position (1-based index),
 * where decode fails */

size_t decode85(
	const char* p, 
	size_t len, 
	decode85_callback_t out, 
	void* context);

/* Encoding function */

void encode85(
	const unsigned char* p, 
	size_t len, 
	encode85_callback_t out, 
	void* context);

/* Streaming encoder */

typedef struct encode85_context_tag {
	char obuf[36];
	unsigned char ibuf[4];
	size_t obufp;
	size_t ibufp;
	encode85_callback_t out;
	void* context;
} encode85_context_t;

typedef struct decode85_context_tag {
	unsigned char obuf[36];
	unsigned int x;
	size_t obufp;
	size_t ibufp;
	decode85_callback_t out;
	void* context;
} decode85_context_t;

/* Streaming encoder */

void encode85_start(
	encode85_context_t* ec, 
	encode85_callback_t out, 
	void* context);

void encode85_p(
	encode85_context_t* ec, 
	const unsigned char* p, 
	size_t len);

void encode85_end(
	encode85_context_t* ec);

/* Streaming decoder */

void decode85_start(
	decode85_context_t* dc, 
	decode85_callback_t out, 
	void* context);

void decode85_end(
	decode85_context_t* dc);

size_t decode85_p(
	decode85_context_t* dc, 
	const char* p, 
	size_t len);

#ifdef  __cplusplus
}
#endif

#endif /* _ASCII85_H_ */
