#ifndef _LSP_H_INCLUDED_
#define _LSP_H_INCLUDED_

#include "lspspecstring.h"
#include "lsp_type.h"
#include "lsp_spec.h"
#include <hdreg.h>

/* WINDOWS PLATFORM SPECIFIC MACROS */

typedef enum _lsp_constants_t {
	LSP_MAX_REQUEST_SIZE = 1500
} lsp_constants_t;

#define LSP_MAX_CONCURRENT_TRANSFER 4

/* OTHER PLATFORM SPECIFIC MACROS */

#ifdef __cplusplus
extern "C" {
#endif

/* basic functions */

extern const lsp_uint32 LSP_SESSION_BUFFER_SIZE;

#ifdef LSP_OPTIMIZED

#define lsp_get_session_buffer_size() LSP_SESSION_BUFFER_SIZE

#else

lsp_uint32
lsp_call
lsp_get_session_buffer_size();

#endif

void*
lsp_call
lsp_get_buffer_to_send(
	lsp_handle_t lsp_handle, 
	lsp_uint32* len);

void*
lsp_call
lsp_get_buffer_to_receive(
	lsp_handle_t lsp_handle, 
	lsp_uint32* len);

lsp_handle_t
lsp_call 
lsp_create_session(
	void* session_buffer,
	void* context);

#ifdef LSP_OPTIMIZED

#define lsp_destroy_session(h)

#else

void
lsp_call 
lsp_destroy_session(
	lsp_handle_t h);

#endif

lsp_status_t
lsp_call
lsp_get_proc_context(
	lsp_handle_t h, 
	void **proc_context);

lsp_status_t
lsp_call 
lsp_set_proc_context(
	lsp_handle_t h, 
	void *proc_context);

lsp_status_t
lsp_call
lsp_process_next(
	lsp_handle_t h);

lsp_status_t 
lsp_call 
lsp_login(
	lsp_handle_t h, 
	const lsp_login_info_t* login_info);

lsp_status_t
lsp_call
lsp_request(
	__in lsp_handle_t h,
	__inout lsp_request_packet_t* request);

lsp_status_t 
lsp_call 
lsp_logout(
	lsp_handle_t h);

lsp_status_t 
lsp_call 
lsp_noop_command(lsp_handle_t h);

lsp_status_t 
lsp_call 
lsp_text_command(
	lsp_handle_t h, 
	lsp_uint8 param_type, 
	lsp_uint8 param_ver, 
	const lsp_uint8 *data, 
	lsp_uint16 data_in_length);

lsp_status_t
lsp_call
lsp_get_text_command_result(
	lsp_handle_t* h,
	lsp_uint8* data_out,
	lsp_uint32* data_out_length);

lsp_status_t 
lsp_call 
lsp_ide_command(
	__in lsp_handle_t h, 
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in const lsp_ide_register_param_t* p, 
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd);

void
lsp_call
lsp_build_ide_command(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id,
	__in lsp_uint32 lun0,
	__in lsp_uint32 lun1,
	__in const lsp_ide_register_param_t* idereg,
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd);

lsp_status_t
lsp_call
lsp_get_ide_command_output_register(
	__in lsp_handle_t h, 
	__out lsp_ide_register_param_t* idereg);

lsp_status_t
lsp_call
lsp_vendor_command(
	__in lsp_handle_t h,
	__in lsp_uint16 vendor_id,
	__in lsp_uint8 vop_ver,
	__in lsp_uint8 vop_code,
	__in_bcount(param_length) const lsp_uint8 *param,
	__in lsp_uint8 param_length,
	__in_opt const lsp_io_data_buffer_t* data_buf);

lsp_status_t
lsp_call
lsp_get_vendor_command_result(
	lsp_handle_t h,
	lsp_uint8 *param,
	lsp_uint8 param_length);

lsp_status_t 
lsp_call 
lsp_get_handle_info(
	lsp_handle_t h, 
	lsp_handle_info_type_t info_type, 
	void *data, 
	lsp_uint32 data_length);

lsp_status_t
lsp_call
lsp_set_options(
	__in lsp_handle_t h, 
	__in lsp_uint32 options);

lsp_status_t
lsp_call
lsp_get_options(
	__in lsp_handle_t h, 
	__out lsp_uint32* options);

/* external encryption support */

void
lsp_call
lsp_encrypt_send_data(
	__in lsp_handle_t h,
	__out_bcount(len) lsp_uint8* dst,
	__in_bcount(len) const lsp_uint8* src, 
	__in lsp_uint32 len);

/*++

lsp_set_chained_return sets the pointer to the function to call
on completion or error of the final process. It returns the existing
pointer to the function.

See also:

lsp_ide_handshake

--*/

typedef lsp_status_t (lsp_call *lsp_chained_proc_t)(lsp_handle_t lsp_handle, lsp_status_t lsp_status);

lsp_chained_proc_t
lsp_set_chained_return_proc(lsp_handle_t handle, lsp_chained_proc_t proc);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_H_INCLUDED_ */
