/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

#ifndef _LSP_HASH_H_
#define _LSP_HASH_H_

#include "lsp_type.h"
#ifdef __cplusplus
extern "C" {
#endif

void 
lsp_call
lsp_hash_uint32_to128(
	__out_bcount(16) lsp_uint8_t* dst,
	__in lsp_uint32_t src,
	__in_bcount(8) const lsp_uint8_t* key);

void
lsp_call
lsp_encrypt32(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t key,
	__in_bcount(8) const lsp_uint8_t* pwd);

void
lsp_call
lsp_encrypt32ex(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_encrypt32exx(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_encrypt32ex_copy(
	__out_bcount(len) lsp_uint8_t* dst,
	__in_bcount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_encrypt32exx_copy(
	__out_bcount(len) lsp_uint8_t* dst,
	__in_bcount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_encrypt32_build_combined_key(
	__out_bcount(4) lsp_uint32_t* ckey,
	__in const lsp_uint32_t key,
	__in_bcount(8) const lsp_uint8_t* pwd);

void
lsp_call
lsp_decrypt32(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in const lsp_uint32_t key,
	__in_bcount(8) const lsp_uint8_t* pwd);

void
lsp_call
lsp_decrypt32ex(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in const lsp_uint32_t ckey);

void
lsp_call
lsp_decrypt32exx(
	__inout_bcount(len) lsp_uint8_t* buf, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_decrypt32_build_combined_key(
	__out_bcount(4) lsp_uint32_t* ckey,
	__in const lsp_uint32_t key,
	__in_bcount(8) const lsp_uint8_t* pwd);

void
lsp_call
lsp_decrypt32ex_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

void
lsp_call
lsp_decrypt32exx_copy(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_uint32_t ckey);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_HASH_H_ */
