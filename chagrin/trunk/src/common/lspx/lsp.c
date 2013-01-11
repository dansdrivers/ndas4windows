#include "lsp_impl.h"
#include "lsp_type_internal.h"
#include "lsp_hash.h"
#include "lsp_binparm.h"
#include "lsp_debug.h"
#include <lsp.h>

#define LSPIMP_THREE_CONCURRENT_TRANSFER

/* 0 : Conventional Key + Password 
 * 1 : Combined Encryption Key
 * 2 : Combined Encryption Key + Integral Type (uint32)
 */
#define LSPIMP_ENC_OPTIMIZE_TYPE 2

#ifndef countof
#define countof(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define LSPINLINE __forceinline

#ifdef LSP_USE_INLINE

void 
LSPINLINE 
lsp_call
_lsp_clear_request_buffer(lsp_handle_context_t* context)
{
	lsp_memset(context->request_pdu_buffer, 0, LSP_SESSION_REQUEST_BUFFER_SIZE);
}
void 
LSPINLINE
lsp_call
_lsp_clear_response_buffer(lsp_handle_context_t* context)
{
	lsp_memset(context->response_pdu_buffer, 0, LSP_SESSION_RESPONSE_BUFFER_SIZE);
}

lsp_pdu_hdr_t* 
LSPINLINE 
lsp_call
_lsp_get_request_buffer(lsp_handle_context_t* context)
{
	return (lsp_pdu_hdr_t*) context->request_pdu_buffer;
}

lsp_pdu_hdr_t* 
LSPINLINE 
lsp_call
_lsp_get_response_buffer(lsp_handle_context_t* context)
{
	return (lsp_pdu_hdr_t*) context->response_pdu_buffer;
}

#else

#define _lsp_clear_request_buffer(context) \
	lsp_memset(context->request_pdu_buffer, 0, LSP_SESSION_REQUEST_BUFFER_SIZE)

#define _lsp_clear_response_buffer(context) \
	lsp_memset(context->response_pdu_buffer, 0, LSP_SESSION_RESPONSE_BUFFER_SIZE)

#define _lsp_get_request_buffer(context) \
	((lsp_pdu_hdr_t*)(context->request_pdu_buffer))

#define _lsp_get_response_buffer(context) \
	((lsp_pdu_hdr_t*)(context->response_pdu_buffer))

#endif

/* error macros */
#define ERROR_T_COMPOSITE(FUNC, PHASE, TYPE, RESPONSE)			\
	((FUNC) << 16 | (PHASE) << 12 | (TYPE) << 8 | (RESPONSE))

#define lsp_debug __noop

enum { LSP_DISK_SECTOR_SIZE = 512 };

typedef enum _lsp_rpe_t {
	LSP_RPE_INVALID_CONTEXT = 1,
	LSP_RPE_HDR_RECV_FAIL   = 2,
	LSP_RPE_HDR_INVALID_LEN = 3,
	LSP_RPE_AHS_RECV_FAIL   = 4,
	LSP_RPE_AHS_INVALID_LEN = 5,
	LSP_RPE_DSG_RECV_FAIL   = 6,
	LSP_RPE_DSG_INVALID_LEN = 7,
	LSP_RPE_DAT_RECV_FAIL   = 8,
	LSP_RPE_DAT_INVALID_LEN = 9
} lsp_rpe_t;

typedef enum _lsp_spe_t {
	LSP_SPE_INVALID_CONTEXT      = 1,
	LSP_SPE_INVALID_AHS_LEN      = 2,
	LSP_SPE_INVALID_DSG_LEN      = 3,
	LSP_SPE_PDU_SEND_FAIL        = 4,
	LSP_SPE_DAT_SEND_FAIL        = 5,
	LSP_SPE_PDU_SEND_WAIT_FAIL   = 6,
	LSP_SPE_PDU_SEND_INVALID_LEN = 7,
	LSP_SPE_DAT_SEND_WAIT_FAIL   = 8,
	LSP_SPE_DAT_SEND_INVALID_LEN = 9
} lsp_spe_t;

void
LSPINLINE
lsp_call
lsp_encrypt32_internal(
	lsp_uint8* buf,
	lsp_uint32 len, 
	lsp_session_data_t* session)
{
#if LSPIMP_ENC_OPTIMIZE_TYPE == 2
	lsp_encrypt32exx(
		buf,
		len,
		session->encrypt_ckey);
#elif LSPIMP_ENC_OPTIMIZE_TYPE == 1
	lsp_encrypt32ex(
		buf,
		len,
		(lsp_uint8 *)&session->encrypt_ckey);
#else
	lsp_encrypt32(
		buf,
		len,
		(lsp_uint8 *)&session->chap_c,
		(lsp_uint8 *)&session->password);
#endif
}

void
LSPINLINE
lsp_call
lsp_decrypt32_internal(
	lsp_uint8* buf, 
	lsp_uint32 len, 
	lsp_session_data_t* session)
{
#if LSPIMP_ENC_OPTIMIZE_TYPE == 2
	lsp_decrypt32exx(
		buf,
		len,
		session->decrypt_ckey);
#elif LSPIMP_ENC_OPTIMIZE_TYPE == 1
	lsp_decrypt32ex(
		buf,
		len,
		(lsp_uint8 *)&session->decrypt_ckey);
#else
	lsp_decrypt32(
		buf,
		len,
		(lsp_uint8 *)&session->chap_c,
		(lsp_uint8 *)&session->password);
#endif
}

void
LSPINLINE
lsp_call
lsp_encrypt32_copy_internal(
	__out_ecount(len) lsp_uint8* dst,
	__in_ecount(len) const lsp_uint8* src, 
	__in lsp_uint32 len,
	__in lsp_session_data_t* session)
{
	if (session->data_enc_alg)
	{
#if LSPIMP_ENC_OPTIMIZE_TYPE == 2
		lsp_encrypt32exx_copy(dst, src, len, session->encrypt_ckey);
#elif LSPIMP_ENC_OPTIMIZE_TYPE == 1
		lsp_encrypt32ex_copy(dst, src, len, (lsp_uint8*)&session->encrypt_ckey);
#else
		lsp_encrypt32ex_copy(dst, src, len, (lsp_uint8*)&session->encrypt_ckey);
#endif
	}
	else
	{
		lsp_memcpy(dst, src, len);
	}
}

/*
 * Directive to enforce lsp_decrypt32_internal and lsp_encrypt32_internal 
 * in the implementation code 
 */
#ifdef _MSC_VER
#pragma deprecated(lsp_encrypt32exx)
#pragma deprecated(lsp_encrypt32ex)
#pragma deprecated(lsp_encrypt32)
#pragma deprecated(lsp_encrypt32ex_copy)
#pragma deprecated(lsp_encrypt32exx_copy)
#pragma deprecated(lsp_decrypt32exx)
#pragma deprecated(lsp_decrypt32ex)
#pragma deprecated(lsp_decrypt32)
#endif

static
void
lsp_call
lsp_encode_data(
	lsp_handle_context_t* context,
	lsp_uint8* buf,
	lsp_uint32 buflen);

static
void
lsp_call
lsp_decode_data(
	lsp_handle_context_t* context,
	lsp_uint8* buf,
	lsp_uint32 buflen);

static
void
lsp_call
lsp_decode_pdu_basic_hdr(
	lsp_handle_context_t* context, 
	lsp_pdu_hdr_t* pdu_hdr);

static
void
lsp_call
lsp_decode_pdu_addendum_hdr(
	lsp_handle_context_t* context,
	lsp_pdu_hdr_t* pdu_hdr);

static
void
lsp_call
lsp_encode_pdu_hdr(
	lsp_handle_context_t* context,
	lsp_pdu_hdr_t* pdu_hdr);

static
void
lsp_call
lsp_encode_data(
	lsp_handle_context_t* context,
	lsp_uint8* buf,
	lsp_uint32 buflen)
{
	lsp_session_data_t* session = &context->session;
	if (session->data_enc_alg)
	{
		lsp_encrypt32_internal(buf, buflen, session);
	}
}

static
void
lsp_call
lsp_decode_data(
	lsp_handle_context_t* context,
	lsp_uint8* buf,
	lsp_uint32 buflen)
{
	if (buflen > 0)
	{
		lsp_session_data_t* session = &context->session;
		/* decrypt data */
		if (session->data_enc_alg)
		{
			lsp_decrypt32_internal(buf, buflen, session);
		}
	}
}

static
void
lsp_call
lsp_decode_pdu_basic_hdr(
	lsp_handle_context_t* context, 
	lsp_pdu_hdr_t* pdu_hdr)
{
	lsp_session_data_t* session = &context->session;
	if (LSP_PHASE_FULL_FEATURE == session->session_phase &&
		session->hdr_enc_alg)
	{
		lsp_decrypt32_internal((lsp_uint8*) pdu_hdr, sizeof(lsp_pdu_hdr_t), session);
	}
	session->pdu_ptrs.header_ptr = pdu_hdr;
}

