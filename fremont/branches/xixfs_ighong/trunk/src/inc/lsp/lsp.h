#ifndef _LSP_H_INCLUDED_
#define _LSP_H_INCLUDED_

#include "lsp_type.h"
#include "lsp_spec.h"
#include <hdreg.h>

/* WINDOWS PLATFORM SPECIFIC MACROS */

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef __w64 unsigned int  size_t;
#endif  /* !_WIN64 */
#define _SIZE_T_DEFINED
#endif  /* !_SIZE_T_DEFINED */

#define LSP_MAX_REQUEST_SIZE	1500

/* OTHER PLATFORM SPECIFIC MACROS */

#ifdef __cplusplus
extern "C" {
#endif

typedef void* 
(lsp_proc_call *lstproc_mem_alloc)(
	void* context, 
	size_t size);

typedef void 
(lsp_proc_call *lstproc_mem_free)(
	void* context, 
	void* pblock);

typedef lsp_trans_error_t 
(lsp_proc_call *lstproc_send)(
	void* context, 
	const void* data, 
	size_t len, 
	size_t* sent, 
	void **wait_handle_ptr);

typedef lsp_trans_error_t 
(lsp_proc_call *lstproc_recv)(
	void* context, 
	void* buffer, 
	size_t len, 
	size_t* recvd, 
	void **wait_handle_ptr);

typedef lsp_trans_error_t 
(lsp_proc_call *lstproc_wait)(
	void* context, 
	size_t *bytes_transferred, 
	void *wait_handle);

typedef struct _lsp_transport_proc
{
	lstproc_mem_alloc mem_alloc;
	lstproc_mem_free mem_free;
	lstproc_send send;
	lstproc_recv recv;
	lstproc_wait wait;
} lsp_transport_proc, *lsp_transport_proc_ptr;

/* basic functions */

lsp_handle 
lsp_call 
lsp_create_session(
	lsp_transport_proc* proc_ptr, 
	void* proc_context);

void
lsp_call 
lsp_destroy_session(
	lsp_handle h);

lsp_error_t
lsp_call
lsp_get_proc_context(
	lsp_handle h, 
	void **proc_context);

lsp_error_t
lsp_call 
lsp_set_proc_context(
	lsp_handle h, 
	void *proc_context);

lsp_error_t 
lsp_call 
lsp_login(
	lsp_handle h, 
	const lsp_login_info_ptr login_info);

lsp_error_t 
lsp_call 
lsp_logout(
	lsp_handle h);

lsp_error_t 
lsp_call 
lsp_get_handle_info(
	lsp_handle h, 
	lsp_handle_info_type info_type, 
	void *data, 
	size_t data_length);

/* 

lsp_error_t 
lsp_call 
lsp_discover(lsp_handle h); 

*/

lsp_error_t 
lsp_call 
lsp_noop_command(lsp_handle h);

lsp_error_t 
lsp_call 
lsp_text_command(
	lsp_handle h, 
	lsp_uint8 param_type, 
	lsp_uint8 param_ver, 
	lsp_uint8 *data, 
	lsp_uint16 data_in_length, 
	lsp_uint16 *data_out_length);

lsp_error_t 
lsp_call 
lsp_ide_command(
	lsp_handle h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_ide_register_param_ptr p, 
	lsp_io_data_buffer_ptr data_buf,
	lsp_extended_command_ptr ext_cmd);

lsp_error_t
lsp_call
lsp_vendor_command(
	lsp_handle h, 
	lsp_uint16 vendor_id, 
	lsp_uint8 vop_ver, 
	lsp_uint8 vop_code, 
	lsp_uint8 *param, 
	lsp_uint8 param_length, 
	lsp_io_data_buffer_ptr data_buf);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_H_INCLUDED_ */
