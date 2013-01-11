#include "lsp_impl.h"
#include "lsp_type_internal.h"
#include "lsp_hash.h"
#include "lsp_binparm.h"
#include <lsp.h>

/* error macros */
#define ERROR_T_COMPOSITE(FUNC, PHASE, TYPE, RESPONSE) \
	((FUNC) << 16 | (PHASE) << 12 | (TYPE) << 8 | (RESPONSE))

#define lsp_debug __noop

static const SECTOR_SIZE = 512;

static
lsp_uint8
_lsp_recv_pdu(
				 lsp_handle_context *context,
				 lsp_uint8 *buffer,
				 lsp_pdu_pointers *pdu,
				 lsp_uint8 *recv_buffer,
				 lsp_uint32 recv_size
				 )
{
	lsp_error_t err;
	lsp_trans_error_t err_trans;
	lsp_uint16 ahs_len;
	lsp_uint32 dataseg_len;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	size_t recvd;

	if(!context)
		return 1;

	if(recv_buffer)
	{
		err_trans = trans->recv(context->proc_context, (lsp_uint8 *)recv_buffer,
			recv_size, &recvd, 0);
		if(LSP_TRANS_SUCCESS != err_trans)
			return 8;

		if(recv_size != recvd)
			return 9;

		// decrypt data
		if(session->iDataEncryptAlgo)
		{
			lsp_decrypt32(
				(lsp_uint8 *)recv_buffer,
				recv_size,
				(lsp_uint8 *)&session->CHAP_C,
				(lsp_uint8 *)&session->iPassword);
		}
	}

	// read lsp_pdu_hdr
	err_trans = trans->recv(context->proc_context, buffer, sizeof(lsp_pdu_hdr), &recvd, 0);
	if(LSP_TRANS_SUCCESS != err_trans)
		return 2;

	if(sizeof(lsp_pdu_hdr) != recvd)
		return 3;

	pdu->header_ptr = (lsp_pdu_hdr *)buffer;
	buffer += sizeof(lsp_pdu_hdr);

	if(LSP_FULL_FEATURE_PHASE == session->iSessionPhase &&
		session->iHeaderEncryptAlgo)
	{
		lsp_decrypt32(
			(lsp_uint8 *)pdu->header_ptr,
			sizeof(lsp_pdu_hdr),
			(lsp_uint8 *)&session->CHAP_C,
			(lsp_uint8 *)&session->iPassword);
	}

	// read ahs
	ahs_len = lsp_ntohs(pdu->header_ptr->ahs_len);
	if(ahs_len > 0)
	{
		err_trans = trans->recv(context->proc_context, buffer, ahs_len, &recvd, 0);
		if(LSP_TRANS_SUCCESS != err_trans)
			return 4;

		if(ahs_len != recvd)
			return 5;

		pdu->ahs_ptr = (lsp_uint8 *)buffer;

		buffer += ahs_len;

		if(LSP_PROTO_VERSION_1_0 != session->HWProtoVersion &&
			LSP_FULL_FEATURE_PHASE == session->iSessionPhase &&
			session->iHeaderEncryptAlgo)
		{
			lsp_decrypt32(
				(lsp_uint8 *)pdu->ahs_ptr,
				ahs_len,
				(lsp_uint8 *)&session->CHAP_C,
				(lsp_uint8 *)&session->iPassword);
		}
	}

	// read header dig
	pdu->header_dig_ptr = 0;

	// read data segment
	dataseg_len = lsp_ntohl(pdu->header_ptr->dataseg_len);
	if(dataseg_len > 0)
	{
		err_trans = trans->recv(context->proc_context, buffer, dataseg_len, &recvd, 0);
		if(LSP_TRANS_SUCCESS != err_trans)
			return 6;

		if(dataseg_len != recvd)
			return 7;

		pdu->data_seg_ptr = (lsp_uint8 *)buffer;

		buffer += dataseg_len;

		if(LSP_FULL_FEATURE_PHASE == session->iSessionPhase &&
			session->iHeaderEncryptAlgo)
		{
			lsp_decrypt32(
				(lsp_uint8 *)pdu->data_seg_ptr,
				dataseg_len,
				(lsp_uint8 *)&session->CHAP_C,
				(lsp_uint8 *)&session->iPassword);
		}
	}

	// read data dig
	pdu->data_dig_ptr = 0;

	return 0;
}

