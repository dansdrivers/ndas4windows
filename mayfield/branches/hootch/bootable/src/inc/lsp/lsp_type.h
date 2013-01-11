/* lsp type definitions */
#ifndef _LSP_TYPE_H_INCLUDED_
#define _LSP_TYPE_H_INCLUDED_

#define lsp_call __stdcall
#define lsp_proc_call __stdcall

typedef __int8  lsp_int8;
typedef __int16 lsp_int16;
typedef __int32 lsp_int32;
typedef __int64 lsp_int64;

typedef unsigned __int8  lsp_uint8;
typedef unsigned __int16 lsp_uint16;
typedef unsigned __int32 lsp_uint32;
typedef unsigned __int64 lsp_uint64;
typedef struct _lsp_uint64_ll
{
	lsp_uint32 low;
	lsp_uint32 high;
} lsp_uint64_ll, *lsp_uint64_ll_ptr;

#define LSP_SUCCESS 0
#define LSP_ERR_SUCCESS 0
#define LSP_ERR_NO_LOGIN_SESSION	0xF10
#define LSP_ERR_INVALID_PARAMETER	0x10
#define LSP_ERR_INVALID_HANDLE		0x20
#define LSP_ERR_NOT_SUPPORTED		0x30
#define LSP_ERR_REPLY_FAIL			0x40
#define LSP_ERR_SEND_FAILED	0x50
#define LSP_ERR_RECV_FAILED 0x60
#define LSP_ERR_COMMAND_FAILED 0x70
#define LSP_ERR_INVALID_LOGIN_MODE	0x80

/* error with phase - type - response */
#define LSP_ERR_COMPOSITE		((ERR) & 0x00ff0000)
#define LSP_ERR_FUNC(ERR)		((ERR) & 0x00ff0000 >> 16)
#define LSP_ERR_PHASE(ERR)		(((ERR) & 0xf000) >> 12)
#define LSP_ERR_TYPE(ERR)		(((ERR) & 0x0f00) >> 8)
#define LSP_ERR_RESPONSE(ERR)	(((ERR) & 0x00ff) >> 0)

// function : 4 bites
#define LSP_ERR_FUNC_NOOP				0xE
#define LSP_ERR_FUNC_LOGIN				0x1
#define LSP_ERR_FUNC_LOGOUT				0x2
#define LSP_ERR_FUNC_TEXT_COMMAND		0x3
#define LSP_ERR_FUNC_TASK_MGMT_COMMAND	0x4
#define LSP_ERR_FUNC_SCSI_COMMAND		0x5
#define LSP_ERR_FUNC_DATA_COMMAND		0x6
#define LSP_ERR_FUNC_IDE_COMMAND		0x8
#define LSP_ERR_FUNC_VENDOR_COMMAND		0xF

// phase : 4 bits

// type : 4 bits
#define LSP_ERR_TYPE_SEND		0x1
#define LSP_ERR_TYPE_RECEIVE	0x2
#define LSP_ERR_TYPE_HEADER		0x3
#define LSP_ERR_TYPE_RESPONSE	0x4
#define LSP_ERR_TYPE_DATA_LEN	0x5
#define LSP_ERR_TYPE_REPLY_PARM	0x6

#define LSP_ERR_TYPE_SEND_PDU	0x8
#define LSP_ERR_TYPE_RECV_PDU	0x9

// response : 8 bits
#define LSP_ERR_RESPONSE_SUCCESS                0x00
#define LSP_ERR_RESPONSE_RI_NOT_EXIST           0x10
#define LSP_ERR_RESPONSE_RI_BAD_COMMAND         0x11
#define LSP_ERR_RESPONSE_RI_COMMAND_FAILED      0x12
#define LSP_ERR_RESPONSE_RI_VERSION_MISMATCH    0x13
#define LSP_ERR_RESPONSE_RI_AUTH_FAILED         0x14
#define LSP_ERR_RESPONSE_T_NOT_EXIST			0x20
#define LSP_ERR_RESPONSE_T_BAD_COMMAND          0x21
#define LSP_ERR_RESPONSE_T_COMMAND_FAILED       0x22
#define LSP_ERR_RESPONSE_T_BROKEN_DATA			0x23

#define LSP_TRANS_SUCCESS						0x00
#define LSP_TRANS_ERROR							0x01

typedef unsigned int lsp_error_t;
typedef unsigned int lsp_trans_error_t;

#ifdef STRICT
struct lsp_handle__ { int unused; };
typedef struct lsp_handle__* lsp_handle;
#else
typedef void* lsp_handle;
#endif

#define LSP_LOGIN_TYPE_NORMAL   0x00
#define LSP_LOGIN_TYPE_DISCOVER 0xFF

typedef lsp_uint8 lsp_login_type_t;

typedef struct _lsp_login_info
{
	lsp_login_type_t login_type;
	lsp_uint64 password;
	lsp_uint64 supervisor_password;
	lsp_uint8 unit_no;
	lsp_uint8 write_access;
} lsp_login_info, *lsp_login_info_ptr;

typedef enum _lsp_handle_info_type
{
	lsp_handle_info_hw_type = 0,
	lsp_handle_info_hw_version,
	lsp_handle_info_hw_proto_type,
	lsp_handle_info_hw_proto_version,
	lsp_handle_info_hw_num_slot,
	lsp_handle_info_hw_max_blocks,
	lsp_handle_info_hw_max_targets,
	lsp_handle_info_hw_max_lus,
	lsp_handle_info_hw_header_encrypt_algo,
	lsp_handle_info_hw_data_encrypt_algo,
} lsp_handle_info_type, *lsp_handle_info_type_ptr;

#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif
#define C_ASSERT_SIZEOF(type, size) C_ASSERT(sizeof(type) == size)

#endif /* _LSP_TYPE_H_INCLUDED_ */
