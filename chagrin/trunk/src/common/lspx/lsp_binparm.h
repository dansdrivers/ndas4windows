#ifndef _LSP_BINPARM_H_
#define _LSP_BINPARM_H_

#include <pshpack4.h>

#include <lsp_type.h>

/*  Parameter Types */

typedef enum _lsp_binparam_version_t {
	LSP_BINPARM_CURRENT_VERSION	= 0
} lsp_binparam_version_t;

typedef enum _lsp_binparam_type_t {
	LSP_BINPARM_TYPE_SECURITY    = 0x1,
	LSP_BINPARM_TYPE_NEGOTIATION = 0x2,
	LSP_BINPARM_TYPE_TARGET_LIST = 0x3,
	LSP_BINPARM_TYPE_TARGET_DATA = 0x4
} lsp_binparam_type_t;

#define LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST		4
#define	LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST		8
#define LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST		32
#define LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST		28
#define	LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST	4
#define	LSP_BINPARM_SIZE_TEXT_TARGET_DATA_REQUEST	16

#define LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY		4
#define	LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY		36
#define LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY		4
#define LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY		28
#define	LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY	4
#define	LSP_BINPARM_SIZE_TEXT_TARGET_DATA_REPLY	8

#define LSP_BINPARM_SIZE_REPLY					36

typedef struct _lsp_binparm_t {
	lsp_uint8  parm_type;
	lsp_uint8  _reserved_1;
	lsp_uint16 _reserved_2;
} lsp_binparm_t;

C_ASSERT_SIZEOF(lsp_binparm_t, 4);

/* LSP_BINPARM_TYPE_SECURITY. */

/* CHAP authentication parameters. */

typedef enum _lsp_auth_method_t {
	LSP_AUTH_METHOD_NONE = 0x0000, /* 1.1 or later only */
	LSP_AUTH_METHOD_CHAP = 0x0001
} lsp_auth_method_t;

typedef enum _lsp_user_t {
	LSP_FIRST_TARGET_RO_USER  = 0x00000001,
	LSP_FIRST_TARGET_RW_USER  = 0x00010001,
	LSP_SECOND_TARGET_RO_USER = 0x00000002,
	LSP_SECOND_TARGET_RW_USER = 0x00020002,
	LSP_NDAS_SUPERVISOR       = 0xFFFFFFFF
} lsp_user_t;

typedef struct _lsp_authparm_chap_t {
	lsp_uint32	chap_a;
	lsp_uint32	chap_i;
	lsp_uint32	chap_n;
	lsp_uint32	chap_r[4];
	lsp_uint32	chap_c[1]; /* variable length */
} lsp_authparm_chap_t;

C_ASSERT_SIZEOF(lsp_authparm_chap_t, 32);

typedef enum _lsp_hash_algorithm_t {
	LSP_HASH_ALGORITHM_MD5 = 0x00000005
} lsp_hash_algorithm_t;

#define LSP_CHAP_MAX_CHALLENGE_LENGTH 4

typedef struct _lsp_binparm_security_t {
	lsp_uint8 parm_type;     /* parameter type */
	lsp_uint8 login_type;    /* login type */
	lsp_uint16 auth_method;  /* authentication method */
	union {
		lsp_uint8 auth_parm[1];	// Variable Length.
		lsp_authparm_chap_t auth_chap;
	};
} lsp_binparm_security_t;

C_ASSERT_SIZEOF(lsp_binparm_security_t, 36);

/* LSP_BINPARM_TYPE_NEGOTIATION */

typedef enum _lsp_hardware_type_t {
	LSP_HARDWARE_TYPE_ASIC = 0x00,
	LSP_HARDWARE_TYPE_UNSPECIFIED = 0xFF
} lsp_hardware_type_t;

typedef enum _lsp_encryption_algorithm_t {
	LSP_ENCRYPT_ALG_NONE    = 0x00,
	LSP_ENCRYPT_ALG_NKCDE_1 = 0x01, /* lsp_enc/lsp_dec */
	LSP_ENCRYPT_ALG_AES128  = 0x02  /* AES32 */
} lsp_encryption_algorithm_t;

typedef enum _lsp_digest_algorithm_t {
	LSP_DIGEST_ALG_NONE  = 0x00, /* 1.0, 1.1 and 2.0 */
	LSP_DIGEST_ALG_CRC32 = 0x02
} lsp_digest_algorithm_t;

typedef struct _lsp_binparm_negotiation_t {
	lsp_uint8  parm_type;     /* parameter type */
	lsp_uint8  hw_type;       /* hardware type */
	lsp_uint8  hw_ver;        /* hardware version */
	lsp_uint8  _reserved_;
	lsp_uint32 slot_cnt;      /* number of slots */
	lsp_uint32 max_blocks;    /* maximum blocks */
	lsp_uint32 max_target_id; /* maximum number of Target ID */
	lsp_uint32 max_lun;       /* maximum number of LUN ID */
#if 1
	lsp_uint8  options;       /* reserved prior to 2.0g, options in 2.0g */
	lsp_uint8  hdr_enc_alg;   /* header encryption algorithm */
	lsp_uint8  _reserved12_;
	lsp_uint8  hdr_dig_alg;   /* header digest algorithm */
	lsp_uint8  _reserved13_;
	lsp_uint8  dat_enc_alg;   /* data encryption algorithm */
	lsp_uint8  _reserved14_;
	lsp_uint8  dat_dig_alg;   /* data digest algorithm */
#else
	lsp_uint16 hdr_enc_alg;   /* header encryption algorithm */
	lsp_uint16 hdr_dig_alg;   /* header digest algorithm */
	lsp_uint16 dat_enc_alg;   /* data encryption algorithm */
	lsp_uint16 dat_dig_alg;   /* data digest algorithm */
#endif
} lsp_binparm_negotiation_t;

C_ASSERT_SIZEOF(lsp_binparm_negotiation_t, 28);

/* LSP_BINPARM_TYPE_TARGET_LIST */

typedef	struct _lsp_binparm_target_list_elem {
	lsp_uint32 target_id;
	lsp_uint8  rw_hosts_cnt;
	lsp_uint8  ro_hosts_cnt;
	lsp_uint16 _reserved_;
	lsp_uint64 target_data;
} lsp_binparm_target_list_elem_t;

C_ASSERT_SIZEOF(lsp_binparm_target_list_elem_t, 16);

typedef struct _lsp_binparm_target_list_t {
	lsp_uint8  parm_type;
	lsp_uint8  target_cnt;
	lsp_uint16 _reserved_;
//	lsp_binparm_target_list_elem targets[1];
} lsp_binparm_target_list_t;

C_ASSERT_SIZEOF(lsp_binparm_target_list_t, 4);

/* LSP_BINPARM_TYPE_TARGET_DATA */

typedef enum _lsp_binparam_op_t {
	LSP_BINPARM_OP_GET = 0x00,
	LSP_BINPARM_OP_SET = 0xFF
} lsp_binparam_op_t;

typedef struct _lsp_binparm_target_data_t {
	lsp_uint8  parm_type;
	lsp_uint8  op_get_or_set;
	lsp_uint16 _reserved_;
	lsp_uint32 target_id;
	lsp_uint64 target_data;
} lsp_binparm_target_data_t;

C_ASSERT_SIZEOF(lsp_binparm_target_data_t, 16);

#include <poppack.h>

#endif /* _LSP_BINPARM_H_ */