static
lsp_uint8
_lsp_send_pdu(
	lsp_handle_context *context,
	lsp_pdu_pointers *pdu,
	lsp_uint8 *send_buffer,
	lsp_uint32 send_buffer_size
	)
{
	lsp_error_t err;
	lsp_trans_error_t err_trans;
	lsp_uint32 dataseg_len;
	lsp_uint16 ahs_len;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	size_t sent, sizesend;
	void *wait_handle_pdu = 0, *wait_handle_data = 0;

	if(!context)
		return 1;

	ahs_len = lsp_ntohs(pdu->header_ptr->ahs_len);
	dataseg_len = lsp_ntohl(pdu->header_ptr->dataseg_len);

	if(LSP_PROTO_VERSION_1_0 == session->HWProtoVersion &&
		(0 != ahs_len || dataseg_len < 0))
	{
		return 2;
	}
	else if(LSP_PROTO_VERSION_1_1 != session->HWProtoVersion &&
		(ahs_len < 0 || dataseg_len < 0))
	{
		return 3;
	}

	// encryption
	if(LSP_FULL_FEATURE_PHASE == session->iSessionPhase)
	{
		// encrypt header
		if(session->iHeaderEncryptAlgo)
		{
			lsp_encrypt32(
				(lsp_uint8 *)pdu->header_ptr,
				sizeof(lsp_pdu_hdr),
				(lsp_uint8 *)&session->CHAP_C,
				(lsp_uint8 *)&session->iPassword);

			if(LSP_PROTO_VERSION_1_0 != session->HWProtoVersion &&
				ahs_len > 0)
			{
				lsp_encrypt32(
					(lsp_uint8 *)pdu->ahs_ptr,
					ahs_len,
					(lsp_uint8 *)&session->CHAP_C, 
					(lsp_uint8 *)&session->iPassword);
			}
		}

		// encrypt data
		if(session->iDataEncryptAlgo && dataseg_len > 0)
		{
			lsp_encrypt32(
				(lsp_uint8 *)pdu->data_seg_ptr,
				dataseg_len,
				(lsp_uint8 *)&session->CHAP_C, 
				(lsp_uint8 *)&session->iPassword);
		}
	}

	// send request
	// typedef lsp_error_t (lsp_proc_call *lstproc_send)(void* context, const void* data, size_t len);
	sizesend= sizeof(lsp_pdu_hdr) + ahs_len + dataseg_len;

	err_trans = trans->send(context->proc_context, pdu->header_ptr, sizesend, &sent, &wait_handle_pdu);
	if(LSP_TRANS_SUCCESS != err_trans || (!wait_handle_pdu && sizesend != sent))
		return 4;

	if(send_buffer)
	{
		// encrypt data
		if(session->iDataEncryptAlgo)
		{
			lsp_encrypt32(
				(lsp_uint8 *)send_buffer,
				send_buffer_size,
				(lsp_uint8 *)&session->CHAP_C,
				(lsp_uint8 *)&session->iPassword);
		}

		err_trans = trans->send(context->proc_context, (lsp_uint8 *)send_buffer,
			send_buffer_size, &sent, &wait_handle_data);
		if(LSP_TRANS_SUCCESS != err_trans || (!wait_handle_pdu && send_buffer_size != sent))
			return 5;
	}

	if(wait_handle_pdu)
	{
		err_trans = trans->wait(context->proc_context, &sent, wait_handle_pdu);
		if(LSP_TRANS_SUCCESS != err_trans)
			return 6;

		wait_handle_pdu = 0;

		if(sizesend != sent)
			return 7;
	}


	if(wait_handle_data)
	{
		err_trans = trans->wait(context->proc_context, &sent, wait_handle_data);
		if(LSP_TRANS_SUCCESS != err_trans)
			return 8;

		wait_handle_data = 0;

		if(send_buffer_size != sent)
			return 9;
	}

	return 0;
}

lsp_handle
lsp_call
lsp_create_session(
				   lsp_transport_proc* proc,
				   void* proc_context)
{
	lsp_handle_context* context = 
		(lsp_handle_context*) proc->mem_alloc(proc_context, sizeof(lsp_handle_context));

	if (0 == context) return 0;
	lsp_memset(context, 0, sizeof(lsp_handle_context));
	context->proc = *proc;
	context->proc_context = proc_context;
	return (lsp_handle) context;
}

void
lsp_call
lsp_destroy_session(lsp_handle h)
{
	lsp_handle_context* context = (lsp_handle_context*) h;
	context->proc.mem_free(context->proc_context, context);
}

lsp_error_t 
lsp_call
lsp_get_proc_context(lsp_handle h, void **proc_context)
{
	lsp_handle_context* context = (lsp_handle_context*) h;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	*proc_context = context->proc_context;
	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call
lsp_set_proc_context(lsp_handle h, void *proc_context)
{
	lsp_handle_context* context = (lsp_handle_context*) h;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	context->proc_context = proc_context;
	return LSP_ERR_SUCCESS;
}

/* protocol specific macro */
#define SET_AHS_OR_DATA_SEG(PDU, HW_PROTO_VERSION, PARAM) \
	if (LSP_IDEPROTO_VERSION_1_0 == (HW_PROTO_VERSION)) \
	{ \
		(PDU).data_seg_ptr = (lsp_uint8 *)(PARAM); \
	} \
	else if (LSP_IDEPROTO_VERSION_1_1 == (HW_PROTO_VERSION)) \
	{ \
		(PDU).ahs_ptr = (lsp_uint8 *)(PARAM); \
	} \
	else \
	{ \
		return LSP_ERR_INVALID_HANDLE; \
	}

#define SET_AHS_OR_DATA_SEG_LEN(PDU_HDR, HW_PROTO_VERSION, LENGTH) \
	if (LSP_IDEPROTO_VERSION_1_0 == (HW_PROTO_VERSION)) \
	{ \
		(PDU_HDR)->dataseg_len = lsp_htonl(LENGTH); \
	} \
	else if (LSP_IDEPROTO_VERSION_1_1 == (HW_PROTO_VERSION)) \
	{ \
		(PDU_HDR)->ahs_len = lsp_htons(LENGTH); \
	} \
	else \
	{ \
		return LSP_ERR_INVALID_HANDLE; \
	}

#define GET_AHS_OR_DATA_SEG(PDU, HW_PROTO_VERSION) \
	((LSP_IDEPROTO_VERSION_1_0 == (HW_PROTO_VERSION)) ? (PDU).data_seg_ptr : \
	(LSP_IDEPROTO_VERSION_1_1 == (HW_PROTO_VERSION)) ? (PDU).ahs_ptr : 0)

#define GET_ASH_OR_DATA_SEG_LEN(PDU_HDR, HW_PROTO_VERSION) \
	((LSP_IDEPROTO_VERSION_1_0 == (HW_PROTO_VERSION)) ? lsp_ntohl((PDU_HDR)->dataseg_len) : \
	(LSP_IDEPROTO_VERSION_1_1 == (HW_PROTO_VERSION)) ? lsp_ntohs((PDU_HDR)->ahs_len) : 0)


#define CHECK_DATA_SEGMENT(PDU, PDU_HDR, HW_PROTO_VERSION, SUB_SEQ_NUM, VALUE) \
	if (LSP_IDEPROTO_VERSION_1_0 == (HW_PROTO_VERSION)) \
	{ \
		if(lsp_ntohl((PDU_HDR)->dataseg_len) < (VALUE) || !(PDU).data_seg_ptr) \
		{ \
			return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, (SUB_SEQ_NUM), LSP_ERR_TYPE_DATA_LEN, 0); \
		} \
	} \
	else if (LSP_IDEPROTO_VERSION_1_1 == (HW_PROTO_VERSION)) \
	{ \
		if(lsp_ntohs((PDU_HDR)->ahs_len) < (VALUE) || !(PDU).ahs_ptr) \
		{ \
			return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, (SUB_SEQ_NUM), LSP_ERR_TYPE_DATA_LEN, 0); \
		} \
	} \
	else \
	{ \
		return LSP_ERR_INVALID_HANDLE; \
	}

