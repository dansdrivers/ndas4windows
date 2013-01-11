#include "lsp_type_internal.h"
#include "lsp_hash.h"
#include "lsp_binparm.h"
#include "lsp_debug.h"
#include "lsp_util.h"
#include <lspx/lsp.h>

#define LSPIMP_THREE_CONCURRENT_TRANSFER

/* 0 : Conventional Key + Password 
 * 1 : Combined Encryption Key
 * 2 : Combined Encryption Key + Integral Type (uint32)
 */
#define LSPIMP_ENC_OPTIMIZE_TYPE 2

#ifndef countof
#define countof(A) (sizeof(A)/sizeof((A)[0]))
#endif

#ifdef LSP_USE_INLINE

LSP_INLINE 
void 
lsp_call
lspp_clear_request_buffer(lsp_handle_context_t* context)
{
	lsp_memset(context->request_pdu_buffer, 0, LSP_SESSION_REQUEST_BUFFER_SIZE);
}

LSP_INLINE
void 
lsp_call
lspp_clear_response_buffer(lsp_handle_context_t* context)
{
	lsp_memset(context->response_pdu_buffer, 0, LSP_SESSION_RESPONSE_BUFFER_SIZE);
}

LSP_INLINE 
lsp_pdu_hdr_t* 
lsp_call
lspp_get_request_buffer(lsp_handle_context_t* context)
{
	return (lsp_pdu_hdr_t*) context->request_pdu_buffer;
}

lsp_pdu_hdr_t* 
LSP_INLINE 
lsp_call
lspp_get_response_buffer(lsp_handle_context_t* context)
{
	return (lsp_pdu_hdr_t*) context->response_pdu_buffer;
}

#else

#define lspp_clear_request_buffer(context) \
	lsp_memset(context->request_pdu_buffer, 0, LSP_SESSION_REQUEST_BUFFER_SIZE)

#define lspp_clear_response_buffer(context) \
	lsp_memset(context->response_pdu_buffer, 0, LSP_SESSION_RESPONSE_BUFFER_SIZE)

#define lspp_get_request_buffer(context) \
	((lsp_pdu_hdr_t*)(context->request_pdu_buffer))

#define lspp_get_response_buffer(context) \
	((lsp_pdu_hdr_t*)(context->response_pdu_buffer))

#endif

/* error macros */

const lsp_uint8_t LSP_LOGIN_PASSWORD_ANY[8]				= { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const lsp_uint8_t LSP_LOGIN_PASSWORD_SAMPLE[8]			= { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const lsp_uint8_t LSP_LOGIN_PASSWORD_DEFAULT[8]			= { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const lsp_uint8_t LSP_LOGIN_PASSWORD_SEAGATE[8]			= { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };
const lsp_uint8_t LSP_LOGIN_PASSWORD_WINDOWS_RO[8]		= { 0xBF, 0x57, 0x53, 0x48, 0x1F, 0x33, 0x7B, 0x3F };
const lsp_uint8_t LSP_LOGIN_PASSWORD_AUTO_REGISTRY[8]	= { 0xCE, 0xD0, 0x6E, 0x3C, 0x2F, 0x6A, 0x3C, 0x5E };

const lsp_uint8_t* LSP_KNOWN_LOGIN_PASSWORD_LIST[] = {

	LSP_LOGIN_PASSWORD_DEFAULT,
	LSP_LOGIN_PASSWORD_SEAGATE,
	LSP_LOGIN_PASSWORD_WINDOWS_RO,
	LSP_LOGIN_PASSWORD_AUTO_REGISTRY,
	LSP_LOGIN_PASSWORD_SAMPLE
};

enum { 
	LSP_DISK_SECTOR_SIZE = 512,
	LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED = -1 
};


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

LSP_INLINE
void
lsp_call
lsp_encrypt32_internal(
	lsp_uint8_t* buf,
	lsp_uint32_t len, 
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
		(lsp_uint8_t *)&session->encrypt_ckey);
#else
	lsp_encrypt32(
		buf,
		len,
		(lsp_uint8_t *)&session->chap_c,
		(lsp_uint8_t *)&session->password);
#endif
}

LSP_INLINE
void
lsp_call
lsp_decrypt32_internal(
	lsp_uint8_t* buf, 
	lsp_uint32_t len, 
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
		(lsp_uint8_t *)&session->decrypt_ckey);
#else
	lsp_decrypt32(
		buf,
		len,
		session->chap_c,
		(lsp_uint8_t *)&session->password);
#endif
}

LSP_INLINE
void
lsp_call
lsp_encrypt32_copy_internal(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_session_data_t* session)
{
	if (session->hardware_data.data_encryption_algorithm)
	{
#if LSPIMP_ENC_OPTIMIZE_TYPE == 2
		lsp_encrypt32exx_copy(dst, src, len, session->encrypt_ckey);
#elif LSPIMP_ENC_OPTIMIZE_TYPE == 1
		lsp_encrypt32ex_copy(dst, src, len, (lsp_uint8_t*)&session->encrypt_ckey);
#else
		lsp_encrypt32ex_copy(dst, src, len, (lsp_uint8_t*)&session->encrypt_ckey);
#endif
	}
	else
	{
		lsp_memcpy(dst, src, len);
	}
}

LSP_INLINE
void
lsp_call
lsp_decrypt32_copy_internal(
	__out_ecount(len) lsp_uint8_t* dst,
	__in_ecount(len) const lsp_uint8_t* src, 
	__in lsp_uint32_t len,
	__in lsp_session_data_t* session)
{
	if (session->hardware_data.data_encryption_algorithm)
	{
#if LSPIMP_ENC_OPTIMIZE_TYPE == 2
		lsp_decrypt32exx_copy(dst, src, len, session->decrypt_ckey);
#elif LSPIMP_ENC_OPTIMIZE_TYPE == 1
		lsp_decrypt32ex_copy(dst, src, len, (lsp_uint8_t*)&session->decrypt_ckey);
#else
		lsp_decrypt32ex_copy(dst, src, len, (lsp_uint8_t*)&session->decrypt_ckey);
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
	lsp_uint8_t* buf,
	lsp_uint32_t buflen);

static
void
lsp_call
lsp_decode_data(
	lsp_handle_context_t* context,
	lsp_uint8_t* buf,
	lsp_uint32_t buflen);

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
	lsp_uint8_t* buf,
	lsp_uint32_t buflen)
{
	lsp_session_data_t* session = &context->session;
	if (session->hardware_data.data_encryption_algorithm)
	{
		lsp_encrypt32_internal(buf, buflen, session);
	}
}

static
void
lsp_call
lsp_decode_data(
	lsp_handle_context_t* context,
	lsp_uint8_t* buf,
	lsp_uint32_t buflen)
{
	if (buflen > 0)
	{
		lsp_session_data_t* session = &context->session;
		/* decrypt data */
		if (session->hardware_data.data_encryption_algorithm)
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
		session->hardware_data.header_encryption_algorithm)
	{
		lsp_decrypt32_internal((lsp_uint8_t*) pdu_hdr, sizeof(lsp_pdu_hdr_t), session);
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
	lsp_uint8_t* buf = ((lsp_uint8_t*)pdu_hdr) + sizeof(lsp_pdu_hdr_t);
	lsp_uint16_t ahs_len;
	lsp_uint32_t dataseg_len;

	ahs_len = lsp_ntohs(pdu_hdr->ahs_len);
	if (ahs_len > 0)
	{
		if (LSP_IDE_PROTOCOL_VERSION_1_0 != session->hardware_data.protocol_version &&
			LSP_PHASE_FULL_FEATURE == session->session_phase &&
			session->hardware_data.header_encryption_algorithm)
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
			session->hardware_data.header_encryption_algorithm)
		{
			lsp_decrypt32_internal(buf, dataseg_len, session);
		}
		session->pdu_ptrs.data_seg_ptr = buf;
//		buf += dataseg_len;
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
	lsp_session_data_t* session;
	lsp_uint8_t* buf;
	lsp_uint16_t ahs_len;
	lsp_uint32_t dataseg_len;

	LSP_ASSERT(context != NULL);
	session = &context->session;

	/* encryption */
	if (LSP_PHASE_FULL_FEATURE != session->session_phase)
	{
		return;
	}
	
	buf = (lsp_uint8_t*) pdu_hdr;
	ahs_len = lsp_ntohs(pdu_hdr->ahs_len);
	dataseg_len = lsp_ntohl(pdu_hdr->dataseg_len);
	
	/* 1.0: ahs_len must be 0 */
	LSP_ASSERT(
		   (LSP_IDE_PROTOCOL_VERSION_1_0 == session->hardware_data.protocol_version && (0 == ahs_len)) ||
		   (LSP_IDE_PROTOCOL_VERSION_1_0 != session->hardware_data.protocol_version));
	
	/* 1.1: dsg_len must be 0 */
	LSP_ASSERT(
		   (LSP_IDE_PROTOCOL_VERSION_1_1 == session->hardware_data.protocol_version && (0 == dataseg_len)) ||
		   (LSP_IDE_PROTOCOL_VERSION_1_1 != session->hardware_data.protocol_version));
	
	
	/* encrypt header */
	if (session->hardware_data.header_encryption_algorithm)
	{
		lsp_encrypt32_internal(buf, sizeof(lsp_pdu_hdr_t), session);
		if (LSP_IDE_PROTOCOL_VERSION_1_0 != session->hardware_data.protocol_version && ahs_len > 0)
		{
			buf = ((lsp_uint8_t*)pdu_hdr) + sizeof(lsp_pdu_hdr_t);
			lsp_encrypt32_internal(buf, ahs_len, session);
		}
	}
	
	/* encrypt data segment */
	if (session->hardware_data.data_encryption_algorithm && dataseg_len > 0)
	{
		buf = ((lsp_uint8_t*)pdu_hdr) + sizeof(lsp_pdu_hdr_t) + ahs_len;
		lsp_encrypt32_internal(buf, dataseg_len, session);
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
#define LSP_SESSION_RESPONSE_PDU_BUFFER_SIZE 60
#define LSP_SESSION_RESPONSE_MAX_AHS_SIZE 512
#define LSP_SESSION_RESPONSE_BUFFER_SIZE \
	(LSP_SESSION_RESPONSE_PDU_BUFFER_SIZE + \
	LSP_SESSION_RESPONSE_MAX_AHS_SIZE + 4)

LSP_C_ASSERT(lsp_hsize, sizeof(lsp_handle_context_t) <= 1536);

#define LSP_SESSION_BUFFER_SIZE_MACRO (\
	sizeof(lsp_handle_context_t) + \
	LSP_SESSION_REQUEST_BUFFER_SIZE + \
	LSP_SESSION_RESPONSE_BUFFER_SIZE)

const lsp_uint32_t LSP_SESSION_BUFFER_SIZE = LSP_SESSION_BUFFER_SIZE_MACRO;

LSP_C_ASSERT(lsp_bufsize, LSP_SESSION_BUFFER_SIZE_MACRO <= 2048);

LSP_INLINE
int
lsp_call
lspp_is_ata_ext_command(
	__in lsp_uint8_t cmdreg);

LSP_INLINE 
void 
lsp_call 
lspp_set_ide_header(
	__out lsp_ide_header_t* idehdr,
	__in const lsp_ide_register_param_t* idereg,
	__in_opt const lsp_extended_command_t* pktcmd);

lsp_uint32_t
lsp_call
lsp_get_session_buffer_size()
{
	return LSP_SESSION_BUFFER_SIZE;
}

lsp_handle_t
lsp_call
lsp_initialize_session(
	__in_bcount(session_buffer_size) void* session_buffer,
	__in lsp_uint32_t session_buffer_size)
{
	lsp_handle_context_t* context;
	lsp_uint8_t* buf;

	context = (lsp_handle_context_t*) session_buffer;

	if (session_buffer_size < LSP_SESSION_BUFFER_SIZE)
	{
		return 0;
	}

	context->session_buffer_size = LSP_SESSION_BUFFER_SIZE;
	lsp_memset(context, 0, LSP_SESSION_BUFFER_SIZE);
	buf = (lsp_uint8_t*) session_buffer;

	buf += sizeof(lsp_handle_context_t);
	context->request_pdu_buffer = buf;

	buf += LSP_SESSION_REQUEST_BUFFER_SIZE;
	context->response_pdu_buffer = buf;

	return context;
}

LSP_INLINE
void*
lsp_call
lspp_get_ahs_or_dataseg(lsp_session_data_t* session)
{
	switch (session->hardware_data.protocol_version)
	{
	case LSP_IDE_PROTOCOL_VERSION_1_0: 
		return session->pdu_ptrs.data_seg_ptr;
	case LSP_IDE_PROTOCOL_VERSION_1_1: 
		return session->pdu_ptrs.ahs_ptr;
	default: 
		return 0;
	}
}

LSP_INLINE
lsp_status_t
lsp_call
lspp_is_valid_data_segment(lsp_handle_context_t* context, lsp_uint32_t len)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*)context->response_pdu_buffer;
	lsp_uint32_t len_in_hdr;
	switch (session->hardware_data.protocol_version)
	{
	case LSP_IDE_PROTOCOL_VERSION_1_0: 
		len_in_hdr = lsp_ntohl(pdu_hdr->dataseg_len);
		if (len_in_hdr < len || !(session->pdu_ptrs.data_seg_ptr))
		{
			return LSP_STATUS_RESPONSE_HEADER_INVALID_DATA_SEGMENT;
		}
		break;
	case LSP_IDE_PROTOCOL_VERSION_1_1: 
		len_in_hdr = lsp_ntohs(pdu_hdr->ahs_len);
		if (len_in_hdr < len || !(session->pdu_ptrs.ahs_ptr))
		{
			return LSP_STATUS_RESPONSE_HEADER_INVALID_DATA_SEGMENT;
		}
		break;
	default:
		LSP_ASSERT(FALSE);
		return LSP_STATUS_RESPONSE_HEADER_INVALID_DATA_SEGMENT;
	}
	return LSP_STATUS_SUCCESS;
}

LSP_INLINE 
void 
lsp_call
lspp_set_next_phase(
	lsp_handle_context_t* context, 
	lsp_session_phase_t phase)
{
	context->session.phase = phase;
	context->session.phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
}

LSP_INLINE
void
lsp_call
lspp_set_next_phase_state(
	lsp_handle_context_t* context, 
	lsp_session_phase_state_t state)
{
	context->session.phase_state = state;
}

LSP_INLINE
void
lsp_call
lspp_set_ahs_or_ds_len(
	lsp_handle_context_t* context, 
	lsp_uint32_t len)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*) context->request_pdu_buffer;

	switch (session->hardware_data.protocol_version)
	{
	case LSP_IDE_PROTOCOL_VERSION_1_0: 
		pdu_hdr->dataseg_len = lsp_htonl(len);
		break;
	case LSP_IDE_PROTOCOL_VERSION_1_1:
		pdu_hdr->ahs_len = lsp_htons((lsp_uint16_t)len);
		break;
	default:
		LSP_ASSERT(FALSE);
	}
}

