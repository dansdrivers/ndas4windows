#ifndef _LSP_TYPE_INTERNAL_H_
#define _LSP_TYPE_INTERNAL_H_

#include <lsp.h>

/* lsp_session_data */
/*  parameter types */
typedef enum _lsp_param_type_t {
	LSP_PARM_TYPE_TEXT   = 0x0,
	LSP_PARM_TYPE_BINARY = 0x1
} lsp_param_type_t;

typedef enum _lsp_session_phase_t {
	LSP_SP_LOGIN_INIT,
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
} lsp_session_phase_state_t;

typedef enum _lsp_session_flags_t {
	LSP_SFG_PROTOCOL_VERSION_IS_SET = 0x00000001,
	LSP_SFG_NO_RESPONSE				= 0x00000002
} lsp_session_flags_t;

struct _lsp_handle_context_t;

typedef lsp_status_t (lsp_call *lsp_prepare_proc_t)(struct _lsp_handle_context_t* context);
typedef lsp_status_t (lsp_call *lsp_process_proc_t)(struct _lsp_handle_context_t* context);

typedef struct _lsp_session_data_t {

	lsp_uint32 session_options;

	lsp_session_phase_t phase;
	lsp_session_phase_state_t phase_state;

	lsp_uint32 hpid;
	lsp_uint16 rpid;
	lsp_uint16 cpslot;
	lsp_uint32 path_cmd_tag;

	lsp_uint32 chap_i;
	lsp_uint32 chap_c;
	lsp_uint8  session_phase;
	lsp_uint8  login_type;
	lsp_uint64 password;

	lsp_uint8  hw_type;
	lsp_uint8  hw_ver;
	lsp_uint8  hw_prot_type;
	lsp_uint8  hw_prot_ver; /* LANSCSI/IDE Protocol versions */
	lsp_uint16 hw_rev;      /* hardware revision, new in 2.0g */

	lsp_uint32 slot_cnt;
	lsp_uint32 max_transfer_blocks;
	lsp_uint32 max_targets;
	lsp_uint32 max_lun;
	lsp_uint8  hdr_enc_alg;
	lsp_uint8  data_enc_alg;
	lsp_uint8  hdr_dig_alg; /* new in 2.0g */
	lsp_uint8  dat_dig_alg; /* new in 2.0g */

	lsp_uint32 encrypt_ckey; /* combined encrypt key */
	lsp_uint32 decrypt_ckey; /* combined decrypt key */

	lsp_pdu_pointers_t pdu_ptrs;

	/* intermediate ones */
	lsp_uint32 flags;

	lsp_uint32 send_buffer_length;
	lsp_uint32 send_data_buffer_length;
	lsp_uint32 receive_data_buffer_length;
	lsp_uint32 receive_buffer_length;

	lsp_uint8* send_buffer;
	lsp_uint8* send_data_buffer;
	lsp_uint8* receive_data_buffer;
	lsp_uint8* receive_buffer;

	lsp_uint8 last_op_code;

	lsp_prepare_proc_t prepare_proc;
	lsp_process_proc_t process_proc;


	lsp_ide_register_param_t last_idereg;

	lsp_request_packet_t* original_request;
	lsp_request_packet_t* current_request;

	lsp_request_packet_t internal_packets[4];

} lsp_session_data_t;

typedef struct _lsp_handle_context_t {
	lsp_session_data_t session;
	void* user_context;
#if 0
	lsp_chained_proc_t chained_proc;
	void* chained_proc_context[4];
#endif
	lsp_uint8* response_pdu_buffer;
	lsp_uint8* request_pdu_buffer;
} lsp_handle_context_t;

#endif /* _LSP_TYPE_INTERNAL_H_ */