lsp_error_t 
lsp_call
lsp_login(
		  lsp_handle h, 
		  const lsp_login_info_ptr login_info
		  )
{
	lsp_error_t              err;
	lsp_uint8                err_pdu;
	lsp_uint8                pdu_buffer[LSP_MAX_REQUEST_SIZE] = {0};
	lsp_handle_context*      context = (lsp_handle_context*) h;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	lsp_uint8                *hw_version = &session->HWVersion;
	lsp_uint8                *hw_proto_version = &session->HWProtoVersion;
	lsp_pdu_pointers         pdu;
	lsp_pdu_hdr*             pdu_hdr;
	lsp_binparm_security_ptr param_secu;
	lsp_authparm_chap_ptr    param_chap;
	lsp_binparm_negotiation_ptr param_nego;
	lsp_uint32               chap_i;
	lsp_uint32               user_id;
	lsp_uint16               sub_seq_num;

	/* parameter validation */

	if(!context || !login_info)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (!(LSP_LOGIN_TYPE_DISCOVER == login_info->login_type ||
		LSP_LOGIN_TYPE_NORMAL   == login_info->login_type) )
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	// try with V1.0 assumed
	*hw_version = LSP_HW_VERSION_1_0;

retry_with_correct_version:
	session->HPID = 0;
	session->RPID = 0; // need not ( not sure )
	session->iCommandTag = 0;

	// suppose HWProtoVersion to max
	// HWProtoVersion will be set correctly after this login phase
	*hw_proto_version = LSP_PROTO_VERSION_MAX;

	// initialize context
	sub_seq_num = 0;
	session->iSessionPhase = LSP_SECURITY_PHASE;
	session->iHeaderEncryptAlgo = 0;
	session->iDataEncryptAlgo = 0;

	// 1st login phase
	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->HPID);

	SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST);

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;
	pdu_hdr->op_data.login.ver_max = *hw_version; // phase specific
	pdu_hdr->op_data.login.ver_min = 0; // phase specific

	param_secu = (lsp_binparm_security_ptr)(pdu_buffer + sizeof(lsp_pdu_hdr));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	pdu.header_ptr = pdu_hdr;
	SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu)

	// send & receive pdu
	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.login.T ||
		LSP_SECURITY_PHASE != pdu_hdr->op_flags.login.CSG ||
		LSP_SECURITY_PHASE != pdu_hdr->op_flags.login.NSG ||
		LSP_HW_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);

	// Set HWVersion, HWProtoVersion with detected one.
	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		/* phase specific */
		if(LSP_ERR_RESPONSE_RI_VERSION_MISMATCH == pdu_hdr->response)
		{
			if(*hw_version != pdu_hdr->op_data.login_response.ver_active)
			{
				*hw_version = pdu_hdr->op_data.login_response.ver_active;
				goto retry_with_correct_version;
			}
		}

		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	/* phase specific */
	*hw_version = pdu_hdr->op_data.login_response.ver_active;
	*hw_proto_version = 
		(*hw_version == LSP_HW_VERSION_1_0) ? LSP_PROTO_VERSION_1_0 : LSP_PROTO_VERSION_1_1;

	CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY);

	param_secu = (lsp_binparm_security_ptr)
		((LSP_PROTO_VERSION_1_0 == *hw_proto_version) ? pdu.data_seg_ptr : pdu.ahs_ptr);

	if(LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type ||
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);

	/* phase specific */
	// store data
	session->RPID = lsp_ntohs(pdu_hdr->rpid);

	// 2nd login phase
	sub_seq_num++;
	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);

	SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST);

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_ptr)(pdu_buffer + sizeof(lsp_pdu_hdr));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_ptr)param_secu->auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);

	pdu.header_ptr = pdu_hdr;
	SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);

	// send & receive pdu
	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 != pdu_hdr->op_flags.login.T ||
		LSP_SECURITY_PHASE != pdu_hdr->op_flags.login.CSG ||
		LSP_SECURITY_PHASE != pdu_hdr->op_flags.login.NSG ||
		LSP_HW_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY);

	param_secu = (lsp_binparm_security_ptr)
		((LSP_PROTO_VERSION_1_0 == *hw_proto_version) ? pdu.data_seg_ptr : pdu.ahs_ptr);

	if(LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type ||
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);

	/* phase specific */
	// store data
	param_chap = &param_secu->auth_chap;
	chap_i = lsp_ntohl(param_chap->chap_i);
	session->CHAP_C = lsp_ntohl(param_chap->chap_c[0]);

	// 3rd login phase
	sub_seq_num++;
	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.login.T = 1;
	pdu_hdr->op_flags.login.CSG = LSP_SECURITY_PHASE;
	pdu_hdr->op_flags.login.NSG = LSP_LOGIN_OPERATION_PHASE;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);

	SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST);

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	param_secu = (lsp_binparm_security_ptr)(pdu_buffer + sizeof(lsp_pdu_hdr));
	param_secu->parm_type = LSP_BINPARM_TYPE_SECURITY;
	param_secu->login_type = login_info->login_type;
	param_secu->auth_method = lsp_htons(LSP_AUTH_METHOD_CHAP);

	/* phase specific */
	param_chap = (lsp_authparm_chap_ptr)param_secu->auth_parm;
	param_chap->chap_a = lsp_htonl(LSP_HASH_ALGORITHM_MD5);
	param_chap->chap_i = lsp_htonl(chap_i);

	user_id =
		(login_info->supervisor) ? LSP_NDAS_SUPERVISOR :
			(0 == login_info->unit_no) ?
				((login_info->write_access) ? 
					LSP_FIRST_TARGET_RW_USER : LSP_FIRST_TARGET_RO_USER) :
				((login_info->write_access) ? 
					LSP_SECOND_TARGET_RW_USER : LSP_SECOND_TARGET_RO_USER);
	param_chap->chap_n =
		(LSP_LOGIN_TYPE_NORMAL == login_info->login_type) ? lsp_htonl(user_id) : 0;

	session->iPassword = login_info->password;
	// hashin...
	lsp_hash32to128(
		(lsp_uint8 *)param_chap->chap_r,
		(lsp_uint8 *)&session->CHAP_C,
		(lsp_uint8 *)&session->iPassword);

	pdu.header_ptr = pdu_hdr;
	SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);

	// send & receive pdu
	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.login.T ||
		LSP_SECURITY_PHASE != pdu_hdr->op_flags.login.CSG ||
		LSP_LOGIN_OPERATION_PHASE != pdu_hdr->op_flags.login.NSG ||
		LSP_HW_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY);

	param_secu = (lsp_binparm_security_ptr)
		((LSP_PROTO_VERSION_1_0 == *hw_proto_version) ? pdu.data_seg_ptr : pdu.ahs_ptr);

	if(LSP_BINPARM_TYPE_SECURITY != param_secu->parm_type ||
		login_info->login_type != param_secu->login_type ||
		lsp_htons(LSP_AUTH_METHOD_CHAP) != param_secu->auth_method)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);

	session->iSessionPhase = LSP_LOGIN_OPERATION_PHASE;

	// 4th login phase
	sub_seq_num++;
	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_LOGIN_REQUEST;
	pdu_hdr->op_flags.login.T = 1;
	pdu_hdr->op_flags.login.CSG = LSP_SECURITY_PHASE;
	pdu_hdr->op_flags.login.NSG = LSP_LOGIN_OPERATION_PHASE;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);

	SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, *hw_proto_version, LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST);

	pdu_hdr->cmd_subpkt_seq = lsp_htons(sub_seq_num);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.login.parm_type = 1;
	pdu_hdr->op_data.login.parm_ver = 0;

	/* phase specific */
	param_nego = (lsp_binparm_negotiation_ptr)(pdu_buffer + sizeof(lsp_pdu_hdr));
	param_nego->parm_type = LSP_BINPARM_TYPE_NEGOTIATION;

	pdu.header_ptr = pdu_hdr;
	SET_AHS_OR_DATA_SEG(pdu, *hw_proto_version, param_secu);

	// send & receive pdu
	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if (LSP_OPCODE_LOGIN_RESPONSE != pdu_hdr->op_code ||
		0 == pdu_hdr->op_flags.login.T ||
		LSP_LOGIN_OPERATION_PHASE != pdu_hdr->op_flags.login.CSG ||
		LSP_FULL_FEATURE_PHASE != pdu_hdr->op_flags.login.NSG ||
		LSP_HW_VERSION_MAX < pdu_hdr->op_data.login_response.ver_active ||
		LSP_PARM_TYPE_BINARY != pdu_hdr->op_data.login_response.parm_type ||
		LSP_BINPARM_CURRENT_VERSION != pdu_hdr->op_data.login_response.parm_ver)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_HEADER, 0);

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	CHECK_DATA_SEGMENT(pdu, pdu_hdr, *hw_proto_version, sub_seq_num, LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY);

	param_nego = (lsp_binparm_negotiation_ptr)
		((LSP_PROTO_VERSION_1_0 == *hw_proto_version) ? pdu.data_seg_ptr : pdu.ahs_ptr);

	if(LSP_BINPARM_TYPE_NEGOTIATION != param_nego->parm_type)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);

	/* phase specific */
	session->HWType = param_nego->hw_type;
	session->HWProtoType = param_nego->hw_type;
	if(session->HWVersion != param_nego->hw_ver)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGIN, sub_seq_num, LSP_ERR_TYPE_REPLY_PARM, 0);

	session->iNumberofSlot = lsp_ntohl(param_nego->slot_cnt);
	session->iMaxBlocks = lsp_ntohl(param_nego->max_blocks);
	session->iMaxTargets = lsp_ntohl(param_nego->max_target_id);
	session->iMaxLUs = lsp_ntohl(param_nego->max_lun);
	session->iHeaderEncryptAlgo = lsp_ntohs(param_nego->hdr_enc_alg);
	session->iDataEncryptAlgo = lsp_ntohs(param_nego->dat_enc_alg);

	session->iSessionPhase = LSP_FULL_FEATURE_PHASE;

	session->LoginType = login_info->login_type;

	// V2.0 bug : A data larger than 52k can be broken rarely.
	if(2 == session->HWVersion && session->iMaxBlocks > 104)
		session->HWVersion = 104; // set to 52k max

	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call
