/* lsp type definitions */
#ifndef _LSP_TYPE_H_INCLUDED_
#define _LSP_TYPE_H_INCLUDED_

#include "lspspecstring.h"

#define lsp_call __fastcall

typedef __int8  lsp_int8;
typedef __int16 lsp_int16;
typedef __int32 lsp_int32;
typedef __int64 lsp_int64;

typedef unsigned __int8  lsp_uint8;
typedef unsigned __int16 lsp_uint16;
typedef unsigned __int32 lsp_uint32;
typedef unsigned __int64 lsp_uint64;

typedef union _lsp_large_integer_t
{
#ifdef LSP_BIG_ENDIAN
	struct {
		lsp_int32 high;
		lsp_uint32 low;
	} u;
#else
	struct {
		lsp_uint32 low;
		lsp_int32 high;
	} u;
#endif
	lsp_int64 quad;
} lsp_large_integer_t;

typedef struct _lsp_unsigned_large_integer_t
{
#ifdef LSP_BIG_ENDIAN
	struct {
		lsp_uint32 high;
		lsp_uint32 low;
	} u;
#else
	struct {
		lsp_uint32 low;
		lsp_uint32 high;
	} u;
#endif
	lsp_uint64 quad;
} lsp_unsigned_large_integer_t;

#define LSP_ERR_SUCCESS 0

#pragma deprecated("LSP_ERR_SUCCESS")

typedef enum _lsp_status_enum_t {
	LSP_STATUS_SUCCESS          = 0,
	LSP_REQUIRE_MORE_PROCESSING = 1,
	LSP_REQUIRES_SEND           = 2,
	LSP_REQUIRES_RECEIVE        = 3,
	LSP_REQUIRES_SYNCHRONIZE    = 4,
	LSP_REQUIRES_DATA_ENCODE    = 5,

	LSP_ERR_INVALID_SESSION     = 0x10
} lsp_status_enum_t;

/* The provided buffer was too small to hold the entire value */
typedef enum _lsp_err_ex_t {
	LSP_ERR_INVALID_HANDLE	   = 0x20,
	LSP_ERR_INVALID_PARAMETER  = 0x10,
	LSP_ERR_INVALID_CALL	   = 0x11,
	LSP_ERR_MORE_DATA          = 0x12,
	LSP_ERR_NO_LOGIN_SESSION   = 0xF10,
	LSP_ERR_NOT_SUPPORTED	   = 0x30,
	LSP_ERR_REPLY_FAIL         = 0x40,
	/* LSP_ERR_SEND_FAILED= 0x50, */
	/* LSP_ERR_RECV_FAILED= 0x60, */
	LSP_ERR_COMMAND_FAILED     = 0x70,
	LSP_ERR_INVALID_LOGIN_MODE = 0x80,
	/* new ones */
	LSP_ERR_HEADER_INVALID_DATA_SEGMENT = 0xF11
} lsp_err_ex_t;

/* function : 4 bits */
typedef enum _lsp_err_func_t {
	LSP_ERR_FUNC_NOOP			   = 0xE,
	LSP_ERR_FUNC_LOGIN			   = 0x1,
	LSP_ERR_FUNC_LOGOUT			   = 0x2,
	LSP_ERR_FUNC_TEXT_COMMAND      = 0x3,
	LSP_ERR_FUNC_TASK_MGMT_COMMAND = 0x4,
	LSP_ERR_FUNC_SCSI_COMMAND	   = 0x5,
	LSP_ERR_FUNC_DATA_COMMAND	   = 0x6,
	LSP_ERR_FUNC_IDE_COMMAND	   = 0x8,
	LSP_ERR_FUNC_VENDOR_COMMAND	   = 0xF,
} lsp_err_func_t;
/* phase : 4 bits */