LSP_INLINE
lsp_uint32_t
lsp_call
lspp_get_ahs_or_ds_len(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr = (lsp_pdu_hdr_t*) context->response_pdu_buffer;

	switch (session->hardware_data.protocol_version)
	{
	case LSP_IDE_PROTOCOL_VERSION_1_0: 
		return lsp_ntohl((lsp_uint32_t)(pdu_hdr->dataseg_len));
	case LSP_IDE_PROTOCOL_VERSION_1_1:
		return lsp_ntohl((lsp_uint32_t)(pdu_hdr->ahs_len));
	default:
		LSP_ASSERT(FALSE);
		return 0;
	}
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
	/* login info is stored to the session at the first phase 
	 * session->login_info data may be changed in the following phase
	 */
	login_info = &session->current_request->u.login.request.login_info;
	session->login_info = *login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	session->hpid = 0;
	session->rpid = 0; // need not ( not sure )
	session->path_cmd_tag = 0;

	// suppose hardware_data.protocol_version to max
	// hardware_data.protocol_version will be set correctly after this login phase
	session->hardware_data.protocol_version = LSP_IDE_PROTOCOL_VERSION_MAX;

	// initialize context
	session->session_phase = LSP_PHASE_SECURITY;
	session->hardware_data.header_encryption_algorithm = 0;
	session->hardware_data.data_encryption_algorithm = 0;
	session->hardware_data.header_digest_algorithm = 0;
	session->hardware_data.data_digest_algorithm = 0;

	// 1st login phase
	lspp_clear_request_buffer(context);
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->hpid);

	lspp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.login.parm_type = 1;
	pdu_hdr->op_data.u.login.parm_ver = 0;
	pdu_hdr->op_data.u.login.ver_max = session->hardware_data.hardware_version; // phase specific
	pdu_hdr->op_data.u.login.ver_min = 0; // phase specific

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
	/* login info here is from the session, not from request */
	login_info = &session->login_info;

	pdu_hdr = lspp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.u.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.u.login.CSG ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.u.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.u.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.u.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.u.login_response.parm_ver)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_HEADER, 0);
	}

	/* Set hardware_data.hardware_version, 
	 * hardware_data.protocol_version with detected one.
	 */

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		/* phase specific */
		if (LSP_ERR_RESPONSE_RI_VERSION_MISMATCH == pdu_hdr->response)
		{
			if (session->hardware_data.hardware_version != 
				pdu_hdr->op_data.u.login_response.ver_active)
			{
				session->hardware_data.hardware_version = 
					pdu_hdr->op_data.u.login_response.ver_active;
				session->flags |= LSP_SFG_PROTOCOL_VERSION_IS_SET;
				lspp_set_next_phase(context, LSP_SP_LOGIN_INIT_RETRY);
				return LSP_REQUIRE_MORE_PROCESSING;
			}
		}

		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	session->flags |= LSP_SFG_PROTOCOL_VERSION_IS_SET;

	/* phase specific */
	session->hardware_data.hardware_version = 
		pdu_hdr->op_data.u.login_response.ver_active;

	switch (session->hardware_data.hardware_version)
	{
	case LSP_HARDWARE_VERSION_1_0:
		session->hardware_data.protocol_version = 
			LSP_IDE_PROTOCOL_VERSION_1_0;
		break;
	case LSP_HARDWARE_VERSION_1_1:
	case LSP_HARDWARE_VERSION_2_0:
		session->hardware_data.protocol_version = 
			LSP_IDE_PROTOCOL_VERSION_1_1;
		break;
	default:
		return LSP_STATUS_UNSUPPORTED_HARDWARE_VERSION;
	}

	/* new in 2.0g */
	session->hardware_data.hardware_revision = 
		lsp_ntohs(pdu_hdr->op_data.u.login_response.revision);

	status = lspp_is_valid_data_segment(
		context, LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY);

	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*) lspp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	/* store data */

	session->rpid = lsp_ntohs(pdu_hdr->rpid);

	lspp_set_next_phase(context, LSP_SP_LOGIN_AUTH_1);

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
	/* login info here is from the session, not from request */
	login_info = &session->login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	lspp_clear_request_buffer(context);
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	lspp_set_ahs_or_ds_len(context, LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.login.ver_max = session->hardware_data.hardware_version;
	pdu_hdr->op_data.u.login.ver_min = 0;
	pdu_hdr->op_data.u.login.parm_type = 1;
	pdu_hdr->op_data.u.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_t*)
		(context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));

	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_t*) param_secu->u.auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);

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
	/* login info here is from the session, not from request */
	login_info = &session->login_info;

	pdu_hdr = lspp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.u.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.u.login.CSG ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.u.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.u.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.u.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.u.login_response.parm_ver)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_HEADER, 0);
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	status = lspp_is_valid_data_segment(
		context, LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY);
	
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*) lspp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	/* store data */
	param_chap = &param_secu->u.auth_chap;

	session->chap_i = lsp_ntohl(param_chap->chap_i);
	session->chap_c = lsp_ntohl(param_chap->chap_c[0]);

	lspp_set_next_phase(context, LSP_SP_LOGIN_AUTH_2);

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
	lsp_uint32_t               user_id;

	lsp_login_info_t* login_info;
        const lsp_uint8_t no_password[8] = {0};
            
//	session = &context->session;
	/* login info here is from the session, not from request */
	login_info = &session->login_info;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	lspp_clear_request_buffer(context);
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.u.login.T = 1;
	pdu_hdr->op_flags.u.login.CSG = LSP_PHASE_SECURITY;
	pdu_hdr->op_flags.u.login.NSG = LSP_PHASE_LOGIN_OPERATION;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	lspp_set_ahs_or_ds_len(
		context, LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.login.ver_max = session->hardware_data.hardware_version;
	pdu_hdr->op_data.u.login.ver_min = 0;
	pdu_hdr->op_data.u.login.parm_type = 1;
	pdu_hdr->op_data.u.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_t*)(
		context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));

	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_t*)param_secu->u.auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);
	param_chap->chap_i = lsp_htonl(session->chap_i);

	/* set up the login user id based on factors:
	 * 1. When superuser password is specified, use LSP_NDAS_SUPERVISOR
	 * 2. Otherwise, infer from unit_no and write_access
	 */
	if (lsp_memcmp(login_info->supervisor_password, no_password, sizeof(login_info->supervisor_password)))
	{
		user_id = (lsp_uint32_t) LSP_NDAS_SUPERVISOR;
	}
	else if (0 == login_info->unit_no)
	{
		if (login_info->write_access)
			user_id = LSP_FIRST_TARGET_RW_USER;
		else
			user_id = LSP_FIRST_TARGET_RO_USER;
	}
	else if (1 == login_info->unit_no)
	{
		if (login_info->write_access)
			user_id = LSP_SECOND_TARGET_RW_USER;
		else
			user_id = LSP_SECOND_TARGET_RO_USER;
	}
	else
	{
		LSP_ASSERT(FALSE);
		return LSP_STATUS_INVALID_PARAMETER;
	}

	/* fill out chap_n */

	if (LSP_LOGIN_TYPE_NORMAL == login_info->login_type)
	{
		param_chap->chap_n = lsp_htonl(user_id);
	}
	else
	{
		param_chap->chap_n = 0;
	}

	/* walk through the well-known password list if applicable */

	if (-1 != session->wlk_pwd_index)
	{
		LSP_ASSERT(session->wlk_pwd_index < 
			countof(LSP_KNOWN_LOGIN_PASSWORD_LIST));

		lsp_memcpy(
			login_info->password,
			LSP_KNOWN_LOGIN_PASSWORD_LIST[session->wlk_pwd_index],
			sizeof(login_info->password));
	}

	/* hash in... */

	lsp_hash_uint32_to128(
		(lsp_uint8_t *)param_chap->chap_r,
		session->chap_c,
		(LSP_NDAS_SUPERVISOR == user_id) ? 
			login_info->supervisor_password : 
			login_info->password);

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
	/* login info here is from the session, not from request */
	login_info = &session->login_info;

	pdu_hdr = lspp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.u.login.T ||
		LSP_PHASE_SECURITY != pdu_hdr->op_flags.u.login.CSG ||
		LSP_PHASE_LOGIN_OPERATION != pdu_hdr->op_flags.u.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.u.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.u.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.u.login_response.parm_ver)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_HEADER, 0);
	}

	/* LSP_ERR_RESPONSE_T_COMMAND_FAILED */
	/* 1. failed because login RW but already RW exists */
	/* 2. failed when the password is invalid */
	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{

		if (-1 != session->wlk_pwd_index &&
			session->wlk_pwd_index + 1 < countof(LSP_KNOWN_LOGIN_PASSWORD_LIST))
		{
			/* if we are using LOGIN_PASSWORD_ANY, try again */
			++session->wlk_pwd_index;

			lsp_debug("SP_LOGIN_AUTH_2 failed, retry with password %d\n", 
				session->wlk_pwd_index);

			lspp_set_next_phase(context, LSP_SP_LOGIN_INIT_RETRY);
			return LSP_REQUIRE_MORE_PROCESSING;
		}
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, 
			sub_seq_num, 
			LSP_ERR_TYPE_RESPONSE, 
			pdu_hdr->response);
	}

	status = lspp_is_valid_data_segment(
		context, LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY);
	
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_secu = (lsp_binparm_security_t*)lspp_get_ahs_or_dataseg(session);

	if (LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type || 
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	session->session_phase = LSP_PHASE_LOGIN_OPERATION;

	lspp_set_next_phase(context, LSP_SP_LOGIN_NEGO);

	return LSP_REQUIRE_MORE_PROCESSING;
}

static
lsp_status_t
lsp_call
lsp_login_phase_4_prepare(
	lsp_handle_context_t* context)
{
	static const lsp_uint16_t sub_seq_num = 3;

	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_binparm_negotiation_t* param_nego;

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	lspp_clear_request_buffer(context);
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.u.login.T = 1;
	pdu_hdr->op_flags.u.login.CSG = LSP_PHASE_LOGIN_OPERATION;
	pdu_hdr->op_flags.u.login.NSG = LSP_PHASE_FULL_FEATURE;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);

	lspp_set_ahs_or_ds_len(
		context, LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST);

	/* additional payload */
	session->send_buffer_length += LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST;

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.login.ver_max = session->hardware_data.hardware_version;
	pdu_hdr->op_data.u.login.ver_min = 0;
	pdu_hdr->op_data.u.login.parm_type = 1;
	pdu_hdr->op_data.u.login.parm_ver = 0;

	/* phase specific */

	param_nego = (lsp_binparm_negotiation_t*)(
		context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t));

	param_nego->parm_type = LSP_BINPARM_TYPE_NEGOTIATION;

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