lsp_logout(
		   lsp_handle h)
{
	lsp_error_t              err;
	lsp_uint8                err_pdu;
	lsp_uint8                pdu_buffer[LSP_MAX_REQUEST_SIZE] = {0};
	lsp_handle_context* context = (lsp_handle_context*) h;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	lsp_pdu_pointers         pdu;
	lsp_pdu_hdr*             pdu_hdr;

	/* parameter validation */

	if(!context)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(LSP_FULL_FEATURE_PHASE != session->iSessionPhase)
	{
		session->iSessionPhase = LSP_LOGOUT_PHASE;
		return LSP_ERR_SUCCESS;
	}
	
	session->iCommandTag++;

	/* init pdu_hdr */
	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_LOGOUT_REQUEST;
	pdu_hdr->op_flags.logout.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);

	pdu.header_ptr = pdu_hdr;

	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
	{
		session->iSessionPhase = LSP_LOGOUT_PHASE;
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGOUT, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);
	}

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
	{
		session->iSessionPhase = LSP_LOGOUT_PHASE;
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGOUT, 1, LSP_ERR_TYPE_RECV_PDU, err_pdu);
	}

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_LOGOUT_RESPONSE != pdu_hdr->op_code)
	{
		session->iSessionPhase = LSP_LOGOUT_PHASE;
		return LSP_ERR_REPLY_FAIL;
	}

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
	{
		session->iSessionPhase = LSP_LOGOUT_PHASE;
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_LOGOUT, 2, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
	}

	session->iSessionPhase = LSP_LOGOUT_PHASE;
	return LSP_ERR_SUCCESS;
}