static
void
lsp_call
lsp_decode_pdu_addendum_hdr(
	lsp_handle_context_t* context,
	lsp_pdu_hdr_t* pdu_hdr)
{
	lsp_session_data_t* session = &context->session;
	lsp_uint8* buf = ((lsp_uint8*)pdu_hdr) + sizeof(lsp_pdu_hdr_t);
	lsp_uint16 ahs_len;
	lsp_uint32 dataseg_len;

	ahs_len = lsp_ntohs(pdu_hdr->ahs_len);
	if (ahs_len > 0)
	{
		if (LSP_IDE_PROTOCOL_VERSION_1_0 != session->hw_prot_ver &&
			LSP_PHASE_FULL_FEATURE == session->session_phase &&
			session->hdr_enc_alg)
		{
			lsp_decrypt32_internal(buf, ahs_len, session);
		}
		session->pdu_ptrs.ahs_ptr = buf;
		buf += ahs_len;
	}

	/* header digest */
	/* none */

	/* data segment */

	dataseg_len = lsp_ntohl(pdu_hdr->dataseg_len);
	if (dataseg_len > 0)
	{
		if (LSP_PHASE_FULL_FEATURE == session->session_phase &&
			session->hdr_enc_alg)
		{
			lsp_decrypt32_internal(buf, dataseg_len, session);
		}
		session->pdu_ptrs.data_seg_ptr = buf;
		buf += dataseg_len;
	}

	/* data digest */
	/* none */
}

static
void
lsp_call
lsp_encode_pdu_hdr(
	lsp_handle_context_t* context,
	lsp_pdu_hdr_t* pdu_hdr)
{
	lsp_session_data_t* session = &context->session;

	ASSERT(context != NULL);

	/* encryption */
	if (LSP_PHASE_FULL_FEATURE == session->session_phase)
	{
		lsp_uint8* buf = (lsp_uint8*) pdu_hdr;
		lsp_uint16 ahs_len = lsp_ntohs(pdu_hdr->ahs_len);
		lsp_uint32 dataseg_len = lsp_ntohl(pdu_hdr->dataseg_len);

		/* 1.0: ahs_len must be 0 */
		ASSERT(
			(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver && (0 == ahs_len)) ||
			(LSP_IDE_PROTOCOL_VERSION_1_0 != session->hw_prot_ver));

		/* 1.1: dsg_len must be 0 */
		ASSERT(
			(LSP_IDE_PROTOCOL_VERSION_1_1 == session->hw_prot_ver && (0 == dataseg_len)) ||
			(LSP_IDE_PROTOCOL_VERSION_1_1 != session->hw_prot_ver));


		/* encrypt header */
		if (session->hdr_enc_alg)
		{
			lsp_encrypt32_internal(buf, sizeof(lsp_pdu_hdr_t), session);
			if (LSP_IDE_PROTOCOL_VERSION_1_0 != session->hw_prot_ver && ahs_len > 0)
			{
				buf = ((lsp_uint8*)pdu_hdr) + sizeof(lsp_pdu_hdr_t);
				lsp_encrypt32_internal(buf, ahs_len, session);
			}
		}

		/* encrypt data segment */
		if (session->data_enc_alg && dataseg_len > 0)
		{
			buf = ((lsp_uint8*)pdu_hdr) + sizeof(lsp_pdu_hdr_t) + ahs_len;
			lsp_encrypt32_internal(buf, dataseg_len, session);
		}
	}
}

/*
 * LSP_SESSION_REQUEST_BUFFER_SIZE = 128 bytes
 *
 * sizeof(lsp_pdu_hdr_t) = 60 bytes +
 * maximum ahs length    = 64 bytes +
 *   For now, the maximum length is used at ide_command ext_command length is 12 bytes.
 *   We allows 64 bytes for other purposes at the request command.
 * alignment             =  4 bytes
 */
#define LSP_SESSION_REQUEST_BUFFER_SIZE 128

/*
 * LSP_SESSION_RESPONSE_BUFFER_SIZE = 576 bytes
 *
 * sizeof(lsp_pdu_hdr_t) =  60 bytes +
 * maximum ahs length    = 512 bytes +
 *   We assume that text_command for host list is maximum length
 *   (8 byte per host * 64 hosts)
 * alignment             =   4 bytes
 */
#define LSP_SESSION_RESPONSE_BUFFER_SIZE 576

/* C_ASSERT(sizeof(lsp_handle_context_t) <= 320); */
C_ASSERT(sizeof(lsp_handle_context_t) <= 1024);

const lsp_uint32 LSP_SESSION_BUFFER_SIZE = 
	sizeof(lsp_handle_context_t) + 
	LSP_SESSION_REQUEST_BUFFER_SIZE + 
	LSP_SESSION_RESPONSE_BUFFER_SIZE;

lsp_uint32
lsp_call
lsp_get_session_buffer_size()
{
	return LSP_SESSION_BUFFER_SIZE;
}

lsp_handle_t
lsp_call
lsp_create_session(
	void* session_buffer,
	void* user_context)
{
	lsp_handle_context_t* context;

	context = (lsp_handle_context_t*) session_buffer;
	lsp_memset(context, 0, lsp_get_session_buffer_size());
	context->request_pdu_buffer = (unsigned char*) session_buffer + sizeof(lsp_handle_context_t);
	context->response_pdu_buffer = (unsigned char*) context->request_pdu_buffer + sizeof(lsp_handle_context_t);

	context->user_context = user_context;
	
	return (lsp_handle_t) context;
}

void
lsp_call
lsp_destroy_session(
	lsp_handle_t h)
{
	h;
}

lsp_status_t 
lsp_call
lsp_get_user_context(
	lsp_handle_t h, 
	void **proc_context)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;

	if (!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	*proc_context = context->user_context;

	return LSP_STATUS_SUCCESS;
}

lsp_status_t 
lsp_call
lsp_set_proc_context(
	lsp_handle_t h, 
	void *proc_context)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;

	if (!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	context->user_context = proc_context;

	return LSP_STATUS_SUCCESS;
}

static
__forceinline
void*
lsp_call
_lsp_get_ahs_or_dataseg(lsp_session_data_t* session)
{
	return 
		LSP_IDE_PROTOCOL_VERSION_1_0 == (session->hw_prot_ver) ? 
			session->pdu_ptrs.data_seg_ptr :
		LSP_IDE_PROTOCOL_VERSION_1_1 == (session->hw_prot_ver) ? 
			session->pdu_ptrs.ahs_ptr : 0;
}

static
__forceinline
lsp_status_t
lsp_call
_lsp_valid_data_segment(lsp_handle_context_t* context, lsp_uint32 len)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*)context->response_pdu_buffer;
	if (LSP_IDE_PROTOCOL_VERSION_1_0 == context->session.hw_prot_ver)
	{
		if (lsp_ntohl(pdu_hdr->dataseg_len) < len || !(session->pdu_ptrs.data_seg_ptr))
		{
			// return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, (SUB_SEQ_NUM), LSP_ERR_TYPE_DATA_LEN, 0);
			return LSP_ERR_HEADER_INVALID_DATA_SEGMENT;
		}
	}
	else if (LSP_IDE_PROTOCOL_VERSION_1_1 == context->session.hw_prot_ver)
	{
		if (lsp_ntohs(pdu_hdr->ahs_len) < len || !(session->pdu_ptrs.ahs_ptr))
		{
			// return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, (SUB_SEQ_NUM), LSP_ERR_TYPE_DATA_LEN, 0);
			return LSP_ERR_HEADER_INVALID_DATA_SEGMENT;
		}
	}
	else
	{
		ASSERT(FALSE);
	}
	return LSP_STATUS_SUCCESS;
}

static
__forceinline 
void 
lsp_call
_lsp_set_next_phase(
	lsp_handle_context_t* context, 
	lsp_session_phase_t phase)
{
	context->session.phase = phase;
	context->session.phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
}

static
__forceinline
void
lsp_call
_lsp_set_next_phase_state(
	lsp_handle_context_t* context, 
	lsp_session_phase_state_t state)
{
	context->session.phase_state = state;
}

static
__forceinline
void
lsp_call
_lsp_set_ahs_or_ds_len(
	lsp_handle_context_t* context, 
	lsp_uint16 len)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*) context->request_pdu_buffer;

	if (LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver)
	{
		pdu_hdr->dataseg_len = lsp_htonl(len);
		//session->ahs_len = 0;
		//session->dataseg_len = len;
		return;
	}
	else if (LSP_IDE_PROTOCOL_VERSION_1_1 == session->hw_prot_ver)
	{
		pdu_hdr->ahs_len = lsp_htons((lsp_uint8)len);
		//session->ahs_len = len;
		//session->dataseg_len = 0;
		return;
	}
	ASSERT(FALSE);
}

static
__forceinline
lsp_uint32
lsp_call
_lsp_get_ahs_or_ds_len(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*) context->response_pdu_buffer;
	if (LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver)
	{
		return lsp_ntohl((lsp_uint32)(pdu_hdr->dataseg_len));
	}
	else if (LSP_IDE_PROTOCOL_VERSION_1_1 == session->hw_prot_ver)
	{
		return lsp_ntohl((lsp_uint32)(pdu_hdr->ahs_len));
	}
	ASSERT(FALSE);
	return 0;
}