//	const lsp_login_info_t* login_info;;

	session = &context->session;
	/* login info here is from the session, not from request */
//	login_info = &session->login_info;

	pdu_hdr = lspp_get_response_buffer(context);

	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.u.login.T ||
		LSP_PHASE_LOGIN_OPERATION != pdu_hdr->op_flags.u.login.CSG ||
		LSP_PHASE_FULL_FEATURE != pdu_hdr->op_flags.u.login.NSG ||
		LSP_HARDWARE_VERSION_MAX < pdu_hdr->op_data.u.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.u.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.u.login_response.parm_ver)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_HEADER, 0);
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	status = lspp_is_valid_data_segment(
		context, LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY);

	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}

	param_nego = (lsp_binparm_negotiation_t*) 
		((LSP_IDE_PROTOCOL_VERSION_1_0 == session->hardware_data.protocol_version) ? 
		session->pdu_ptrs.data_seg_ptr : 
		session->pdu_ptrs.ahs_ptr);

	if (LSP_BINPARM_TYPE_NEGOTIATION != param_nego->parm_type)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	/* phase specific */
	session->hardware_data.hardware_type = param_nego->hw_type;
	session->hardware_data.protocol_type = param_nego->hw_type;
	if (session->hardware_data.hardware_version != param_nego->hw_ver)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN, sub_seq_num, 
			LSP_ERR_TYPE_REPLY_PARM, 0);
	}

	session->hardware_data.number_of_slots = lsp_ntohl(param_nego->slot_cnt);
	session->hardware_data.maximum_transfer_blocks = lsp_ntohl(param_nego->max_blocks);
	session->hardware_data.maximum_target_id = lsp_ntohl(param_nego->max_target_id);
	session->hardware_data.maximum_lun = lsp_ntohl(param_nego->max_lun);
#if 0
	session->hardware_data.header_encryption_algorithm = lsp_ntohs(param_nego->hdr_enc_alg);
	session->hardware_data.data_encryption_algorithm = lsp_ntohs(param_nego->dat_enc_alg);
#else
	session->hardware_data.header_encryption_algorithm = param_nego->hdr_enc_alg;
	session->hardware_data.data_encryption_algorithm = param_nego->dat_enc_alg;
#endif
	/* header digest and data digest is not actually introduced in 2.0g yet */
	/* we are just reserving the location for 2.5 or later */
	session->hardware_data.header_digest_algorithm = 0;
	session->hardware_data.data_digest_algorithm = 0;

	session->session_phase = LSP_PHASE_FULL_FEATURE;

	/* THIS BUG HAS TO BE DEALT WITH VERIFIED-WRITE POLICY, but NOT HERE */
	/* AS OF APRIL 2007: 2.0 rev 0 must not use UDMA but MWDMA or lower mode */
	/*  where data corruption does not occur */
#if 0
	/* V2.0 bug : A data larger than 52k can be broken rarely. */
	if (2 == session->hardware_data.hardware_version && 
		0 == session->hardware_data.hardware_revision &&
		session->max_transfer_blocks > 104)
	{
		session->max_transfer_blocks = 104; /* set to 52k max */
	}
#endif

	/* set key128 from the key and the password */

	lsp_encrypt32_build_combined_key(
		&session->encrypt_ckey,
		session->chap_c,
		(lsp_uint8_t*) &session->login_info.password);

	lsp_decrypt32_build_combined_key(
		&session->decrypt_ckey,
		session->chap_c,
		(lsp_uint8_t*) &session->login_info.password);

	lsp_debug("encrypt_ckey : %08X\n", session->encrypt_ckey);
	lsp_debug("decrypt_ckey : %08X\n", session->decrypt_ckey);
	lsp_debug("chap_c : %08X\n", session->chap_c);
	lsp_debug("password : %02X %02X %02X %02X %02X %02X %02X %02X \n",
		session->login_info.password[0], 
		session->login_info.password[1],
		session->login_info.password[2], 
		session->login_info.password[3],
		session->login_info.password[4], 
		session->login_info.password[5],
		session->login_info.password[6],
		session->login_info.password[7]);

	lspp_set_next_phase(context, LSP_SP_FULL_PHASE);

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
		lsp_login_phase_1_prepare,
		lsp_login_phase_2_prepare,
		lsp_login_phase_3_prepare,
		lsp_login_phase_4_prepare
	};

	lsp_session_data_t* session = &context->session;
	LSP_ASSERT(session->phase < countof(prepare_procs));
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
		lsp_login_phase_1_process,
		lsp_login_phase_2_process,
		lsp_login_phase_3_process,
		lsp_login_phase_4_process
	};

	lsp_session_data_t* session = &context->session;
	LSP_ASSERT(session->phase < countof(process_procs));
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

	lspp_clear_request_buffer(context);
	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	/* init pdu_hdr */
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_LOGOUT_REQUEST;
	pdu_hdr->op_flags.u.logout.F = 1;
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

	pdu_hdr = lspp_get_response_buffer(context);
	if (LSP_OPCODE_LOGOUT_RESPONSE != pdu_hdr->op_code)
	{
		session->session_phase = LSP_PHASE_LOGOUT;
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		session->session_phase = LSP_PHASE_LOGOUT;
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGOUT, 2, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
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
	lsp_session_data_t* session;
	lsp_pdu_hdr_t *pdu_hdr;

	lsp_ide_command_request_t* request;

	const lsp_ide_register_param_t* p;
	const lsp_io_data_buffer_t* data_buf;

	lsp_uint8_t extcmd;
	lsp_uint8_t cmdreg;

	session = &context->session;
	request = &session->current_request->u.ide_command.request;
	p = &request->reg;
	data_buf = &request->data_buf;

	LSP_ASSERT(LSP_IDE_PROTOCOL_VERSION_1_0 == 
		session->hardware_data.protocol_version);

	LSP_ASSERT(NULL != p);
	LSP_ASSERT(NULL != data_buf);

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_IDE_COMMAND;

	lspp_clear_request_buffer(context);

	/* initialize pdu header */
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.u.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->target_id = lsp_htonl(session->login_info.unit_no);
	pdu_hdr->lun0 = lsp_htonl(0);
	pdu_hdr->lun1 = lsp_htonl(0);

	ide_data_v0_ptr = &(pdu_hdr->op_data.u.ide_command_v0);
	lsp_memset(ide_data_v0_ptr, 0, sizeof(lsp_ide_data_v0_t));
	
	/* set pdu flags */

	if (data_buf->recv_size > 0)
	{
		LSP_ASSERT(data_buf->recv_buffer);
		pdu_hdr->op_flags.u.ide_command.R = 1;
		pdu_hdr->op_flags.u.ide_command.W = 0;
	}
	if (data_buf->send_size > 0)
	{
		LSP_ASSERT(data_buf->send_buffer);
		pdu_hdr->op_flags.u.ide_command.R = 0;
		pdu_hdr->op_flags.u.ide_command.W = 1;
	}

	/* set device */
	ide_data_v0_ptr->u.device = p->device.device;

	/* translate command */

	extcmd = 0;
	cmdreg = p->command.command;
	switch (cmdreg)
	{
	/* commands translation required */
	case LSP_IDE_CMD_READ_SECTORS:
		cmdreg = LSP_IDE_CMD_READ_DMA;
		break;
	case LSP_IDE_CMD_READ_SECTORS_EXT:
		cmdreg = LSP_IDE_CMD_READ_DMA_EXT;
		extcmd = 1;
		break;
	case LSP_IDE_CMD_WRITE_SECTORS:
		cmdreg = LSP_IDE_CMD_WRITE_DMA;
		break;
	case LSP_IDE_CMD_WRITE_SECTORS_EXT:
		cmdreg = LSP_IDE_CMD_WRITE_DMA_EXT;
		extcmd = 1;
		break;
	/* commands originally supported */
	case LSP_IDE_CMD_READ_DMA:
	case LSP_IDE_CMD_WRITE_DMA:
	case LSP_IDE_CMD_READ_VERIFY_SECTORS:
	case LSP_IDE_CMD_IDENTIFY_DEVICE:
	case LSP_IDE_CMD_SET_FEATURES:
		break;
	case LSP_IDE_CMD_READ_DMA_EXT:
	case LSP_IDE_CMD_WRITE_DMA_EXT:
	case LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT:
		extcmd = 1;
		break;
	default:
		/* V1.0 does not support all the ide commands */
		return LSP_STATUS_NOT_SUPPORTED;
	}

	ide_data_v0_ptr->command = cmdreg;

	/* set location, sector count, feature */
	if (extcmd)
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

	if (LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.u.ide_command.F /* ||
		pdu_hdr->op_data.ide_command.register_data.command != 
		session->current_request->u.ide_command.request.reg.command.command */)
	{
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_IDE_COMMAND, 4, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	return LSP_STATUS_SUCCESS;
}

LSP_INLINE
int
lsp_call
lspp_is_ata_ext_command(lsp_uint8_t cmdreg)
{
	switch (cmdreg)
	{
	case /* DM EXT */ LSP_IDE_CMD_READ_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_READ_STREAM_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_FUA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_STREAM_DMA_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_READ_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_FUA_EXT:

	case /* ND EXT */ LSP_IDE_CMD_FLUSH_CACHE_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_SET_MAX_ADDRESS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_LOG_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_FUA_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_SECTORS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_STREAM_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_LOG_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_MULTIPLE_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_SECTORS_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_STREAM_EXT:

		return 1;

	default:

		return 0;
	}
}

LSP_INLINE 
void 
lsp_call 
lspp_set_ide_header(
	__out lsp_ide_header_t* idehdr,
	__in const lsp_ide_register_param_t* idereg,
	__in_opt const lsp_extended_command_t* pktcmd)
{
	switch (idereg->command.command)
	{
	case /* DM */ LSP_IDE_CMD_READ_DMA:
	case /* DM */ LSP_IDE_CMD_WRITE_DMA:
	case /* DMQ */ LSP_IDE_CMD_READ_DMA_QUEUED:
	case /* DMQ */ LSP_IDE_CMD_WRITE_DMA_QUEUED:
		idehdr->com_type_d_p = 1;
		idehdr->com_type_e = 0;
		break;
	case /* DM EXT */ LSP_IDE_CMD_READ_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_READ_STREAM_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_DMA_FUA_EXT:
	case /* DM EXT */ LSP_IDE_CMD_WRITE_STREAM_DMA_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_READ_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_EXT:
	case /* DMQ EXT */ LSP_IDE_CMD_WRITE_DMA_QUEUED_FUA_EXT:
		idehdr->com_type_d_p = 1;
		idehdr->com_type_e = 1;
		break;
	case /* ND EXT */ LSP_IDE_CMD_FLUSH_CACHE_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT:
	case /* ND EXT */ LSP_IDE_CMD_SET_MAX_ADDRESS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_LOG_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_MULTIPLE_FUA_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_SECTORS_EXT:
	case /* PO EXT */ LSP_IDE_CMD_WRITE_STREAM_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_LOG_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_MULTIPLE_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_SECTORS_EXT:
	case /* PI EXT */ LSP_IDE_CMD_READ_STREAM_EXT:
		idehdr->com_type_e = 1;
		break;
	case /* P/DMQ */ LSP_IDE_CMD_SERVICE:

	case /* DD */ LSP_IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC:
	case /* DR */ LSP_IDE_CMD_DEVICE_RESET:
	case /* ND */ LSP_IDE_CMD_CFA_ERASE_SECTORS:
	case /* ND */ LSP_IDE_CMD_CFA_REQUEST_EXTENDED_ERROR_CODE:
	case /* ND */ LSP_IDE_CMD_CHECK_MEDIA_TYPE_CARD:
	case /* ND */ LSP_IDE_CMD_CHECK_POWER_MODE:
	case /* ND */ LSP_IDE_CMD_CONFIGURE_STREAM:
	case /* ND */ LSP_IDE_CMD_FLUSH_CACHE:
	case /* ND */ LSP_IDE_CMD_GET_MEDIA_STATUS:
	case /* ND */ LSP_IDE_CMD_IDLE:
	case /* ND */ LSP_IDE_CMD_IDLE_IMMEDIATE:
	case /* ND */ LSP_IDE_CMD_MEDIA_EJECT:
	case /* ND */ LSP_IDE_CMD_MEDIA_LOCK:
	case /* ND */ LSP_IDE_CMD_MEDIA_UNLOCK:
	case /* ND */ LSP_IDE_CMD_NOP:
	case /* ND */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS:
	case /* ND */ LSP_IDE_CMD_READ_VERIFY_SECTORS:
	case /* ND */ LSP_IDE_CMD_SECURITY_ERASE_PREPARE:
	case /* ND */ LSP_IDE_CMD_SECURITY_FREEZE_LOCK:
	case /* ND */ LSP_IDE_CMD_SET_FEATURES:
	case /* ND */ LSP_IDE_CMD_SET_MAX:
	case /* ND */ LSP_IDE_CMD_SET_MULTIPLE_MODE:
	case /* ND */ LSP_IDE_CMD_SLEEP:
	case /* ND */ LSP_IDE_CMD_STANDBY:
	case /* ND */ LSP_IDE_CMD_STANDBY_IMMEDIATE:
	case /* ND/P */ LSP_IDE_CMD_DEVICE_CONFIGURATION:
	case /* ND/P */ LSP_IDE_CMD_SMART:
	case /* PI */ LSP_IDE_CMD_CFA_TRANSLATE_SECTOR:
	case /* PI */ LSP_IDE_CMD_IDENTIFY_DEVICE:
	case /* PI */ LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE:
	case /* PI */ LSP_IDE_CMD_READ_BUFFER:
	case /* PI */ LSP_IDE_CMD_READ_MULTIPLE:
	case /* PI */ LSP_IDE_CMD_READ_SECTORS:
	case /* PO */ LSP_IDE_CMD_CFA_WRITE_MULTIPLE_WITHOUT_ERASE:
	case /* PO */ LSP_IDE_CMD_CFA_WRITE_SECTORS_WITHOUT_ERASE:
	case /* PO */ LSP_IDE_CMD_DEVICE_DOWNLOAD_MICROCODE:
	case /* PO */ LSP_IDE_CMD_SECURITY_DISABLE_PASSWORD:
	case /* PO */ LSP_IDE_CMD_SECURITY_ERASE_UNIT:
	case /* PO */ LSP_IDE_CMD_SECURITY_SET_PASSWORD:
	case /* PO */ LSP_IDE_CMD_SECURITY_UNLOCK:
	case /* PO */ LSP_IDE_CMD_WRITE_BUFFER:
	case /* PO */ LSP_IDE_CMD_WRITE_MULTIPLE:
	case /* PO */ LSP_IDE_CMD_WRITE_SECTORS:
		break;
	case /* P  */ LSP_IDE_CMD_PACKET:
		idehdr->com_type_p = 1;
		/* 0 bit of the feature register is set, when the data transfer is DMA */
		if (idereg->reg.named.features & 0x01)
		{
			idehdr->com_type_d_p = 1; 
		}
		LSP_ASSERT(pktcmd);
		LSP_ASSERT(pktcmd->cmd_buffer);
		LSP_ASSERT(pktcmd->cmd_size > 0);
		/* only CDB SCSIOP_SEND_KEY used com_type_k */
		if (0xA3 == pktcmd->cmd_buffer[0])
		{
			idehdr->com_type_k = 1;
		}
		break;
	}
}

static
lsp_status_t
lsp_call
lsp_ide_command_prepare_v1(lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	lsp_pdu_hdr_t *pdu_hdr;

	lsp_ide_header_t ide_header;
	lsp_ide_register_t* ide_register;
	lsp_uint32_t data_trans_len;

	lsp_request_packet_t* request = session->current_request;
	lsp_ide_command_request_t* ide_request = &request->u.ide_command.request;

	const lsp_ide_register_param_t* p = &ide_request->reg;
	const lsp_io_data_buffer_t* data_buf = &ide_request->data_buf;
	const lsp_extended_command_t* ext_cmd = &ide_request->ext_cmd;

	lsp_uint32_t data_buf_send_size = 0, data_buf_recv_size = 0;

	LSP_ASSERT(LSP_IDE_PROTOCOL_VERSION_1_1 == 
		session->hardware_data.protocol_version);

	LSP_ASSERT(NULL != p);
	LSP_ASSERT(NULL != data_buf);
	LSP_ASSERT(NULL != ext_cmd);

	LSP_ASSERT((ext_cmd->cmd_size && ext_cmd->cmd_buffer) || 
		(!ext_cmd->cmd_size && !ext_cmd->cmd_buffer));

	LSP_ASSERT(!(0 != data_buf->recv_size && 0 != data_buf->send_size));

	session->send_buffer = context->request_pdu_buffer;

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	lspp_clear_request_buffer(context);

	session->last_op_code = LSP_OPCODE_IDE_COMMAND;

	/* initialize pdu header */
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.u.ide_command.F = 1;
	pdu_hdr->op_flags.u.ide_command.R = (data_buf->recv_size > 0) ? 1 : 0;
	pdu_hdr->op_flags.u.ide_command.W = (data_buf->send_size > 0) ? 1 : 0;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = (ext_cmd) ? lsp_htons((lsp_uint16_t)ext_cmd->cmd_size) : 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	data_trans_len =
		(pdu_hdr->op_flags.u.ide_command.W) ? (data_buf->send_size) :
		(pdu_hdr->op_flags.u.ide_command.R) ? (data_buf->recv_size) : 0;
	pdu_hdr->data_trans_len = lsp_htonl(data_trans_len);
	pdu_hdr->target_id = lsp_htonl(session->login_info.unit_no);
	pdu_hdr->lun0 = lsp_htonl(0);
	pdu_hdr->lun1 = lsp_htonl(0);

	/* additional payload if ahs_len is set */
	session->send_buffer_length += ext_cmd->cmd_size;

	/* set ide header */
	lsp_memset(&ide_header, 0x00, sizeof(lsp_ide_header_t));
	/* ide_header.com_type_p = (LSP_IDE_CMD_PACKET == p->command.command) ?  1 : 0; */
	/* ide_header.com_type_k = (p->use_type_k) ? 1 : 0; */
	/* ide_header.com_type_d_p = (p->use_dma) ? 1 : 0; */
	ide_header.com_type_w = pdu_hdr->op_flags.u.ide_command.W;
	ide_header.com_type_r = pdu_hdr->op_flags.u.ide_command.R;
	/* ide_header.com_type_e = (p->use_lba48) ? 1 : 0; */

	/* lspp_set_ide_header sets com_type_p, com_type_k, 
	   com_type_d_p, com_type_e using idereg and pktcmd*/
	lspp_set_ide_header(&ide_header, p, ext_cmd);

	/* com_len is set here as host endian first.
	 * endian swapping is done for all ide_command.header in (EC) */
	ide_header.com_len = data_trans_len & 0x03FFFFFF; // 32 -> 26 bit

	/* (EC) convert all ide_command.header to network endian here */
	*(lsp_uint32_t*) &pdu_hdr->op_data.u.ide_command.header = 
		lsp_htonl(*(lsp_uint32_t*)&ide_header);

	/* set ide register */
	ide_register = &(pdu_hdr->op_data.u.ide_command.register_data);
	lsp_memset(ide_register, 0x00, sizeof(lsp_ide_register_t));
	ide_register->device = p->device.device;
	ide_register->command = p->command.command;

	if (ide_header.com_type_e)
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
		ide_register->feature_prev = 0;
		ide_register->feature_cur = p->reg.named.features;
		ide_register->sector_count_prev = 0;
		ide_register->sector_count_cur = p->reg.named.sector_count;
		ide_register->lba_low_prev = 0;
		ide_register->lba_low_cur = p->reg.named.lba_low;
		ide_register->lba_mid_prev = 0;
		ide_register->lba_mid_cur = p->reg.named.lba_mid;
		ide_register->lba_high_prev = 0;
		ide_register->lba_high_cur = p->reg.named.lba_high;
	}

	if (ext_cmd->cmd_size > 0)
	{
		LSP_ASSERT(NULL != ext_cmd->cmd_buffer);
		/* attach ext_cmd to pdu_hdr */
		lsp_memcpy(
			context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t), 
			ext_cmd->cmd_buffer, 
			ext_cmd->cmd_size);
	}

	data_buf_send_size = data_buf->send_size;
	data_buf_recv_size = data_buf->recv_size;

	if (data_buf->send_buffer)
	{
		data_buf_send_size = data_buf_send_size + 3;
		data_buf_send_size = (data_buf_send_size /4) *4;
	}
	if (data_buf->recv_buffer)
	{
		data_buf_recv_size = data_buf_recv_size + 3;
		data_buf_recv_size = (data_buf_recv_size /4) *4;
	}
	
	/* data for packet command should be aligned to 4 bytes */
	if  (LSP_IDE_CMD_PACKET == p->command.command)
	{
		if ((data_buf->send_buffer && 0 != (data_buf_send_size % 4)) ||
			(data_buf->recv_buffer && 0 != (data_buf_recv_size % 4)))
		{
			return LSP_MAKE_ERROR(
				LSP_ERR_FUNC_IDE_COMMAND, 2, 
				LSP_ERR_TYPE_DATA_LEN, 0);
		}
	}

	session->send_data_buffer = data_buf->send_buffer;
	session->send_data_buffer_length = data_buf_send_size;

	session->receive_data_buffer = data_buf->recv_buffer;
	session->receive_data_buffer_length = data_buf_recv_size;

	return LSP_STATUS_SUCCESS;
}


