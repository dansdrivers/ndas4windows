#ifndef _LSP_BINPARM_H_
#define _LSP_BINPARM_H_

#include <pshpack4.h>

#include <lsp_type.h>

/*  Parameter Types */

#define LSP_BINPARM_CURRENT_VERSION	0

#define	LSP_BINPARM_TYPE_SECURITY		0x1
#define	LSP_BINPARM_TYPE_NEGOTIATION	0x2
#define	LSP_BINPARM_TYPE_TARGET_LIST	0x3
#define	LSP_BINPARM_TYPE_TARGET_DATA	0x4

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

typedef struct _lsp_binparm {
	lsp_uint8  parm_type;
	lsp_uint8  _reserved_1;
	lsp_uint16 _reserved_2;
} lsp_binparm, *lsp_binparm_ptr;

C_ASSERT_SIZEOF(lsp_binparm, 4);

/* LSP_BINPARM_TYPE_SECURITY. */

#define	LSP_LOGIN_TYPE_NORMAL   0x00
#define	LSP_LOGIN_TYPE_DISCOVER 0xFF

/* CHAP authentication parameters. */

#define LSP_AUTH_METHOD_NONE 0x0000 /* 1.1 or later only */
#define	LSP_AUTH_METHOD_CHAP 0x0001

// 
// User ID for NDAS 1.0, 1.1, 2.0
//
#define LSP_FIRST_TARGET_RO_USER  0x00000001
#define LSP_FIRST_TARGET_RW_USER  0x00010001
#define LSP_SECOND_TARGET_RO_USER 0x00000002
#define LSP_SECOND_TARGET_RW_USER 0x00020002
#define	LSP_NDAS_SUPERVISOR       0xFFFFFFFF

//
// User account added at NDAS 2.5 
// User Id is 0~6, Super user id is 7
//
#define LSP_USER_PERMISSION_RO		0x0001
#define LSP_USER_PERMISSION_SW		0x0002
#define LSP_USER_PERMISSION_EW		0x0004
#define LSP_USER_PERMISSION_MASK	0x0007
#define LSP_MAKE_USER_ID(_usernum, _perm) ((_usernum)<<16 | (_perm)) 
#define LSP_USER_NUM_FROM_USER_ID(_uid) (_uid>>16)
#define LSP_USER_PERM_FROM_USER_ID(_uid) (_uid & 0x0ffff)
#define	LSP_USER_SUPERVISOR				0x00000000
#define LSP_SUPERVISOR_USER_NUM			0x0000
#define LSP_DEFAULT_USER_NUM			0x0001
#define LSP_MAX_USER_COUNT				0x0007	// Excluding super user

typedef struct _lsp_authparm_chap {
	lsp_uint32	chap_a;
	lsp_uint32	chap_i;
	lsp_uint32	chap_n;
	union {
		struct {
			lsp_uint32	chap_r[4];
			lsp_uint32	chap_c[1]; /* variable length */
		} v1;	// NDAS 1.0, 1.1, 2.0
		struct {
			lsp_uint32	reserved;
			lsp_uint32	chap_cr[4];	// CHAP_CR is used both for Challenge and result
		} v2;	// NDAS 2.5
	};
} lsp_authparm_chap, *lsp_authparm_chap_ptr;

C_ASSERT_SIZEOF(lsp_authparm_chap, 32);

#define LSP_HASH_ALGORITHM_MD5        0x00000005
#define LSP_HASH_ALGORITHM_AES128		0x00000006 // Uses AES128

#define LSP_CHAP_MAX_CHALLENGE_LENGTH 4

typedef struct _lsp_binparm_security {
	lsp_uint8 parm_type;     /* parameter type */
	lsp_uint8 login_type;    /* login type */
	lsp_uint16 auth_method;  /* authentication method */
	union {
		lsp_uint8 auth_parm[1];	// Variable Length.
		lsp_authparm_chap auth_chap;
	};
} lsp_binparm_security, *lsp_binparm_security_ptr;