static
lsp_status_t
lsp_call
lsp_login_phase_1_prepare(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 0 };
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	session->hpid = 0;
	session->rpid = 0; // need not ( not sure )
	session->path_cmd_tag = 0;

	// suppose hw_prot_ver to max
	// hw_prot_ver will be set correctly after this login phase
	session->hw_prot_ver = LSP_IDE_PROTOCOL_VERSION_MAX;

	// initialize context
	session->session_phase = LSP_PHASE_SECURITY;
	session->hdr_enc_alg = 0;
	session->data_enc_alg = 0;
	session->hdr_dig_alg = 0;
	session->dat_dig_alg = 0;

	// 1st login phase
	_lsp_clear_request_buffer(context);
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->hpid);

	// SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST);
	_lsp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;
	pdu_hdr->op_data.login.ver_max = session->hw_ver; // phase specific
	pdu_hdr->op_data.login.ver_min = 0; // phase specific

	param_secu = (lsp_binparm_security_t*)(context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	// SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu)

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_login_phase_1_process(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 0 };

	lsp_status_t status;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	pdu_hdr = _lsp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.login.CSG ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);
	}

	// Set hw_ver, hw_prot_ver with detected one.
	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		/* phase specific */
		if (LSP_ERR_RESPONSE_RI_VERSION_MISMATCH == pdu_hdr->response)
		{
			if (session->hw_ver != pdu_hdr->op_data.login_response.ver_active)
			{
				session->hw_ver = pdu_hdr->op_data.login_response.ver_active;
				session->flags |= LSP_SFG_PROTOCOL_VERSION_IS_SET;
				_lsp_set_next_phase(context, LSP_SP_LOGIN_INIT);
				return LSP_REQUIRE_MORE_PROCESSING;
			}
		}

		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	session->flags |= LSP_SFG_PROTOCOL_VERSION_IS_SET;

	/* phase specific */
	session->hw_ver = pdu_hdr->op_data.login_response.ver_active;
	session->hw_prot_ver = (session->hw_ver == LSP_HARDWARE_VERSION_1_0) ? 
		LSP_IDE_PROTOCOL_VERSION_1_0 : LSP_IDE_PROTOCOL_VERSION_1_1;
	/* new in 2.0g */
	session->hw_rev = lsp_ntohs(pdu_hdr->op_data.login_response.revision);

	// CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY);
	status = _lsp_valid_data_segment(context, LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY);
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*) _lsp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	// store data
	session->rpid = lsp_ntohs(pdu_hdr->rpid);

	_lsp_set_next_phase(context, LSP_SP_LOGIN_AUTH_1);
	return LSP_REQUIRE_MORE_PROCESSING;
}

static
lsp_status_t
lsp_call
lsp_login_phase_2_prepare(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 1 };

	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;
	lsp_authparm_chap_t*    param_chap;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	_lsp_clear_request_buffer(context);
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	// SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST);
	_lsp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.login.ver_max = session->hw_ver;
	pdu_hdr->op_data.login.ver_min = 0;
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_t*)(context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_t*) param_secu->auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);

	//pdu.header_ptr = pdu_hdr;
	//SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_login_phase_2_process(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 1 };

	lsp_status_t status;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;
	lsp_authparm_chap_t*    param_chap;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	pdu_hdr = _lsp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.login.CSG ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	// CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY);
	status = _lsp_valid_data_segment(context, LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY);
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*) _lsp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	/* store data */
	param_chap = &param_secu->auth_chap;
	session->chap_i = lsp_ntohl(param_chap->chap_i);
	session->chap_c = lsp_ntohl(param_chap->chap_c[0]);

	_lsp_set_next_phase(context, LSP_SP_LOGIN_AUTH_2);
	return LSP_REQUIRE_MORE_PROCESSING;
}

static
lsp_status_t
lsp_call
lsp_login_phase_3_prepare(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 2 };

	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;
	lsp_authparm_chap_t*    param_chap;
	lsp_uint32               user_id;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	_lsp_clear_request_buffer(context);
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.login.T = 1;
	pdu_hdr->op_flags.login.CSG = LSP_PHASE_SECURITY;
	pdu_hdr->op_flags.login.NSG = LSP_PHASE_LOGIN_OPERATION;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	// SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST);
	_lsp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.login.ver_max = session->hw_ver;
	pdu_hdr->op_data.login.ver_min = 0;
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_t*)(context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_t*)param_secu->auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);
	param_chap->chap_i = lsp_htonl(session->chap_i);

	user_id =
		(login_info->supervisor_password) ? 
			LSP_NDAS_SUPERVISOR :
		(0 == login_info->unit_no) ?
			((login_info->write_access) ? 
				 LSP_FIRST_TARGET_RW_USER : LSP_FIRST_TARGET_RO_USER) :
			((login_info->write_access) ? 
				 LSP_SECOND_TARGET_RW_USER : LSP_SECOND_TARGET_RO_USER);

	param_chap->chap_n =
		(LSP_LOGIN_TYPE_NORMAL == login_info->login_type) ? lsp_htonl(user_id) : 0;

	session->password = login_info->password;

	/* hash in... */

	lsp_hash32to128(
		(lsp_uint8 *)param_chap->chap_r,
		(lsp_uint8 *)&session->chap_c,
		(lsp_uint8 *)((LSP_NDAS_SUPERVISOR == user_id) ? 
			&login_info->supervisor_password : &session->password));

	//pdu.header_ptr = pdu_hdr;
	//SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);
	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_login_phase_3_process(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 2 };

	lsp_status_t status;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_security_t* param_secu;

	const lsp_login_info_t* login_info;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	pdu_hdr = _lsp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.login.CSG ||
		LSP_PHASE_LOGIN_OPERATION != pdu_hdr->op_flags.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);
	}

	/* LSP_ERR_RESPONSE_T_COMMAND_FAILED : may failed because login RW but already RW exists */
	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	// CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY);
	status = _lsp_valid_data_segment(context, LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY);
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*)_lsp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	session->session_phase = LSP_PHASE_LOGIN_OPERATION;

	_lsp_set_next_phase(context, LSP_SP_LOGIN_NEGO);
	return LSP_REQUIRE_MORE_PROCESSING;
}