static
lsp_status_t
lsp_call
lsp_ide_command_process_v1(lsp_handle_context_t* context)
{
	lsp_session_data_t* session;
	const lsp_pdu_hdr_t* pdu_hdr;
	const lsp_ide_register_t* ide_register;
	lsp_ide_command_response_t* response;
	lsp_ide_register_param_t* p;

	session = &context->session;
	pdu_hdr = session->pdu_ptrs.header_ptr;
	ide_register = &(pdu_hdr->op_data.u.ide_command.register_data);
	response = &session->current_request->u.ide_command.response;
	p = &response->reg;

	if (LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.u.ide_command.F)
	{
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	/* store results before testing pdu_hdr->response
	   for packet command(and a few other commands), 
	   return values have meaning. */

	lsp_memset(p, 0, sizeof(lsp_ide_register_param_t));
	p->device.device = ide_register->device;
	p->command.command = ide_register->command; // status
	p->reg.ret.err.err_na = ide_register->feature_cur; // error

	/* You can't ensure whether ide_register has 48 bit or not. 
	   So copy all the bytes. */

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
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_IDE_COMMAND, 4, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
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

	lsp_uint16_t vendor_id = vc_request->vendor_id;
	lsp_uint8_t vop_ver = vc_request->vop_ver;
	lsp_uint8_t vop_code = vc_request->vop_code;
	lsp_uint8_t param_length = vc_request->param_length;
	lsp_extended_command_t *ahs_request = vc_request->ahs_request;
	const lsp_uint8_t *param = vc_request->param;
	const lsp_io_data_buffer_t* data_buf = &vc_request->data_buf;

	session->send_buffer = context->request_pdu_buffer;

	lspp_clear_request_buffer(context);

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_VENDOR_SPECIFIC_COMMAND;

	/* initialize pdu header */
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_VENDOR_SPECIFIC_COMMAND;
	pdu_hdr->op_flags.u.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	if(ahs_request) {
		pdu_hdr->ahs_len = (lsp_uint16_t)ahs_request->cmd_size;
		// Assuming ahs is continuous to lsp_pdu_hdr
		lsp_memcpy(pdu_hdr + 1, ahs_request->cmd_buffer, ahs_request->cmd_size);
		session->send_buffer_length += ahs_request->cmd_size;
	} else {
		pdu_hdr->ahs_len = 0;
	}
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.vendor_command.vendor_id = lsp_htons(vendor_id);
	pdu_hdr->op_data.u.vendor_command.vop_ver = vop_ver;
	pdu_hdr->op_data.u.vendor_command.vop_code = vop_code;

	if (param_length > 0) {
		lsp_memcpy(
				   pdu_hdr->op_data.u.vendor_command.vop_parm,
				   param,
				   param_length);
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
lsp_vendor_command_process(
	lsp_handle_context_t* context)
{
	lsp_session_data_t* session = &context->session;
	const lsp_pdu_hdr_t* pdu_hdr = session->pdu_ptrs.header_ptr;

	lsp_request_packet_t* request = session->current_request;
	lsp_vendor_command_request_t* vc_request = &request->u.vendor_command.request;

	void* outbuf;
	lsp_uint32_t outlen;

	if (LSP_OPCODE_VENDOR_SPECIFIC_RESPONSE != pdu_hdr->op_code)
	{
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	if (0 == pdu_hdr->op_flags.u.vendor_command.F)
	{
		return LSP_STATUS_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_VENDOR_COMMAND, 4, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	/* store results */
	/*
	if (vc_request->param_length > 0) {
		lsp_ntohx(
				  vc_request->param, 
				  pdu_hdr->op_data.u.vendor_command.vop_parm, 
				  (8 > vc_request->param_length ? vc_request->param_length : 8));
		if(vc_request->param_length > 8) {
			// Copy parameter2 without endian conversion.
			lsp_memcpy(
					   vc_request->param + 8,
					   pdu_hdr->op_data.u.vendor_command.vop_parm + 8,
					   vc_request->param_length - 8);
		}
	}
*/
	outbuf = lspp_get_ahs_or_dataseg(session);
	outlen = lspp_get_ahs_or_ds_len(context);

	if (outlen && vc_request->ahs_response)
	{
		/* store AHS results */
		lsp_memcpy(
			vc_request->ahs_response->cmd_buffer,
			outbuf,
			outlen <= vc_request->ahs_response->cmd_size ?
				outlen : vc_request->ahs_response->cmd_size);
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
	lsp_uint8_t* param_text;

	lsp_request_packet_t* request = session->current_request;
	lsp_text_command_request_t* text_request = &request->u.text_command.request;

	lsp_uint8_t param_type = text_request->param_type;
	lsp_uint8_t param_ver = text_request->param_ver;
	const lsp_uint8_t *data = text_request->data_in;
	lsp_uint32_t data_in_length = text_request->data_in_length;

	session->send_buffer = context->request_pdu_buffer;

	lspp_clear_request_buffer(context);

	/* payload for the basic header */
	session->send_buffer_length = sizeof(lsp_pdu_hdr_t);

	++session->path_cmd_tag;

	session->last_op_code = LSP_OPCODE_NOP_H2R;

	/* initialize pdu header */
	pdu_hdr = lspp_get_request_buffer(context);
	pdu_hdr->op_code = LSP_OPCODE_TEXT_REQUEST;
	pdu_hdr->op_flags.u.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->hpid);
	pdu_hdr->rpid = lsp_htons(session->rpid);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->path_cmd_tag);
	pdu_hdr->op_data.u.text_command.parm_type = param_type;
	pdu_hdr->op_data.u.text_command.parm_ver = param_ver;

	param_text = context->request_pdu_buffer + sizeof(lsp_pdu_hdr_t);

	/* additional header data */
	lspp_set_ahs_or_ds_len(context, data_in_length);
	lsp_memcpy(param_text, data, data_in_length);
	session->send_buffer_length += data_in_length;

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

	lsp_uint8_t param_type = text_request->param_type;
	lsp_uint8_t param_ver = text_request->param_ver;

	void* outbuf;
	lsp_uint32_t outlen;

	if (LSP_OPCODE_TEXT_RESPONSE != pdu_hdr->op_code ||
		param_type != pdu_hdr->op_data.u.text_command.parm_type ||
		param_ver != pdu_hdr->op_data.u.text_command.parm_ver)
	{
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	if (0 == pdu_hdr->op_flags.u.text_command.F)
	{
		return LSP_STATUS_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_TEXT_COMMAND, 4, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	outbuf = lspp_get_ahs_or_dataseg(session);
	outlen = lspp_get_ahs_or_ds_len(context);

	if (text_request->data_out_length > 0)
	{
		/* store results */
		lsp_memcpy(
			text_request->data_out,
			outbuf,
			text_request->data_out_length);
	}

	text_request->data_out_length = outlen;

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

	lspp_clear_request_buffer(context);

	session->last_op_code = LSP_OPCODE_NOP_H2R;

	pdu_hdr = lspp_get_request_buffer(context);
	/* initialize pdu header */
	pdu_hdr->op_code = LSP_OPCODE_NOP_H2R;
	pdu_hdr->op_flags.u.ide_command.F = 1;
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
		return LSP_STATUS_RESPONSE_HEADER_INVALID;
	}

	if (0 == pdu_hdr->op_flags.vendor_command.F)
	{
		return LSP_STATUS_COMMAND_FAILED;
	}

	if (LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		return LSP_MAKE_ERROR(
			LSP_ERR_FUNC_NOOP, 4, 
			LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}
#else /* __LSP_NOOP_RECEIVE_RESULT__ */
	context = context;
#endif /* __LSP_NOOP_RECEIVE_RESULT__ */

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_complete_request(
	lsp_handle_context_t* h, 
	lsp_request_packet_t* request, 
	lsp_status_t status)
{
	request->status = status;

	if (request->completion_routine)
	{
		return (*request->completion_routine)(h, request);
	}

	return status;
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

		pdu_hdr = lspp_get_request_buffer(context);
		status = session->prepare_proc(context);
		if (LSP_STATUS_SUCCESS != status)
		{
			session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
			return lsp_complete_request(
				context, 
				context->session.current_request, 
				status);
		}

#ifdef LSPIMP_DEBUG
		lsp_debug_payload("REQ PDU", pdu_hdr, sizeof(lsp_pdu_hdr_t));
		if (pdu_hdr->ahs_len > 0)
		{
			lsp_debug_payload(
				"REQ AHS", 
				((char*)pdu_hdr) + sizeof(lsp_pdu_hdr_t), 
				lsp_ntohs(pdu_hdr->ahs_len));
		}
		if (pdu_hdr->dataseg_len > 0)
		{
			lsp_debug_payload(
				"REQ DS", 
				((char*)pdu_hdr) + sizeof(lsp_pdu_hdr_t) + 
				lsp_ntohs(pdu_hdr->ahs_len), 
				lsp_ntohl(pdu_hdr->dataseg_len));
		}
#endif

		lsp_encode_pdu_hdr(context, pdu_hdr);

#ifdef LSPIMP_DEBUG_ENC
		lsp_debug_payload("REQ PDU ENC", pdu_hdr, sizeof(lsp_pdu_hdr_t));
		if (pdu_hdr->ahs_len > 0)
		{
			lsp_debug_payload(
				"REQ AHS ENC", 
				((char*)pdu_hdr) + sizeof(lsp_pdu_hdr_t), 
				lsp_ntohs(pdu_hdr->ahs_len));
		}
		if (pdu_hdr->dataseg_len > 0)
		{
			lsp_debug_payload(
				"REQ DS ENC", 
				((char*)pdu_hdr) + sizeof(lsp_pdu_hdr_t) + 
				lsp_ntohs(pdu_hdr->ahs_len), 
				lsp_ntohl(pdu_hdr->dataseg_len));
		}
#endif

		session->phase_state = LSP_SP_STATE_BEGIN_SEND_HEADER;
		goto reset_phase;

	case LSP_SP_STATE_BEGIN_SEND_HEADER:

		session->phase_state = LSP_SP_STATE_ENCODE_SEND_DATA;

		if (session->session_options & LSP_SO_USE_DISTINCT_SEND_RECEIVE)
		{
			return LSP_REQUIRES_SEND_INTERNAL_DATA;
		}
		else
		{
			return LSP_REQUIRES_SEND;
		}

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
			if (session->session_options & LSP_SO_USE_DISTINCT_SEND_RECEIVE)
			{
				return LSP_REQUIRES_SEND_USER_DATA;
			}
			else
			{
				return LSP_REQUIRES_SEND;
			}
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

		if (session->receive_data_buffer_length > 0)
		{
			session->receive_buffer = session->receive_data_buffer;
			session->receive_buffer_length = session->receive_data_buffer_length;
			session->phase_state = LSP_SP_STATE_END_RECEIVE_DATA;

			if (session->session_options & LSP_SO_USE_DISTINCT_SEND_RECEIVE)
			{
				if (session->receive_buffer == &session->identify_data.ata)
				{
					return LSP_REQUIRES_RECEIVE_INTERNAL_DATA;
				}
				else
				{
					return LSP_REQUIRES_RECEIVE_USER_DATA;
				}
			}
			else
			{
				return LSP_REQUIRES_RECEIVE;
			}
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
		if (session->flags & LSP_SFG_NO_RESPONSE)
		{
			session->phase_state = LSP_SP_STATE_END_RECEIVE_HEADER;
			goto reset_phase;
		}

		lsp_memset(&session->pdu_ptrs, 0, sizeof(session->pdu_ptrs));

		session->receive_buffer = context->response_pdu_buffer;
		session->receive_buffer_length = sizeof(lsp_pdu_hdr_t);
		session->phase_state = LSP_SP_STATE_END_RECEIVE_HEADER;

		if (session->session_options & LSP_SO_USE_DISTINCT_SEND_RECEIVE)
		{
			return LSP_REQUIRES_RECEIVE_INTERNAL_DATA;
		}
		else
		{
			return LSP_REQUIRES_RECEIVE;
		}

	case LSP_SP_STATE_END_RECEIVE_HEADER:

		session->phase_state = LSP_SP_STATE_PROCESS_RECEIVED_HEADER;

		return LSP_REQUIRES_SYNCHRONIZE;

	case LSP_SP_STATE_PROCESS_RECEIVED_HEADER:

		/* noop does not respond */
		if (session->flags & LSP_SFG_NO_RESPONSE)
		{
			/* clear the flag */
			session->flags &= ~(LSP_SFG_NO_RESPONSE);
			session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
			lsp_debug("no response op, returns 0x%X\n", LSP_STATUS_SUCCESS);
			return lsp_complete_request(
				context, 
				context->session.current_request, 
				LSP_STATUS_SUCCESS);
		}

		pdu_hdr = lspp_get_response_buffer(context);

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
		lsp_debug_payload("REP PDU ENC", pdu_hdr, sizeof(lsp_pdu_hdr_t));
#endif

		lsp_decode_pdu_basic_hdr(context, pdu_hdr);

		lsp_debug_payload("REP PDU", pdu_hdr, sizeof(lsp_pdu_hdr_t));

		session->receive_buffer = 
			context->response_pdu_buffer + sizeof(lsp_pdu_hdr_t);

		session->receive_buffer_length = 
			lsp_ntohs(pdu_hdr->ahs_len) + 
			lsp_ntohl(pdu_hdr->dataseg_len);

		if (session->receive_buffer_length > LSP_SESSION_RESPONSE_MAX_AHS_SIZE)
		{
			/* received pdu header indicates the AHS length is larger than 
			 * the buffer. It is most likely header corruption. */
			session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;
			lsp_debug("Process Received Header, returns 0x%X\n", 
				LSP_STATUS_RESPONSE_HEADER_DATA_OVERFLOW);
			return lsp_complete_request(
				context, 
				context->session.current_request, 
				LSP_STATUS_RESPONSE_HEADER_DATA_OVERFLOW);
		}

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

		if (session->session_options & LSP_SO_USE_DISTINCT_SEND_RECEIVE)
		{
			return LSP_REQUIRES_RECEIVE_INTERNAL_DATA;
		}
		else
		{
			return LSP_REQUIRES_RECEIVE;
		}

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

		lsp_debug("Response Processed, returns 0x%X\n", 
			status);

		if (LSP_REQUIRE_MORE_PROCESSING == status)
		{
			goto reset_phase;
		}

		if (LSP_STATUS_SUCCESS == status && session->receive_data_buffer_length > 0)
		{
		
			if ((session->session_options & LSP_SO_USE_EXTERNAL_DATA_DECODE) &&
				&session->identify_data.ata != session->receive_data_buffer)
			{
				session->phase_state = LSP_SP_STATE_DECODE_RECEIVED_DATA;

				lsp_debug("Requires data decode, returns 0x%X\n", 
					LSP_REQUIRES_DATA_DECODE);
				return LSP_REQUIRES_DATA_DECODE;
			}
			else
			{
	    			lsp_decode_data(
	    				context, 
	    				session->receive_data_buffer, 
	    				session->receive_data_buffer_length);
			}

			lsp_debug_payload("REP DAT", 
				session->receive_data_buffer, 
				session->receive_data_buffer_length);
		}

		session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;

		return lsp_complete_request(
			context, 
			context->session.current_request, 
			status);

	case LSP_SP_STATE_DECODE_RECEIVED_DATA:

		session->phase_state = LSP_SP_STATE_PREPARE_SEND_HEADER;

		return lsp_complete_request(
			context, 
			context->session.current_request, 
			LSP_STATUS_SUCCESS);
		
	default:
		return LSP_STATUS_INVALID_SESSION;
	}
}

void*
lsp_call
lsp_get_buffer_to_send(
	__in lsp_handle_t lsp_handle, 
	__out lsp_uint32_t* len)
{
	LSP_ASSERT(NULL != lsp_handle);
	*len = lsp_handle->session.send_buffer_length;
	return lsp_handle->session.send_buffer;
}

void*
lsp_call
lsp_get_buffer_to_receive(
	__in lsp_handle_t lsp_handle,
	__out lsp_uint32_t* len)
{
	LSP_ASSERT(NULL != lsp_handle);
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

	LSP_ASSERT(NULL != h);
	LSP_ASSERT(NULL != login_info);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_LOGIN;
	request->u.login.request.login_info = *login_info;

	return lsp_request(h, request);
}

const lsp_login_info_t*
lsp_call
lsp_get_login_info(
	__in lsp_handle_t h)
{
	LSP_ASSERT(NULL != h);
	return &h->session.login_info;
}

lsp_status_t
lsp_call
lsp_logout(
	__in lsp_handle_t h)
{
	lsp_request_packet_t* request;

	LSP_ASSERT(NULL != h);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_LOGOUT;

	return lsp_request(h, request);
}

lsp_status_t 
lsp_call
lsp_ide_command(
	__in lsp_handle_t h,
	__inout lsp_ide_register_param_t* idereg,
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd)
{
	lsp_request_packet_t* request;
	lsp_status_t status;

	request = &h->session.internal_packets[0];

	lsp_build_ide_command(
		request, 
		h,
		idereg, 
		data_buf, 
		ext_cmd);

	status = lsp_request(h, request);

	*idereg = request->u.ide_command.response.reg;

	return status;
}

void
lsp_call
lsp_build_ide_command(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in const lsp_ide_register_param_t* idereg,
	__in_opt const lsp_io_data_buffer_t* data_buf,
	__in_opt const lsp_extended_command_t* ext_cmd)
{
	lsp_ide_command_request_t* ide_request;

	h = h;

	LSP_ASSERT(NULL != h);
	LSP_ASSERT(NULL != idereg);

	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_IDE_COMMAND;
	ide_request = &request->u.ide_command.request;
	ide_request->reg = *idereg;
	if (data_buf) ide_request->data_buf = *data_buf;
	if (ext_cmd) ide_request->ext_cmd = *ext_cmd;
}

/* TODO: finalize ata handshake */

lsp_status_t
lsp_call
lsp_hs_preset_pio_mode(
	__in lsp_handle_context_t* h,
	__in lsp_request_packet_t* original_request);

lsp_status_t
lsp_call
lsp_hs_preset_pio_mode_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* request);

lsp_status_t
lsp_call
lsp_hs_identify(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request);

lsp_status_t
lsp_call
lsp_hs_identify_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* request);

lsp_status_t
lsp_call
lsp_hs_set_pio_mode(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request);

lsp_status_t
lsp_call
lsp_hs_set_pio_mode_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* request);

lsp_status_t
lsp_call
lsp_hs_set_dma_mode(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request);

lsp_status_t
lsp_call
lsp_hs_set_dma_mode_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* internal_request);

lsp_status_t
lsp_call
lsp_hs_identify_data_integrity_check(
	__in const lsp_ide_identify_device_data_t* ident);

int
lsp_call
lspp_find_pio_mode(
	__inout lsp_ata_handshake_data_t* hsdat,
	__in const lsp_ide_identify_device_data_t* ident,
	__in lsp_uint16_t hardware_version,
	__in lsp_uint16_t hardware_revision);

int
lsp_call
lspp_find_dma_mode(
	__inout lsp_ata_handshake_data_t* hsdat,
	__in const lsp_ide_identify_device_data_t* ident,
	__in lsp_uint16_t hardware_version,
	__in lsp_uint16_t hardware_revision);

lsp_status_t 
lsp_call 
lsp_request_ex_ata_handshake(
	lsp_handle_context_t* h,
	lsp_request_packet_t* request)
{
	h->session.handshake_set_dma_count = 0;
	h->session.handshake_set_pio_count = 0;

	lsp_memset(
		&h->session.handshake_data, 
		0, 
		sizeof(lsp_ata_handshake_data_t));

#ifdef LSPIMP_USE_PRESET_PIO_MODE
	return lsp_hs_preset_pio_mode(h, request);
#else
	return lsp_hs_identify(h, request);
#endif
}

lsp_status_t
lsp_call
lsp_hs_preset_pio_mode(
	__in lsp_handle_context_t* h,
	__in lsp_request_packet_t* original_request)
{
	lsp_request_packet_t* internal_request;

	LSP_ASSERT(original_request->type == LSP_REQUEST_EX_ATA_HANDSHAKE_COMMAND);

	internal_request = &h->session.internal_packets[1];
	lsp_memset(internal_request, 0, sizeof(lsp_request_packet_t));

	lsp_build_ide_set_features(
		internal_request,
		h,
		LSP_IDE_SET_FEATURES_SET_TRANSFER_MODE,
		/* 7:3 5 bits */ LSP_IDE_TRANSFER_MODE_PIO_FLOW_CONTROL | 
		/* 2:0 3 bits */ 0x4,
		0, 0, 0);

	internal_request->original_request = original_request;
	internal_request->completion_routine = 
		lsp_hs_preset_pio_mode_completion;

	return lsp_request(h, internal_request);
}

lsp_status_t
lsp_call
lsp_hs_preset_pio_mode_completion(
	lsp_handle_t h,
	struct _lsp_request_packet_t* request)
{
	/* we do not check the error for the preset pio transaction */
	return lsp_hs_identify(h, request->original_request);
}

lsp_status_t
lsp_call
lsp_hs_identify(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request)
{
	lsp_request_packet_t* internal_request;

	internal_request = &h->session.internal_packets[1];
	lsp_memset(internal_request, 0, sizeof(lsp_request_packet_t));

	if (0 != h->session.handshake_data.device_type)
	{
		lsp_build_ide_identify_packet_device(
			internal_request,
			h,
			&h->session.identify_data.atapi);
	}
	else
	{
		lsp_build_ide_identify(
			internal_request,
			h,
			&h->session.identify_data.ata);
	}

	internal_request->original_request = original_request;
	internal_request->completion_routine = 
		lsp_hs_identify_completion;

	return lsp_request(h, internal_request);
}

lsp_status_t
lsp_call
lsp_hs_identify_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* request)
{
	lsp_ide_identify_device_data_t* ident;
	lsp_ata_handshake_data_t* handshake_data;
	lsp_request_packet_t* original_request;
	lsp_status_t status;
	int i;
    
	if (LSP_STATUS_SUCCESS != request->status)
	{
		if (LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE != request->u.ide_command.
			request.reg.command.command)
		{
			original_request = request->original_request;

			lsp_build_ide_identify_packet_device(
				request,
				h,
				&h->session.identify_data.atapi);

			request->original_request = original_request;
			request->completion_routine = 
				lsp_hs_identify_completion;

			status = lsp_request(h, request);
			return status;
		}
		else
		{
			LSP_ASSERT(NULL != request->original_request);
			status = request->status;
			return lsp_complete_request(h, request->original_request, status);
		}
	}

	LSP_ASSERT(
		(void*)request->u.ide_command.request.data_buf.recv_buffer ==
		(void*)&h->session.identify_data.ata);

	ident = &h->session.identify_data.ata;

	/*
	// The use of this word is optional. If bits (7:0) of this word contain 
	// the signature A5h, bits (15:8) contain the data structure checksum. 
	// The data structure checksum is the two’s complement of the sum of 
	// all bytes in words (254:0) and the byte consisting of bits (7:0) 
	// in word 255. Each byte shall be added with unsigned arithmetic, 
	// and overflow shall be ignored. The sum of all 512 bytes is zero 
	// when the checksum is correct.
	*/
	if (0xA5 == ident->signature)
	{
		status = lsp_hs_identify_data_integrity_check(ident);
		if (LSP_STATUS_SUCCESS != status)
		{
			return lsp_complete_request(h, request->original_request, status);
		}
	}

	handshake_data = &h->session.handshake_data;

	lsp_memset(handshake_data, 0, sizeof(lsp_ata_handshake_data_t));

	handshake_data->device_type = (0 != ident->general_configuration.device_type);

	/* NDAS device only handles LBA mode */
	if (!ident->capabilities.lba_supported)
	{
		status = LSP_STATUS_HANDSHAKE_NO_LBA_SUPPORT;
		return lsp_complete_request(h, request->original_request, status);
	}

	/* LBA 28 */

	handshake_data->lba = ident->capabilities.lba_supported;

	/* LBA 48 */

	if (ident->command_set_support.big_lba &&
		ident->command_set_active.big_lba)
	{
		handshake_data->lba48 = 1;
	}

	/* capacity */
	/* note that ident data are little-endian */

	if (ident->capabilities.lba_supported)
	{
		if (ident->command_set_support.big_lba &&
			ident->command_set_active.big_lba)
		{
			/* LBA 48 */
			handshake_data->lba_capacity.u.low = 
				lsp_letohl(ident->lba48_capacity_lsw);
			handshake_data->lba_capacity.u.high = 
				lsp_letohl(ident->lba48_capacity_msw);
		}
		else
		{
			/* LBA 28 */
			/* 0x0FFFFFFF is the maximum LBA in LBA28 */
			handshake_data->lba_capacity.u.low = 
				lsp_letohl(ident->lba28_capacity);
			handshake_data->lba_capacity.u.high = 0;

			LSP_ASSERT(handshake_data->lba_capacity.u.low <= 0x0FFFFFFF);
		}
	}
	else
	{
		/* CHS mode */
		handshake_data->lba_capacity.u.low = 
			lsp_letohs(ident->num_cylinders);
		handshake_data->lba_capacity.u.low *= 
			lsp_letohs(ident->num_heads);
		handshake_data->lba_capacity.u.low *= 
			lsp_letohs(ident->num_sectors_per_track);
		handshake_data->lba_capacity.u.high = 0;
	}

	/* logical block size */
	if (0 == ident->physical_logical_sector_size.cleared_to_zero &&
		1 == ident->physical_logical_sector_size.set_to_one)
	{
		if ( 1 == ident->physical_logical_sector_size.logical_sector_longer_than_256_words) {
			// Calc Logical sector size.			
			handshake_data->logical_block_size = lsp_letohl(ident->words_per_logical_sector);			
		} else {
			handshake_data->logical_block_size = 512;
		}
		
		if ( 1 == ident->physical_logical_sector_size.multiple_logical_sectors_per_physical_sector) {
			// Calc Physical block size.
//			uint32_t physical_block_size = pow(2, ident->physical_logical_sector_size.logical_sectors_per_physical_sector_pwr2) * handshake_data->logical_block_size;
			
			// Check alignment.
			
		}
	} 
	else 
	{
		/* for now this is fixed to 512 */
		handshake_data->logical_block_size = 512;
	}
	
	if (0 == ident->command_set_support.valid_clear_to_zero &&
		1 == ident->command_set_support.valid_set_to_one)
	{
		handshake_data->support.write_cache = 
			ident->command_set_support.write_cache;
		handshake_data->support.smart_commands = 
			ident->command_set_support.smart_commands;
		handshake_data->support.smart_error_log = 
			ident->command_set_support.smart_error_log;
		handshake_data->support.write_fua = 
			ident->command_set_support.write_fua;
	}
	if (0 == ident->command_set_active.valid_clear_to_zero &&
		1 == ident->command_set_active.valid_set_to_one)
	{
		handshake_data->active.write_cache = 
			ident->command_set_active.write_cache;
		handshake_data->active.smart_commands = 
			ident->command_set_active.smart_commands;
		handshake_data->active.smart_error_log = 
			ident->command_set_active.smart_error_log;
	}

	/* Negotiate transfer modes if required */

	if (0 == h->session.handshake_set_pio_count)
	{
		return lsp_hs_set_pio_mode(h, request->original_request);
	}
	if (0 == h->session.handshake_set_dma_count)
	{
		return lsp_hs_set_dma_mode(h, request->original_request);
	}

	/* set PIO transfer mode in handshake data */

	lspp_find_pio_mode(
		handshake_data,
		ident,
		h->session.hardware_data.hardware_version,
		h->session.hardware_data.hardware_revision);

	/* set DMA transfer mode in handshake data */

	lspp_find_dma_mode(
		handshake_data,
		ident,
		h->session.hardware_data.hardware_version,
		h->session.hardware_data.hardware_revision);

	handshake_data->dma_supported = ident->capabilities.dma_supported;

	handshake_data->num_cylinders = lsp_letohs(ident->num_cylinders);
	handshake_data->num_heads = lsp_letohs(ident->num_heads);
	handshake_data->num_sectors_per_track = lsp_letohs(ident->num_sectors_per_track);

	lsp_memset(
		handshake_data->model_number, 
		0, 
		sizeof(handshake_data->model_number));

	for (i = 0; i < sizeof(ident->model_number); i += 2)
	{
		*(lsp_uint16_t*)&handshake_data->model_number[i] = lsp_byteswap_ushort(
			*(lsp_uint16_t*)&ident->model_number[i]);
	}

	lsp_memset(
		handshake_data->firmware_revision,
		0,
		sizeof(handshake_data->firmware_revision));

	for (i = 0; i < sizeof(ident->firmware_revision); i += 2)
	{
		*(lsp_uint16_t*)&handshake_data->firmware_revision[i] = lsp_byteswap_ushort(
			*(lsp_uint16_t*)&ident->firmware_revision[i]);
	}

	lsp_memset(
		handshake_data->serial_number, 
		0,
		sizeof(handshake_data->serial_number));

	for (i = 0; i < sizeof(ident->serial_number); i += 2)
	{
		*(lsp_uint16_t*)&handshake_data->serial_number[i] = lsp_byteswap_ushort(
			*(lsp_uint16_t*)&ident->serial_number[i]);
	}

	handshake_data->valid = 1;

	request->original_request->u.ata_handshake_command.
		response.handshake_data = handshake_data;

	request->original_request->u.ata_handshake_command.
		response.identify_data.ata = &h->session.identify_data.ata;

	status = LSP_STATUS_SUCCESS;
	
	/* Seagate restriction */
	if (0 == lsp_memcmp(
		h->session.login_info.password,
		LSP_LOGIN_PASSWORD_SEAGATE,
		sizeof(LSP_LOGIN_PASSWORD_SEAGATE)))
	{
		if ('S' != handshake_data->model_number[0] ||
			'T' != handshake_data->model_number[1])
		{
			status = LSP_STATUS_HANDSHAKE_NON_SEAGATE_DEVICE;
		}
	}
		
	return lsp_complete_request(h, request->original_request, status);

}

lsp_status_t
lsp_call
lsp_hs_identify_data_integrity_check(
	__in const lsp_ide_identify_device_data_t* ident)
{
	int i;
	lsp_uint8_t *p;
	lsp_uint8_t sum;

	p = (lsp_uint8_t*) ident;
	sum = 0;
	for (i = 0; i < 512; ++i)
	{
		sum += *p;
	}

	if (0 != sum)
	{
		return LSP_STATUS_HANDSHAKE_DATA_INTEGRITY_FAILURE;
	}

	return LSP_STATUS_SUCCESS;
}

/*
* return non-zero if the set transfer mode is required, 
* otherwise returns zero 
*/
LSP_INLINE
lsp_uint32_t
lspp_find_highest_mode(
	__in lsp_uint8_t support_bits,
	__in lsp_uint8_t active_bits,
	__out lsp_int8_t* highest_level,
	__out lsp_int8_t* active_level)
{
	/* find max_mode until the higher bit is zero */
	/* e.g. 0001 1111 */
	*highest_level = LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;
	*active_level = LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;

	if (support_bits != 0)
	{
		while (support_bits & (1 << ((*highest_level) + 1)))
		{
			++(*highest_level);
		}
	}
	else
	{
		return 0;
	}

	/* find the active mode until the bit is not zero */
	/* e.g. 0000 1000 */
	if (active_bits != 0)
	{
		*active_level = 0;
		while (0 == (active_bits & (1 << (*active_level))))
		{
			++(*active_level);
		}
	}

	return (*active_level) != (*highest_level);
}

int
lsp_call
lspp_find_dma_mode(
	__inout lsp_ata_handshake_data_t* hsdat,
	__in const lsp_ide_identify_device_data_t* ident,
	__in lsp_uint16_t hardware_version,
	__in lsp_uint16_t hardware_revision)
{
	lsp_int8_t highest_level, active_level;
	lsp_uint8_t support_bits, active_bits;
	lsp_uint32_t require_set_transfer_mode;
	lsp_uint8_t transfer_mode;

	highest_level = LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;
	active_level = LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;
	require_set_transfer_mode = 0;

	transfer_mode = LSP_IDE_TRANSFER_MODE_UNSPECIFIED;

	/* DMA transfer mode */

	if (!ident->capabilities.dma_supported)
	{
		hsdat->active.dma_mode = LSP_IDE_TRANSFER_MODE_UNSPECIFIED;
		hsdat->active.dma_level = (lsp_uint8_t)LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;

		hsdat->support.dma_mode = LSP_IDE_TRANSFER_MODE_UNSPECIFIED;
		hsdat->support.dma_level = (lsp_uint8_t)LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED;
	}

	/* HV: LSP_HARDWARE_VERSION SPECIFIC*/
	if ((hardware_version > LSP_HARDWARE_VERSION_2_0) ||
		(hardware_version == LSP_HARDWARE_VERSION_2_0 && 
		hardware_revision != LSP_HARDWARE_V20_REV_0))
	{
		/* 2.0 rev 1 or later supports UDMA */
		support_bits = ident->ultra_dma_support;
		active_bits = ident->ultra_dma_active;

		lsp_debug("UDMA: (bits) support=%02X, active=%02X", 
			support_bits, active_bits);

		if (support_bits)
		{
			require_set_transfer_mode = lspp_find_highest_mode(
				support_bits, 
				active_bits, 
				&highest_level,
				&active_level);

			/* as we comes here only if support_bits is non-zero
			 * highest_level cannot be LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED
			 */
			LSP_ASSERT(highest_level != LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED);

			lsp_debug("UDMA: (level) highest=%d, active=%d, require_to_set=%d", 
				highest_level, active_level, require_set_transfer_mode);

			transfer_mode = LSP_IDE_TRANSFER_MODE_ULTRA_DMA;

			hsdat->active.dma_mode = transfer_mode;
			hsdat->active.dma_level = active_level;

			hsdat->support.dma_mode = transfer_mode;
			hsdat->support.dma_level = highest_level;
		}
	}

	if (LSP_IDE_TRANSFER_MODE_UNSPECIFIED == transfer_mode)
	{
		support_bits = ident->multiword_dma_support;
		active_bits = ident->multiword_dma_active;

		lsp_debug("MWDMA: (bits) support=%02X, active=%02X", 
			support_bits, active_bits);

		if (support_bits)
		{
			require_set_transfer_mode = lspp_find_highest_mode(
				support_bits, 
				active_bits, 
				&highest_level,
				&active_level);

			/* as we comes here only if support_bits is non-zero
			* highest_level cannot be LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED
			*/
			LSP_ASSERT(highest_level != LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED);

			lsp_debug("MWDMA: (level) highest=%d, active=%d, require_to_set=%d", 
				highest_level, active_level, require_set_transfer_mode);

			transfer_mode = LSP_IDE_TRANSFER_MODE_MULTIWORD_DMA;

			hsdat->active.dma_mode = transfer_mode;
			hsdat->active.dma_level = active_level;

			hsdat->support.dma_mode = transfer_mode;
			hsdat->support.dma_level = highest_level;
		}
	}

	if (LSP_IDE_TRANSFER_MODE_UNSPECIFIED == transfer_mode)
	{
		support_bits = ident->singleword_dma_support;
		active_bits = ident->singleword_dma_active;

		lsp_debug("SWDMA: (bits) support=%02X, active=%02X", 
			support_bits, active_bits);

		if (support_bits)
		{
			require_set_transfer_mode = lspp_find_highest_mode(
				support_bits, 
				active_bits, 
				&highest_level,
				&active_level);

			/* as we comes here only if support_bits is non-zero
			* highest_level cannot be LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED
			*/
			LSP_ASSERT(highest_level != LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED);

			lsp_debug("SWDMA: (level) highest=%d, active=%d, require_to_set=%d", 
				highest_level, active_level, require_set_transfer_mode);

			transfer_mode = LSP_IDE_TRANSFER_MODE_SINGLEWORD_DMA;

			hsdat->active.dma_mode = transfer_mode;
			hsdat->active.dma_level = active_level;

			hsdat->support.dma_mode = transfer_mode;
			hsdat->support.dma_level = highest_level;
		}
	}

	return require_set_transfer_mode;
}

int
lsp_call
lspp_find_pio_mode(
	__inout lsp_ata_handshake_data_t* hsdat,
	__in const lsp_ide_identify_device_data_t* ident,
	__in lsp_uint16_t hardware_version,
	__in lsp_uint16_t hardware_revision)
{
	lsp_int8_t i;
	static const lsp_uint16_t pio_timing[] = {
		600, 383, 240, 180, 120 
	};

	LSP_UNREFERENCED_PARAMETER(hardware_version);
	LSP_UNREFERENCED_PARAMETER(hardware_revision);

	hsdat->support.pio_level = (lsp_uint8_t) -1;

	/* bit 2 in zero-based index should be 1 if pio data are valid */ 
	if (!(ident->translation_fields_valid & 0x4))
	{
		return -1;
	}

	if (ident->advanced_pio_modes)
	{
		for (i = 7; i >= 0; --i)
		{
			if ((1 << i) & ident->advanced_pio_modes)
			{
				hsdat->support.pio_level = i + 3;
				break;
			}
		}
	}
	else
	{
		if ((lsp_uint8_t)-1 == hsdat->support.pio_level)
		{
			for (i = 0; i < countof(pio_timing); ++i)
			{
				if (pio_timing[i] == ident->minimum_pio_cycle_time_iordy)
				{
					hsdat->support.pio_level = i;
				}
			}
		}
		if ((lsp_uint8_t)-1 == hsdat->support.pio_level)
		{
			for (i = 0; i < countof(pio_timing); ++i)
			{
				if (pio_timing[i] == ident->minimum_pio_cycle_time)
				{
					hsdat->support.pio_level = i;
					break;
				}
			}
		}
	}

	return 0;
}

lsp_status_t
lsp_call
lsp_hs_set_pio_mode(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request)
{
	const lsp_ide_identify_device_data_t* ident;
	lsp_ata_handshake_data_t* handshake_data;
	lsp_request_packet_t* internal_request;

	ident = &h->session.identify_data.ata;
	handshake_data = &h->session.handshake_data;

	++h->session.handshake_set_pio_count;

	/* negotiate PIO mode */

	lspp_find_pio_mode(
		handshake_data,
		ident,
		h->session.hardware_data.hardware_version,
		h->session.hardware_data.hardware_revision);

	/* if pio mode is not supported, set the dma mode */
	if ((lsp_uint8_t)-1 == handshake_data->support.pio_level)
	{
		return lsp_hs_set_dma_mode(h, original_request);
	}

	internal_request = &h->session.internal_packets[1];
	lsp_memset(internal_request, 0, sizeof(lsp_request_packet_t));

	lsp_build_ide_set_features(
		internal_request,
		h,
		LSP_IDE_SET_FEATURES_SET_TRANSFER_MODE,
		/* 7:3 5 bits */ LSP_IDE_TRANSFER_MODE_PIO_FLOW_CONTROL | 
		/* 2:0 3 bits */ handshake_data->support.pio_level,
		0, 0, 0);

	internal_request->original_request = original_request;
	internal_request->completion_routine = 
		lsp_hs_set_pio_mode_completion;

	return lsp_request(h, internal_request);
}

lsp_status_t
lsp_call
lsp_hs_set_pio_mode_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* request)
{
	/* we do not check the status of the pio mode changes */

	return lsp_hs_set_dma_mode(h, request->original_request);
}

lsp_status_t
lsp_call
lsp_hs_set_dma_mode(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* original_request)
{
	const lsp_ide_identify_device_data_t* ident;
	lsp_ata_handshake_data_t* handshake_data;
	int require_set_transfer_mode;

	ident = &h->session.identify_data.ata;
	handshake_data = &h->session.handshake_data;

	/* DMA transfer mode negotiation */

	require_set_transfer_mode = lspp_find_dma_mode(
		handshake_data,
		ident,
		h->session.hardware_data.hardware_version,
		h->session.hardware_data.hardware_revision);

	if ((0 == h->session.handshake_set_dma_count) ||
		(h->session.handshake_set_dma_count < 2 && require_set_transfer_mode))
	{
		lsp_request_packet_t* internal_request;

		lsp_uint8_t new_dma_mode = handshake_data->support.dma_mode;
		lsp_uint8_t new_dma_level = handshake_data->support.dma_level;

		LSP_ASSERT(new_dma_mode != LSP_IDE_TRANSFER_MODE_UNSPECIFIED);
		LSP_ASSERT(new_dma_level != LSP_IDE_TRANSFER_MODE_LEVEL_UNSPECIFIED);

		internal_request = &h->session.internal_packets[1];
		lsp_memset(internal_request, 0, sizeof(lsp_request_packet_t));

		lsp_build_ide_set_features(
			internal_request,
			h,
			LSP_IDE_SET_FEATURES_SET_TRANSFER_MODE,
			/* 7:3 5 bits */ new_dma_mode | 
			/* 2:0 3 bits */ new_dma_level,
			0, 0, 0);

		internal_request->original_request = original_request;
		internal_request->completion_routine = 
			lsp_hs_set_dma_mode_completion;

		return lsp_request(h, internal_request);
	}

	return lsp_hs_identify(h, original_request);
}

lsp_status_t
lsp_call
lsp_hs_set_dma_mode_completion(
	__in lsp_handle_t h,
	__in struct _lsp_request_packet_t* internal_request)
{
	lsp_status_t status;

	if (LSP_STATUS_SUCCESS != internal_request->status)
	{
		status = LSP_STATUS_HANDSHAKE_SET_TRANSFER_MODE_FAILURE;
		return lsp_complete_request(h, internal_request->original_request, status);
	}

	++h->session.handshake_set_dma_count;

	return lsp_hs_identify(h, internal_request->original_request);
}

lsp_status_t
lsp_call
lsp_get_ide_command_output_register(
	__in lsp_handle_t h, 
	__out lsp_ide_register_param_t* idereg)
{
	lsp_session_data_t* session;

	if (!h) return LSP_STATUS_INVALID_HANDLE;

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
	lsp_uint8_t *param,
	lsp_uint8_t param_length,
	lsp_extended_command_t *ahs_response
	)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session;
	const lsp_pdu_hdr_t* pdu_hdr;
	void* outbuf;
	lsp_uint32_t outlen;

	if (!context) return LSP_STATUS_INVALID_HANDLE;

	session = &context->session;
	pdu_hdr = session->pdu_ptrs.header_ptr;

	if (!pdu_hdr) return LSP_STATUS_INVALID_HANDLE;

	if (LSP_OPCODE_VENDOR_SPECIFIC_COMMAND != session->last_op_code)
	{
		return LSP_STATUS_INVALID_CALL;
	}

#if 0   /* lsp don't know contents of param. Just return it */
	/* store results */
	lsp_ntohx(
		param, 
		pdu_hdr->op_data.vendor_command.vop_parm, 
		param_length);
#else
        lsp_memcpy(param, pdu_hdr->op_data.u.vendor_command.vop_parm, 
		param_length);
#endif

	outbuf = lspp_get_ahs_or_dataseg(session);
	outlen = lspp_get_ahs_or_ds_len(context);

	if (outlen && ahs_response)
	{
		/* store AHS results */
		lsp_memcpy(
			ahs_response->cmd_buffer,
			outbuf,
			outlen <= ahs_response->cmd_size ?
				outlen : ahs_response->cmd_size);
	}

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_get_text_command_result(
	lsp_handle_t* h,
	lsp_uint8_t* data_out,
	lsp_uint32_t* data_out_length)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session;
	lsp_pdu_hdr_t* pdu_hdr;
	lsp_uint32_t datalen;

	if (!context) return LSP_STATUS_INVALID_HANDLE;

	session = &context->session; /* cannot be null */
	pdu_hdr = session->pdu_ptrs.header_ptr;

	if (!pdu_hdr) return LSP_STATUS_INVALID_HANDLE;
	if (!data_out_length) return LSP_STATUS_INVALID_PARAMETER;

	if (LSP_OPCODE_TEXT_REQUEST != session->last_op_code)
	{
		return LSP_STATUS_INVALID_CALL;
	}

	datalen = lspp_get_ahs_or_ds_len(context);

	if (*data_out_length < datalen)
	{
		*data_out_length = datalen;
		return LSP_STATUS_MORE_DATA;
	}

	if (data_out)
	{
		/* store results */
		lsp_memcpy(
			data_out,
			lspp_get_ahs_or_dataseg(session),
			datalen);
	}

	return LSP_STATUS_SUCCESS;
}

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
	__in_opt lsp_extended_command_t *ahs_response)
{
	lsp_request_packet_t* request;
	lsp_vendor_command_request_t* vc_request;

	LSP_ASSERT(NULL != h);
	LSP_ASSERT(NULL != param);
	LSP_ASSERT(param_length <= 12);

	if (param_length > 0 && !param)
	{
		return LSP_STATUS_INVALID_PARAMETER + 0x1000;
	}

	if (param_length > 12)
	{
		return LSP_STATUS_INVALID_PARAMETER + 0x1001;
	}

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = vendor_id;
	vc_request->vop_ver = vop_ver;
	vc_request->vop_code = vop_code;
	vc_request->param = (lsp_uint8_t *)param;
	vc_request->param_length = param_length;
	vc_request->ahs_request = ahs_request;
	vc_request->ahs_response = ahs_response;
	if (data_buf) vc_request->data_buf = *data_buf;

	return lsp_request(h, request);
}