static
lsp_error_t 
_lsp_ide_command_v0(
					lsp_handle_context *context, 
					lsp_uint32 target_id, 
					lsp_uint32 lun0,
					lsp_uint32 lun1,
					lsp_ide_register_param_ptr p,
					lsp_io_data_buffer_ptr data_buf)
{
	lsp_error_t err;
	lsp_uint8 err_pdu;
	lsp_trans_error_t err_trans;
	lsp_uint8 pdu_buffer[LSP_MAX_REQUEST_SIZE];
	lsp_pdu_hdr *pdu_hdr;
	lsp_ide_data_v0 *ide_data_v0_ptr;
	lsp_pdu_pointers pdu;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	size_t					sent, recvd;
	void* wait_handle = 0;

	if(LSP_PROTO_VERSION_1_0 != session->HWProtoVersion)
		return LSP_ERR_INVALID_PARAMETER;

	session->iCommandTag++;

	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);	

	// initialize pdu header
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->target_id = lsp_htonl(target_id);
	pdu_hdr->lun0 = lsp_htonl(lun0);
	pdu_hdr->lun1 = lsp_htonl(lun1);

	ide_data_v0_ptr = &(pdu_hdr->op_data.ide_command_v0);
	lsp_memset(ide_data_v0_ptr, 0x00, sizeof(lsp_ide_data_v0));

	ide_data_v0_ptr->dev =	(0 == target_id) ? 0 : 1;

	// set pdu flags
	pdu_hdr->op_flags.ide_command.R = 0;
	pdu_hdr->op_flags.ide_command.W = 0;
	if(data_buf)
	{
		if(data_buf->recv_buffer)
			pdu_hdr->op_flags.ide_command.R = 1;
		if(data_buf->send_buffer)
			pdu_hdr->op_flags.ide_command.W = 1;
	}

	// p->use_dma is ignored, V1.0 supports PIO only
	// set device
	ide_data_v0_ptr->device = p->device.device;

	// translate command
	switch(p->command.command)
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

	// set location, sector count, feature
	if(p->use_48)
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

	lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	pdu.header_ptr = pdu_hdr;

	if(err_pdu = _lsp_send_pdu(
		context, &pdu, 
		data_buf ? data_buf->send_buffer : 0, 
		data_buf ? data_buf->send_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 
		data_buf ? data_buf->recv_buffer : 0, 
		data_buf ? data_buf->recv_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 3, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code)
		return LSP_ERR_REPLY_FAIL;

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	return LSP_ERR_SUCCESS;
}

