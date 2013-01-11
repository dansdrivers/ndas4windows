/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

/* lsp type definitions */
#ifndef _LSP_TYPE_H_INCLUDED_
#define _LSP_TYPE_H_INCLUDED_

#if !defined(_MSC_VER) && !defined(__GNUC__)
#error The compiler is not supported.
#endif

#ifdef __GNUC__
#include <stdint.h>
#endif
#if defined(_MSC_VER)
#include <stddef.h> /* size_t */
#endif

#ifdef __GNUC__
#define __lsp_attr_packed__ __attribute__((packed)) 
#else
/* For other system use pshpack?.h */
#define __lsp_attr_packed__ 
#endif

#include "lspspecstring.h"

#if defined(_MSC_VER)
#define lsp_call __fastcall
#else
#define lsp_call
#endif

#if defined(_MSC_VER)

typedef __int8  lsp_int8_t;
typedef __int16 lsp_int16_t;
typedef __int32 lsp_int32_t;
typedef __int64 lsp_int64_t;

typedef unsigned __int8  lsp_uint8_t;
typedef unsigned __int16 lsp_uint16_t;
typedef unsigned __int32 lsp_uint32_t;
typedef unsigned __int64 lsp_uint64_t;

#elif defined(__GNUC__)

typedef int8_t  lsp_int8_t;
typedef int16_t lsp_int16_t;
typedef int32_t lsp_int32_t;
typedef int64_t lsp_int64_t;

typedef uint8_t  lsp_uint8_t;
typedef uint16_t lsp_uint16_t;
typedef uint32_t lsp_uint32_t;
typedef uint64_t lsp_uint64_t;

#endif

typedef union _lsp_large_integer_t
{
#ifdef __BIG_ENDIAN__
	struct {
		lsp_int32_t high;
		lsp_uint32_t low;
	} u;
#else
	struct {
		lsp_uint32_t low;
		lsp_int32_t high;
	} u;
#endif
	lsp_int64_t quad;
} lsp_large_integer_t;

typedef struct _lsp_unsigned_large_integer_t
{
#ifdef __BIG_ENDIAN__
	struct {
		lsp_uint32_t high;
		lsp_uint32_t low;
	} u;
#else
	struct {
		lsp_uint32_t low;
		lsp_uint32_t high;
	} u;
#endif
	lsp_uint64_t quad;
} lsp_unsigned_large_integer_t;

#define LSP_ERR_SUCCESS 0

#ifdef _MSC_VER
#pragma deprecated("LSP_ERR_SUCCESS")
#endif

typedef unsigned int lsp_status_t;
typedef lsp_status_t lsp_error_t;

#ifdef _MSC_VER
#pragma deprecated(lsp_error_t)
#endif

typedef enum _lsp_status_enum_t {
	LSP_STATUS_SUCCESS          = 0,
	LSP_REQUIRE_MORE_PROCESSING = 1,
	LSP_REQUIRES_SEND           = 2,
	LSP_REQUIRES_RECEIVE        = 3,
	LSP_REQUIRES_SYNCHRONIZE    = 4,
	LSP_REQUIRES_DATA_ENCODE    = 5,
	LSP_REQUIRES_DATA_DECODE    = 6,
	LSP_REQUIRES_SEND_INTERNAL_DATA,
	LSP_REQUIRES_SEND_USER_DATA,
	LSP_REQUIRES_RECEIVE_INTERNAL_DATA,
	LSP_REQUIRES_RECEIVE_USER_DATA,
} lsp_status_enum_t;

typedef union _lsp_status_detail_t {
	struct {
		lsp_uint32_t response : 8;
		lsp_uint32_t type : 4;
		lsp_uint32_t sequence : 4;
		lsp_uint32_t function : 4;
		lsp_uint32_t reserved : 12;
	} detail;
	lsp_status_t status;
} lsp_status_detail_t;

