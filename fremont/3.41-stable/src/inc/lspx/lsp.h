/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

#ifndef _LSP_H_INCLUDED_
#define _LSP_H_INCLUDED_

#include "lspspecstring.h"
#include "lsp_type.h"
#include "lsp_spec.h"
#include "lsp_ide_def.h"
#include "lsp_host.h"

#define LSP_MAX_CONCURRENT_TRANSFER 4

#ifdef __cplusplus
extern "C" {
#endif

/* in memory types */

typedef struct _lsp_io_data_buffer_t {
	lsp_uint8_t *recv_buffer;
	lsp_uint32_t recv_size;
	lsp_uint8_t *send_buffer;
	lsp_uint32_t send_size;
} lsp_io_data_buffer_t;

typedef struct _lsp_text_target_list_element_t {
	lsp_uint32_t target_id;
	lsp_uint8_t  rw_hosts;
	lsp_uint8_t  ro_hosts;
	lsp_uint16_t reserved;
	lsp_uint8_t  target_data[8];
} lsp_text_target_list_element_t;

typedef struct _lsp_text_target_list_t {
	lsp_uint8_t  type; /* LSP_TEXT_BINPARAM_TYPE_TARGET_LIST */
	lsp_uint8_t  number_of_elements;
	lsp_uint16_t reserved;
	lsp_text_target_list_element_t elements[2];
} lsp_text_target_list_t;

typedef struct _lsp_text_target_data_t {
	lsp_uint8_t  type; /* LSP_TEXT_BINPARAM_TYPE_TARGET_DATA */
	lsp_uint8_t  to_set; /* 0 to read, 1 to write */
	lsp_uint16_t reserved;
	lsp_uint32_t target_id;
	lsp_uint8_t  target_data[8];
} lsp_text_target_data_t;

typedef enum _lsp_text_binparam_type_t {
	LSP_TEXT_BINPARAM_TYPE_TARGET_LIST = 0x03,
	LSP_TEXT_BINPARAM_TYPE_TARGET_DATA = 0x04
} lsp_text_binparam_type_t;

/* used for packet command. V 1.1 or later. */
typedef struct _lsp_extended_command_t {
	lsp_uint8_t *cmd_buffer;
	lsp_uint32_t cmd_size; /* ATM, 12 bytes of command only */
} lsp_extended_command_t;

typedef struct _lsp_login_request_t {
	lsp_login_info_t login_info;
} lsp_login_request_t;

typedef struct _lsp_login_response_t {
	lsp_uint32_t reserved;
} lsp_login_response_t;

typedef struct _lsp_logout_request_t {
	lsp_uint32_t reserved;
} lsp_logout_request_t;

typedef struct _lsp_logout_response_t {
	lsp_uint32_t reserved;
} lsp_logout_response_t;

typedef struct _lsp_ata_handshake_request_t {
	lsp_uint32_t reserved;
} lsp_ata_handshake_request_t;

typedef struct _lsp_ata_handshake_response_t {
	const lsp_ata_handshake_data_t* handshake_data;
	union {
		const lsp_ide_identify_device_data_t* ata;
		const lsp_ide_identify_packet_device_data_t* atapi;
	} identify_data;
} lsp_ata_handshake_response_t;

typedef struct _lsp_ide_command_request_t {
	lsp_ide_register_param_t reg;
	lsp_io_data_buffer_t data_buf;
	lsp_extended_command_t ext_cmd;
} lsp_ide_command_request_t;

typedef struct _lsp_ide_command_response_t {
	lsp_ide_register_param_t reg;
} lsp_ide_command_response_t;

typedef struct _lsp_vendor_command_request_t {
	lsp_uint16_t vendor_id; 
	lsp_uint8_t vop_ver;
	lsp_uint8_t vop_code;
	lsp_uint8_t *param;
	lsp_uint8_t param_length;
	lsp_extended_command_t *ahs_request;
	lsp_extended_command_t *ahs_response;
	lsp_io_data_buffer_t data_buf;
} lsp_vendor_command_request_t;

typedef struct _lsp_vendor_command_response_t {
	lsp_uint32_t reserved;
} lsp_vendor_command_response_t;

typedef struct _lsp_text_command_request_t {
	lsp_uint8_t param_type;
	lsp_uint8_t param_ver;
	lsp_uint32_t data_in_length;
	const void* data_in;
	lsp_uint32_t data_out_length;
	void* data_out;
} lsp_text_command_request_t;

typedef struct _lsp_text_command_response_t {
	lsp_uint32_t reserved;
} lsp_text_command_response_t;

struct _lsp_request_packet_t;

typedef
lsp_status_t
(lsp_call *lsp_completion_routine_t)(
	lsp_handle_t h,
	struct _lsp_request_packet_t* request);

typedef struct _lsp_request_packet_t {
	lsp_request_type_t type;
	union {
		struct {
			lsp_login_request_t request;
			lsp_login_response_t response;
		} login;
		struct {
			lsp_logout_request_t request;
			lsp_logout_response_t response;
		} logout;
		struct {
			lsp_ide_command_request_t request;
			lsp_ide_command_response_t response;
		} ide_command;
		struct {
			lsp_text_command_request_t request;
			lsp_text_command_response_t response;
		} text_command;

		struct {
			lsp_vendor_command_request_t request;
			lsp_vendor_command_response_t response;
		} vendor_command;

		struct {
			lsp_ata_handshake_request_t request;
			lsp_ata_handshake_response_t response;
		} ata_handshake_command;
		
	} u;
	lsp_completion_routine_t completion_routine;
	struct _lsp_request_packet_t* original_request;
	/* output */
	__out lsp_status_t status;
} lsp_request_packet_t;

/* basic functions */

extern const lsp_uint32_t LSP_SESSION_BUFFER_SIZE;

#ifdef LSP_OPTIMIZED

#define lsp_get_session_buffer_size() LSP_SESSION_BUFFER_SIZE

#else

lsp_uint32_t
lsp_call
lsp_get_session_buffer_size(void);

#endif

void*
lsp_call
lsp_get_buffer_to_send(
	lsp_handle_t lsp_handle, 
	lsp_uint32_t* len);

void*
lsp_call
lsp_get_buffer_to_receive(
	lsp_handle_t lsp_handle, 
	lsp_uint32_t* len);

lsp_handle_t
lsp_call
lsp_initialize_session(
	__in_bcount(session_buffer_size) void* session_buffer,
	__in lsp_uint32_t session_buffer_size);

lsp_status_t
lsp_call
lsp_process_next(
	lsp_handle_t h);

lsp_status_t 
lsp_call 
lsp_login(
	__in lsp_handle_t h, 
	const lsp_login_info_t* login_info);

const lsp_login_info_t*
lsp_call
lsp_get_login_info(
	__in lsp_handle_t h);

const lsp_hardware_data_t*
lsp_call
lsp_get_hardware_data(
	__in lsp_handle_t h);

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
	__in lsp_handle_t h, 
	__in lsp_uint8_t param_type, 
	__in lsp_uint8_t param_ver, 
	__in_bcount(inbuf_len) const void*inbuf, 
	__in lsp_uint16_t inbuf_len,
	__out_bcount(outbuf_len) void* outbuf,
	__in lsp_uint16_t outbuf_len);