static
lsp_error_t 
_lsp_ide_command_v1(
					lsp_handle_context *context, 
					lsp_uint32 target_id, 
					lsp_uint32 lun0,
					lsp_uint32 lun1,
					lsp_ide_register_param_ptr p,
					lsp_io_data_buffer_ptr data_buf)
{
	lsp_error_t err;
	lsp_uint8 err_pdu;
	lsp_trans_error_t err_trans;
	lsp_uint8 pdu_buffer[LSP_MAX_REQUEST_SIZE];
	lsp_pdu_hdr *pdu_hdr;
	lsp_ide_header_ptr ide_header;
	lsp_ide_register_ptr ide_register;
	lsp_pdu_pointers pdu;
	lsp_session_data*        session = &context->session;
	lsp_transport_proc*      trans   = &context->proc;
	lsp_uint32				data_trans_len;
	size_t					sent, recvd;
	void* wait_handle = 0;


	if(LSP_PROTO_VERSION_1_1 != session->HWProtoVersion)
		return LSP_ERR_INVALID_PARAMETER;

	session->iCommandTag++;

	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);	

	// initialize pdu header
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_IDE_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->op_flags.ide_command.R =
		(data_buf->recv_buffer && data_buf->recv_size > 0) ? 1 : 0;
	pdu_hdr->op_flags.ide_command.W =
		(data_buf->send_buffer && data_buf->send_size > 0) ? 1 : 0;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	data_trans_len =
		(pdu_hdr->op_flags.ide_command.W) ? (data_buf->send_size) :
		(pdu_hdr->op_flags.ide_command.R) ? (data_buf->recv_size) : 0;
	pdu_hdr->data_trans_len = lsp_htonl(data_trans_len);
	pdu_hdr->target_id = lsp_htonl(target_id);
	pdu_hdr->lun0 = lsp_htonl(lun0);
	pdu_hdr->lun1 = lsp_htonl(lun1);

	// set ide header
	ide_header = &(pdu_hdr->op_data.ide_command.header);
	lsp_memset(ide_header, 0x00, sizeof(lsp_ide_header));
	ide_header->com_type_p = (WIN_PACKETCMD == p->command.command) ?  1 : 0;
	ide_header->com_type_k = 0;
	ide_header->com_type_d_p = (p->use_dma) ? 1 : 0;
	ide_header->com_type_w = pdu_hdr->op_flags.ide_command.W;
	ide_header->com_type_r = pdu_hdr->op_flags.ide_command.R;
	ide_header->com_type_e = (p->use_48) ? 1 : 0;
	ide_header->com_len = data_trans_len & 0x03FFFFFF; // 32 -> 26 bit
	*(lsp_uint32 *)ide_header = lsp_htonl(*(lsp_uint32 *)ide_header);

	// set ide register
	ide_register = &(pdu_hdr->op_data.ide_command.register_data);
	lsp_memset(ide_register, 0x00, sizeof(lsp_ide_register));
	ide_register->device = p->device.device;
	ide_register->command = p->command.command;
	if(p->use_48)
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
		// set prev == cur for NDAS chip protection
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

	lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	pdu.header_ptr = pdu_hdr;

	if(err_pdu = _lsp_send_pdu(
		context, &pdu, 
		data_buf ? data_buf->send_buffer : 0, 
		data_buf ? data_buf->send_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 
		data_buf ? data_buf->recv_buffer : 0, 
		data_buf ? data_buf->recv_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 3, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_IDE_RESPONSE != pdu_hdr->op_code)
		return LSP_ERR_REPLY_FAIL;

	if(0 == pdu_hdr->op_flags.ide_command.F)
		return LSP_ERR_COMMAND_FAILED;

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_IDE_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	// store results
//	ide_header = &pdu.header_ptr->op_data.ide_command.header;
	ide_register = &pdu_hdr->op_data.ide_command.register_data;
	p->command.command = ide_register->command; // status
	p->reg.ret.err.err_na = ide_register->feature_cur; // error

	p->reg.named_48.prev.features = ide_register->feature_prev;
//	p->reg.named_48.cur.features = ide_register->feature_cur; // err
	p->reg.named_48.prev.sector_count = ide_register->sector_count_prev;
	p->reg.named_48.cur.sector_count = ide_register->sector_count_cur;
	p->reg.named_48.prev.lba_low = ide_register->lba_low_prev;
	p->reg.named_48.cur.lba_low = ide_register->lba_low_cur;
	p->reg.named_48.prev.lba_mid = ide_register->lba_mid_prev;
	p->reg.named_48.cur.lba_mid = ide_register->lba_mid_cur;
	p->reg.named_48.prev.lba_high = ide_register->lba_high_prev;
	p->reg.named_48.cur.lba_high = ide_register->lba_high_cur;


	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call
lsp_ide_command(
				lsp_handle h, 
				lsp_uint32 target_id, 
				lsp_uint32 lun0,
				lsp_uint32 lun1,
				lsp_ide_register_param_ptr p,
				lsp_io_data_buffer_ptr data_buf)
{
	lsp_handle_context *context = (lsp_handle_context*) h;
	lsp_session_data* session = &context->session;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(0 != lun0 || 0 != lun1)
		return LSP_ERR_INVALID_PARAMETER;

	if(!p /* || !p2 */) // p2 is null for most case
		return LSP_ERR_INVALID_PARAMETER;

	if(data_buf && 
		((data_buf->recv_buffer && 0 == data_buf->recv_size) ||
		(data_buf->send_buffer && 0 == data_buf->send_size)))
		return LSP_ERR_INVALID_PARAMETER;

	if(LSP_LOGIN_TYPE_DISCOVER == session->LoginType)
		return LSP_ERR_INVALID_LOGIN_MODE;

	switch(context->session.HWProtoVersion)
	{
	case LSP_PROTO_VERSION_1_0:
		return _lsp_ide_command_v0(h, target_id, lun0, lun1, p, data_buf);
	case LSP_PROTO_VERSION_1_1:
		return _lsp_ide_command_v1(h, target_id, lun0, lun1, p, data_buf);
	default:
		return LSP_ERR_INVALID_HANDLE;
	}


	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call 
lsp_text_command(
				 lsp_handle h,
				 lsp_uint8 param_type,
				 lsp_uint8 param_ver,
				 lsp_uint8 *data,
				 lsp_uint16 data_in_length,
				 lsp_uint16 *data_out_length)
{
	lsp_error_t					err;
	lsp_uint8					err_pdu;
	lsp_uint8					pdu_buffer[LSP_MAX_REQUEST_SIZE];
	lsp_pdu_hdr					*pdu_hdr;
	lsp_ide_header_ptr			ide_header;
	lsp_ide_register_ptr		ide_register;
	lsp_pdu_pointers			pdu;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;
	lsp_uint8					*param_text;


	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(LSP_LOGIN_TYPE_DISCOVER != session->LoginType)
		return LSP_ERR_INVALID_LOGIN_MODE;

	session->iCommandTag++;

	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);	

	// initialize pdu header
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_TEXT_REQUEST;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	SET_AHS_OR_DATA_SEG_LEN(pdu_hdr, session->HWProtoVersion, data_in_length);
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.text_command.parm_type = param_type;
	pdu_hdr->op_data.text_command.parm_ver = param_ver;

	if(!data || 0 == data_in_length || !data_out_length)
		return LSP_ERR_INVALID_HANDLE;

	param_text = pdu_buffer + sizeof(lsp_pdu_hdr);
	lsp_memcpy(param_text, data, data_in_length);

	lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	pdu.header_ptr = pdu_hdr;
	SET_AHS_OR_DATA_SEG(pdu, session->HWProtoVersion, param_text);

	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_TEXT_COMMAND, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_TEXT_COMMAND, 3, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_TEXT_RESPONSE != pdu_hdr->op_code ||
		param_type != pdu_hdr->op_data.text_command.parm_type ||
		param_ver != pdu_hdr->op_data.text_command.parm_ver)
		return LSP_ERR_REPLY_FAIL;

	if(0 == pdu_hdr->op_flags.text_command.F)
		return LSP_ERR_COMMAND_FAILED;

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_TEXT_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	if(*data_out_length < GET_ASH_OR_DATA_SEG_LEN(pdu_hdr, session->HWProtoVersion)) // == data_length
	{
		*data_out_length = GET_ASH_OR_DATA_SEG_LEN(pdu_hdr, session->HWProtoVersion);
		return LSP_ERR_INVALID_PARAMETER;
	}

	// store results
	lsp_memcpy(
		data, 
		GET_AHS_OR_DATA_SEG(pdu, session->HWProtoVersion), 
		GET_ASH_OR_DATA_SEG_LEN(pdu_hdr, session->HWProtoVersion));

	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call
lsp_vendor_command(
				   lsp_handle h,
				   lsp_uint16 vendor_id,
				   lsp_uint8 vop_ver,
				   lsp_uint8 vop_code,
				   lsp_uint8 *param,
				   lsp_uint8 param_length,
				   lsp_io_data_buffer_ptr data_buf)
{
	lsp_error_t					err;
	lsp_uint8					err_pdu;
	lsp_trans_error_t			err_trans;
	lsp_uint8					pdu_buffer[LSP_MAX_REQUEST_SIZE];
	lsp_pdu_hdr					*pdu_hdr;
	lsp_ide_header_ptr			ide_header;
	lsp_ide_register_ptr		ide_register;
	lsp_pdu_pointers			pdu;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;
	size_t						sent, recvd;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(!param)
		return LSP_ERR_INVALID_PARAMETER;

	if(LSP_LOGIN_TYPE_DISCOVER == session->LoginType)
		return LSP_ERR_INVALID_LOGIN_MODE;

	if(LSP_HW_VERSION_1_0 == session->HWVersion)
	{
		if(4 != param_length)
			return LSP_ERR_INVALID_PARAMETER;
	}
	else if(LSP_HW_VERSION_1_1 == session->HWVersion ||
		LSP_HW_VERSION_2_0 == session->HWVersion)
	{
		if(8 != param_length)
			return LSP_ERR_INVALID_PARAMETER;
	}
	else
		return LSP_ERR_INVALID_PARAMETER;

	if(data_buf && 
		((data_buf->recv_buffer && 0 == data_buf->recv_size) ||
		(data_buf->send_buffer && 0 == data_buf->send_size)))
		return LSP_ERR_INVALID_PARAMETER;

	session->iCommandTag++;

	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);	

	// initialize pdu header
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_VENDOR_SPECIFIC_COMMAND;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);
	pdu_hdr->op_data.vendor_command.vendor_id = lsp_htons(vendor_id);
	pdu_hdr->op_data.vendor_command.vop_ver = vop_ver;
	pdu_hdr->op_data.vendor_command.vop_code = vop_code;
	lsp_htonx(pdu_hdr->op_data.vendor_command.vop_parm,	param, param_length);

	lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	pdu.header_ptr = pdu_hdr;

	if(err_pdu = _lsp_send_pdu(
		context, &pdu, 
		data_buf ? data_buf->send_buffer : 0, 
		data_buf ? data_buf->send_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_VENDOR_COMMAND, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);

	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 
		data_buf ? data_buf->recv_buffer : 0, 
		data_buf ? data_buf->recv_size : 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_VENDOR_COMMAND, 3, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_VENDOR_SPECIFIC_RESPONSE != pdu_hdr->op_code)
		return LSP_ERR_REPLY_FAIL;

	if(0 == pdu_hdr->op_flags.vendor_command.F)
		return LSP_ERR_COMMAND_FAILED;

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_VENDOR_COMMAND, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);

	// store results
	lsp_ntohx(param, pdu_hdr->op_data.vendor_command.vop_parm, param_length);


	return LSP_ERR_SUCCESS;
}