/* type : 4 bits */
typedef enum _lsp_err_type_t {
	/* LSP_ERR_TYPE_SEND       = 0x1, */
	/* LSP_ERR_TYPE_RECEIVE    = 0x2, */
	LSP_ERR_TYPE_HEADER		= 0x3,
	LSP_ERR_TYPE_RESPONSE	= 0x4,
	LSP_ERR_TYPE_DATA_LEN	= 0x5,
	LSP_ERR_TYPE_REPLY_PARM	= 0x6
} lsp_err_type_t;

/* response : 8 bits */
typedef enum _lsp_err_response_t {
	LSP_ERR_RESPONSE_SUCCESS               = 0x00,
	LSP_ERR_RESPONSE_RI_NOT_EXIST          = 0x10,
	LSP_ERR_RESPONSE_RI_BAD_COMMAND        = 0x11,
	LSP_ERR_RESPONSE_RI_COMMAND_FAILED     = 0x12,
	LSP_ERR_RESPONSE_RI_VERSION_MISMATCH   = 0x13,
	LSP_ERR_RESPONSE_RI_AUTH_FAILED        = 0x14,
	LSP_ERR_RESPONSE_T_NOT_EXIST           = 0x20,
	LSP_ERR_RESPONSE_T_BAD_COMMAND         = 0x21,
	LSP_ERR_RESPONSE_T_COMMAND_FAILED      = 0x22,
	LSP_ERR_RESPONSE_T_BROKEN_DATA         = 0x23,
	LSP_ERR_RESPONSE_T_NO_PERMISSION       = 0x24,
	/* Returned when user has access permission(?) */
	LSP_ERR_RESPONSE_T_CONFLICT_PERMISSION = 0x25
} lsp_err_response_t;

/* LSP Session Options */

#define LSP_SO_USE_EXTERNAL_DATA_ENCODE 0x00000001

typedef unsigned int lsp_status_t;
typedef lsp_status_t lsp_error_t;

#pragma deprecated(lsp_error_t)

/* lspx users should not directly modify the context structure */
typedef struct _lsp_handle_context_t lsp_handle_context_t, * lsp_handle_t;

typedef enum _lsp_login_type_t {
	LSP_LOGIN_TYPE_NORMAL   = 0x00,
	LSP_LOGIN_TYPE_DISCOVER = 0xFF
} lsp_login_type_t;

typedef struct _lsp_login_info_t {
	lsp_uint8 login_type;
	lsp_uint64 password;
	lsp_uint64 supervisor_password;
	lsp_uint8 unit_no;
	lsp_uint8 write_access;
} lsp_login_info_t;

typedef enum _lsp_handle_info_type_t {
	LSP_PROP_HW_TYPE = 0,
	LSP_PROP_HW_VERSION,
	LSP_PROP_HW_PROTOCOL_TYPE,
	LSP_PROP_HW_PROTOCOL_VERSION,
	LSP_PROP_HW_SLOTS,
	LSP_PROP_HW_MAX_TRANSFER_BLOCKS,
	LSP_PROP_HW_MAX_TARGETS,
	LSP_PROP_HW_MAX_LUS,
	LSP_PROP_HW_HEADER_ENCRYPTION_ALGORITHM,
	LSP_PROP_HW_DATA_ENCRYPTION_ALGORITHM,
	LSP_PROP_HW_REVISION,
	LSP_PROP_HW_HEADER_DIGEST_ALGORITHM,
	LSP_PROP_HW_DATA_DIGEST_ALGORITHM
} lsp_handle_info_type_t;

typedef enum _lsp_request_type_t {
	LSP_REQUEST_NONE,
	LSP_REQUEST_LOGIN,
	LSP_REQUEST_LOGOUT,
	LSP_REQUEST_VENDOR_COMMAND,
	LSP_REQUEST_TEXT_COMMAND,
	LSP_REQUEST_IDE_COMMAND,
	LSP_REQUEST_NOOP_COMMAND
} lsp_request_type_t;

#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif
#define C_ASSERT_SIZEOF(type, size) C_ASSERT(sizeof(type) == size)

#endif /* _LSP_TYPE_H_INCLUDED_ */
