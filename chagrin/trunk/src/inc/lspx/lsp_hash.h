#ifndef _LSP_HASH_H_
#define _LSP_HASH_H_

#include <lsp_type.h>
#ifdef __cplusplus
extern "C" {
#endif

void 
lsp_call
lsp_hash32to128(
	__out_bcount(16) lsp_uint8* dst,
	__in_bcount(4) const lsp_uint8* src,  /*  32 bits,  4 bytes */
	__in_bcount(8) const lsp_uint8* key);

void
lsp_call
lsp_encrypt32(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_bcount(4) const lsp_uint8* key,
	__in_bcount(8) const lsp_uint8* pwd);

void
lsp_call
lsp_encrypt32ex(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_bcount(4) const lsp_uint8* ckey);

void
lsp_call
lsp_encrypt32exx(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey);

void
lsp_call
lsp_encrypt32ex_copy(
	__out_bcount(len) lsp_uint8* dst,
	__in_bcount(len) const lsp_uint8* src, 
	__in lsp_uint32 len,
	__in_bcount(4) const lsp_uint8* ckey);

void
lsp_call
lsp_encrypt32exx_copy(
	__out_bcount(len) lsp_uint8* dst,
	__in_bcount(len) const lsp_uint8* src, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey);

void
lsp_call
lsp_encrypt32_build_combined_key(
	__out_bcount(4) lsp_uint32* ckey,
	__in_bcount(4) const lsp_uint8* key,
	__in_bcount(8) const lsp_uint8* pwd);

void
lsp_call
lsp_decrypt32(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_bcount(4) const lsp_uint8* key,
	__in_bcount(8) const lsp_uint8* pwd);

void
lsp_call
lsp_decrypt32ex(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in_bcount(4) const lsp_uint8* ckey);

void
lsp_call
lsp_decrypt32exx(
	__inout_bcount(len) lsp_uint8* buf, 
	__in lsp_uint32 len,
	__in lsp_uint32 ckey);

void
lsp_call
lsp_decrypt32_build_combined_key(
	__out_bcount(4) lsp_uint32* ckey,
	__in_bcount(4) const lsp_uint8* key,
	__in_bcount(8) const lsp_uint8* pwd);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_HASH_H_ */