C_ASSERT_SIZEOF(lsp_binparm_security, 36);

/* LSP_BINPARM_TYPE_NEGOTIATION */

#define LSP_HW_TYPE_ASIC   0x00
#define LSP_HW_TYPE_UNSPEC 0xFF

#define LSP_HW_VER_1_0    0x00
#define LSP_HW_VER_1_1    0x01
#define LSP_HW_VER_2_0    0x02
#define LSP_HW_VER_2_5    0x03

#define LSP_HW_VER_CUR    0x00
#define LSP_HW_VER_UNSPEC 0xFF

#define LSP_ENCRYPT_ALGO_NONE	0x00
#define LSP_ENCRYPT_ALGO_NKCDE_1	0x01 // Uses Decrypt32/Encrypt32
#define LSP_ENCRYPT_ALGO_AES128	0x02	//  AES32

#define LSP_DIGEST_ALGO_NONE	0x00	// Used by NDAS 1.0, 1.1, 2.0
#define LSP_DIGEST_ALGO_CRC32	0x02

typedef struct _lsp_binparm_negotiation {
	lsp_uint8  parm_type;     /* parameter type */
	lsp_uint8  hw_type;       /* hardware type */
	lsp_uint8  hw_ver;        /* hardware version */
	lsp_uint8  _reserved1_;
	lsp_uint32 slot_cnt;      /* number of slots */
	lsp_uint32 max_blocks;    /* maximum blocks */
	lsp_uint32 max_target_id; /* maximum number of Target ID */
	lsp_uint32 max_lun;       /* maximum number of LUN ID */
	lsp_uint8  options;
	lsp_uint8  hdr_enc_alg;   /* header encryption algorithm */
	lsp_uint8  _reserved12_;	
	lsp_uint8 hdr_dig_alg;   /* header digest algorithm */
	lsp_uint8  _reserved13_;	
	lsp_uint8 dat_enc_alg;   /* data encryption algorithm */
	lsp_uint8  _reserved14_;	
	lsp_uint8 dat_dig_alg;   /* data digest algorithm */
} lsp_binparm_negotiation, *lsp_binparm_negotiation_ptr;

C_ASSERT_SIZEOF(lsp_binparm_negotiation, 28);

/* LSP_BINPARM_TYPE_TARGET_LIST */

typedef	struct _lsp_binparm_target_list_elem {
	lsp_uint32 target_id;
	union {
		struct {
			lsp_uint8  rw_hosts_cnt;
			lsp_uint8  ro_hosts_cnt;
			lsp_uint16 _reserved_;
		} v1;
		struct {
			lsp_uint8  ew_hosts_cnt;
			lsp_uint8  sw_hosts_cnt;
			lsp_uint8  ro_hosts_cnt;
			lsp_uint8 _reserved_;
		} v2;
	};
	lsp_uint64 target_data;
} lsp_binparm_target_list_elem, *lsp_binparm_target_list_elem_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_list_elem, 16);

typedef struct _lsp_target_list {
	lsp_uint8  parm_type;
	lsp_uint8  target_cnt;
	lsp_uint16 _reserved_;
//	lsp_binparm_target_list_elem targets[1];
} lsp_binparm_target_list, *lsp_binparm_target_list_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_list, 4);

/* LSP_BINPARM_TYPE_TARGET_DATA */

#define	LSP_BINPARM_OP_GET	0x00
#define	LSP_BINPARM_OP_SET	0xFF

typedef struct _lsp_target_data {
	lsp_uint8  parm_type;
	lsp_uint8  op_get_or_set;
	lsp_uint16 _reserved_;
	lsp_uint32 target_id;
	lsp_uint64 target_data;
} lsp_binparm_target_data, *lsp_binparm_target_data_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_data, 16);

#include <poppack.h>

#endif /* _LSP_BINPARM_H_ */