static
lsp_status_t
lsp_call
lsp_login_phase_4_prepare(
	lsp_handle_context_t* context)
{
	static const lsp_uint16 sub_seq_num = 3;

	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_negotiation_t* param_nego;

	/* const lsp_login_info_t* login_info = session->phase_data.login.login_info; */

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	_lsp_clear_request_buffer(context);
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.login.T = 1;
	pdu_hdr->op_flags.login.CSG = LSP_PHASE_LOGIN_OPERATION;
	pdu_hdr->op_flags.login.NSG = LSP_PHASE_FULL_FEATURE;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	// SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST);
	_lsp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST);
	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.login.ver_max = session->hw_ver;
	pdu_hdr->op_data.login.ver_min = 0;
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	/* phase specific */
	param_nego = (lsp_binparm_negotiation_t*) (context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));
	param_nego->parm_type = LSP_BINPARM_TYPE_NEGOTIATION;

	// SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_login_phase_4_process(
	lsp_handle_context_t* context)
{
	enum { sub_seq_num = 3 };

	lsp_status_t status;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_negotiation_t* param_nego;

	const lsp_login_info_t* login_info;;

	session = &context->session;
	login_info = &session->current_request->u.login.request.login_info;

	pdu_hdr = _lsp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.login.T ||
		LSP_PHASE_LOGIN_OPERATION != pdu_hdr->op_flags.login.CSG ||
		LSP_PHASE_FULL_FEATURE != pdu_hdr->op_flags.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	// CHECK_DATA_SEGMENT(pdu, pdu_hdr, session->hw_prot_ver, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY);
	status = _lsp_valid_data_segment(context, LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY);
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_nego = (lsp_binparm_negotiation_t*) 
		((LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? 
		session->pdu_ptrs.data_seg_ptr : 
		session->pdu_ptrs.ahs_ptr);

	if (LSP_BINPARM_TYPE_NEGOTIATION != param_nego->parm_type)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	session->hw_type = param_nego->hw_type;
	session->hw_prot_type = param_nego->hw_type;
	if (session->hw_ver != param_nego->hw_ver)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	session->slot_cnt = lsp_ntohl(param_nego->slot_cnt);
	session->max_transfer_blocks = lsp_ntohl(param_nego->max_blocks);
	session->max_targets = lsp_ntohl(param_nego->max_target_id);
	session->max_lun = lsp_ntohl(param_nego->max_lun);
#if 0
	session->hdr_enc_alg = lsp_ntohs(param_nego->hdr_enc_alg);
	session->data_enc_alg = lsp_ntohs(param_nego->dat_enc_alg);
#else
	session->hdr_enc_alg = param_nego->hdr_enc_alg;
	session->data_enc_alg = param_nego->dat_enc_alg;
#endif
	/* header digest and data digest is not actually introduced in 2.0g yet */
	/* we are just reserving the location for 2.5 or later */
	session->hdr_dig_alg = 0;
	session->dat_dig_alg = 0;

	session->session_phase = LSP_PHASE_FULL_FEATURE;
	session->login_type = login_info->login_type;

	/* THIS BUG HAS TO BE DEALT WITH VERIFIED-WRITE POLICY, but NOT HERE */
#if 0
	/* TODO: use another way to work-around v2.0 bug. */
	/* V2.0 bug : A data larger than 52k can be broken rarely. */
	if (2 == session->hw_ver && 
		0 == session->hw_rev &&
		session->max_transfer_blocks > 104)
	{
		session->max_transfer_blocks = 104; /* set to 52k max */
	}
#endif

	/* set key128 from the key and the password */

	lsp_encrypt32_build_combined_key(
		&session->encrypt_ckey,
		(lsp_uint8*) &session->chap_c,
		(lsp_uint8*) &session->password);

	lsp_decrypt32_build_combined_key(
		&session->decrypt_ckey,
		(lsp_uint8*) &session->chap_c,
		(lsp_uint8*) &session->password);


	_lsp_set_next_phase(context, LSP_SP_FULL_PHASE);

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_login_prepare(
	lsp_handle_context_t* context)
{
	static const lsp_prepare_proc_t prepare_procs[] = {
		lsp_login_phase_1_prepare,
		lsp_login_phase_2_prepare,
		lsp_login_phase_3_prepare,
		lsp_login_phase_4_prepare
	};

	lsp_session_data_t* session = &context->session;
	ASSERT(session->phase < countof(prepare_procs));
	return (*prepare_procs[session->phase])(context);
}

static
lsp_status_t
lsp_call
lsp_login_process(
	lsp_handle_context_t* context)
{
	static const lsp_process_proc_t process_procs[] = {
		lsp_login_phase_1_process,
		lsp_login_phase_2_process,
		lsp_login_phase_3_process,
		lsp_login_phase_4_process
	};

	lsp_session_data_t* session = &context->session;
	ASSERT(session->phase < countof(process_procs));
	return (*process_procs[session->phase])(context);
}

static
lsp_status_t
lsp_call
lsp_logout_prepare(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;

	//if (LSP_PHASE_FULL_FEATURE != session->session_phase)
	//{
	//	session->session_phase = LSP_PHASE_LOGOUT;
	//	return LSP_STATUS_SUCCESS;
	//}

	_lsp_clear_request_buffer(context);
	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	/* init pdu_hdr */
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGOUT_REQUEST;
	pdu_hdr->op_flags.logout.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_logout_process(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;

	pdu_hdr = _lsp_get_response_buffer(context);
	if (LSP_OPCODE_LOGOUT_RESPONSE != pdu_hdr->op_code)
	{
		session->session_phase = LSP_PHASE_LOGOUT;
		return LSP_ERR_REPLY_FAIL;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		session->session_phase = LSP_PHASE_LOGOUT;
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGOUT, 2, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	/* logout phase is complete */
	session->session_phase = LSP_PHASE_SECURITY; /* LSP_PHASE_LOGOUT; */
	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_ide_command_prepare_v0(lsp_handle_context_t* context)
{
	lsp_ide_data_v0_t *ide_data_v0_ptr;
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t *pdu_hdr;

	lsp_ide_command_request_t* request = &session->current_request->u.ide_command.request;

	lsp_uint32 target_id = request->target_id;
	lsp_uint32 lun0 = request->lun0;
	lsp_uint32 lun1 = request->lun1;
	const lsp_ide_register_param_t* p = &request->reg;
	const lsp_io_data_buffer_t* data_buf = &request->data_buf;

	ASSERT(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver);

	ASSERT(NULL != p);
	ASSERT(NULL != data_buf);

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_IDE_COMMAND;

	_lsp_clear_request_buffer(context);

	/* initialize pdu header */
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->target_id = lsp_htonl(target_id);
	pdu_hdr->lun0 = lsp_htonl(lun0);
	pdu_hdr->lun1 = lsp_htonl(lun1);

	ide_data_v0_ptr = &(pdu_hdr->op_data.ide_command_v0);
	lsp_memset(ide_data_v0_ptr, 0x00, sizeof(lsp_ide_data_v0_t));

	ide_data_v0_ptr->dev =	(0 == target_id) ? 0 : 1;

	/* set pdu flags */
	pdu_hdr->op_flags.ide_command.R = 0;
	pdu_hdr->op_flags.ide_command.W = 0;

	if (data_buf->recv_size > 0)
	{
		ASSERT(data_buf->recv_buffer);
		pdu_hdr->op_flags.ide_command.R = 1;
	}
	if (data_buf->send_size > 0)
	{
		ASSERT(data_buf->send_buffer);
		pdu_hdr->op_flags.ide_command.W = 1;
	}

	/* p->use_dma is ignored, V1.0 supports PIO only */
	/* set device */
	ide_data_v0_ptr->device = p->device.device;

	/* translate command */
	switch (p->command.command)
	{
	case WIN_READ:
		//	case WIN_READDMA:
		//	case WIN_READDMA_EXT:
		ide_data_v0_ptr->command = (p->use_48) ? WIN_READDMA_EXT : WIN_READDMA;
		break;
	case WIN_WRITE:
		//	case WIN_WRITEDMA:
		//	case WIN_WRITEDMA_EXT:
		ide_data_v0_ptr->command = (p->use_48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA;
		break;
	case WIN_VERIFY:
		//	case WIN_VERIFY_EXT:
		ide_data_v0_ptr->command = (p->use_48) ? WIN_VERIFY_EXT : WIN_VERIFY;
		break;
	case WIN_IDENTIFY:
	case WIN_SETFEATURES:
		ide_data_v0_ptr->command = p->command.command;
		break;
	default:
		// V1.0 does not support all the ide commands
		return LSP_ERR_NOT_SUPPORTED;
	}

	/* set location, sector count, feature */
	if (p->use_48)
	{
		ide_data_v0_ptr->feature = p->reg.named_48.prev.features;
		ide_data_v0_ptr->sector_count_prev = p->reg.named_48.prev.sector_count;
		ide_data_v0_ptr->sector_count_cur = p->reg.named_48.cur.sector_count;
		ide_data_v0_ptr->lba_low_prev = p->reg.named_48.prev.lba_low;
		ide_data_v0_ptr->lba_low_cur = p->reg.named_48.cur.lba_low;
		ide_data_v0_ptr->lba_mid_prev = p->reg.named_48.prev.lba_mid;
		ide_data_v0_ptr->lba_mid_cur = p->reg.named_48.cur.lba_mid;
		ide_data_v0_ptr->lba_high_prev = p->reg.named_48.prev.lba_high;
		ide_data_v0_ptr->lba_high_cur = p->reg.named_48.cur.lba_high;
	}
	else
	{
		ide_data_v0_ptr->feature = p->reg.named.features;
		ide_data_v0_ptr->sector_count_cur = p->reg.named.sector_count;
		ide_data_v0_ptr->lba_low_cur = p->reg.named.lba_low;
		ide_data_v0_ptr->lba_mid_cur = p->reg.named.lba_mid;
		ide_data_v0_ptr->lba_high_cur = p->reg.named.lba_high;
	}

	session->send_data_buffer = data_buf->send_size ? data_buf->send_buffer : 0;
	session->send_data_buffer_length = data_buf->send_size ? data_buf->send_size : 0;

	session->receive_data_buffer = data_buf->recv_size ? data_buf->recv_buffer : 0;
	session->receive_data_buffer_length = data_buf->recv_size ? data_buf->recv_size : 0;

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_ide_command_process_v0(lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	const lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;

	if (LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code)
	{
		return LSP_ERR_REPLY_FAIL;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t
lsp_call
lsp_ide_command_prepare_v1(lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t *pdu_hdr;

	lsp_ide_header_t* ide_header;
	lsp_ide_register_t* ide_register;
	lsp_uint32 data_trans_len;

	lsp_request_packet_t* request = session->current_request;
	lsp_ide_command_request_t* ide_request = &request->u.ide_command.request;

	lsp_uint32 target_id = ide_request->target_id;
	lsp_uint32 lun0 = ide_request->lun0;
	lsp_uint32 lun1 = ide_request->lun1;

	const lsp_ide_register_param_t* p = &ide_request->reg;
	const lsp_io_data_buffer_t* data_buf = &ide_request->data_buf;
	const lsp_extended_command_t* ext_cmd = &ide_request->ext_cmd;

	ASSERT(LSP_IDE_PROTOCOL_VERSION_1_1 == session->hw_prot_ver);

	ASSERT(NULL != p);
	ASSERT(NULL != data_buf);
	ASSERT(NULL != ext_cmd);

	ASSERT((ext_cmd->cmd_size && ext_cmd->cmd_buffer) || 
		(!ext_cmd->cmd_size && !ext_cmd->cmd_buffer));

	ASSERT(!(0 != data_buf->recv_size && 0 != data_buf->send_size));

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	_lsp_clear_request_buffer(context);

	session->last_op_code = LSP_OPCODE_IDE_COMMAND;

	/* initialize pdu header */
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->op_flags.ide_command.R = (data_buf->recv_size > 0) ? 1 : 0;
	pdu_hdr->op_flags.ide_command.W = (data_buf->send_size > 0) ? 1 : 0;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = (ext_cmd) ? lsp_htons((lsp_uint16)ext_cmd->cmd_size) : 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	data_trans_len =
		(pdu_hdr->op_flags.ide_command.W) ? (data_buf->send_size) :
		(pdu_hdr->op_flags.ide_command.R) ? (data_buf->recv_size) : 0;
	pdu_hdr->data_trans_len = lsp_htonl(data_trans_len);
	pdu_hdr->target_id = lsp_htonl(target_id);
	pdu_hdr->lun0 = lsp_htonl(lun0);
	pdu_hdr->lun1 = lsp_htonl(lun1);

	/* additional payload if ahs_len is set */
	session->send_buffer_length += ext_cmd->cmd_size;

	/* set ide header */
	ide_header = &(pdu_hdr->op_data.ide_command.header);
	lsp_memset(ide_header, 0x00, sizeof(lsp_ide_header_t));
	ide_header->com_type_p = (WIN_PACKETCMD == p->command.command) ?  1 : 0;
	ide_header->com_type_k = 0;
	ide_header->com_type_d_p = (p->use_dma) ? 1 : 0;
	ide_header->com_type_w = pdu_hdr->op_flags.ide_command.W;
	ide_header->com_type_r = pdu_hdr->op_flags.ide_command.R;
	ide_header->com_type_e = (p->use_48) ? 1 : 0;
	ide_header->com_len = data_trans_len & 0x03FFFFFF; // 32 -> 26 bit
	*(lsp_uint32 *)ide_header = lsp_htonl(*(lsp_uint32 *)ide_header);

	/* set ide register */
	ide_register = &(pdu_hdr->op_data.ide_command.register_data);
	lsp_memset(ide_register, 0x00, sizeof(lsp_ide_register_t));
	ide_register->device = p->device.device;
	ide_register->command = p->command.command;
	if (p->use_48)
	{
		ide_register->feature_prev = p->reg.named_48.prev.features;
		ide_register->feature_cur = p->reg.named_48.cur.features;
		ide_register->sector_count_prev = p->reg.named_48.prev.sector_count;
		ide_register->sector_count_cur = p->reg.named_48.cur.sector_count;
		ide_register->lba_low_prev = p->reg.named_48.prev.lba_low;
		ide_register->lba_low_cur = p->reg.named_48.cur.lba_low;
		ide_register->lba_mid_prev = p->reg.named_48.prev.lba_mid;
		ide_register->lba_mid_cur = p->reg.named_48.cur.lba_mid;
		ide_register->lba_high_prev = p->reg.named_48.prev.lba_high;
		ide_register->lba_high_cur = p->reg.named_48.cur.lba_high;
	}
	else
	{
		/* set prev == cur to protect NDAS chip */
		ide_register->feature_prev = p->reg.named.features;
		ide_register->feature_cur = p->reg.named.features;
		ide_register->sector_count_prev = p->reg.named.sector_count;
		ide_register->sector_count_cur = p->reg.named.sector_count;
		ide_register->lba_low_prev = p->reg.named.lba_low;
		ide_register->lba_low_cur = p->reg.named.lba_low;
		ide_register->lba_mid_prev = p->reg.named.lba_mid;
		ide_register->lba_mid_cur = p->reg.named.lba_mid;
		ide_register->lba_high_prev = p->reg.named.lba_high;
		ide_register->lba_high_cur = p->reg.named.lba_high;
	}

	//lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	//pdu.header_ptr = pdu_hdr;
	if (ext_cmd->cmd_size > 0)
	{
		ASSERT(NULL != ext_cmd->cmd_buffer);
		// attach ext_cmd to pdu_hdr
		lsp_memcpy(
			context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t), 
			ext_cmd->cmd_buffer, 
			ext_cmd->cmd_size);
		// SET_AHS_OR_DATA_SEG(pdu, session->hw_prot_ver, context->pdu_buffer + sizeof(lsp_pdu_hdr));
	}

	/* data for packet command should be aligned to 4 bytes */
	if  (
		WIN_PACKETCMD == p->command.command &&
		(
		(data_buf->send_buffer && 0 != (data_buf->send_size % 4)) ||
		(data_buf->recv_buffer && 0 != (data_buf->recv_size % 4))
		)
		)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 2, LSP_ERR_TYPE_DATA_LEN, 0);
	}

	session->send_data_buffer = data_buf->send_buffer;
	session->send_data_buffer_length = data_buf->send_size;

	session->receive_data_buffer = data_buf->recv_buffer;
	session->receive_data_buffer_length = data_buf->recv_size;

	return LSP_STATUS_SUCCESS;
}


static
lsp_status_t
lsp_call
lsp_ide_command_process_v1(lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	const lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;
	const lsp_ide_register_t* ide_register = &(pdu_hdr->op_data.ide_command.register_data);
	lsp_ide_command_response_t* response = &session->current_request->u.ide_command.response;
	lsp_ide_register_param_t* p = &response->reg;

	if (LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code)
	{
		return LSP_ERR_REPLY_FAIL;
	}

	if (0 == pdu_hdr->op_flags.ide_command.F)
	{
		return LSP_ERR_COMMAND_FAILED;
	}

	// store results before testing pdu_hdr->response
	// for packet command(and a few other commands), return values have meaning.
	lsp_memset(p, 0, sizeof(lsp_ide_register_param_t));
	p->device.device = ide_register->device;
	p->command.command = ide_register->command; // status
	p->reg.ret.err.err_na = ide_register->feature_cur; // error

	//// You can't ensure whether ide_register has 48 bit or not. So copy all the bytes.
	p->reg.named_48.prev.features = ide_register->feature_prev;
	p->reg.named_48.cur.features = ide_register->feature_cur; // err
	p->reg.named_48.prev.sector_count = ide_register->sector_count_prev;
	p->reg.named_48.cur.sector_count = ide_register->sector_count_cur;
	p->reg.named_48.prev.lba_low = ide_register->lba_low_prev;
	p->reg.named_48.cur.lba_low = ide_register->lba_low_cur;
	p->reg.named_48.prev.lba_mid = ide_register->lba_mid_prev;
	p->reg.named_48.cur.lba_mid = ide_register->lba_mid_cur;
	p->reg.named_48.prev.lba_high = ide_register->lba_high_prev;
	p->reg.named_48.cur.lba_high = ide_register->lba_high_cur;

	/* save the last ide register in the session */
	session->last_idereg = *p;

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t 
lsp_call 
lsp_vendor_command_prepare(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t *pdu_hdr;

	lsp_request_packet_t* request = session->current_request;
	lsp_vendor_command_request_t* vc_request = &request->u.vendor_command.request;

	lsp_uint16 vendor_id = vc_request->vendor_id;
	lsp_uint8 vop_ver = vc_request->vop_ver;
	lsp_uint8 vop_code = vc_request->vop_code;
	lsp_uint8 param_length = vc_request->param_length;
	const lsp_uint8 *param = vc_request->param;
	const lsp_io_data_buffer_t* data_buf = &vc_request->data_buf;

	session->send_buffer = context->request_pdu_buffer;

	_lsp_clear_request_buffer(context);

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_VENDOR_SPECIFIC_COMMAND;

	/* initialize pdu header */
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_VENDOR_SPECIFIC_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.vendor_command.vendor_id = lsp_htons(vendor_id);
	pdu_hdr->op_data.vendor_command.vop_ver = vop_ver;
	pdu_hdr->op_data.vendor_command.vop_code = vop_code;

	lsp_htonx(pdu_hdr->op_data.vendor_command.vop_parm,	param, param_length);

	session->send_data_buffer = data_buf->send_buffer;
	session->send_data_buffer_length = data_buf->send_size;

	session->receive_data_buffer = data_buf->recv_buffer;
	session->receive_data_buffer_length = data_buf->recv_size;

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t 
lsp_call 
lsp_vendor_command_process(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	const lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;

	if (LSP_OPCODE_VENDOR_SPECIFIC_RESPONSE != pdu_hdr->op_code)
	{
		return LSP_ERR_REPLY_FAIL;
	}

	if (0 == pdu_hdr->op_flags.vendor_command.F)
	{
		return LSP_ERR_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_VENDOR_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t 
lsp_call 
lsp_text_command_prepare(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_uint8* param_text;

	lsp_request_packet_t* request = session->current_request;
	lsp_text_command_request_t* text_request = &request->u.text_command.request;

	lsp_uint8 param_type = text_request->param_type;
	lsp_uint8 param_ver = text_request->param_ver;
	const lsp_uint8 *data = text_request->data;
	lsp_uint16 data_in_length = text_request->data_in_length;

	_lsp_clear_request_buffer(context);

	session->send_buffer = context->request_pdu_buffer;

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_NOP_H2R;

	/* initialize pdu header */
	pdu_hdr = _lsp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_TEXT_REQUEST;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	_lsp_set_ahs_or_ds_len(context, data_in_length);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.text_command.parm_type = param_type;
	pdu_hdr->op_data.text_command.parm_ver = param_ver;

	param_text = context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t);

	lsp_memcpy(param_text, data, data_in_length);

	// lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	//	pdu.header_ptr = pdu_hdr;
	//	SET_AHS_OR_DATA_SEG(pdu, session->hw_prot_ver, param_text);

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t 
lsp_call 
lsp_text_command_process(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;

	lsp_request_packet_t* request = session->current_request;
	lsp_text_command_request_t* text_request = &request->u.text_command.request;

	lsp_uint8 param_type = text_request->param_type;
	lsp_uint8 param_ver = text_request->param_ver;

	if (LSP_OPCODE_TEXT_RESPONSE != pdu_hdr->op_code ||
		param_type != pdu_hdr->op_data.text_command.parm_type ||
		param_ver != pdu_hdr->op_data.text_command.parm_ver)
	{
		return LSP_ERR_REPLY_FAIL;
	}

	if (0 == pdu_hdr->op_flags.text_command.F)
	{
		return LSP_ERR_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_TEXT_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	return LSP_STATUS_SUCCESS;
}

static
lsp_status_t 
lsp_call 
lsp_noop_command_prepare(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t *pdu_hdr;

	session->send_buffer = context->request_pdu_buffer;
	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	_lsp_clear_request_buffer(context);

	session->last_op_code = LSP_OPCODE_NOP_H2R;

	pdu_hdr = _lsp_get_request_buffer(context);
	/* initialize pdu header */
	pdu_hdr->op_code = LSP_OPCODE_NOP_H2R;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);

	//lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	//pdu.header_ptr = pdu_hdr;

	/* 1.0, 1.1, 2.0 does not respond with noop command */
	session->flags |= LSP_SFG_NO_RESPONSE;

	return LSP_STATUS_SUCCESS;
}

lsp_status_t 
lsp_call 
lsp_noop_command_process(
	lsp_handle_context_t* context)
{
#ifdef __LSP_NOOP_RECEIVE_RESULT__
	lsp_session_data_t* session = &context->session;
	const lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;

	if (LSP_OPCODE_NOP_R2H != pdu_hdr->op_code)
	{
		return LSP_ERR_REPLY_FAIL;
	}

	if (0 == pdu_hdr->op_flags.vendor_command.F)
	{
		return LSP_ERR_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_NOOP, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}
#else
	context;
#endif // __LSP_NOOP_RECEIVE_RESULT__

	return LSP_STATUS_SUCCESS;
}

#if 0
lsp_chained_proc_t
lsp_set_chained_return(lsp_handle_t handle, lsp_chained_proc_t proc)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) handle;
	lsp_chained_proc_t oldproc = context->chained_proc;
	context->chained_proc = proc;
	return oldproc;
}
#endif

lsp_status_t
lsp_chained_return(lsp_handle_context_t* context, lsp_status_t oldstatus)
{
#if 0
	if (context->chained_proc)
	{
		lsp_status_t newstatus = (*context->chained_proc)((lsp_handle_t) context, oldstatus);
		return newstatus;
	}
#endif
#if 0
	lsp_request_packet_t* request = context->session.current_request;
	request->status = oldstatus;
	if (request->next_request)
	{
		return lsp_request(context, request->next_request);
	}
	else
	{
		return oldstatus;
	}
#endif
	context;
	return oldstatus;
}

lsp_status_t
lsp_call
lsp_process_next(lsp_handle_context_t* context)
{
	lsp_status_t status;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;

	session = &context->session;

reset_phase:

	switch (session->phase_state)
	{
	case LSP_SP_STATE_PREPARE_SEND_HEADER:

		/* clear buffer status */
		session->send_buffer_length = 0;
		session->send_data_buffer_length = 0;
		session->receive_data_buffer_length = 0;
		session->receive_buffer_length = 0;
		session->send_data_buffer = 0;
		session->receive_data_buffer = 0;

		session->send_buffer = 0;
		session->send_data_buffer = 0;
		session->receive_data_buffer = 0;
		session->receive_buffer = 0;

		pdu_hdr = _lsp_get_request_buffer(context);
		status = session->prepare_proc(context);
		if (LSP_STATUS_SUCCESS != status)
		{
			session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
			return lsp_chained_return(context, status);
		}

#ifdef LSPIMP_DEBUG
		lsp_debug_payload("REQ BHS", pdu_hdr, sizeof(lsp_pdu_hdr_t));
		if (pdu_hdr->ahs_len > 0)
		{
			lsp_debug_payload("REQ AHS", ((char*)pdu_hdr)+ sizeof(lsp_pdu_hdr_t), lsp_ntohs(pdu_hdr->ahs_len));
		}
		if (pdu_hdr->dataseg_len > 0)
		{
			lsp_debug_payload("REQ DS", ((char*)pdu_hdr)+ sizeof(lsp_pdu_hdr_t) + lsp_ntohs(pdu_hdr->ahs_len), lsp_ntohl(pdu_hdr->dataseg_len));
		}
#endif

		lsp_encode_pdu_hdr(context, pdu_hdr);

#ifdef LSPIMP_DEBUG_ENC
		lsp_debug_payload("REQ BHS ENC", pdu_hdr, sizeof(lsp_pdu_hdr_t));
		if (pdu_hdr->ahs_len > 0)
		{
			lsp_debug_payload("REQ AHS ENC", ((char*)pdu_hdr)+ sizeof(lsp_pdu_hdr_t), lsp_ntohs(pdu_hdr->ahs_len));
		}
		if (pdu_hdr->dataseg_len > 0)
		{
			lsp_debug_payload("REQ DS ENC", ((char*)pdu_hdr)+ sizeof(lsp_pdu_hdr_t) + lsp_ntohs(pdu_hdr->ahs_len), lsp_ntohl(pdu_hdr->dataseg_len));
		}
#endif

		session->phase_state = LSP_SP_STATE_BEGIN_SEND_HEADER;
		goto reset_phase;

	case LSP_SP_STATE_BEGIN_SEND_HEADER:

		session->phase_state = LSP_SP_STATE_ENCODE_SEND_DATA;
		return LSP_REQUIRES_SEND;

	case LSP_SP_STATE_ENCODE_SEND_DATA:

		session->phase_state = LSP_SP_STATE_BEGIN_SEND_DATA;
		if (session->send_data_buffer_length > 0)
		{
			if (session->session_options & LSP_SO_USE_EXTERNAL_DATA_ENCODE)
			{
				return LSP_REQUIRES_DATA_ENCODE;
			}
			else
			{
				lsp_encode_data(
					context, 
					session->send_data_buffer, 
					session->send_data_buffer_length);
			}
		}
		/* goto reset_phase; */

	case LSP_SP_STATE_BEGIN_SEND_DATA:

		session->phase_state = LSP_SP_STATE_END_SEND_HEADER;
		if (session->send_data_buffer_length > 0)
		{
			session->send_buffer = session->send_data_buffer;
			session->send_buffer_length = session->send_data_buffer_length;
			return LSP_REQUIRES_SEND;
		}
		/* goto reset_phase; */

	case LSP_SP_STATE_END_SEND_HEADER:

		session->phase_state = LSP_SP_STATE_END_SEND_DATA;
		/* goto reset_phase; */

	case LSP_SP_STATE_END_SEND_DATA:

		session->phase_state = LSP_SP_STATE_BEGIN_RECEIVE_DATA;
#ifdef LSPIMP_THREE_CONCURRENT_TRANSFER
		/* goto reset_phase; */
#else
		return LSP_REQUIRES_SYNCHRONIZE;
#endif

	case LSP_SP_STATE_BEGIN_RECEIVE_DATA:

		/* noop does not respond */
		if (session->flags & LSP_SFG_NO_RESPONSE)
		{
			/* clear the flag */
			session->flags &= ~(LSP_SFG_NO_RESPONSE);
			return lsp_chained_return(context, LSP_STATUS_SUCCESS);
		}

		if (session->receive_data_buffer_length > 0)
		{
			session->receive_buffer = session->receive_data_buffer;
			session->receive_buffer_length = session->receive_data_buffer_length;
			session->phase_state = LSP_SP_STATE_END_RECEIVE_DATA;
			return LSP_REQUIRES_RECEIVE;
		}

		session->phase_state = LSP_SP_STATE_END_RECEIVE_DATA;
		/* goto reset_phase; */

	case LSP_SP_STATE_END_RECEIVE_DATA:

		session->phase_state = LSP_SP_STATE_BEGIN_RECEIVE_HEADER;
#ifdef LSPIMP_THREE_CONCURRENT_TRANSFER
		/* goto reset_phase; */
#else
		return LSP_REQUIRES_SYNCHRONIZE;
#endif

	case LSP_SP_STATE_BEGIN_RECEIVE_HEADER:

		/* noop does not respond */
		/* this step is redundant as lsp_sps_begin_rcv_dat will clear this */
		if (session->flags & LSP_SFG_NO_RESPONSE)
		{
			/* clear the flag */
			session->flags &= ~(LSP_SFG_NO_RESPONSE);
			session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
			return lsp_chained_return(context, LSP_STATUS_SUCCESS);
		}

		lsp_memset(&session->pdu_ptrs, 0, sizeof(session->pdu_ptrs));

		session->receive_buffer = context->response_pdu_buffer;
		session->receive_buffer_length = sizeof(lsp_pdu_hdr_t);
		session->phase_state = LSP_SP_STATE_END_RECEIVE_HEADER;

		return LSP_REQUIRES_RECEIVE;

	case LSP_SP_STATE_END_RECEIVE_HEADER:

		session->phase_state = LSP_SP_STATE_PROCESS_RECEIVED_HEADER;

		return LSP_REQUIRES_SYNCHRONIZE;

	case LSP_SP_STATE_PROCESS_RECEIVED_HEADER:

		pdu_hdr = _lsp_get_response_buffer(context);

		session->pdu_ptrs.header_ptr = pdu_hdr;

#ifdef LSPIMP_DEBUG_ENC
		if (session->receive_data_buffer_length > 0)
		{
			lsp_debug_payload("REP DAT ENC", 
				session->receive_data_buffer, 
				session->receive_data_buffer_length);
		}
#endif

#ifdef LSPIMP_DEBUG_ENC
		lsp_debug_payload("REP BHS ENC", pdu_hdr, sizeof(lsp_pdu_hdr_t));
#endif

		lsp_decode_pdu_basic_hdr(context, pdu_hdr);

		lsp_debug_payload("REP BHS", pdu_hdr, sizeof(lsp_pdu_hdr_t));

		session->receive_buffer = context->response_pdu_buffer + sizeof(lsp_pdu_hdr_t);
		session->receive_buffer_length = 
			lsp_ntohs(pdu_hdr->ahs_len) + 
			lsp_ntohl(pdu_hdr->dataseg_len);

		if (session->receive_buffer_length > 0)
		{
			session->phase_state = LSP_SP_STATE_BEGIN_RECEIVE_AH;
			goto reset_phase;
		}
		else
		{
			session->phase_state = LSP_SP_STATE_PROCESS_RECEIVED_AH;
			goto reset_phase;
		}

	case LSP_SP_STATE_BEGIN_RECEIVE_AH:

		session->phase_state = LSP_SP_STATE_END_RECEIVE_AH;
		return LSP_REQUIRES_RECEIVE;

	case LSP_SP_STATE_END_RECEIVE_AH:

		session->phase_state = LSP_SP_STATE_PROCESS_RECEIVED_AH;
		return LSP_REQUIRES_SYNCHRONIZE;

	case LSP_SP_STATE_PROCESS_RECEIVED_AH:

		pdu_hdr = (lsp_pdu_hdr_t*) context->response_pdu_buffer;

		lsp_decode_pdu_addendum_hdr(context, pdu_hdr);

#ifdef LSPIMP_DEBUG
		if (lsp_ntohs(pdu_hdr->ahs_len) > 0)
		{
			lsp_debug_payload("REP AHS", 
				((char*)pdu_hdr)+sizeof(lsp_pdu_hdr_t), 
				lsp_ntohs(pdu_hdr->ahs_len));
		}
		if (lsp_ntohl(pdu_hdr->dataseg_len) > 0)
		{
			lsp_debug_payload("REP DS", 
				((char*)pdu_hdr)+sizeof(lsp_pdu_hdr_t) + lsp_ntohs(pdu_hdr->ahs_len),
				lsp_ntohl(pdu_hdr->dataseg_len));
		}
#endif

		status = session->process_proc(context);

		if (LSP_REQUIRE_MORE_PROCESSING == status)
		{
			goto reset_phase;
		}

		if (LSP_STATUS_SUCCESS == status && session->receive_data_buffer_length > 0)
		{
			lsp_decode_data(
				context, 
				session->receive_data_buffer, 
				session->receive_data_buffer_length);

			lsp_debug_payload("REP DAT", 
				session->receive_data_buffer, 
				session->receive_data_buffer_length);
		}

		session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
		return lsp_chained_return(context, status);

	default:
		return LSP_ERR_INVALID_SESSION;
	}
}

void*
lsp_call
lsp_get_buffer_to_send(
	__in lsp_handle_t lsp_handle, 
	__out lsp_uint32* len)
{
	ASSERT(NULL != lsp_handle);
	*len = lsp_handle->session.send_buffer_length;
	return lsp_handle->session.send_buffer;
}

void*
lsp_call
lsp_get_buffer_to_receive(
	__in lsp_handle_t lsp_handle,
	__out lsp_uint32* len)
{
	ASSERT(NULL != lsp_handle);
	*len = lsp_handle->session.receive_buffer_length;
	return lsp_handle->session.receive_buffer;
}

lsp_status_t
lsp_call
lsp_login(
	__in lsp_handle_t h, 
	__in const lsp_login_info_t* login_info)
{
	lsp_request_packet_t* request;

	ASSERT(NULL != h);
	ASSERT(NULL != login_info);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_LOGIN;
	request->u.login.request.login_info = *login_info;

	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_logout(
	__in lsp_handle_t h)
{
	lsp_request_packet_t* request;

	ASSERT(NULL != h);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_LOGOUT;

	return lsp_request(h, request);
}

lsp_status_t 
lsp_call
lsp_ide_command(
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0,
	__in lsp_uint32 lun1,
	__in const lsp_ide_register_param_t* idereg,
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd)
{
	lsp_request_packet_t* request;

	request = &h->session.internal_packets[0];

	lsp_build_ide_command(
		request, 
		h,
		target_id,
		lun0, 
		lun1, 
		idereg, 
		data_buf, 
		ext_cmd);

	return lsp_request(h, request);
}

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
	__in_opt const lsp_extended_command_t* ext_cmd)
{
	lsp_ide_command_request_t* ide_request;

	h;

	ASSERT(NULL != h);
	ASSERT(NULL != idereg);

	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_IDE_COMMAND;
	ide_request = &request->u.ide_command.request;
	ide_request->target_id = target_id;
	ide_request->lun0 = lun0;
	ide_request->lun1 = lun1;
	ide_request->reg = *idereg;
	if (data_buf) ide_request->data_buf = *data_buf;
	if (ext_cmd) ide_request->ext_cmd = *ext_cmd;
}

lsp_status_t
lsp_call
lsp_get_ide_command_output_register(
	__in lsp_handle_t h, 
	__out lsp_ide_register_param_t* idereg)
{
	lsp_session_data_t* session;

	if (!h) return LSP_ERR_INVALID_HANDLE;

	session = &h->session;

	/* 1.0 does support IDE register ? */

	lsp_memcpy(
		idereg,
		&session->last_idereg,
		sizeof(lsp_ide_register_param_t));

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_get_vendor_command_result(
	lsp_handle_t h,
	lsp_uint8 *param,
	lsp_uint8 param_length)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session;
	const lsp_pdu_hdr_t* pdu_hdr;

	if (!context) return LSP_ERR_INVALID_HANDLE;

	session = &context->session;
	pdu_hdr = session->pdu_ptrs.header_ptr;

	if (!pdu_hdr) return LSP_ERR_INVALID_HANDLE;

	if (LSP_OPCODE_VENDOR_SPECIFIC_COMMAND != session->last_op_code)
	{
		return LSP_ERR_INVALID_CALL;
	}

	/* store results */
	lsp_ntohx(param, pdu_hdr->op_data.vendor_command.vop_parm, param_length);
	
	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_get_text_command_result(
	lsp_handle_t* h,
	lsp_uint8* data_out,
	lsp_uint32* data_out_length)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_uint32 datalen;

	if (!context) return LSP_ERR_INVALID_HANDLE;

	session = &context->session; /* cannot be null */
	pdu_hdr = session->pdu_ptrs.header_ptr;

	if (!pdu_hdr) return LSP_ERR_INVALID_HANDLE;
	if (!data_out_length) return LSP_ERR_INVALID_PARAMETER;

	if (LSP_OPCODE_TEXT_REQUEST != session->last_op_code)
	{
		return LSP_ERR_INVALID_CALL;
	}

	datalen = _lsp_get_ahs_or_ds_len(context);

	if (*data_out_length < datalen)
	{
		*data_out_length = datalen;
		return LSP_ERR_MORE_DATA;
	}

	if (data_out)
	{
		/* store results */
		lsp_memcpy(
			data_out,
			_lsp_get_ahs_or_dataseg(session),
			datalen);
	}

	return LSP_STATUS_SUCCESS;
}

lsp_status_t 
lsp_call
lsp_vendor_command(
	__in lsp_handle_t h,
	__in lsp_uint16 vendor_id,
	__in lsp_uint8 vop_ver,
	__in lsp_uint8 vop_code,
	__in_bcount(param_length) const lsp_uint8 *param,
	__in lsp_uint8 param_length,
	__in_opt const lsp_io_data_buffer_t* data_buf)
{
	lsp_request_packet_t* request;
	lsp_vendor_command_request_t* vc_request;

	ASSERT(NULL != h);
	ASSERT(NULL != param);
	ASSERT(param_length <= 8);

	if (!param)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (param_length > 8)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = vendor_id;
	vc_request->vop_ver = vop_ver;
	vc_request->vop_code = vop_code;
	lsp_memcpy(vc_request->param, param, param_length);
	vc_request->param_length = param_length;
	if (data_buf) vc_request->data_buf = *data_buf;

	return lsp_request(h, request);
}

lsp_status_t 
lsp_call 
lsp_text_command(
	__in lsp_handle_t h,
	__in lsp_uint8 param_type,
	__in lsp_uint8 param_ver,
	__in_bcount(data_in_length) const lsp_uint8 *data,
	__in lsp_uint16 data_in_length)
{
	lsp_request_packet_t* request;
	lsp_text_command_request_t* text_request;

	ASSERT(NULL != h);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_TEXT_COMMAND;
	text_request = &request->u.text_command.request;
	text_request->param_type = param_type;
	text_request->param_ver = param_ver;
	text_request->data = data;
	text_request->data_in_length = data_in_length;

	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_request(
	__in lsp_handle_t h,
	__inout lsp_request_packet_t* request)
{
	lsp_session_data_t* session;

	ASSERT(NULL != h);
	ASSERT(NULL != request);

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if (NULL == request)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	session = &h->session;

	switch (request->type)
	{
	case LSP_REQUEST_LOGIN:

		if (LSP_PHASE_SECURITY != session->session_phase)
		{
			return request->status = LSP_ERR_INVALID_SESSION;
		}

		/* Hardware Version 1.0 is assumed */
		session->hw_ver = LSP_HARDWARE_VERSION_1_0;

		_lsp_set_next_phase(h, LSP_SP_LOGIN_INIT);
		session->prepare_proc = lsp_login_prepare;
		session->process_proc = lsp_login_process;

		break;

	case LSP_REQUEST_LOGOUT:

		if (LSP_PHASE_FULL_FEATURE != session->session_phase)
		{
			return request->status = LSP_ERR_INVALID_SESSION;
		}

		session->prepare_proc = lsp_logout_prepare;
		session->process_proc = lsp_logout_process;

		break;

	case LSP_REQUEST_IDE_COMMAND:

		/* lun0 and lun1 must be 0.*/
		if (0 != request->u.ide_command.request.lun0 || 
			0 != request->u.ide_command.request.lun1)
		{
			return request->status = LSP_ERR_INVALID_PARAMETER;
		}

		/* there is no bi-directional IDE command.
		 * caller cannot specify both recv_buffer and send_buffer non-null
		 */
		if (request->u.ide_command.request.data_buf.recv_size > 0 && 
			request->u.ide_command.request.data_buf.send_size > 0)
		{
			/* both recv and send buffers are specified */
			return request->status = LSP_ERR_INVALID_PARAMETER;
		}

		/* only full feature phase supports IDE commands */
		if (LSP_PHASE_FULL_FEATURE != session->session_phase)
		{
			return request->status = LSP_ERR_INVALID_SESSION;
		}

		/* discover login does not allow IDE commands */
		if (LSP_LOGIN_TYPE_DISCOVER == session->login_type)
		{
			return request->status = LSP_ERR_INVALID_LOGIN_MODE;
		}

		switch (session->hw_prot_ver)
		{
		case LSP_IDE_PROTOCOL_VERSION_1_0:
			session->prepare_proc = lsp_ide_command_prepare_v0;
			session->process_proc = lsp_ide_command_process_v0;
			break;
		case LSP_IDE_PROTOCOL_VERSION_1_1:
			session->prepare_proc = lsp_ide_command_prepare_v1;
			session->process_proc = lsp_ide_command_process_v1;
			break;
		default:
			return request->status = LSP_ERR_INVALID_HANDLE;
		}
		break;
	
	case LSP_REQUEST_TEXT_COMMAND:

		/* intentionally, we allow only DISCOVER login type for TEXT_COMMAND */

		if (LSP_LOGIN_TYPE_DISCOVER != session->login_type)
		{
			return request->status = LSP_ERR_INVALID_LOGIN_MODE;
		}

		h->session.prepare_proc = lsp_text_command_prepare;
		h->session.process_proc = lsp_text_command_process;

		break;
	
	case LSP_REQUEST_VENDOR_COMMAND:

		if (LSP_LOGIN_TYPE_DISCOVER == session->login_type)
		{
			return request->status = LSP_ERR_INVALID_LOGIN_MODE;
		}

		if (LSP_HARDWARE_VERSION_1_0 == session->hw_ver ||
			LSP_HARDWARE_VERSION_1_1 == session->hw_ver ||
			LSP_HARDWARE_VERSION_2_0 == session->hw_ver)
		{
			if (8 != request->u.vendor_command.request.param_length)
			{
				return request->status = LSP_ERR_INVALID_PARAMETER;
			}
		}
		else
		{
			return request->status = LSP_ERR_INVALID_PARAMETER;
		}

		/* there is no bi-directional data flow is allowed.
		* caller cannot specify both recv_buffer and send_buffer non-null
		*/
		if (request->u.vendor_command.request.data_buf.recv_size > 0 && 
			request->u.vendor_command.request.data_buf.send_size > 0)
		{
			/* both recv and send buffers are specified */
			return request->status = LSP_ERR_INVALID_PARAMETER;
		}

		h->session.prepare_proc = lsp_vendor_command_prepare;
		h->session.process_proc = lsp_vendor_command_process;

		break;

	case LSP_REQUEST_NOOP_COMMAND:
	
		if (LSP_LOGIN_TYPE_DISCOVER == session->login_type)
		{
			return request->status = LSP_ERR_INVALID_LOGIN_MODE;
		}

		h->session.prepare_proc = lsp_noop_command_prepare;
		h->session.process_proc = lsp_noop_command_process;

		break;

	default:
		return request->status = LSP_ERR_INVALID_PARAMETER;
	}
	session->current_request = request;
	session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
	return lsp_process_next(h);
}

lsp_status_t 
lsp_call
lsp_noop_command(lsp_handle_t h)
{
	lsp_request_packet_t* request;
	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_NOOP_COMMAND;
	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_set_options(
	__in lsp_handle_t h, 
	__in lsp_uint32 options)
{
	lsp_session_data_t *session;
	
	if (!h) return LSP_ERR_INVALID_HANDLE;

	session = &h->session;

	session->session_options = options;

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_get_options(
	__in lsp_handle_t h, 
	__out lsp_uint32* options)
{
	lsp_session_data_t *session;

	if (!h) return LSP_ERR_INVALID_HANDLE;

	session = &h->session;

	*options = session->session_options;

	return LSP_STATUS_SUCCESS;
}

void
lsp_call
lsp_encrypt_send_data(
	__in lsp_handle_t h,
	__out_bcount(len) lsp_uint8* dst,
	__in_bcount(len) const lsp_uint8* src, 
	__in lsp_uint32 len)
{
	lsp_session_data_t *session;

	if (!h) return;

	session = &h->session;

	lsp_encrypt32_copy_internal(dst, src, len, session);
}

lsp_status_t
lsp_call
lsp_get_handle_info(
	lsp_handle_t h,
	lsp_handle_info_type_t info_type,
	void *data,
	lsp_uint32 data_length)
{
	lsp_handle_context_t *context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session;

	if (!context) return LSP_ERR_INVALID_HANDLE;
	if (!data) return LSP_ERR_INVALID_PARAMETER;

	session = &context->session;

#define LSP_SET_HANDLE_INFO(INFO_TYPE, DATA, DATA_LENGTH, VAR, VAR_TYPE) \
	case (INFO_TYPE) :													\
		if ((DATA_LENGTH) < sizeof(VAR_TYPE))                            \
		{	                                                            \
			return LSP_ERR_INVALID_PARAMETER;    						\
		}                                                               \
	*(VAR_TYPE *)(DATA) = VAR;											\
	break;

	switch (info_type)
	{
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_TYPE, data, data_length, session->hw_type, lsp_uint8);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_VERSION, data, data_length, session->hw_ver, lsp_uint8);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_PROTOCOL_TYPE, data, data_length, session->hw_prot_type, lsp_uint8);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_PROTOCOL_VERSION, data, data_length, session->hw_prot_ver, lsp_uint8);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_SLOTS, data, data_length, session->slot_cnt, lsp_uint32);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_MAX_TRANSFER_BLOCKS, data, data_length, session->max_transfer_blocks, lsp_uint32);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_MAX_TARGETS, data, data_length, session->max_targets, lsp_uint32);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_MAX_LUS, data, data_length, session->max_lun, lsp_uint32);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_HEADER_ENCRYPTION_ALGORITHM, data, data_length, session->hdr_enc_alg, lsp_uint16);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_DATA_ENCRYPTION_ALGORITHM, data, data_length, session->data_enc_alg, lsp_uint16);

		LSP_SET_HANDLE_INFO(LSP_PROP_HW_REVISION, data, data_length, session->hw_rev, lsp_uint16);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_HEADER_DIGEST_ALGORITHM, data, data_length, session->hdr_dig_alg, lsp_uint16);
		LSP_SET_HANDLE_INFO(LSP_PROP_HW_DATA_DIGEST_ALGORITHM, data, data_length, session->dat_dig_alg, lsp_uint16);
		default:
		return LSP_ERR_NOT_SUPPORTED;
	}
#undef LSP_SET_HANDLE_INFO

	return LSP_STATUS_SUCCESS;
}