/* function : 4 bits */
typedef enum _lsp_err_func_t {
	LSP_ERR_FUNC_NONE              = 0x0,
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
	LSP_ERR_RESPONSE_RI_SET_SEMA_FAIL        = 0x11,
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

#define LSP_MAKE_ERROR(FUNC, PHASE, TYPE, RESPONSE) 	((FUNC) << 16 | (PHASE) << 12 | (TYPE) << 8 | (RESPONSE))

/* The provided buffer was too small to hold the entire value */
typedef enum _lsp_error_status_t {
	LSP_STATUS_INVALID_SESSION     = 0x10,
	LSP_STATUS_INVALID_HANDLE	   = 0x11,
	LSP_STATUS_INVALID_PARAMETER   = 0x12,
	LSP_STATUS_INVALID_CALL	       = 0x13,
	LSP_STATUS_INVALID_LOGIN_MODE  = 0x14,
	LSP_STATUS_MORE_DATA           = 0x15,
	LSP_STATUS_NOT_SUPPORTED	   = 0x16,
	LSP_STATUS_UNSUPPORTED_HARDWARE_VERSION         = 0x81,
	LSP_STATUS_REQUIRES_HANDSHAKE                   = 0x82,
	LSP_STATUS_HANDSHAKE_DATA_INTEGRITY_FAILURE     = 0x83,
	LSP_STATUS_HANDSHAKE_NO_LBA_SUPPORT             = 0x84,
	LSP_STATUS_HANDSHAKE_SET_TRANSFER_MODE_FAILURE  = 0x85,
	LSP_STATUS_HANDSHAKE_TRANSFER_MODE_NEGO_FAILURE = 0x86,
	LSP_STATUS_HANDSHAKE_NON_SEAGATE_DEVICE			= 0x87,
	LSP_STATUS_COMMAND_FAILED						= 0x21,
	LSP_STATUS_RESPONSE_HEADER_INVALID				= 0x22,
	LSP_STATUS_RESPONSE_HEADER_INVALID_DATA_SEGMENT = 0x23,
	LSP_STATUS_RESPONSE_HEADER_DATA_OVERFLOW		= 0xF1,
	
	LSP_STATUS_WRITE_ACCESS_DENIED = 
		LSP_MAKE_ERROR(
			LSP_ERR_FUNC_LOGIN,2,
			LSP_ERR_TYPE_RESPONSE,
			LSP_ERR_RESPONSE_T_COMMAND_FAILED),
	LSP_STATUS_LOCK_FAILED = 
		LSP_MAKE_ERROR(
			LSP_ERR_FUNC_VENDOR_COMMAND, 4, 
			LSP_ERR_TYPE_RESPONSE, 
			LSP_ERR_RESPONSE_RI_SET_SEMA_FAIL),
} lsp_error_status_t;

/* LSP Session Options */

typedef enum _LSP_SESSION_OPTIONS {
	LSP_SO_USE_EXTERNAL_DATA_ENCODE = 0x00000001,
	LSP_SO_USE_EXTERNAL_DATA_DECODE = 0x00000002,
	LSP_SO_USE_DISTINCT_SEND_RECEIVE = 0x00000004,
} LSP_SESSION_OPTIONS;

/* lspx users should not directly modify the context structure */
typedef struct _lsp_handle_context_t * lsp_handle_t;

typedef enum _lsp_login_type_t {
	LSP_LOGIN_TYPE_NORMAL   = 0x00,
	LSP_LOGIN_TYPE_DISCOVER = 0xFF
} lsp_login_type_t;

typedef enum _lsp_access_mode_t {
	LSP_WRITE_ACCESS_NONE = 0,
	LSP_WRITE_ACCESS_ANY = 1,
	LSP_WRITE_ACCESS_SHARED_PRIMARY = 2,
	LSP_WRITE_ACCESS_SHARED_SECONDARY = 3,
	LSP_WRITE_ACCESS_SHARED_ANY = 4,
	LSP_WRITE_ACCESS_EXCLUSIVE = 5, /* reserved for 2.5 */
} lsp_access_mode_t;

typedef struct _lsp_login_info_t {
	lsp_uint8_t login_type;
       lsp_uint8_t _reserved_1[7];
	lsp_uint8_t password[8];
	lsp_uint8_t supervisor_password[8];
	lsp_uint8_t unit_no;
	lsp_uint8_t write_access;
       lsp_uint8_t _reserved_2[6];
} lsp_login_info_t;

typedef struct _lsp_ata_handshake_data_t {
	lsp_uint8_t valid : 1;
	lsp_uint8_t device_type : 1; /* 0: ata, 1: atapi */
	lsp_uint8_t lba   : 1;
	lsp_uint8_t lba48 : 1; /* valid only if lba = 1 */
	lsp_uint8_t dma_supported : 1;
	lsp_uint8_t reserved : 3;
	struct {
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t smart_commands : 1 ;
		lsp_uint8_t smart_error_log : 1 ;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t reserved : 4;
		lsp_uint8_t pio_reserved_1; /* pio_mode; */
		lsp_uint8_t pio_level;
		lsp_uint8_t dma_mode;
		lsp_uint8_t dma_level;
	} support;
	struct {
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t smart_commands : 1 ;
		lsp_uint8_t smart_error_log : 1 ;
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t reserved : 4;
		lsp_uint8_t pio_reserved_1; /* pio_mode; */
		lsp_uint8_t pio_reserved_2; /* pio_level; */
		lsp_uint8_t dma_mode;
		lsp_uint8_t dma_level;
	} active;
	/* lba_capacity is LBA + 1 */
	lsp_large_integer_t lba_capacity;
	lsp_uint32_t logical_block_size;
	lsp_uint16_t num_cylinders;
	lsp_uint16_t num_heads;
	lsp_uint16_t num_sectors_per_track;
	lsp_uint8_t model_number[40 + 4];
	lsp_uint8_t firmware_revision[8 + 4];
	lsp_uint8_t serial_number[20 + 4];
	 
} lsp_ata_handshake_data_t;

typedef struct _lsp_hardware_data_t {
	lsp_uint8_t  hardware_type;
	lsp_uint8_t  hardware_version;
	lsp_uint8_t  protocol_type;
	lsp_uint8_t  protocol_version;
	lsp_uint16_t hardware_revision; /* hardware revision, new in 2.0g */
	lsp_uint32_t number_of_slots;
	lsp_uint32_t maximum_transfer_blocks;
	lsp_uint32_t maximum_target_id;
	lsp_uint32_t maximum_lun;
	lsp_uint8_t  header_encryption_algorithm;
	lsp_uint8_t  data_encryption_algorithm;
	lsp_uint8_t  header_digest_algorithm; /* new in 2.0g */
	lsp_uint8_t  data_digest_algorithm; /* new in 2.0g */
} lsp_hardware_data_t;

//typedef enum _lsp_handle_info_type_t {
//	LSP_PROP_HW_TYPE = 0,
//	LSP_PROP_HW_VERSION,
//	LSP_PROP_HW_PROTOCOL_TYPE,
//	LSP_PROP_HW_PROTOCOL_VERSION,
//	LSP_PROP_HW_SLOTS,
//	LSP_PROP_HW_MAX_TRANSFER_BLOCKS,
//	LSP_PROP_HW_MAX_TARGETS,
//	LSP_PROP_HW_MAX_LUS,
//	LSP_PROP_HW_HEADER_ENCRYPTION_ALGORITHM,
//	LSP_PROP_HW_DATA_ENCRYPTION_ALGORITHM,
//	LSP_PROP_HW_REVISION,
//	LSP_PROP_HW_HEADER_DIGEST_ALGORITHM,
//	LSP_PROP_HW_DATA_DIGEST_ALGORITHM
//} lsp_handle_info_type_t;

typedef enum _lsp_request_type_t {
	LSP_REQUEST_NONE,
	LSP_REQUEST_LOGIN,
	LSP_REQUEST_LOGOUT,
	LSP_REQUEST_VENDOR_COMMAND,
	LSP_REQUEST_TEXT_COMMAND,
	LSP_REQUEST_IDE_COMMAND,
	LSP_REQUEST_NOOP_COMMAND,
	LSP_REQUEST_EX_ATA_HANDSHAKE_COMMAND
} lsp_request_type_t;

#ifndef LSP_C_ASSERT
#define LSP_C_ASSERT(t, e) typedef char __LSP_C_ASSERT__##t[(e)?1:-1]
#endif

#ifndef LSP_C_ASSERT_SIZEOF
#define LSP_C_ASSERT_SIZEOF(type, size) LSP_C_ASSERT(type, sizeof(type) == size)
#endif

#endif /* _LSP_TYPE_H_INCLUDED_ */