lsp_error_t 
lsp_call 
lsp_noop_command(
				 lsp_handle h)
{
	lsp_uint8					pdu_buffer[LSP_MAX_REQUEST_SIZE];
	lsp_pdu_hdr					*pdu_hdr;
	lsp_ide_header_ptr			ide_header;
	lsp_ide_register_ptr		ide_register;
	lsp_pdu_pointers			pdu;
	lsp_error_t					err;
	lsp_uint8					err_pdu;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;
	size_t						sent, recvd;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(LSP_LOGIN_TYPE_DISCOVER == session->LoginType)
		return LSP_ERR_INVALID_LOGIN_MODE;

	session->iCommandTag++;

	lsp_memset(pdu_buffer, 0x00, LSP_MAX_REQUEST_SIZE);	

	// initialize pdu header
	pdu_hdr = (lsp_pdu_hdr *)pdu_buffer;
	pdu_hdr->op_code = LSP_OPCODE_NOP_H2R;
	pdu_hdr->op_flags.ide_command.F = 1;
	pdu_hdr->hpid = lsp_htonl(session->HPID);
	pdu_hdr->rpid = lsp_htons(session->RPID);
	pdu_hdr->cpslot = 0;
	pdu_hdr->dataseg_len = 0;
	pdu_hdr->ahs_len = 0;
	pdu_hdr->cmd_subpkt_seq = 0;
	pdu_hdr->path_cmd_tag = lsp_htonl(session->iCommandTag);

	lsp_memset(&pdu, 0x00, sizeof(lsp_pdu_pointers));
	pdu.header_ptr = pdu_hdr;

	if(err_pdu = _lsp_send_pdu(context, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_NOOP, 0, LSP_ERR_TYPE_SEND_PDU, err_pdu);

#ifdef __LSP_NOOP_RECEIVE_RESULT__
	if(err_pdu = _lsp_recv_pdu(context, pdu_buffer, &pdu, 0, 0))
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_NOOP, 3, LSP_ERR_TYPE_RECV_PDU, err_pdu);

	pdu_hdr = pdu.header_ptr;
	if(LSP_OPCODE_NOP_R2H != pdu_hdr->op_code)
		return LSP_ERR_REPLY_FAIL;

	if(0 == pdu_hdr->op_flags.vendor_command.F)
		return LSP_ERR_COMMAND_FAILED;

	if(LSP_ERR_RESPONSE_SUCCESS != pdu_hdr->response)
		return ERROR_T_COMPOSITE(LSP_ERR_FUNC_NOOP, 4, LSP_ERR_TYPE_RESPONSE, pdu_hdr->response);
#endif // __LSP_NOOP_RECEIVE_RESULT__

	return LSP_ERR_SUCCESS;
}

