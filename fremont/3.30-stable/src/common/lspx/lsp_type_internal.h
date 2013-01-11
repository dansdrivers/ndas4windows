#ifndef _LSP_TYPE_INTERNAL_H_
#define _LSP_TYPE_INTERNAL_H_

#include <lsp.h>

typedef struct _lsp_pdu_pointers_t {
	lsp_pdu_hdr_t *header_ptr;
	lsp_uint8_t  *ahs_ptr;
	lsp_uint32_t ahs_len;
	lsp_uint8_t  *header_dig_ptr;
	lsp_uint8_t  *data_seg_ptr;
	lsp_uint32_t data_seg_len;
	lsp_uint8_t  *data_dig_ptr;
} lsp_pdu_pointers_t;

/* lsp_session_data */
/*  parameter types */
typedef enum _lsp_param_type_t {
	LSP_PARM_TYPE_TEXT   = 0x0,
	LSP_PARM_TYPE_BINARY = 0x1
} lsp_param_type_t;

typedef enum _lsp_session_phase_t {
	LSP_SP_LOGIN_INIT,
	LSP_SP_LOGIN_INIT_RETRY,
	LSP_SP_LOGIN_AUTH_1,
	LSP_SP_LOGIN_AUTH_2,
	LSP_SP_LOGIN_NEGO,
	LSP_SP_FULL_PHASE,
	LSP_SP_LOGGED_OUT
} lsp_session_phase_t;

typedef enum _lsp_session_phase_state_t {
	LSP_SP_STATE_PREPARE_SEND_HEADER,
	LSP_SP_STATE_BEGIN_SEND_HEADER,
	LSP_SP_STATE_ENCODE_SEND_DATA,
	LSP_SP_STATE_BEGIN_SEND_DATA,
	LSP_SP_STATE_END_SEND_HEADER,
	LSP_SP_STATE_END_SEND_DATA,
	LSP_SP_STATE_BEGIN_RECEIVE_DATA,
	LSP_SP_STATE_END_RECEIVE_DATA,
	LSP_SP_STATE_PROCESS_RECEIVED_DATA,
	LSP_SP_STATE_BEGIN_RECEIVE_HEADER,
	LSP_SP_STATE_END_RECEIVE_HEADER,
	LSP_SP_STATE_PROCESS_RECEIVED_HEADER,
	LSP_SP_STATE_BEGIN_RECEIVE_AH,
	LSP_SP_STATE_END_RECEIVE_AH,
	LSP_SP_STATE_PROCESS_RECEIVED_AH,
	LSP_SP_STATE_DECODE_RECEIVED_DATA,
} lsp_session_phase_state_t;

typedef enum _lsp_session_flags_t {
	LSP_SFG_PROTOCOL_VERSION_IS_SET = 0x00000001,
	LSP_SFG_NO_RESPONSE				= 0x00000002
} lsp_session_flags_t;

struct _lsp_handle_context_t;

typedef 
lsp_status_t 
(lsp_call *lsp_prepare_proc_t)(
	struct _lsp_handle_context_t* context);

typedef 
lsp_status_t 
(lsp_call *lsp_process_proc_t)(
	struct _lsp_handle_context_t* context);

typedef struct _lsp_session_data_t {

	lsp_uint32_t session_options;

	lsp_session_phase_t phase;
	lsp_session_phase_state_t phase_state;

	lsp_uint32_t hpid;
	lsp_uint16_t rpid;
	lsp_uint16_t cpslot;
	lsp_uint32_t path_cmd_tag;

	lsp_uint32_t chap_i;
	lsp_uint32_t chap_c;
	lsp_uint8_t  session_phase;

	/* login info cache */
	lsp_login_info_t login_info;

	/* handshake info cache */
	lsp_uint32_t handshake_set_dma_count;
	lsp_uint32_t handshake_set_pio_count;
	lsp_ata_handshake_data_t handshake_data;
	
	/* identify data cache */
	union
	{
		lsp_ide_identify_device_data_t ata;
		lsp_ide_identify_packet_device_data_t atapi;
	} identify_data;

	/* hardware data cache */
	lsp_hardware_data_t hardware_data;

	/* combined encrypt/decrypt key */
	lsp_uint32_t encrypt_ckey; 
	lsp_uint32_t decrypt_ckey;

	lsp_pdu_pointers_t pdu_ptrs;

	/* intermediate ones */
	lsp_uint32_t flags;

	lsp_uint32_t send_buffer_length;
	lsp_uint32_t send_data_buffer_length;
	lsp_uint32_t receive_data_buffer_length;
	lsp_uint32_t receive_buffer_length;

	void * send_buffer;
	void * send_data_buffer;
	void * receive_data_buffer;
	void * receive_buffer;

	lsp_uint8_t last_op_code;

	lsp_prepare_proc_t prepare_proc;
	lsp_process_proc_t process_proc;

	lsp_ide_register_param_t last_idereg;

	lsp_request_packet_t* current_request;

	lsp_request_packet_t internal_packets[2];

	lsp_int32_t wlk_pwd_index;

} lsp_session_data_t;

typedef struct _lsp_handle_context_t {
	lsp_session_data_t session;
	lsp_uint32_t session_buffer_size;
	lsp_uint8_t* response_pdu_buffer;
	lsp_uint8_t* request_pdu_buffer;
} lsp_handle_context_t;

#endif /* _LSP_TYPE_INTERNAL_H_ */