lsp_status_t
lsp_call
lsp_get_text_command_result(
	__in lsp_handle_t* h,
	__in lsp_uint8_t* data_out,
	__out lsp_uint32_t* data_out_length);

lsp_status_t 
lsp_call 
lsp_ide_command(
	__in lsp_handle_t h, 
	__inout lsp_ide_register_param_t* p, 
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd);

void
lsp_call
lsp_build_ide_command(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
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
	__in lsp_uint16_t vendor_id,
	__in lsp_uint8_t vop_ver,
	__in lsp_uint8_t vop_code,
	__in_bcount(param_length) const lsp_uint8_t *param,
	__in lsp_uint8_t param_length,
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt lsp_extended_command_t *ahs_request,
	__in_opt lsp_extended_command_t *ahs_response);

lsp_status_t
lsp_call
lsp_get_vendor_command_result(
	__in lsp_handle_t h,
	__out_bcount(param_length) lsp_uint8_t *param,
	__out lsp_uint8_t param_length,
	__out lsp_extended_command_t *ahs_response);

lsp_status_t
lsp_call
lsp_set_options(
	__in lsp_handle_t h, 
	__in lsp_uint32_t options);

lsp_status_t
lsp_call
lsp_get_options(
	__in lsp_handle_t h, 
	__out lsp_uint32_t* options);

/* external encryption support */

void
lsp_call
lsp_encrypt_send_data(
	__in lsp_handle_t h,
	__out_bcount(len) void* dst,
	__in_bcount(len) const void* src, 
	__in lsp_uint32_t len);

void
lsp_call
lsp_decrypt_recv_data(
	__in lsp_handle_t h,
	__out_bcount(len) void* dst,
	__in_bcount(len) const void* src, 
	__in lsp_uint32_t len);

void
lsp_call
lsp_encrypt_send_data_inplace(
	__in lsp_handle_t h,
	__inout_bcount(len) void* buf,
	__in lsp_uint32_t len);

void
lsp_call
lsp_decrypt_recv_data_inplace(
	__in lsp_handle_t h,
	__inout_bcount(len) void* buf,
	__in lsp_uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_H_INCLUDED_ */