#define LSP_SET_HANDLE_INFO(INFO_TYPE, DATA, DATA_LENGTH, VAR, VAR_TYPE) \
	case (INFO_TYPE) : \
	if((DATA_LENGTH) < sizeof(VAR_TYPE)) \
		return LSP_ERR_INVALID_HANDLE; \
	*(VAR_TYPE *)(DATA) = VAR; \
	break;

lsp_error_t
lsp_call
lsp_get_handle_info(
					lsp_handle h,
					lsp_handle_info_type info_type,
					void *data,
					size_t data_length)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(!data)
		return LSP_ERR_INVALID_PARAMETER;
	
	switch(info_type)
	{
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_type, data, data_length, session->HWType, lsp_uint8)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_version, data, data_length, session->HWVersion, lsp_uint8)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_proto_type, data, data_length, session->HWProtoType, lsp_uint8)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_proto_version, data, data_length, session->HWProtoVersion, lsp_uint8)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_num_slot, data, data_length, session->iNumberofSlot, lsp_uint32)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_max_blocks, data, data_length, session->iMaxBlocks, lsp_uint32)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_max_targets, data, data_length, session->iMaxTargets, lsp_uint32)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_max_lus, data, data_length, session->iMaxLUs, lsp_uint32)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_header_encrypt_algo, data, data_length, session->iHeaderEncryptAlgo, lsp_uint16)
	LSP_SET_HANDLE_INFO(lsp_handle_info_hw_data_encrypt_algo, data, data_length, session->iDataEncryptAlgo, lsp_uint16)
	default:
		LSP_ERR_NOT_SUPPORTED;
	}

	return LSP_ERR_SUCCESS;
}

#undef LSP_SET_HANDLE_INFO
