#ifndef _LSP_TYPE_INTERNAL_H_
#define _LSP_TYPE_INTERNAL_H_
#include <lsp_type.h>

/* lsp_session_data */

// hardware version
#define LSP_HW_VERSION_1_0			0
#define LSP_HW_VERSION_1_1			1
#define LSP_HW_VERSION_2_0			2
#define LSP_HW_VERSION_MAX			LSP_HW_VERSION_2_0

// hardware protocol version
#define LSP_PROTO_VERSION_1_0		0
#define LSP_PROTO_VERSION_1_1		1
#define LSP_PROTO_VERSION_MAX		LSP_PROTO_VERSION_1_1

// stages
#define	LSP_SECURITY_PHASE			0
#define	LSP_LOGIN_OPERATION_PHASE	1
#define	LSP_FULL_FEATURE_PHASE		3
#define LSP_LOGOUT_PHASE			4

// parameter types
#define	LSP_PARM_TYPE_TEXT		0x0
#define	LSP_PARM_TYPE_BINARY	0x1

typedef struct _lsp_session_data {
	lsp_uint32 HPID;
	lsp_uint16 RPID;
	lsp_uint16 CPSlot;
	lsp_uint32 iCommandTag;

	lsp_uint32 CHAP_C;
	lsp_uint8  iSessionPhase;
//	lsp_login_type_t LoginType;
	lsp_uint64 iPassword;

	lsp_uint8  HWType;
	lsp_uint8  HWVersion;
	lsp_uint8  HWProtoType;
	lsp_uint8  HWProtoVersion; // Lanscsi/IDE Protocol versions

	lsp_uint32 iNumberofSlot;
	lsp_uint32 iMaxBlocks;
	lsp_uint32 iMaxTargets;
	lsp_uint32 iMaxLUs;
	lsp_uint16 iHeaderEncryptAlgo;
	lsp_uint16 iDataEncryptAlgo;

} lsp_session_data;

typedef struct _lsp_handle_context {
	void* proc_context;
	lsp_transport_proc proc;
	lsp_session_data   session;
} lsp_handle_context;

typedef struct _lsp_pdu_ptrs
{
	int unused;
} lsp_pdu_ptrs;

#endif /* _LSP_TYPE_INTERNAL_H_ */