lsp_status_t 
lsp_call 
lsp_text_command(
	__in lsp_handle_t h, 
	__in lsp_uint8_t param_type, 
	__in lsp_uint8_t param_ver, 
	__in_bcount(inbuf_len) const void *inbuf, 
	__in lsp_uint16_t inbuf_len,
	__out_bcount(outbuf_len) void* outbuf,
	__in lsp_uint16_t outbuf_len)
{
	lsp_request_packet_t* request;
	lsp_text_command_request_t* text_request;

	LSP_ASSERT(NULL != h);

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_TEXT_COMMAND;
	text_request = &request->u.text_command.request;
	text_request->param_type = param_type;
	text_request->param_ver = param_ver;
	text_request->data_in = inbuf;
	text_request->data_in_length = inbuf_len;
	text_request->data_out = outbuf;
	text_request->data_out_length = outbuf_len;

	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_request(
	__in lsp_handle_t h,
	__inout lsp_request_packet_t* request)
{
	lsp_session_data_t* session;
	lsp_status_t status;
	int enforce_handshake;

	LSP_ASSERT(NULL != h);
	LSP_ASSERT(NULL != request);

	session = &h->session;

	switch (request->type)
	{
	case LSP_REQUEST_LOGIN:

		if (LSP_PHASE_SECURITY != session->session_phase)
		{
			status = LSP_STATUS_INVALID_SESSION;
			return lsp_complete_request(h, request, status);
		}

		/* Hardware Version 1.0 is assumed */
		session->hardware_data.hardware_version = LSP_HARDWARE_VERSION_1_0;

		if (0 == lsp_memcmp(
			request->u.login.request.login_info.password, 
			LSP_LOGIN_PASSWORD_ANY, 
			sizeof(LSP_LOGIN_PASSWORD_ANY)))
		{
			session->wlk_pwd_index = 0;
		}
		else
		{
			session->wlk_pwd_index = -1;
		}

		lspp_set_next_phase(h, LSP_SP_LOGIN_INIT);
		session->prepare_proc = lsp_login_prepare;
		session->process_proc = lsp_login_process;

		break;

	case LSP_REQUEST_LOGOUT:

		if (LSP_PHASE_FULL_FEATURE != session->session_phase)
		{
			status = LSP_STATUS_INVALID_SESSION;
			return lsp_complete_request(h, request, status);
		}

		session->prepare_proc = lsp_logout_prepare;
		session->process_proc = lsp_logout_process;

		break;

	case LSP_REQUEST_IDE_COMMAND:

		/* lun0 and lun1 must be 0.*/
		//if (0 != request->u.ide_command.request.lun0 || 
		//	0 != request->u.ide_command.request.lun1)
		//{
		//	return request->status = LSP_ERR_INVALID_PARAMETER;
		//}

		enforce_handshake = 1;

		switch (request->u.ide_command.request.reg.command.command)
		{
		case LSP_IDE_CMD_IDENTIFY_DEVICE:
		case LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE:
		case LSP_IDE_CMD_SMART:
			enforce_handshake = 0;
		}

		if (enforce_handshake &&
			request != &h->session.internal_packets[1] &&
			!h->session.handshake_data.valid)
		{
			/* both recv and send buffers are specified */
			status = LSP_STATUS_REQUIRES_HANDSHAKE;
			return lsp_complete_request(h, request, status);
		}

		/* there is no bi-directional IDE command.
		 * caller cannot specify both recv_buffer and send_buffer non-null
		 */
		if (request->u.ide_command.request.data_buf.recv_size > 0 && 
			request->u.ide_command.request.data_buf.send_size > 0)
		{
			/* both recv and send buffers are specified */
			status = LSP_STATUS_INVALID_PARAMETER;
			return lsp_complete_request(h, request, status);
		}

		/* only full feature phase supports IDE commands */
		if (LSP_PHASE_FULL_FEATURE != session->session_phase)
		{
			status = LSP_STATUS_INVALID_SESSION;
			return lsp_complete_request(h, request, status);
		}

		/* discover login does not allow IDE commands */
		if (LSP_LOGIN_TYPE_DISCOVER == session->login_info.login_type)
		{
			status = LSP_STATUS_INVALID_LOGIN_MODE;
			return lsp_complete_request(h, request, status);
		}

		switch (session->hardware_data.protocol_version)
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
			status = LSP_STATUS_UNSUPPORTED_HARDWARE_VERSION;
			return lsp_complete_request(h, request, status);
		}

		break;
	
	case LSP_REQUEST_TEXT_COMMAND:

		/* intentionally, we allow only DISCOVER login type for TEXT_COMMAND */

		/*
		if (LSP_LOGIN_TYPE_DISCOVER != session->login_info.login_type)
		{
			status = LSP_ERR_INVALID_LOGIN_MODE;
			return lsp_complete_request(h, request, status);
		}
		*/

		h->session.prepare_proc = lsp_text_command_prepare;
		h->session.process_proc = lsp_text_command_process;

		break;
	
	case LSP_REQUEST_VENDOR_COMMAND:

		if (LSP_LOGIN_TYPE_DISCOVER == session->login_info.login_type)
		{
			status = LSP_STATUS_INVALID_LOGIN_MODE;
			return lsp_complete_request(h, request, status);
		}

		if (LSP_HARDWARE_VERSION_1_0 == session->hardware_data.hardware_version ||
			LSP_HARDWARE_VERSION_1_1 == session->hardware_data.hardware_version ||
			LSP_HARDWARE_VERSION_2_0 == session->hardware_data.hardware_version)
		{
			if (12 < request->u.vendor_command.request.param_length)
			{
				status = LSP_STATUS_INVALID_PARAMETER;
				return lsp_complete_request(h, request, status);
			}
		}
		else
		{
			status = LSP_STATUS_INVALID_PARAMETER;
			return lsp_complete_request(h, request, status);
		}

		/* there is no bi-directional data flow is allowed.
		* caller cannot specify both recv_buffer and send_buffer non-null
		*/
		if (request->u.vendor_command.request.data_buf.recv_size > 0 && 
			request->u.vendor_command.request.data_buf.send_size > 0)
		{
			/* both recv and send buffers are specified */
			status = LSP_STATUS_INVALID_PARAMETER;
			return lsp_complete_request(h, request, status);
		}

		h->session.prepare_proc = lsp_vendor_command_prepare;
		h->session.process_proc = lsp_vendor_command_process;

		break;

	case LSP_REQUEST_NOOP_COMMAND:
	
		if (LSP_LOGIN_TYPE_DISCOVER == session->login_info.login_type)
		{
			status = LSP_STATUS_INVALID_LOGIN_MODE;
			return lsp_complete_request(h, request, status);
		}

		h->session.prepare_proc = lsp_noop_command_prepare;
		h->session.process_proc = lsp_noop_command_process;

		break;

	case LSP_REQUEST_EX_ATA_HANDSHAKE_COMMAND:

		if (LSP_LOGIN_TYPE_DISCOVER == session->login_info.login_type)
		{
			status = LSP_STATUS_INVALID_LOGIN_MODE;
			return lsp_complete_request(h, request, status);
		}

		return lsp_request_ex_ata_handshake(h, request);

	default:

		status = LSP_STATUS_INVALID_PARAMETER;
		return lsp_complete_request(h, request, status);
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
	__in lsp_uint32_t options)
{
	lsp_session_data_t *session;
	
	LSP_ASSERT(NULL != h);

	session = &h->session;
	session->session_options = options;

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_get_options(
	__in lsp_handle_t h, 
	__out lsp_uint32_t* options)
{
	lsp_session_data_t *session;

	LSP_ASSERT(NULL != h);

	session = &h->session;
	*options = session->session_options;

	return LSP_STATUS_SUCCESS;
}

void
lsp_call
lsp_encrypt_send_data(
	__in lsp_handle_t h,
	__out_bcount(len) void* dst,
	__in_bcount(len) const void* src, 
	__in lsp_uint32_t len)
{
	lsp_session_data_t *session;

	LSP_ASSERT(NULL != h);

	session = &h->session;

	lsp_encrypt32_copy_internal(dst, src, len, session);
}


void
lsp_call
lsp_decrypt_recv_data(
	__in lsp_handle_t h,
	__out_bcount(len) void* dst,
	__in_bcount(len) const void* src, 
	__in lsp_uint32_t len)
{
	lsp_session_data_t *session;

	LSP_ASSERT(NULL != h);

	session = &h->session;

	lsp_decrypt32_copy_internal(dst, src, len, session);
}


void
lsp_call
lsp_encrypt_send_data_inplace(
	__in lsp_handle_t h,
	__inout_bcount(len) void* buf,
	__in lsp_uint32_t len)
{
	lsp_session_data_t *session;

	LSP_ASSERT(NULL != h);

	session = &h->session;

	if (!session->hardware_data.data_encryption_algorithm)
	{
		return;
	}

	lsp_encrypt32_internal(buf, len, session);
}

void
lsp_call
lsp_decrypt_recv_data_inplace(
	__in lsp_handle_t h,
	__inout_bcount(len) void* buf,
	__in lsp_uint32_t len)
{
	lsp_session_data_t *session;

	LSP_ASSERT(NULL != h);

	session = &h->session;

	if (!session->hardware_data.data_encryption_algorithm)
	{
		return;
	}

	lsp_decrypt32_internal(buf, len, session);
}

